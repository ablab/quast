/*
 * $Id: wgraph.c,v 1.189.2.14 2016/09/14 18:22:41 markisch Exp $
 */

/* GNUPLOT - win/wgraph.c */
/*[
 * Copyright 1992, 1993, 1998, 2004   Maurice Castro, Russell Lang
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
 * AUTHORS
 *
 *   Maurice Castro
 *   Russell Lang
 */

#include "syscfg.h"

#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>           /* for _chdir */
#include "winmain.h"
#include "wresourc.h"
#include "wcommon.h"
#include "term_api.h"         /* for enum JUSTIFY */
#ifdef USE_MOUSE
# include "gpexecute.h"
# include "mouse.h"
# include "command.h"
#endif
#include "color.h"
#include "getcolor.h"
#ifdef HAVE_GDIPLUS
# include "wgdiplus.h"
#endif
#include "plot.h"

#ifndef WM_MOUSEHWHEEL /* requires _WIN32_WINNT >= 0x0600 */
# define WM_MOUSEHWHEEL 0x020E
#endif

#ifdef USE_MOUSE
/* Petr Mikulik, February 2001
 * Declarations similar to src/os2/gclient.c -- see section
 * "PM: Now variables for mouse" there in.
 */

/* Status of the ruler */
static struct Ruler {
    TBOOLEAN on;		/* ruler active ? */
    int x, y;			/* ruler position */
} ruler = {FALSE,0,0,};

/* Status of the line from ruler to cursor */
static struct RulerLineTo {
    TBOOLEAN on;		/* ruler line active ? */
    int x, y;			/* ruler line end position (previous cursor position) */
} ruler_lineto = {FALSE,0,0,};

/* Status of zoom box */
static struct Zoombox {
    TBOOLEAN on;		/* set to TRUE during zooming */
    POINT from, to;		/* corners of the zoom box */
    LPCSTR text1, text2;	/* texts in the corners (i.e. positions) */
} zoombox = { FALSE, {0,0}, {0,0}, NULL, NULL };

/* Pointer definitions */
HCURSOR hptrDefault, hptrCrossHair, hptrScaling, hptrRotating, hptrZooming, hptrCurrent;

/* Mouse support routines */
static void	Wnd_exec_event(LPGW lpgw, LPARAM lparam, char type, int par1);
static void	Wnd_refresh_zoombox(LPGW lpgw, LPARAM lParam);
static void	Wnd_refresh_ruler_lineto(LPGW lpgw, LPARAM lParam);
static void	GetMousePosViewport(LPGW lpgw, int *mx, int *my);
static void	Draw_XOR_Text(LPGW lpgw, const char *text, size_t length, int x, int y);
#endif
static void	UpdateStatusLine(LPGW lpgw, const char text[]);
static void	UpdateToolbar(LPGW lpgw);
#ifdef USE_MOUSE
static void	DrawRuler(LPGW lpgw);
static void	DrawRulerLineTo(LPGW lpgw);
static void	DrawZoomBox(LPGW lpgw);
static void	LoadCursors(LPGW lpgw);
static void	DestroyCursors(LPGW lpgw);
#endif /* USE_MOUSE */

/* ================================== */

#define WGDEFCOLOR 15
COLORREF wginitcolor[WGDEFCOLOR] =  {
	RGB(255,0,0),	/* red */
	RGB(0,255,0),	/* green */
	RGB(0,0,255),	/* blue */
	RGB(255,0,255), /* magenta */
	RGB(0,0,128),	/* dark blue */
	RGB(128,0,0),	/* dark red */
	RGB(0,128,128),	/* dark cyan */
	RGB(0,0,0),		/* black */
	RGB(128,128,128), /* grey */
	RGB(0,128,64),	/* very dark cyan */
	RGB(128,128,0), /* dark yellow */
	RGB(128,0,128),	/* dark magenta */
	RGB(192,192,192), /* light grey */
	RGB(0,255,255),	/* cyan */
	RGB(255,255,0),	/* yellow */
};
#define WGDEFSTYLE 5
int wginitstyle[WGDEFSTYLE] = {PS_SOLID, PS_DASH, PS_DOT, PS_DASHDOT, PS_DASHDOTDOT};

/* Maximum number of GWOPBLK arrays to be remembered. */
/* HBB 20010218: moved here from wgnuplib.h: other parts of the program don't
 * need to know about it */
#define GWOPMAX 4096

#define MINMAX(a,val,b) (((val) <= (a)) ? (a) : ((val) <= (b) ? (val) : (b)))

/* bitmaps for filled boxes (ULIG) */
/* zeros represent the foreground color and ones represent the background color */
#define PATTERN_BITMAP_LENGTH 16
static const unsigned char pattern_bitmaps[][PATTERN_BITMAP_LENGTH] = {
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* no fill */
  {0xFE, 0xFE, 0x7D, 0x7D, 0xBB, 0xBB, 0xD7, 0xD7,
   0xEF, 0xEF, 0xD7, 0xD7, 0xBB, 0xBB, 0x7D, 0x7D}, /* cross-hatch (1) */
  {0x77, 0x77, 0xAA, 0xAA, 0xDD, 0xDD, 0xAA, 0xAA,
   0x77, 0x77, 0xAA, 0xAA, 0xDD, 0xDD, 0xAA, 0xAA}, /* double cross-hatch (2) */
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* solid fill (3) */
  {0x7F, 0x7F, 0xBF, 0xBF, 0xDF, 0xDF, 0xEF, 0xEF,
   0xF7, 0xF7, 0xFB, 0xFB, 0xFD, 0xFD, 0xFE, 0xFE}, /* diagonals (4) */
  {0xFE, 0xFE, 0xFD, 0xFD, 0xFB, 0xFB, 0xF7, 0xF7,
   0xEF, 0xEF, 0xDF, 0xDF, 0xBF, 0xBF, 0x7F, 0x7F}, /* diagonals (5) */
  {0x77, 0x77, 0x77, 0x77, 0xBB, 0xBB, 0xBB, 0xBB,
   0xDD, 0xDD, 0xDD, 0xDD, 0xEE, 0xEE, 0xEE, 0xEE}, /* steep diagonals (6) */
  {0xEE, 0xEE, 0xEE, 0xEE, 0xDD, 0xDD, 0xDD, 0xDD,
   0xBB, 0xBB, 0xBB, 0xBB, 0x77, 0x77, 0x77, 0x77}  /* steep diagonals (7) */
#if (0)
 ,{0xFC, 0xFC, 0xF3, 0xF3, 0xCF, 0xCF, 0x3F, 0x3F,
   0xFC, 0xFC, 0xF3, 0xF3, 0xCF, 0xCF, 0x3F, 0x3F}, /* shallow diagonals (old 5) */
  {0x3F, 0x3F, 0xCF, 0xCF, 0xF3, 0xF3, 0xFC, 0xFC,
   0x3F, 0x3F, 0xCF, 0xCF, 0xF3, 0xF3, 0xFC, 0xFC}  /* shallow diagonals (old 6) */
#endif
};
#define pattern_num (sizeof(pattern_bitmaps)/(sizeof(*pattern_bitmaps)))
static HBRUSH pattern_brush[pattern_num];
static BITMAP pattern_bitdata[pattern_num];
static HBITMAP pattern_bitmap[pattern_num];

static TBOOLEAN brushes_initialized = FALSE;


/* Internal state of enhanced text processing.
   Do not access outside draw_enhanced_text, GraphEnhancedOpen or
   GraphEnhancedFlush.
*/
static struct {
	LPGW lpgw;           /* graph window */
	HDC  hdc;            /* device context */
	LPRECT rect;         /* rect to update */
	BOOL opened_string;  /* started processing of substring? */
	BOOL show;           /* print this substring? */
	int overprint;       /* overprint flag */
	BOOL widthflag;      /* FALSE for zero width boxes */
	BOOL sizeonly;       /* only measure length of substring? */
	double base;         /* current baseline position (above initial baseline) */
	int xsave, ysave;    /* save text position for overprinted text */
	int x, y;            /* current text position */
	char fontname[MAXFONTNAME]; /* current font name */
	double fontsize;     /* current font size */
	int totalwidth;      /* total width of printed text */
	int totalasc;        /* total height above center line */
	int totaldesc;       /* total height below center line */
	double res_scale;    /* scaling due to different resolution (printers) */
	int shift;           /* baseline alignment */
} enhstate;


/* ================================== */

/* prototypes for module-local functions */

LRESULT CALLBACK WndGraphProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK LineStyleDlgProc(HWND hdlg, UINT wmsg, WPARAM wparam, LPARAM lparam);

static void	DestroyBlocks(LPGW lpgw);
static BOOL	AddBlock(LPGW lpgw);
static void	StorePen(LPGW lpgw, int i, COLORREF ref, int colorstyle, int monostyle);
static void	MakePens(LPGW lpgw, HDC hdc);
static void	DestroyPens(LPGW lpgw);
static void	Wnd_GetTextSize(HDC hdc, LPCSTR str, size_t len, int *cx, int *cy);
static void	GetPlotRect(LPGW lpgw, LPRECT rect);
static void	MakeFonts(LPGW lpgw, LPRECT lprect, HDC hdc);
static void	DestroyFonts(LPGW lpgw);
static void	SelFont(LPGW lpgw);
static void	dot(HDC hdc, int xdash, int ydash);
static unsigned int WDPROC GraphGetTextLength(LPGW lpgw, HDC hdc, LPCSTR text);
static void	draw_text_justify(HDC hdc, int justify);
static void	draw_put_text(LPGW lpgw, HDC hdc, int x, int y, char * str);
static void	drawgraph(LPGW lpgw, HDC hdc, LPRECT rect);
static void	CopyClip(LPGW lpgw);
static void	SaveAsEMF(LPGW lpgw);
static void	CopyPrint(LPGW lpgw);
static void	WriteGraphIni(LPGW lpgw);
static void	ReadGraphIni(LPGW lpgw);
static void	track_tooltip(LPGW lpgw, int x, int y);
static COLORREF	GetColor(HWND hwnd, COLORREF ref);
#ifdef WIN_CUSTOM_PENS
static void	UpdateColorSample(HWND hdlg);
static BOOL	LineStyle(LPGW lpgw);
#endif

/* ================================== */

/* Helper functions for GraphOp(): */

/* destroy memory blocks holding graph operations */
static void
DestroyBlocks(LPGW lpgw)
{
	struct GWOPBLK *this, *next;
	struct GWOP *gwop;
	unsigned int i;

	this = lpgw->gwopblk_head;
	while (this != NULL) {
		next = this->next;
		if (!this->gwop) {
			this->gwop = (struct GWOP *)GlobalLock(this->hblk);
		}
		if (this->gwop) {
			/* free all text strings within this block */
			gwop = this->gwop;
			for (i=0; i<GWOPMAX; i++) {
				if (gwop->htext)
					LocalFree(gwop->htext);
				gwop++;
			}
		}
		GlobalUnlock(this->hblk);
		GlobalFree(this->hblk);
		LocalFreePtr(this);
		this = next;
	}
	lpgw->gwopblk_head = NULL;
	lpgw->gwopblk_tail = NULL;
	lpgw->nGWOP = 0;
}


/* add a new memory block for graph operations */
/* returns TRUE if block allocated */
static BOOL
AddBlock(LPGW lpgw)
{
	HGLOBAL hblk;
	struct GWOPBLK *next, *this;

	/* create new block */
	next = (struct GWOPBLK *)LocalAllocPtr(LHND, sizeof(struct GWOPBLK) );
	if (next == NULL)
		return FALSE;
	hblk = GlobalAlloc(GHND, GWOPMAX*sizeof(struct GWOP));
	if (hblk == NULL)
		return FALSE;
	next->hblk = hblk;
	next->gwop = (struct GWOP *)NULL;
	next->next = (struct GWOPBLK *)NULL;
	next->used = 0;

	/* attach it to list */
	this = lpgw->gwopblk_tail;
	if (this == NULL) {
		lpgw->gwopblk_head = next;
	} else {
		this->next = next;
		this->gwop = (struct GWOP *)NULL;
		GlobalUnlock(this->hblk);
	}
	lpgw->gwopblk_tail = next;
	next->gwop = (struct GWOP *)GlobalLock(next->hblk);
	if (next->gwop == (struct GWOP *)NULL)
		return FALSE;

	return TRUE;
}


void WDPROC
GraphOp(LPGW lpgw, UINT op, UINT x, UINT y, LPCSTR str)
{
	if (str)
		GraphOpSize(lpgw, op, x, y, str, _fstrlen(str)+1);
	else
		GraphOpSize(lpgw, op, x, y, NULL, 0);
}


void WDPROC
GraphOpSize(LPGW lpgw, UINT op, UINT x, UINT y, LPCSTR str, DWORD size)
{
	struct GWOPBLK *this;
	struct GWOP *gwop;
	char *npstr;

	this = lpgw->gwopblk_tail;
	if ( (this==NULL) || (this->used >= GWOPMAX) ) {
		/* not enough space so get new block */
		if (!AddBlock(lpgw))
			return;
		this = lpgw->gwopblk_tail;
	}
	gwop = &this->gwop[this->used];
	gwop->op = op;
	gwop->x = x;
	gwop->y = y;
	gwop->htext = 0;
	if (str) {
		gwop->htext = LocalAlloc(LHND, size);
		npstr = LocalLock(gwop->htext);
		if (gwop->htext && (npstr != (char *)NULL))
			memcpy(npstr, str, size);
		LocalUnlock(gwop->htext);
	}
	this->used++;
	lpgw->nGWOP++;
	lpgw->buffervalid = FALSE;
	return;
}

/* ================================== */

/* Initialize the LPGW struct:
 * set default values and read options from ini file */
void WDPROC
GraphInitStruct(LPGW lpgw)
{
	if (!lpgw->initialized) {
		int i;

		lpgw->initialized = TRUE;
		if (lpgw != listgraphs) {
			char titlestr[100];

			/* copy important fields from window #0 */
			LPGW graph0 = listgraphs;
			lpgw->IniFile = graph0->IniFile;
			lpgw->hInstance = graph0->hInstance;
			lpgw->hPrevInstance = graph0->hPrevInstance;
			lpgw->lptw = graph0->lptw;

			/* window title */
			sprintf(titlestr, "%s %i", WINGRAPHTITLE, lpgw->Id);
			lpgw->Title = strdup(titlestr);
		} else {
			lpgw->Title = strdup(WINGRAPHTITLE);
		}
		lpgw->sampling = 1;
		lpgw->fontscale = 1.;
		lpgw->linewidth = 1.;
		lpgw->color = TRUE;
		lpgw->dashed = FALSE;
		lpgw->IniSection = "WGNUPLOT";
		lpgw->fontsize = WINFONTSIZE;
		lpgw->maxkeyboxes = 0;
		lpgw->keyboxes = 0;
		lpgw->buffervalid = FALSE;
		lpgw->maxhideplots = MAXPLOTSHIDE;
		lpgw->hideplot = (BOOL *) calloc(MAXPLOTSHIDE, sizeof(BOOL));
		/* init pens */
#ifndef WIN_CUSTOM_PENS
		StorePen(lpgw, 0, RGB(0, 0, 0), PS_SOLID, PS_SOLID);
		StorePen(lpgw, 1, RGB(192, 192, 192), PS_DOT, PS_DOT);
		for (i = 0; i < WGNUMPENS; i++) {
			COLORREF ref = wginitcolor[i % WGDEFCOLOR];
			int colorstyle = wginitstyle[(i / WGDEFCOLOR) % WGDEFSTYLE];
			int monostyle  = wginitstyle[i % WGDEFSTYLE];
			StorePen(lpgw, i + 2, ref, colorstyle, monostyle);
		}
#endif
		ReadGraphIni(lpgw);
	}
}


/* Prepare Graph window for being displayed by windows, update
 * the window's menus and show it */
void WDPROC
GraphInit(LPGW lpgw)
{
	HMENU sysmenu;
	WNDCLASS wndclass;
	char buf[MAX_PATH];

	if (!lpgw->hPrevInstance) {
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndGraphProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 2 * sizeof(void *);
		wndclass.hInstance = lpgw->hInstance;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = szGraphClass;
		RegisterClass(&wndclass);
	}

	lpgw->hWndGraph = CreateWindow(szGraphClass, lpgw->Title,
		WS_OVERLAPPEDWINDOW,
		lpgw->Origin.x, lpgw->Origin.y,
		lpgw->Size.x, lpgw->Size.y,
		NULL, NULL, lpgw->hInstance, lpgw);

	if (lpgw->hWndGraph)
		SetClassLongPtr(lpgw->hWndGraph, GCLP_HICON,
			(LONG_PTR) LoadIcon(lpgw->hInstance, "GRPICON"));

	lpgw->hStatusbar = CreateWindowEx(0, STATUSCLASSNAME, (LPSTR)NULL,
				  WS_CHILD | SBARS_SIZEGRIP,
				  0, 0, 0, 0,
				  lpgw->hWndGraph, (HMENU)ID_GRAPHSTATUS,
				  lpgw->hInstance, lpgw);
	if (lpgw->hStatusbar) {
		RECT rect;
		/* auto-adjust size */
		SendMessage(lpgw->hStatusbar, WM_SIZE, (WPARAM)0, (LPARAM)0);
		ShowWindow(lpgw->hStatusbar, TRUE);

		/* make room */
		GetClientRect(lpgw->hStatusbar, &rect);
		lpgw->StatusHeight = rect.bottom - rect.top;
	}

	/* create a toolbar */
	lpgw->hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
		WS_CHILD | TBSTYLE_LIST | TBSTYLE_TOOLTIPS, // TBSTYLE_WRAPABLE
		0, 0, 0, 0,
		lpgw->hWndGraph, (HMENU)ID_TOOLBAR, lpgw->hInstance, lpgw);

	if (lpgw->hToolbar != NULL) {
		RECT rect;
		int i;
		TBBUTTON button;
		BOOL ret;
		char buttontext[10];
		unsigned num = 0;
		TBADDBITMAP bitmap = {0};

		SendMessage(lpgw->hToolbar, TB_SETBITMAPSIZE, (WPARAM)0, (LPARAM)((16<<16) + 16));
		/* load standard toolbar icons: standard, history & view */
		SendMessage(lpgw->hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
		bitmap.hInst = HINST_COMMCTRL;
		bitmap.nID = IDB_STD_SMALL_COLOR;
		SendMessage(lpgw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);
		bitmap.nID = IDB_HIST_SMALL_COLOR;
		SendMessage(lpgw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);
		bitmap.nID = IDB_VIEW_SMALL_COLOR;
		SendMessage(lpgw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);

		/* create buttons */
		SendMessage(lpgw->hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

		ZeroMemory(&button, sizeof(button));
		button.fsState = TBSTATE_ENABLED;
		button.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_NOPREFIX;
		button.iString = 0;

		/* copy */
		button.iBitmap = STD_COPY;
		button.idCommand = M_COPY_CLIP;
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* print */
		button.iBitmap = STD_PRINT;
		button.idCommand = M_PRINT;
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* save as EMF */
		button.iBitmap = STD_FILESAVE;
		button.idCommand = M_SAVE_AS_EMF;
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* options */
		button.iBitmap = STD_PROPERTIES;
		button.idCommand = 0; /* unused */
		button.iString = (INT_PTR) "Options";
		button.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_NOPREFIX | BTNS_WHOLEDROPDOWN;
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* TODO: Add the following buttons:
			replot/refresh, toggle grid(?), previous/next zoom, autoscale, help
		*/

		button.fsStyle = BTNS_AUTOSIZE | BTNS_NOPREFIX | BTNS_SEP;
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* hide grid */
		button.iBitmap = STD_CUT;
		button.idCommand = M_HIDEGRID;
		button.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_NOPREFIX | BTNS_CHECK;
		button.iString = (INT_PTR) "Grid";
		ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);

		/* hide graphs */
		for (i = 0; i < MAXPLOTSHIDE; i++) {
			button.iBitmap = STD_CUT;
			button.idCommand = M_HIDEPLOT + i;
			sprintf(buttontext, "%i", i + 1);
			button.iString = (UINT_PTR) buttontext;
			button.dwData = i;
			ret = SendMessage(lpgw->hToolbar, TB_INSERTBUTTON, (WPARAM)num++, (LPARAM)&button);
		}

		/* auto-resize and show */
		SendMessage(lpgw->hToolbar, TB_AUTOSIZE, (WPARAM)0, (LPARAM)0);
		ShowWindow(lpgw->hToolbar, TRUE);

		/* make room */
		GetClientRect(lpgw->hToolbar, &rect);
		lpgw->ToolbarHeight = rect.bottom - rect.top + 1;
	}

	lpgw->hPopMenu = CreatePopupMenu();
	/* actions */
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_COPY_CLIP, "&Copy to Clipboard (Ctrl+C)");
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_SAVE_AS_EMF, "&Save as EMF... (Ctrl+S)");
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_PRINT, "&Print...");
	/* settings */
	AppendMenu(lpgw->hPopMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->graphtotop ? MF_CHECKED : MF_UNCHECKED),
		M_GRAPH_TO_TOP, "Bring to &Top");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->color ? MF_CHECKED : MF_UNCHECKED),
		M_COLOR, "C&olor");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->doublebuffer ? MF_CHECKED : MF_UNCHECKED),
		M_DOUBLEBUFFER, "&Double buffer");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->oversample ? MF_CHECKED : MF_UNCHECKED),
		M_OVERSAMPLE, "O&versampling");
#ifdef HAVE_GDIPLUS
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->gdiplus ? MF_CHECKED : MF_UNCHECKED),
		M_GDIPLUS, "GDI&+ only backend");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->antialiasing ? MF_CHECKED : MF_UNCHECKED),
		M_ANTIALIASING, "&Antialiasing");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->polyaa ? MF_CHECKED : MF_UNCHECKED),
		M_POLYAA, "Antialiasing of pol&ygons");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->patternaa || lpgw->gdiplus ? MF_CHECKED : MF_UNCHECKED),
		M_PATTERNAA, "Antialiasing of patt&erns");
	AppendMenu(lpgw->hPopMenu, MF_STRING | (lpgw->fastrotation ? MF_CHECKED : MF_UNCHECKED),
		M_FASTROTATE, "Fast &rotation w/o antialiasing ");
	if (lpgw->gdiplus)
		EnableMenuItem(lpgw->hPopMenu, M_PATTERNAA, MF_BYCOMMAND | MF_DISABLED);
#endif
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_BACKGROUND, "&Background...");
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_CHOOSE_FONT, "&Font...");
#ifdef WIN_CUSTOM_PENS
	AppendMenu(lpgw->hPopMenu, MF_STRING, M_LINESTYLE, "&Line Styles...");
#endif
	/* save settings */
	AppendMenu(lpgw->hPopMenu, MF_SEPARATOR, 0, NULL);
	if (lpgw->IniFile != (LPSTR)NULL) {
		wsprintf(buf,"&Update %s",lpgw->IniFile);
		AppendMenu(lpgw->hPopMenu, MF_STRING, M_WRITEINI, (LPSTR)buf);
	}

	/* modify the system menu to have the new items we want */
	sysmenu = GetSystemMenu(lpgw->hWndGraph,0);
	AppendMenu(sysmenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(sysmenu, MF_POPUP, (UINT_PTR)lpgw->hPopMenu, "&Options");
	AppendMenu(sysmenu, MF_STRING, M_ABOUT, "&About");

#ifndef WGP_CONSOLE
	if (!IsWindowVisible(lpgw->lptw->hWndParent)) {
		AppendMenu(sysmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(sysmenu, MF_STRING, M_COMMANDLINE, "C&ommand Line");
	}
#endif

	/* determine the size of the window decoration: border, toolbar, caption, statusbar etc. */
	{
		RECT wrect, rect;

		/* get real window size, not lpgw->Size */
		GetWindowRect(lpgw->hWndGraph, &wrect);
		GetClientRect(lpgw->hWndGraph, &rect);
		lpgw->Decoration.x = wrect.right - wrect.left + rect.left - rect.right;
		lpgw->Decoration.y = wrect.bottom - wrect.top + rect.top- rect.bottom + lpgw->ToolbarHeight + lpgw->StatusHeight;
	}

	/* resize to match requested canvas size */
	if (lpgw->Canvas.x != 0) {
		lpgw->Size.x = lpgw->Canvas.x + lpgw->Decoration.x;
		lpgw->Size.y = lpgw->Canvas.y + lpgw->Decoration.y;
		SetWindowPos(lpgw->hWndGraph, HWND_BOTTOM, lpgw->Origin.x, lpgw->Origin.y, lpgw->Size.x, lpgw->Size.y, SWP_NOACTIVATE | SWP_NOZORDER  | SWP_NOMOVE);
	}

	ShowWindow(lpgw->hWndGraph, SW_SHOWNORMAL);
}


void WDPROC
GraphUpdateWindowPosSize(LPGW lpgw)
{
	/* resize to match requested canvas size */
	if (lpgw->Canvas.x != 0) {
		lpgw->Size.x = lpgw->Canvas.x + lpgw->Decoration.x;
		lpgw->Size.y = lpgw->Canvas.y + lpgw->Decoration.y;
	}
	SetWindowPos(lpgw->hWndGraph, HWND_BOTTOM, lpgw->Origin.x, lpgw->Origin.y, lpgw->Size.x, lpgw->Size.y, SWP_NOACTIVATE | SWP_NOZORDER);
}


/* close a graph window */
void WDPROC
GraphClose(LPGW lpgw)
{
#ifdef USE_MOUSE
	/* Pass it through mouse handling to check for "bind Close" */
	Wnd_exec_event(lpgw, (LPARAM)0, GE_reset, 0);
#endif
	/* close window */
	if (lpgw->hWndGraph)
		DestroyWindow(lpgw->hWndGraph);
	WinMessageLoop();
	lpgw->hWndGraph = NULL;

	lpgw->locked = TRUE;
	DestroyBlocks(lpgw);
	lpgw->locked = FALSE;
}


void WDPROC
GraphStart(LPGW lpgw, double pointsize)
{
	lpgw->locked = TRUE;
	lpgw->buffervalid = FALSE;
	DestroyBlocks(lpgw);
	lpgw->org_pointsize = pointsize;
	if (!lpgw->hWndGraph || !IsWindow(lpgw->hWndGraph))
		GraphInit(lpgw);
	if (IsIconic(lpgw->hWndGraph))
		ShowWindow(lpgw->hWndGraph, SW_SHOWNORMAL);
	if (lpgw->graphtotop) {
		/* HBB NEW 20040221: avoid grabbing the keyboard focus
		 * unless mouse mode is on */
#ifdef USE_MOUSE
		if (mouse_setting.on) {
			BringWindowToTop(lpgw->hWndGraph);
			return;
		}
#endif /* USE_MOUSE */
		SetWindowPos(lpgw->hWndGraph,
			     HWND_TOP, 0,0,0,0,
			     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	}
}


void WDPROC
GraphEnd(LPGW lpgw)
{
	RECT rect;

	GetClientRect(lpgw->hWndGraph, &rect);
	InvalidateRect(lpgw->hWndGraph, (LPRECT) &rect, 1);
	lpgw->buffervalid = FALSE;
	lpgw->locked = FALSE;
	UpdateWindow(lpgw->hWndGraph);
#ifdef USE_MOUSE
	gp_exec_event(GE_plotdone, 0, 0, 0, 0, 0);	/* notify main program */
#endif
}


/* shige */
void WDPROC
GraphChangeTitle(LPGW lpgw)
{
	if (GraphHasWindow(lpgw))
		SetWindowText(lpgw->hWndGraph, lpgw->Title);
}


void WDPROC
GraphResume(LPGW lpgw)
{
	lpgw->locked = TRUE;
}


void WDPROC
GraphPrint(LPGW lpgw)
{
	if (GraphHasWindow(lpgw))
		SendMessage(lpgw->hWndGraph, WM_COMMAND, M_PRINT, 0L);
}


void WDPROC
GraphRedraw(LPGW lpgw)
{
	lpgw->buffervalid = FALSE;
	if (GraphHasWindow(lpgw))
		SendMessage(lpgw->hWndGraph, WM_COMMAND, M_REBUILDTOOLS, 0L);
}


TBOOLEAN
GraphHasWindow(LPGW lpgw)
{
	return (lpgw != NULL) && (lpgw->hWndGraph != NULL) && IsWindow(lpgw->hWndGraph);
}


/* ================================== */

/* Helper functions for bookkeeping of pens, brushes and fonts */

/* Set up LOGPEN structures based on information coming from wgnuplot.ini, via
 * ReadGraphIni() */
static void
StorePen(LPGW lpgw, int i, COLORREF ref, int colorstyle, int monostyle)
{
	LOGPEN *plp;

	plp = &lpgw->colorpen[i];
	plp->lopnColor = ref;
	if (colorstyle < 0) {
		plp->lopnWidth.x = -colorstyle;
		plp->lopnStyle = 0;
	} else {
		plp->lopnWidth.x = 1;
		plp->lopnStyle = colorstyle % 5;
	}
	plp->lopnWidth.y = 0;

	plp = &lpgw->monopen[i];
	plp->lopnColor = RGB(0,0,0);
	if (monostyle < 0) {
		plp->lopnWidth.x = -monostyle;
		plp->lopnStyle = 0;
	} else {
		plp->lopnWidth.x = 1;
		plp->lopnStyle = monostyle % 5;
	}
	plp->lopnWidth.y = 0;
}


/* Prepare pens and brushes (--> colors) for use by the driver. Pens are (now) created
 * on-the-fly (--> DeleteObject(SelectObject(...)) idiom), but the brushes are still
 * all created statically, and kept until the window is closed */
static void
MakePens(LPGW lpgw, HDC hdc)
{
	int i;
	LOGPEN pen;

	if ((GetDeviceCaps(hdc, NUMCOLORS) == 2) || !lpgw->color) {
		pen = lpgw->monopen[1];
		pen.lopnWidth.x *= lpgw->linewidth * lpgw->sampling;
		lpgw->hapen = CreatePenIndirect(&pen); 	/* axis */
		lpgw->hbrush = CreateSolidBrush(lpgw->background);
		for (i = 0; i < WGNUMPENS + 2; i++)
			lpgw->colorbrush[i] = CreateSolidBrush(lpgw->monopen[i].lopnColor);
	} else {
		pen = lpgw->colorpen[1];
		pen.lopnWidth.x *= lpgw->linewidth * lpgw->sampling;
		lpgw->hapen = CreatePenIndirect(&pen); 	/* axis */
		lpgw->hbrush = CreateSolidBrush(lpgw->background);
		for (i = 0; i < WGNUMPENS + 2; i++)
			lpgw->colorbrush[i] = CreateSolidBrush(lpgw->colorpen[i].lopnColor);
	}
	lpgw->hnull = CreatePen(PS_NULL, 0, 0); /* border for filled areas */

	/* build pattern brushes for filled boxes (ULIG) */
	if (!brushes_initialized) {
		int i;

		for (i = 0; i < pattern_num; i++) {
			pattern_bitdata[i].bmType       = 0;
			pattern_bitdata[i].bmWidth      = 16;
			pattern_bitdata[i].bmHeight     = 8;
			pattern_bitdata[i].bmWidthBytes = 2;
			pattern_bitdata[i].bmPlanes     = 1;
			pattern_bitdata[i].bmBitsPixel  = 1;
			pattern_bitdata[i].bmBits       = (char *) pattern_bitmaps[i];
			pattern_bitmap[i] = CreateBitmapIndirect(&pattern_bitdata[i]);
			pattern_brush[i] = CreatePatternBrush(pattern_bitmap[i]);
		}
		brushes_initialized = TRUE;
	}
}


/* Undo effect of MakePens(). To be called just before the window is closed. */
static void
DestroyPens(LPGW lpgw)
{
	int i;

	DeleteObject(lpgw->hbrush);
	DeleteObject(lpgw->hapen);
	DeleteObject(lpgw->hsolid);
	for (i = 0; i < WGNUMPENS + 2; i++)
		DeleteObject(lpgw->colorbrush[i]);
	DeleteObject(lpgw->hnull);

	/* delete brushes used for filled areas */
	if (brushes_initialized) {
		int i;

		for (i = 0; i < pattern_num; i++) {
			DeleteObject(pattern_bitmap[i]);
			DeleteObject(pattern_brush[i]);
		}
		brushes_initialized = FALSE;
	}
}

/* ================================== */

/* HBB 20010218: new function. An isolated snippet from MakeFont(), now also
 * used in Wnd_put_tmptext() to size the temporary bitmap. */
static void
Wnd_GetTextSize(HDC hdc, LPCSTR str, size_t len, int *cx, int *cy)
{
	SIZE size;

	GetTextExtentPoint32(hdc, str, len, &size);
	*cx = size.cx;
	*cy = size.cy;
}


static void
GetPlotRect(LPGW lpgw, LPRECT rect)
{
	GetClientRect(lpgw->hWndGraph, rect);
	rect->bottom -= lpgw->StatusHeight; /* leave some room for the status line */
	rect->top += lpgw->ToolbarHeight + 1;
	if (rect->bottom < rect->top) rect->bottom = rect->top;
}


static void
GetPlotRectInMM(LPGW lpgw, LPRECT rect, HDC hdc)
{
	int iWidthMM, iHeightMM, iWidthPels, iHeightPels;

	GetPlotRect (lpgw, rect);

	/* Taken from
	http://msdn.microsoft.com/en-us/library/dd183519(VS.85).aspx
	 */
	// Determine the picture frame dimensions.
	// iWidthMM is the display width in millimeters.
	// iHeightMM is the display height in millimeters.
	// iWidthPels is the display width in pixels.
	// iHeightPels is the display height in pixels
	iWidthMM = GetDeviceCaps(hdc, HORZSIZE);
	iHeightMM = GetDeviceCaps(hdc, VERTSIZE);
	iWidthPels = GetDeviceCaps(hdc, HORZRES);
	iHeightPels = GetDeviceCaps(hdc, VERTRES);

	// Convert client coordinates to .01-mm units.
	// Use iWidthMM, iWidthPels, iHeightMM, and
	// iHeightPels to determine the number of
	// .01-millimeter units per pixel in the x-
	//  and y-directions.
	rect->left = (rect->left * iWidthMM * 100) / iWidthPels;
	rect->top = (rect->top * iHeightMM * 100) / iHeightPels;
	rect->right = (rect->right * iWidthMM * 100) / iWidthPels;
	rect->bottom = (rect->bottom * iHeightMM * 100) / iHeightPels;
}


static TBOOLEAN
TryCreateFont(LPGW lpgw, char * fontface, HDC hdc)
{
	if (fontface != NULL)
		strncpy(lpgw->lf.lfFaceName, fontface, LF_FACESIZE);

	lpgw->hfonth = CreateFontIndirect((LOGFONT *)&(lpgw->lf));

	if (lpgw->hfonth != 0) {
		/* Test if we actually got the requested font. Note that this also seems to work
			with GDI's magic font substitutions (e.g. Helvetica->Arial, Times->Times New Roman) */
		HFONT hfontold = (HFONT) SelectObject(hdc, lpgw->hfonth);
		if (hfontold != NULL) {
			char fontname[MAXFONTNAME];
			GetTextFace(hdc, sizeof(fontname), fontname);
			SelectObject(hdc, hfontold);
			if (strcmp(fontname, lpgw->lf.lfFaceName) == 0) {
				return TRUE;
			} else {
				FPRINTF((stderr, "Warning: GDI would have substituted \"%s\" for \"%s\"\n", fontname, lpgw->lf.lfFaceName));
				DeleteObject(lpgw->hfonth);
				lpgw->hfonth = 0;
				return FALSE;
			}
		}
		return TRUE;
	} else {
		return FALSE;
	}
	return FALSE;
}


static void
MakeFonts(LPGW lpgw, LPRECT lprect, HDC hdc)
{
	HFONT hfontold;
	TEXTMETRIC tm;
	int result;
	char *p;
	int cx, cy;

	lpgw->rotate = FALSE;
	memset(&(lpgw->lf), 0, sizeof(LOGFONT));
	strncpy(lpgw->lf.lfFaceName, lpgw->fontname, LF_FACESIZE);
	lpgw->lf.lfHeight = -MulDiv(lpgw->fontsize * lpgw->fontscale, GetDeviceCaps(hdc, LOGPIXELSY), 72) * lpgw->sampling;
	lpgw->lf.lfCharSet = DEFAULT_CHARSET;
	if (((p = strstr(lpgw->fontname," Italic")) != NULL) ||
		((p = strstr(lpgw->fontname,":Italic")) != NULL)) {
		lpgw->lf.lfFaceName[(unsigned int) (p - lpgw->fontname)] = NUL;
		lpgw->lf.lfItalic = TRUE;
	}
	if (((p = strstr(lpgw->fontname," Bold")) != NULL) ||
		((p = strstr(lpgw->fontname,":Bold")) != NULL)) {
		lpgw->lf.lfFaceName[(unsigned int) (p - lpgw->fontname)] = NUL;
		lpgw->lf.lfWeight = FW_BOLD;
	}
	lpgw->lf.lfOutPrecision = OUT_OUTLINE_PRECIS;
	lpgw->lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	/* ClearType quality is only supported on XP or later */
	lpgw->lf.lfQuality =
		IsWindowsXPorLater() && lpgw->antialiasing ? CLEARTYPE_QUALITY : PROOF_QUALITY;

	if (!TryCreateFont(lpgw, NULL, hdc)) {
		if (strcmp(lpgw->fontname, lpgw->deffontname) != 0) {
			fprintf(stderr, "Warning:  font \"%s\" not available, trying \"%s\" instead.\n", lpgw->fontname, lpgw->deffontname);
			if (!TryCreateFont(lpgw, lpgw->deffontname, hdc)) {
				if (strcmp(lpgw->deffontname, GraphDefaultFont()) != 0) {
					fprintf(stderr, "Warning:  font \"%s\" not available, trying \"%s\" instead.\n", lpgw->deffontname, GraphDefaultFont());
					if (!TryCreateFont(lpgw, GraphDefaultFont(), hdc)) {
						fprintf(stderr, "Error:  could not substitute another font. Giving up.\n");
					}
				} else {
					fprintf(stderr, "Error:  could not substitute another font. Giving up.\n");
				}
			}
		} else {
			if (strcmp(lpgw->fontname, GraphDefaultFont()) != 0) {
				fprintf(stderr, "Warning:  font \"%s\" not available, trying \"%s\" instead.\n", lpgw->fontname, GraphDefaultFont());
				if (!TryCreateFont(lpgw, GraphDefaultFont(), hdc)) {
					fprintf(stderr, "Error:  could not substitute another font. Giving up.\n");
				}
			} else {
				fprintf(stderr, "Error:  font \"%s\" not available, but don't know which font to substitute.\n", lpgw->fontname);
			}
		}
	}

	/* we do need a 90 degree font */
	if (lpgw->hfontv)
		DeleteObject(lpgw->hfontv);
	lpgw->lf.lfEscapement = 900;
	lpgw->lf.lfOrientation = 900;
	lpgw->hfontv = CreateFontIndirect(&(lpgw->lf));

	/* save text size */
	hfontold = (HFONT) SelectObject(hdc, lpgw->hfonth);
	Wnd_GetTextSize(hdc, "0123456789", 10, &cx, &cy);
	lpgw->vchar = MulDiv(cy, lpgw->ymax, lprect->bottom - lprect->top);
	lpgw->hchar = MulDiv(cx/10, lpgw->xmax, lprect->right - lprect->left);

	/* CMW: Base tick size on character size */
	lpgw->htic = MulDiv(lpgw->hchar, 2, 5);
	cy = MulDiv(cx/10, 2*GetDeviceCaps(hdc, LOGPIXELSY), 5*GetDeviceCaps(hdc, LOGPIXELSX));
	lpgw->vtic = MulDiv(cy, lpgw->ymax, lprect->bottom - lprect->top);
	/* find out if we can rotate text 90deg */
	SelectObject(hdc, lpgw->hfontv);
	result = GetDeviceCaps(hdc, TEXTCAPS);
	if ((result & TC_CR_90) || (result & TC_CR_ANY))
		lpgw->rotate = TRUE;
	GetTextMetrics(hdc, (TEXTMETRIC *)&tm);
	if (tm.tmPitchAndFamily & TMPF_VECTOR)
		lpgw->rotate = TRUE;	/* vector fonts can all be rotated */
	if (tm.tmPitchAndFamily & TMPF_TRUETYPE)
		lpgw->rotate = TRUE;	/* truetype fonts can all be rotated */
	/* store text metrics for later use */
	lpgw->tmHeight = tm.tmHeight;
	lpgw->tmAscent = tm.tmAscent;
	lpgw->tmDescent = tm.tmDescent;
	SelectObject(hdc, hfontold);
	return;
}


static void
DestroyFonts(LPGW lpgw)
{
	if (lpgw->hfonth) {
		DeleteObject(lpgw->hfonth);
		lpgw->hfonth = 0;
	}
	if (lpgw->hfontv) {
		DeleteObject(lpgw->hfontv);
		lpgw->hfontv = 0;
	}
	return;
}


void
SetFont(LPGW lpgw, HDC hdc)
{
	SelectObject(hdc, lpgw->hfonth);
	if (lpgw->rotate && lpgw->angle) {
		if (lpgw->hfontv)
			DeleteObject(lpgw->hfontv);
		lpgw->lf.lfEscapement = lpgw->lf.lfOrientation  = lpgw->angle * 10;
		lpgw->hfontv = CreateFontIndirect(&(lpgw->lf));
		if (lpgw->hfontv)
			SelectObject(hdc, lpgw->hfontv);
	}
	return;
}


static void
SelFont(LPGW lpgw)
{
	LOGFONT lf;
	CHOOSEFONT cf;
	HDC hdc;
	char lpszStyle[LF_FACESIZE];
	char *p;

	/* Set all structure fields to zero. */
	_fmemset(&cf, 0, sizeof(CHOOSEFONT));
	_fmemset(&lf, 0, sizeof(LOGFONT));
	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = lpgw->hWndGraph;
	_fstrncpy(lf.lfFaceName,lpgw->fontname,LF_FACESIZE);
	if ((p = _fstrstr(lpgw->fontname," Bold")) != (LPSTR)NULL) {
		_fstrncpy(lpszStyle,p+1,LF_FACESIZE);
		lf.lfFaceName[ (unsigned int)(p-lpgw->fontname) ] = '\0';
	} else if ((p = _fstrstr(lpgw->fontname," Italic")) != (LPSTR)NULL) {
		_fstrncpy(lpszStyle,p+1,LF_FACESIZE);
		lf.lfFaceName[ (unsigned int)(p-lpgw->fontname) ] = '\0';
	} else {
		_fstrcpy(lpszStyle,"Regular");
	}
	cf.lpszStyle = lpszStyle;
	hdc = GetDC(lpgw->hWndGraph);
	lf.lfHeight = -MulDiv(lpgw->fontsize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseDC(lpgw->hWndGraph, hdc);
	cf.lpLogFont = &lf;
	cf.nFontType = SCREEN_FONTTYPE;
	cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_USESTYLE;
	if (ChooseFont(&cf)) {
		_fstrcpy(lpgw->fontname,lf.lfFaceName);
		lpgw->fontsize = cf.iPointSize / 10;
		if (cf.nFontType & BOLD_FONTTYPE)
			lstrcat(lpgw->fontname," Bold");
		if (cf.nFontType & ITALIC_FONTTYPE)
			lstrcat(lpgw->fontname," Italic");
		/* set current font as default font */
		strcpy(lpgw->deffontname, lpgw->fontname);
		lpgw->deffontsize = lpgw->fontsize;
		SendMessage(lpgw->hWndGraph,WM_COMMAND,M_REBUILDTOOLS,0L);
	}
}


LPWSTR
UnicodeText(const char *str, enum set_encoding_id encoding)
{
    UINT codepage;
    LPWSTR textw = NULL;

    /* For a list of code page identifiers see
       http://msdn.microsoft.com/en-us/library/dd317756%28v=vs.85%29.aspx
    */
    switch (encoding) {
        case S_ENC_DEFAULT:    codepage = CP_ACP; break;
        case S_ENC_ISO8859_1:  codepage = 28591; break;
        case S_ENC_ISO8859_2:  codepage = 28592; break;
        case S_ENC_ISO8859_9:  codepage = 28599; break;
        case S_ENC_ISO8859_15: codepage = 28605; break;
        case S_ENC_CP437:      codepage =   437; break;
        case S_ENC_CP850:      codepage =   850; break;
        case S_ENC_CP852:      codepage =   852; break;
        case S_ENC_CP950:      codepage =   950; break;
        case S_ENC_CP1250:     codepage =  1250; break;
        case S_ENC_CP1251:     codepage =  1251; break;
        case S_ENC_CP1252:     codepage =  1252; break;
        case S_ENC_CP1254:     codepage =  1254; break;
        case S_ENC_KOI8_R:     codepage = 20866; break;
        case S_ENC_KOI8_U:     codepage = 21866; break;
        case S_ENC_SJIS:       codepage =   932; break;
        case S_ENC_UTF8:       codepage = CP_UTF8; break;
        default:               codepage = 0xffffffff;
    }
    if (codepage != 0xffffffff) {
        int length;

        /* get length of converted string */
        length = MultiByteToWideChar(codepage, 0, str, -1, NULL, 0);
        textw = (LPWSTR) malloc(sizeof(WCHAR) * length);

        /* convert string to UTF-16 */
        length = MultiByteToWideChar(codepage, 0, str, -1, textw, length);
    }
    return textw;
}


#ifdef USE_MOUSE
/* ================================== */

static void
LoadCursors(LPGW lpgw)
{
	/* 3 of them are standard cursor shapes: */
	hptrDefault = LoadCursor(NULL, IDC_ARROW);
	hptrZooming = LoadCursor(NULL, IDC_SIZEALL);
	hptrCrossHair = LoadCursor( NULL, IDC_CROSS);
	/* the other 2 are kept in the resource file: */
	hptrScaling = LoadCursor( lpgw->hInstance, MAKEINTRESOURCE(IDC_SCALING));
	hptrRotating = LoadCursor( lpgw->hInstance, MAKEINTRESOURCE(IDC_ROTATING));

	hptrCurrent = hptrCrossHair;
}

static void
DestroyCursors(LPGW lpgw)
{
	/* No-op. Cursors from LoadCursor() don't need destroying */
	return;
}

#endif /* USE_MOUSE */

/* ================================== */


static void
dot(HDC hdc, int xdash, int ydash)
{
	MoveTo(hdc, xdash, ydash);
	LineTo(hdc, xdash, ydash+1);
}


unsigned
luma_from_color(unsigned red, unsigned green, unsigned blue)
{
	/* convert to gray */
	return (unsigned)(red * 0.30 + green * 0.59 + blue * 0.11);
}


static unsigned int WDPROC
GraphGetTextLength(LPGW lpgw, HDC hdc, LPCSTR text)
{
    SIZE size;
    LPWSTR textw;

    textw = UnicodeText(text, lpgw->encoding);
    if (textw) {
        GetTextExtentPoint32W(hdc, textw, wcslen(textw), &size);
        free(textw);
    } else
        GetTextExtentPoint32(hdc, text, strlen(text), &size);
    size.cx += GetTextCharacterExtra(hdc);
    return size.cx;
}


void WDPROC
GraphEnhancedOpen(char *fontname, double fontsize, double base,
    BOOL widthflag, BOOL showflag, int overprint)
{
	const int win_scale = 1; /* scaling of base offset */
	char *fontstring;

	/* There are two special cases:
	 * overprint = 3 means save current position
	 * overprint = 4 means restore saved position
	 */
	if (overprint == 3) {
		enhstate.xsave = enhstate.x;
		enhstate.ysave = enhstate.y;
		return;
	} else if (overprint == 4) {
		enhstate.x = enhstate.xsave;
		enhstate.y = enhstate.ysave;
		return;
	}

	if (!enhstate.opened_string) {
		/* Start new text fragment */
		enhstate.opened_string = TRUE;
		enhanced_cur_text = enhanced_text;
		/* Keep track of whether we are supposed to show this string */
		enhstate.show = showflag;
		/* 0/1/2  no overprint / 1st pass / 2nd pass */
		enhstate.overprint = overprint;
		/* widthflag FALSE means do not update text position after printing */
		enhstate.widthflag = widthflag;
		/* Select font */
		if ((fontname != NULL) && (strlen(fontname) > 0))
			fontstring = fontname;
		else
			fontstring = enhstate.lpgw->deffontname;
		strcpy(enhstate.fontname, fontstring);
		enhstate.fontsize = fontsize;
		GraphChangeFont(enhstate.lpgw, enhstate.fontname, enhstate.fontsize,
		                enhstate.hdc, *(enhstate.rect));
		SetFont(enhstate.lpgw, enhstate.hdc);

		/* Scale fractional font height to vertical units of display */
		/* TODO: Proper use of OUTLINEFONTMETRICS would yield better
		   results. */
		enhstate.base = win_scale * base *
						enhstate.lpgw->sampling * enhstate.lpgw->fontscale *
						enhstate.res_scale;
	}
}


void WDPROC
GraphEnhancedFlush(void)
{
	int width, height;
	unsigned int x, y, len;
	double angle = M_PI/180. * enhstate.lpgw->angle;

	if (!enhstate.opened_string) return;
	*enhanced_cur_text = '\0';

	/* print the string fragment, perhaps invisibly */
	/* NB: base expresses offset from current y pos */
	x = enhstate.x - enhstate.base * sin(angle);
	y = enhstate.y - enhstate.base * cos(angle);

	/* calculate length of string first */
	len = GraphGetTextLength(enhstate.lpgw, enhstate.hdc, enhanced_text);
	width = cos(angle) * len;
	height = -sin(angle) * len;

	if (enhstate.widthflag && !enhstate.sizeonly && !enhstate.overprint) {
		/* do not take rotation into account */
		int ypos = -enhstate.base;
		enhstate.totalwidth += len;
		if (enhstate.totalasc > (ypos + enhstate.shift - enhstate.lpgw->tmAscent))
			enhstate.totalasc = ypos + enhstate.shift - enhstate.lpgw->tmAscent;
		if (enhstate.totaldesc < (ypos + enhstate.shift + enhstate.lpgw->tmDescent))
			enhstate.totaldesc = ypos + enhstate.shift + enhstate.lpgw->tmDescent;
	}

	/* display string */
	if (enhstate.show && !enhstate.sizeonly)
		draw_put_text(enhstate.lpgw, enhstate.hdc, x, y, enhanced_text);

	/* update drawing position according to text length */
	if (!enhstate.widthflag) {
		width = 0;
		height = 0;
	}
	if (enhstate.sizeonly) {
		/* This is the first pass for justified printing.        */
		/* We just adjust the starting position for second pass. */
		if (enhstate.lpgw->justify == RIGHT) {
			enhstate.x -= width;
			enhstate.y -= height;
		}
		else if (enhstate.lpgw->justify == CENTRE) {
			enhstate.x -= width / 2;
			enhstate.y -= height / 2;
		}
		/* nothing to do for LEFT justified text */
	} else if (enhstate.overprint == 1) {
		/* Save current position */
		enhstate.xsave = enhstate.x + width;
		enhstate.ysave = enhstate.y + height;
		/* First pass of overprint, leave position in center of fragment */
		enhstate.x += width / 2;
		enhstate.y += height / 2;
	} else if (enhstate.overprint == 2) {
		/* Restore current position,                          */
		/* this sets the position behind the overprinted text */
		enhstate.x = enhstate.xsave;
		enhstate.y = enhstate.ysave;
	} else {
		/* Normal case is to update position to end of fragment */
		enhstate.x += width;
		enhstate.y += height;
	}
	enhstate.opened_string = FALSE;
}


int
draw_enhanced_text(LPGW lpgw, HDC hdc, LPRECT rect, int x, int y, const char * str)
{
	const char * original_string = str;
	unsigned int pass, num_passes;
	struct termentry *tsave;
	char save_fontname[MAXFONTNAME];
	int save_fontsize;

	/* Init enhanced text state */
	enhstate.lpgw = lpgw;
	enhstate.hdc = hdc;
	enhstate.rect = rect;
	enhstate.opened_string = FALSE;
	strcpy(enhstate.fontname, lpgw->fontname);
	enhstate.fontsize = lpgw->fontsize;
	/* Store the start position */
	enhstate.x = x;
	enhstate.y = y;
	enhstate.totalwidth = 0;
	enhstate.totalasc = 0;
	enhstate.totaldesc = 0;

	enhstate.res_scale = 1.;
	if ((GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER)) {
		HDC hdc_screen = GetDC(NULL);
		enhstate.res_scale = (double) GetDeviceCaps(hdc, VERTRES) /
		           (double) GetDeviceCaps(hdc_screen, VERTRES);
		ReleaseDC(NULL, hdc_screen);
	}

	/* Save font information */
	strcpy(save_fontname, lpgw->fontname);
	save_fontsize = lpgw->fontsize;

	/* Set up global variables needed by enhanced_recursion() */
	enhanced_fontscale = 1.0;
	strncpy(enhanced_escape_format, "%c", sizeof(enhanced_escape_format));

	/* Text justification requires two passes. During the first pass we */
	/* don't draw anything, we just measure the space it will take.     */
	/* Without justification one pass is enough                         */
	if (enhstate.lpgw->justify == LEFT) {
		num_passes = 1;
	} else {
		num_passes = 2;
		enhstate.sizeonly = TRUE;
	}

	/* We actually print everything left to right. */
	SetTextAlign(hdc, TA_LEFT|TA_BASELINE);
	/* Adjust baseline position: */
	enhstate.shift = lpgw->tmHeight/2 - lpgw->tmDescent;
	enhstate.x += sin(lpgw->angle * M_PI/180) * enhstate.shift;
	enhstate.y += cos(lpgw->angle * M_PI/180) * enhstate.shift;

	/* enhanced_recursion() uses the callback functions of the current
	   terminal. So we have to switch temporarily. */
	if (WIN_term) {
		tsave = term;
		term = WIN_term;
	}

	for (pass = 1; pass <= num_passes; pass++) {
		/* Set the recursion going. We say to keep going until a
		 * closing brace, but we don't really expect to find one.
		 * If the return value is not the nul-terminator of the
		 * string, that can only mean that we did find an unmatched
		 * closing brace in the string. We increment past it (else
		 * we get stuck in an infinite loop) and try again.
		 */
		while (*(str = enhanced_recursion(str, TRUE,
				save_fontname, save_fontsize,
				0.0, TRUE, TRUE, 0))) {
			GraphEnhancedFlush();
			if (!*++str)
				break; /* end of string */
			/* else carry on and process the rest of the string */
		}

		/* In order to do text justification we need to do a second pass
		 * that uses information stored during the first pass, see
		 * GraphEnhancedFlush().
		 */
		if (pass == 1) {
			/* do the actual printing in the next pass */
			enhstate.sizeonly = FALSE;
			str = original_string;
		}
	}

	/* restore terminal */
	if (WIN_term) term = tsave;

	/* restore previous font */
	GraphChangeFont(lpgw, save_fontname, save_fontsize, hdc, *rect);
	SetFont(lpgw, hdc);

	/* restore text alignment */
	draw_text_justify(hdc, enhstate.lpgw->justify);

	return enhstate.totalwidth;
}


void
draw_get_enhanced_text_extend(PRECT extend)
{
	switch (enhstate.lpgw->justify) {
	case LEFT:
		extend->left  = 0;
		extend->right = enhstate.totalwidth;
		break;
	case CENTRE:
		extend->left  = enhstate.totalwidth / 2;
		extend->right = enhstate.totalwidth / 2;
		break;
	case RIGHT:
		extend->left  = enhstate.totalwidth;
		extend->right = 0;
		break;
	}
	extend->top = - enhstate.totalasc;
	extend->bottom = enhstate.totaldesc;
}


static void
draw_text_justify(HDC hdc, int justify)
{
	switch (justify)
	{
		case LEFT:
			SetTextAlign(hdc, TA_LEFT|TA_BOTTOM);
			break;
		case RIGHT:
			SetTextAlign(hdc, TA_RIGHT|TA_BOTTOM);
			break;
		case CENTRE:
			SetTextAlign(hdc, TA_CENTER|TA_BOTTOM);
			break;
	}
}


static void
draw_put_text(LPGW lpgw, HDC hdc, int x, int y, char * str)
{
	SetBkMode(hdc, TRANSPARENT);

	/* support text encoding */
	if ((lpgw->encoding == S_ENC_DEFAULT) || (lpgw->encoding == S_ENC_INVALID)) {
		TextOut(hdc, x, y, str, lstrlen(str));
	} else {
		LPWSTR textw = UnicodeText(str, lpgw->encoding);
		if (textw) {
			TextOutW(hdc, x, y, textw, wcslen(textw));
			free(textw);
		} else {
			/* print this only once */
			if (lpgw->encoding != lpgw->encoding_error) {
				fprintf(stderr, "windows terminal: encoding %s not supported\n", encoding_names[lpgw->encoding]);
				lpgw->encoding_error = lpgw->encoding;
			}
			/* fall back to standard encoding */
			TextOut(hdc, x, y, str, strlen(str));
		}
	}
	SetBkMode(hdc, OPAQUE);
}


static void
draw_new_pens(LPGW lpgw, HDC hdc, LOGPEN cur_penstruct)
{
	HPEN old_hapen = lpgw->hapen;
	HPEN old_hsolid = lpgw->hsolid;

	if (cur_penstruct.lopnWidth.x <= 1) {
		/* shige: work-around for Windows clipboard bug */
		lpgw->hapen = CreatePenIndirect((LOGPEN *) &cur_penstruct);
		lpgw->hsolid = CreatePen(PS_SOLID, 1, cur_penstruct.lopnColor);
	} else {
		LOGBRUSH lb;
		lb.lbStyle = BS_SOLID;
		lb.lbColor = cur_penstruct.lopnColor;
		lpgw->hapen = ExtCreatePen(
			PS_GEOMETRIC | cur_penstruct.lopnStyle |
			(lpgw->rounded ? PS_ENDCAP_ROUND | PS_JOIN_ROUND : PS_ENDCAP_SQUARE | PS_JOIN_MITER),
			cur_penstruct.lopnWidth.x, &lb, 0, 0);
		lpgw->hsolid = ExtCreatePen(
			PS_GEOMETRIC | PS_SOLID |
			(lpgw->rounded ? PS_ENDCAP_ROUND | PS_JOIN_ROUND : PS_ENDCAP_SQUARE | PS_JOIN_MITER),
			cur_penstruct.lopnWidth.x, &lb, 0, 0);
	}

	SelectObject(hdc, lpgw->hapen);
	DeleteObject(old_hapen);
	DeleteObject(old_hsolid);
}


static void
draw_new_brush(LPGW lpgw, HDC hdc, COLORREF color)
{
	HBRUSH new_brush = CreateSolidBrush(color);
	SelectObject(hdc, new_brush);
	if (lpgw->hcolorbrush)
		DeleteObject(lpgw->hcolorbrush);
	lpgw->hcolorbrush = new_brush;
}

void
draw_update_keybox(LPGW lpgw, unsigned plotno, unsigned x, unsigned y)
{
	LPRECT bb;
	if (plotno == 0) return;
	if (plotno > lpgw->maxkeyboxes) {
		int i;
		lpgw->maxkeyboxes += 10;
		lpgw->keyboxes = (LPRECT) realloc(lpgw->keyboxes,
				lpgw->maxkeyboxes * sizeof(RECT));
		for (i = plotno - 1; i < lpgw->maxkeyboxes; i++) {
			lpgw->keyboxes[i].left = INT_MAX;
			lpgw->keyboxes[i].right = 0;
			lpgw->keyboxes[i].bottom = INT_MAX;
			lpgw->keyboxes[i].top = 0;
		}
	}
	bb = lpgw->keyboxes + plotno - 1;
	if (x < bb->left)   bb->left = x;
	if (x > bb->right)  bb->right = x;
	if (y < bb->bottom) bb->bottom = y;
	if (y > bb->top)    bb->top = y;
}


static void
draw_grey_out_key_box(LPGW lpgw, HDC hdc, int plotno)
{
	HDC memdc;
	HBITMAP membmp, oldbmp;
	BLENDFUNCTION ftn;
	HBRUSH oldbrush;
	LPRECT bb;
	int width, height;

	bb = lpgw->keyboxes + plotno - 1;
	width = bb->right - bb->left + 1;
	height = bb->top - bb->bottom + 1;
	memdc = CreateCompatibleDC(hdc);
	membmp = CreateCompatibleBitmap(hdc, width, height);
	oldbmp = (HBITMAP) SelectObject(memdc, membmp);
	oldbrush = SelectObject(memdc, (HBRUSH) GetStockObject(LTGRAY_BRUSH));
	PatBlt(memdc, 0, 0, width, height, PATCOPY);
	ftn.AlphaFormat = 0; /* no alpha channel in bitmap */
	ftn.SourceConstantAlpha = (UCHAR)(128); /* global alpha */
	ftn.BlendOp = AC_SRC_OVER;
	ftn.BlendFlags = 0;
	AlphaBlend(hdc, bb->left, bb->bottom, width, height,
		   memdc, 0, 0, width, height, ftn);
	SelectObject(memdc, oldbrush);
	SelectObject(memdc, oldbmp);
	DeleteObject(membmp);
	DeleteDC(memdc);
}


void
draw_image(LPGW lpgw, HDC hdc, char *image, POINT corners[4], unsigned int width, unsigned int height, int color_mode)
{
	BITMAPINFO bmi;
	HRGN hrgn;
	int rc;

	if (image == NULL)
		return;

	ZeroMemory(&bmi, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;

	/* create clip region */
	hrgn = CreateRectRgn(
		GPMIN(corners[2].x, corners[3].x), GPMIN(corners[2].y, corners[3].y),
		GPMAX(corners[2].x, corners[3].x) + 1, GPMAX(corners[2].y, corners[3].y) + 1);
	SelectClipRgn(hdc, hrgn);

	if (color_mode != IC_RGBA) {
		char * dibimage;
		bmi.bmiHeader.biBitCount = 24;

		if (!lpgw->color) {
			/* create a copy of the color image */
			int pad_bytes = (4 - (3 * width) % 4) % 4; /* scan lines start on ULONG boundaries */
			int image_size = (width * 3 + pad_bytes) * height;
			int x, y;
			dibimage = (char *) malloc(image_size);
			memcpy(dibimage, image, image_size);
			for (y = 0; y < height; y ++) {
				for (x = 0; x < width; x ++) {
					BYTE * p = (BYTE *) dibimage + y * (3 * width + pad_bytes) + x * 3;
					/* convert to gray */
					unsigned luma = luma_from_color(p[2], p[1], p[0]);
					p[0] = p[1] = p[2] = luma;
				}
			}

		} else {
			dibimage = image;
		}

		rc = StretchDIBits(hdc,
			GPMIN(corners[0].x, corners[1].x) , GPMIN(corners[0].y, corners[1].y),
			abs(corners[1].x - corners[0].x), abs(corners[1].y - corners[0].y),
			0, 0, width, height,
			dibimage, &bmi, DIB_RGB_COLORS, SRCCOPY);

		if (!lpgw->color) free(dibimage);
	} else {
		HDC memdc;
		HBITMAP membmp, oldbmp;
		UINT32 *pvBits;
		BLENDFUNCTION ftn;

		bmi.bmiHeader.biBitCount = 32;
		memdc = CreateCompatibleDC(hdc);
		membmp = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void **)&pvBits, NULL, 0x0);
		oldbmp = (HBITMAP)SelectObject(memdc, membmp);

		memcpy(pvBits, image, width * height * 4);

		/* convert to grayscale? */
		if (!lpgw->color) {
			int x, y;
			for (y = 0; y < height; y ++) {
				for (x = 0; x < width; x ++) {
					UINT32 * p = pvBits + y * width + x;
					/* convert to gray */
					unsigned luma = luma_from_color(GetRValue(*p), GetGValue(*p), GetBValue(*p));
					*p = (*p & 0xff000000) | RGB(luma, luma, luma);
				}
			}
		}

		ftn.BlendOp = AC_SRC_OVER;
		ftn.BlendFlags = 0;
		ftn.AlphaFormat = AC_SRC_ALPHA; /* bitmap has an alpha channel */
		ftn.SourceConstantAlpha = 0xff;
		AlphaBlend(hdc,
			GPMIN(corners[0].x, corners[1].x) , GPMIN(corners[0].y, corners[1].y),
			abs(corners[1].x - corners[0].x), abs(corners[1].y - corners[0].y),
			memdc, 0, 0, width, height, ftn);

		SelectObject(memdc, oldbmp);
		DeleteObject(membmp);
		DeleteDC(memdc);
	}
	SelectClipRgn(hdc, NULL);
}


/* This one is really the heart of this module: it executes the stored set of
 * commands, whenever it changed or a redraw is necessary */
static void
drawgraph(LPGW lpgw, HDC hdc, LPRECT rect)
{
	/* draw ops */
	unsigned int ngwop = 0;
	struct GWOP *curptr;
	struct GWOPBLK *blkptr;

	/* layers and hypertext */
	unsigned plotno = 0;
	BOOL gridline = FALSE;
	BOOL skipplot = FALSE;
	BOOL keysample = FALSE;
	BOOL interactive;
	LPWSTR hypertext = NULL;
	int hypertype = 0;

	/* colors */
	BOOL isColor;				/* use colors? */
	COLORREF last_color = 0;	/* currently selected color */
	double alpha_c = 1.;		/* alpha for transparency */

	/* lines */
	double line_width = lpgw->sampling * lpgw->linewidth;	/* current line width */
	double lw_scale = 1.;
	LOGPEN cur_penstruct;		/* current pen settings */

	/* polylines and polygons */
	int polymax = 200;			/* size of ppt */
	int polyi = 0;				/* number of points in ppt */
	POINT *ppt;					/* storage of polyline/polygon-points */
	unsigned int lastop=-1;		/* used for plotting last point on a line */
	POINT cpoint;				/* current GDI location */

	/* filled polygons and boxes */
	unsigned int fillstyle = 0;	/* current fill style */
	BOOL transparent = FALSE;	/* transparent fill? */
	double alpha = 0.;			/* alpha for transarency */
	int pattern = 0;			/* patter number */
	COLORREF fill_color = 0;	/* color to use for fills */
	HBRUSH solid_brush = 0;		/* current solid fill brush */
	int shadedblendcaps = SB_CONST_ALPHA;	/* displays can always do AlphaBlend */
	bool warn_no_transparent = FALSE;

	/* images */
	POINT corners[4];			/* image corners */
	int color_mode = 0;			/* image color mode */

#ifdef EAM_BOXED_TEXT
	struct s_boxedtext {
		TBOOLEAN boxing;
		t_textbox_options option;
		POINT margin;
		POINT start;
		RECT  box;
		int   angle;
	} boxedtext;
#endif

	/* point symbols */
	bool ps_caching = FALSE;
	int last_symbol = 0;
	HDC cb_memdc = NULL;
	HBITMAP cb_old_bmp;
	HBITMAP cb_membmp;
	POINT cb_ofs;

	/* coordinates and lengths */
	int xdash, ydash;			/* the transformed coordinates */
	int rr, rl, rt, rb;			/* coordinates of drawing area */
	int htic, vtic;				/* tic sizes */
	int hshift, vshift;			/* correction of text position */

	/* indices */
	int seq = 0;				/* sequence counter for W_image and W_boxedtext */
	int i;

    if (lpgw->locked) return;

	/* clear hypertexts only in display sessions */
	interactive = (GetObjectType(hdc) == OBJ_MEMDC) ||
		((GetObjectType(hdc) == OBJ_DC) && (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASDISPLAY));
	if (interactive)
		clear_tooltips(lpgw);

	/* The GDI status query functions don't work on metafile, printer or
	 * plotter handles, so can't know whether the screen is actually showing
	 * color or not, if drawgraph() is being called from CopyClip().
	 * Solve by defaulting isColor to TRUE in those cases.
	 * Note that info on color capabilities of printers would be available
	 * via DeviceCapabilities().
	 */
	isColor = (((GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL)) > 2)
	       || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_METAFILE)
	       || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_PLOTTER)
	       || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER));

	if (isColor) {
		SetBkColor(hdc, lpgw->background);
		FillRect(hdc, rect, lpgw->hbrush);
	} else {
		FillRect(hdc, rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
	}

	/* Need to scale line widths for raster printers so they are the same
	   as on screen */
	if ((GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER)) {
		HDC hdc_screen = GetDC(NULL);
		lw_scale = (double) GetDeviceCaps(hdc, VERTRES) /
		           (double) GetDeviceCaps(hdc_screen, VERTRES);
		line_width *= lw_scale;
		ReleaseDC(NULL, hdc_screen);
		/* Does the printer support AlphaBlend with transparency (const alpha)? */
		shadedblendcaps = GetDeviceCaps(hdc, SHADEBLENDCAPS);
		warn_no_transparent = ((shadedblendcaps & SB_CONST_ALPHA) == 0);
	}

	ps_caching = !((GetDeviceCaps(hdc, TECHNOLOGY) == DT_METAFILE)
	            || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_PLOTTER)
	            || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER));

	ppt = (POINT *)LocalAllocPtr(LHND, (polymax+1) * sizeof(POINT));

	rr = rect->right;
	rl = rect->left;
	rt = rect->top;
	rb = rect->bottom;

	htic = (lpgw->org_pointsize * MulDiv(lpgw->htic, rr - rl, lpgw->xmax) + 1);
	vtic = (lpgw->org_pointsize * MulDiv(lpgw->vtic, rb - rt, lpgw->ymax) + 1);

	/* (re-)init GDI fonts */
	GraphChangeFont(lpgw, lpgw->deffontname, lpgw->deffontsize, hdc, *rect);
	lpgw->angle = 0;
	SetFont(lpgw, hdc);
	lpgw->justify = LEFT;
	SetTextAlign(hdc, TA_LEFT|TA_BOTTOM);

	/* calculate text shifting for horizontal text */
	hshift = 0;
	vshift = MulDiv(lpgw->vchar, rb - rt, lpgw->ymax)/2;

	/* init layer variables */
	lpgw->numplots = 0;
	lpgw->hasgrid = FALSE;
	for (i = 0; i < lpgw->maxkeyboxes; i++) {
		lpgw->keyboxes[i].left = INT_MAX;
		lpgw->keyboxes[i].right = 0;
		lpgw->keyboxes[i].bottom = INT_MAX;
		lpgw->keyboxes[i].top = 0;
	}

#ifdef EAM_BOXED_TEXT
	boxedtext.boxing = FALSE;
#endif

	SelectObject(hdc, lpgw->hapen); /* background brush */
	SelectObject(hdc, lpgw->colorbrush[2]); /* first user pen */

	/* do the drawing */
	blkptr = lpgw->gwopblk_head;
	curptr = NULL;
	if (blkptr != NULL) {
		if (!blkptr->gwop)
			blkptr->gwop = (struct GWOP *)GlobalLock(blkptr->hblk);
		if (!blkptr->gwop)
			return;
		curptr = (struct GWOP *)blkptr->gwop;
	}
	if (curptr == NULL)
	    return;

	while (ngwop < lpgw->nGWOP) {
		/* transform the coordinates */
		xdash = MulDiv(curptr->x, rr-rl-1, lpgw->xmax) + rl;
		ydash = MulDiv(curptr->y, rt-rb+1, lpgw->ymax) + rb - 1;

		/* ignore superfluous moves - see bug #1523 */
		/* FIXME: we should do this in win.trm, not here */
		if ((lastop == W_vect) && (curptr->op == W_move) && (xdash == ppt[polyi -1].x) && (ydash == ppt[polyi -1].y)) {
		    curptr->op = 0;
		}

		/* finish last polygon / polyline */
		if ((lastop == W_vect) && (curptr->op != W_vect) && (curptr->op != 0)) {
			if (polyi >= 2) {
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing)
					gdiplusPolyline(hdc, ppt, polyi, &cur_penstruct, alpha_c);
				else
#endif
					Polyline(hdc, ppt, polyi);
				/* move internal GDI state to last point */
				MoveTo(hdc, ppt[polyi-1].x, ppt[polyi-1].y);
				cpoint = ppt[polyi-1];
			} else if (polyi == 1) {
				/* degenerate case e.g. when using 'linecolor variable' */
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing)
					gdiplusLine(hdc, cpoint, ppt[0], &cur_penstruct, alpha_c);
				else
#endif
					LineTo(hdc, ppt[0].x, ppt[0].y);
				cpoint = ppt[0];
			}
			polyi = 0;
		}

		/* handle layer commands first */
		if (curptr->op == W_layer) {
			t_termlayer layer = curptr->x;
			switch (layer) {
				case TERM_LAYER_BEFORE_PLOT:
					plotno++;
					lpgw->numplots = plotno;
					if (plotno >= lpgw->maxhideplots) {
						int idx;
						lpgw->maxhideplots += 10;
						lpgw->hideplot = (BOOL *) realloc(lpgw->hideplot, lpgw->maxhideplots * sizeof(BOOL));
						for (idx = plotno; idx < lpgw->maxhideplots; idx++)
							lpgw->hideplot[idx] = FALSE;
					}
					if (plotno <= lpgw->maxhideplots)
						skipplot = lpgw->hideplot[plotno - 1];
					break;
				case TERM_LAYER_AFTER_PLOT:
					skipplot = FALSE;
					break;
#if 0
				case TERM_LAYER_BACKTEXT:
				case TERM_LAYER_FRONTTEXT:
					break;
#endif
				case TERM_LAYER_BEGIN_GRID:
					gridline = TRUE;
					lpgw->hasgrid = TRUE;
					break;
				case TERM_LAYER_END_GRID:
					gridline = FALSE;
					break;
				case TERM_LAYER_BEGIN_KEYSAMPLE:
					keysample = TRUE;
					break;
				case TERM_LAYER_END_KEYSAMPLE:
					/* grey out keysample if graph is hidden */
					if ((plotno <= lpgw->maxhideplots) && lpgw->hideplot[plotno - 1])
						draw_grey_out_key_box(lpgw, hdc, plotno);
					keysample = FALSE;
					break;
				case TERM_LAYER_RESET:
				case TERM_LAYER_RESET_PLOTNO:
					plotno = 0;
					break;
				default:
					break;
			};
		}

		/* hide this layer? */
		if (!(skipplot || (gridline && lpgw->hidegrid)) ||
			keysample || (curptr->op == W_line_type) || (curptr->op == W_setcolor)
			          || (curptr->op == W_pointsize) || (curptr->op == W_line_width)) {

		/* special case hypertexts */
		if ((hypertext != NULL) && (hypertype == TERM_HYPERTEXT_TOOLTIP)) {
			/* point symbols */
			if ((curptr->op >= W_dot) && (curptr->op <= W_dot + WIN_POINT_TYPES)) {
				RECT rect;
				rect.left = xdash - htic;
				rect.right = xdash + htic;
				rect.top = ydash - vtic;
				rect.bottom = ydash + vtic;
				add_tooltip(lpgw, &rect, hypertext);
				hypertext = NULL;
			}
		}

		switch (curptr->op) {
		case 0:	/* have run past last in this block */
			break;

		case W_layer: /* already handled above */
			break;

		case W_move:
			ppt[0].x = xdash;
			ppt[0].y = ydash;
			polyi = 1;
			if (keysample)
				draw_update_keybox(lpgw, plotno, xdash, ydash);
			break;

		case W_vect:
			ppt[polyi].x = xdash;
			ppt[polyi].y = ydash;
			polyi++;
			if (polyi >= polymax) {
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing)
					gdiplusPolyline(hdc, ppt, polyi, &cur_penstruct, alpha_c);
				else
#endif
					Polyline(hdc, ppt, polyi);
				MoveTo(hdc, xdash, ydash);
				ppt[0].x = xdash;
				ppt[0].y = ydash;
				polyi = 1;
				cpoint = ppt[0];
			}
			if (keysample)
				draw_update_keybox(lpgw, plotno, xdash, ydash);
			break;

		case W_line_type: {
			int cur_pen = (int)curptr->x % WGNUMPENS;

			/* create new pens */
			if (cur_pen > LT_NODRAW) {
				cur_pen += 2;
				cur_penstruct =  (lpgw->color && isColor) ?  lpgw->colorpen[cur_pen] : lpgw->monopen[cur_pen];
				cur_penstruct.lopnStyle =
					lpgw->dashed ? lpgw->monopen[cur_pen].lopnStyle : lpgw->colorpen[cur_pen].lopnStyle;
			} else if (cur_pen == LT_NODRAW) {
				cur_pen = WGNUMPENS;
				cur_penstruct.lopnStyle = PS_NULL;
				cur_penstruct.lopnColor = 0;
				cur_penstruct.lopnWidth.x = 1;
			} else { /* <= LT_BACKGROUND */
				cur_pen = WGNUMPENS;
				cur_penstruct.lopnStyle = PS_SOLID;
				cur_penstruct.lopnColor = lpgw->background;
				cur_penstruct.lopnWidth.x = 1;
			}
			cur_penstruct.lopnWidth.x *= line_width;
			draw_new_pens(lpgw, hdc, cur_penstruct);

			/* select new brush */
			if (cur_pen < WGNUMPENS)
				solid_brush = lpgw->colorbrush[cur_pen];
			else
				solid_brush = lpgw->hbrush;
			SelectObject(hdc, solid_brush);

			/* set text color, also used for pattern fill */
			SetTextColor(hdc, cur_penstruct.lopnColor);

			/* remember this color */
			last_color = cur_penstruct.lopnColor;
			fill_color = last_color;
			alpha_c = 1.;
			break;
		}

		case W_dash_type: {
			int dt = curptr->x;

			if (dt >= 0) {
				dt %= WGNUMPENS;
				dt += 2;
				cur_penstruct.lopnStyle = lpgw->monopen[dt].lopnStyle;
				draw_new_pens(lpgw, hdc, cur_penstruct);
			} else if (dt == DASHTYPE_SOLID) {
				cur_penstruct.lopnStyle = PS_SOLID;
				draw_new_pens(lpgw, hdc, cur_penstruct);
			} else if (dt == DASHTYPE_AXIS) {
				dt = 1;
				cur_penstruct.lopnStyle =
					lpgw->dashed ? lpgw->monopen[dt].lopnStyle : lpgw->colorpen[dt].lopnStyle;
				draw_new_pens(lpgw, hdc, cur_penstruct);
			} else if (dt == DASHTYPE_CUSTOM) {
				; /* ignored */
			}
			break;
		}

		case W_text_encoding:
			lpgw->encoding = curptr->x;
			break;

		case W_put_text: {
			char * str;
			str = LocalLock(curptr->htext);
			if (str) {
				int dxl, dxr;
				int slen, vsize;

				/* shift correctly for rotated text */
				draw_put_text(lpgw, hdc, xdash + hshift, ydash + vshift, str);
#ifndef EAM_BOXED_TEXT
				if (keysample) {
#else
				if (keysample || boxedtext.boxing) {
#endif
					slen  = GraphGetTextLength(lpgw, hdc, str);
					vsize = MulDiv(lpgw->vchar, rb - rt, 2 * lpgw->ymax);
					if (lpgw->justify == LEFT) {
						dxl = 0;
						dxr = slen;
					} else if (lpgw->justify == CENTRE) {
						dxl = dxr = slen / 2;
					} else {
						dxl = slen;
						dxr = 0;
					}
				}

				if (keysample) {
					draw_update_keybox(lpgw, plotno, xdash - dxl, ydash - vsize);
					draw_update_keybox(lpgw, plotno, xdash + dxr, ydash + vsize);
				}
#ifdef EAM_BOXED_TEXT
				if (boxedtext.boxing) {
					if (boxedtext.box.left > (xdash - boxedtext.start.x - dxl))
						boxedtext.box.left = xdash - boxedtext.start.x - dxl;
					if (boxedtext.box.right < (xdash - boxedtext.start.x + dxr))
						boxedtext.box.right = xdash - boxedtext.start.x + dxr;
					if (boxedtext.box.top > (ydash - boxedtext.start.y - vsize))
						boxedtext.box.top = ydash - boxedtext.start.y - vsize;
					if (boxedtext.box.bottom < (ydash - boxedtext.start.y + vsize))
						boxedtext.box.bottom = ydash - boxedtext.start.y + vsize;
					/* We have to remember the text angle as well. */
					boxedtext.angle = lpgw->angle;
				}
#endif
			}
			LocalUnlock(curptr->htext);
			break;
		}

		case W_enhanced_text: {
			char * str;
			str = LocalLock(curptr->htext);
			if (str) {
				RECT extend;

				draw_enhanced_text(lpgw, hdc, rect, xdash, ydash, str);
				draw_get_enhanced_text_extend(&extend);
				if (keysample) {
					draw_update_keybox(lpgw, plotno, xdash - extend.left, ydash - extend.top);
					draw_update_keybox(lpgw, plotno, xdash + extend.right, ydash + extend.bottom);
				}
#ifdef EAM_BOXED_TEXT
				if (boxedtext.boxing) {
					if (boxedtext.box.left > (boxedtext.start.x - xdash - extend.left))
						boxedtext.box.left = boxedtext.start.x - xdash - extend.left;
					if (boxedtext.box.right < (boxedtext.start.x - xdash + extend.right))
						boxedtext.box.right = boxedtext.start.x - xdash + extend.right;
					if (boxedtext.box.top > (boxedtext.start.y - ydash - extend.top))
						boxedtext.box.top = boxedtext.start.y - ydash - extend.top;
					if (boxedtext.box.bottom < (boxedtext.start.y - ydash + extend.bottom))
						boxedtext.box.bottom = boxedtext.start.y - ydash + extend.bottom;
					/* We have to store the text angle as well. */
					boxedtext.angle = lpgw->angle;
				}
#endif
			}
			LocalUnlock(curptr->htext);
			break;
		}

		case W_hypertext:
			if (interactive) {
				/* Make a copy for future reference */
				char * str = LocalLock(curptr->htext);
				free(hypertext);
				hypertext = UnicodeText(str, lpgw->encoding);
				hypertype = curptr->x;
				LocalUnlock(curptr->htext);
			}
			break;

#ifdef EAM_BOXED_TEXT
		case W_boxedtext:
			if (seq == 0) {
				boxedtext.option = curptr->x;
				seq++;
				break;
			}
			seq = 0;
			switch (boxedtext.option) {
			case TEXTBOX_INIT:
				/* initialise bounding box */
				boxedtext.box.left   = boxedtext.box.right = 0;
				boxedtext.box.bottom = boxedtext.box.top   = 0;
				boxedtext.start.x = xdash;
				boxedtext.start.y = ydash;
				/* Note: initialising the text angle here would be best IMHO,
				   but current core code does not set this until the actual
				   print-out is done. */
				boxedtext.angle = lpgw->angle;
				boxedtext.boxing = TRUE;
				break;
			case TEXTBOX_OUTLINE:
			case TEXTBOX_BACKGROUNDFILL: {
				/* draw rectangle */
				int dx = boxedtext.margin.x;
				int dy = boxedtext.margin.y;
#if 0
				printf("left %i right %i top %i bottom %i angle %i\n",
					boxedtext.box.left - dx, boxedtext.box.right + dx,
					boxedtext.box.top - dy, boxedtext.box.bottom + dy,
					boxedtext.angle);
#endif
				if ((boxedtext.angle % 90) == 0) {
					RECT rect;

					switch (boxedtext.angle) {
					case 0:
						rect.left   = + boxedtext.box.left   - dx;
						rect.right  = + boxedtext.box.right  + dx;
						rect.top    = + boxedtext.box.top    - dy;
						rect.bottom = + boxedtext.box.bottom + dy;
						break;
					case 90:
						rect.left   = + boxedtext.box.top    - dy;
						rect.right  = + boxedtext.box.bottom + dy;
						rect.top    = - boxedtext.box.right  - dx;
						rect.bottom = - boxedtext.box.left   + dx;
						break;
					case 180:
						rect.left   = - boxedtext.box.right  - dx;
						rect.right  = - boxedtext.box.left   + dx;
						rect.top    = - boxedtext.box.bottom - dy;
						rect.bottom = - boxedtext.box.top    + dy;
						break;
					case 270:
						rect.left   = - boxedtext.box.bottom - dy;
						rect.right  = - boxedtext.box.top    + dy;
						rect.top    = + boxedtext.box.left   - dx;
						rect.bottom = + boxedtext.box.right  + dx;
						break;
					}
					rect.left   += boxedtext.start.x;
					rect.right  += boxedtext.start.x;
					rect.top    += boxedtext.start.y;
					rect.bottom += boxedtext.start.y;
					if (boxedtext.option == TEXTBOX_OUTLINE)
						/* FIXME: Shouldn't we use the current color brush lpgw->hcolorbrush? */
						FrameRect(hdc, &rect, GetStockBrush(BLACK_BRUSH));
					else
						/* Fill bounding box with background color. */
						FillRect(hdc, &rect, lpgw->hbrush);
				} else {
					double theta = boxedtext.angle * M_PI/180.;
					double sin_theta = sin(theta);
					double cos_theta = cos(theta);
					POINT  rect[5];

					rect[0].x =  (boxedtext.box.left   - dx) * cos_theta +
								 (boxedtext.box.top    - dy) * sin_theta;
					rect[0].y = -(boxedtext.box.left   - dx) * sin_theta +
								 (boxedtext.box.top    - dy) * cos_theta;
					rect[1].x =  (boxedtext.box.left   - dx) * cos_theta +
								 (boxedtext.box.bottom + dy) * sin_theta;
					rect[1].y = -(boxedtext.box.left   - dx) * sin_theta +
								 (boxedtext.box.bottom + dy) * cos_theta;
					rect[2].x =  (boxedtext.box.right  + dx) * cos_theta +
								 (boxedtext.box.bottom + dy) * sin_theta;
					rect[2].y = -(boxedtext.box.right  + dx) * sin_theta +
								 (boxedtext.box.bottom + dy) * cos_theta;
					rect[3].x =  (boxedtext.box.right  + dx) * cos_theta +
								 (boxedtext.box.top    - dy) * sin_theta;
					rect[3].y = -(boxedtext.box.right  + dx) * sin_theta +
								 (boxedtext.box.top    - dy) * cos_theta;
					for (i = 0; i < 4; i++) {
						rect[i].x += boxedtext.start.x;
						rect[i].y += boxedtext.start.y;
					}
#ifdef HAVE_GDIPLUS
					if (!lpgw->antialiasing)
#endif
					{
						HBRUSH save_brush;
						HPEN save_pen;

						if (boxedtext.option == TEXTBOX_OUTLINE) {
							save_brush = SelectObject(hdc, GetStockBrush(NULL_BRUSH));
							/* FIXME: Shouldn't we use the current color brush lpgw->hcolorbrush? */
							save_pen = SelectObject(hdc, GetStockPen(BLACK_PEN));
						} else {
							/* Fill bounding box with background color. */
							save_brush = SelectObject(hdc, lpgw->hbrush);
							save_pen = SelectObject(hdc, GetStockPen(NULL_PEN));
						}
						Polygon(hdc, rect, 4);
						SelectObject(hdc, save_brush);
						SelectObject(hdc, save_pen);
					}
#ifdef HAVE_GDIPLUS
					else {
						if (boxedtext.option == TEXTBOX_OUTLINE) {
							rect[4].x = rect[0].x;
							rect[4].y = rect[0].y;
							gdiplusPolylineEx(hdc, rect, 5, PS_SOLID, line_width, RGB(0,0,0) /* last_color */, 1.);
						} else {
							gdiplusSolidFilledPolygonEx(hdc, rect, 4, lpgw->background, 1., lpgw->polyaa);
						}
					}
#endif
				}
				boxedtext.boxing = FALSE;
				break;
			}
			case TEXTBOX_MARGINS:
				/* Adjust size of whitespace around text: default is 1/2 char height + 2 char widths. */
				boxedtext.margin.x = MulDiv(curptr->y, (rr - rl) * lpgw->hchar, 100 * lpgw->xmax);
				boxedtext.margin.y = MulDiv(curptr->y, (rb - rt) * lpgw->vchar, 400 * lpgw->ymax);
				break;
			default:
				break;
			}
			break;
#endif

		case W_fillstyle:
			/* HBB 20010916: new entry, needed to squeeze the many
			 * parameters of a filled box call through the bottleneck
			 * of the fixed number of parameters in GraphOp() and
			 * struct GWOP, respectively. */
			fillstyle = curptr->x;
			transparent = FALSE;
			alpha = 0.;
			switch (fillstyle & 0x0f) {
			case FS_TRANSPARENT_SOLID:
				alpha = (fillstyle >> 4) / 100.;
				if ((shadedblendcaps & SB_CONST_ALPHA) != 0) {
					transparent = TRUE;
					/* we already have a brush with that color */
				} else {
					/* Printer does not support AlphaBlend() */
					COLORREF color =
						RGB(255 - alpha * (255 - GetRValue(last_color)),
							255 - alpha * (255 - GetGValue(last_color)),
							255 - alpha * (255 - GetBValue(last_color)));
					solid_brush = lpgw->hcolorbrush;
					fill_color = color;
					draw_new_brush(lpgw, hdc, fill_color);
					fillstyle = (fillstyle & 0xfffffff0) | FS_SOLID;
					if (warn_no_transparent) {
						fprintf(stderr, "Warning: Transparency not supported on this device.\n");
						warn_no_transparent = FALSE; /* Warn only once */
					}
				}
				break;
			case FS_SOLID: {
				if (alpha_c < 1.) {
					alpha = alpha_c;
					fill_color = last_color;
					draw_new_brush(lpgw, hdc, fill_color);
					solid_brush = lpgw->hcolorbrush;
				} else if ((int)(fillstyle >> 4) == 100) {
					/* special case this common choice */
					// FIXME: we should already have that!
					fill_color = last_color;
					draw_new_brush(lpgw, hdc, fill_color);
					solid_brush = lpgw->hcolorbrush;
				} else {
					double density = MINMAX(0, (int)(fillstyle >> 4), 100) * 0.01;
					COLORREF color =
						RGB(255 - density * (255 - GetRValue(last_color)),
							255 - density * (255 - GetGValue(last_color)),
							255 - density * (255 - GetBValue(last_color)));
					solid_brush = lpgw->hcolorbrush;
					fill_color = color;
				}
				break;
			}
			case FS_TRANSPARENT_PATTERN:
				if ((shadedblendcaps & SB_CONST_ALPHA) != 0) {
					transparent = TRUE;
					alpha = 1.;
				} else {
					/* Printer does not support AlphaBlend() */
					fillstyle = (fillstyle & 0xfffffff0) | FS_PATTERN;
					if (warn_no_transparent) {
						fprintf(stderr, "Warning: Transparency not supported on this device.\n");
						warn_no_transparent = FALSE; /* Warn only once */
					}
				}
				/* intentionally fall through */
			case FS_PATTERN:
				/* style == 2 --> use fill pattern according to
						 * fillpattern. Pattern number is enumerated */
				pattern = GPMAX(fillstyle >> 4, 0) % pattern_num;
				SelectObject(hdc, pattern_brush[pattern]);
				break;
			case FS_EMPTY:
				/* FIXME: Instead of filling with background color, we should not fill at all in this case! */
				/* fill with background color */
				SelectObject(hdc, lpgw->hbrush);
				fill_color = lpgw->background;
				solid_brush = lpgw->hbrush;
				break;
			case FS_DEFAULT:
			default:
				/* Leave the current brush and color in place */
				break;
			}
			break;

		case W_boxfill: {  /* ULIG */
			/* NOTE: the x and y passed with this call are the coordinates of the
			 * lower right corner of the box. The upper left corner was stored into
			 * ppt[0] by a preceding W_move, and the style was set
			 * by a W_fillstyle call. */
			POINT p;
			UINT  height, width;

			width = abs(xdash - ppt[0].x);
			height = abs(ppt[0].y - ydash);
			p.x = GPMIN(ppt[0].x, xdash);
			p.y = GPMIN(ydash, ppt[0].y);

#ifdef HAVE_GDIPLUS
			if (lpgw->antialiasing && lpgw->patternaa &&
			    (((fillstyle & 0x0f) == FS_PATTERN) ||
			     ((fillstyle & 0x0f) == FS_TRANSPARENT_PATTERN))) {
				ppt[1].x = ppt[0].x;
				ppt[1].y = ydash;
				ppt[2].x = xdash;
				ppt[2].y = ydash;
				ppt[3].x = xdash;
				ppt[3].y = ppt[0].y;
				ppt[4].x = ppt[0].x;
				ppt[4].y = ppt[0].y;
				gdiplusPatternFilledPolygonEx(hdc, ppt, 5, fill_color, 1., lpgw->background, transparent, pattern);
			} else
#endif
			if (transparent) {
				HDC memdc;
				HBITMAP membmp, oldbmp;
				BLENDFUNCTION ftn;
				HBRUSH old_brush;

				/* create memory device context for bitmap */
				memdc = CreateCompatibleDC(hdc);

				/* create standard bitmap, no alpha channel needed */
				membmp = CreateCompatibleBitmap(hdc, width, height);
				oldbmp = (HBITMAP)SelectObject(memdc, membmp);

				/* prepare memory context */
				SetTextColor(memdc, fill_color);
				if ((fillstyle & 0x0f) == FS_TRANSPARENT_PATTERN)
					old_brush = SelectObject(memdc, pattern_brush[pattern]);
				else
					old_brush = SelectObject(memdc, solid_brush);

				/* draw into memory bitmap */
				PatBlt(memdc, 0, 0, width, height, PATCOPY);

				/* copy bitmap back */
				if ((fillstyle & 0x0f) == FS_TRANSPARENT_PATTERN) {
					TransparentBlt(hdc, p.x, p.y, width, height,
						   memdc, 0, 0, width, height, 0x00ffffff);
				} else {
					ftn.AlphaFormat = 0; /* no alpha channel in bitmap */
					ftn.SourceConstantAlpha = (UCHAR)(alpha * 0xff); /* global alpha */
					ftn.BlendOp = AC_SRC_OVER;
					ftn.BlendFlags = 0;
					AlphaBlend(hdc, p.x, p.y, width, height,
							   memdc, 0, 0, width, height, ftn);
				}

				/* clean up */
				SelectObject(memdc, old_brush);
				SelectObject(memdc, oldbmp);
				DeleteObject(membmp);
				DeleteDC(memdc);
			} else {
				/* not transparent */
				/* FIXME: this actually is transparent, but probably shouldn't be */
				PatBlt(hdc, p.x, p.y, width, height, PATCOPY);
			/*
				SelectObject(hdc, lpgw->hnull);
				Rectangle(hdc, p.x, p.y, p.x + width + 1, p.y + height + 1);
				SelectObject(hdc, lpgw->hapen);
			*/
			}
			polyi = 0;
			if (keysample)
				draw_update_keybox(lpgw, plotno, xdash + 1, ydash);
			break;
		}

		case W_text_angle:
			if (lpgw->angle != (int)curptr->x) {
				lpgw->angle = (int)curptr->x;
				SetFont(lpgw, hdc);
				/* recalculate shifting of rotated text */
				hshift = sin(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rr-rl, lpgw->xmax) / 2;
				vshift = cos(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rb-rt, lpgw->ymax) / 2;
			}
			break;

		case W_justify:
			draw_text_justify(hdc, curptr->x);
			lpgw->justify = curptr->x;
			break;

		case W_font: {
			int size = curptr->x;
			char * font = LocalLock(curptr->htext);
			/* GraphChangeFont already handles font==NULL and size==0,
			   so the checks below are a bit paranoid...
			*/
			GraphChangeFont(lpgw,
				font != NULL ? font : lpgw->deffontname,
				size > 0 ? size : lpgw->deffontsize,
				hdc, *rect);
			LocalUnlock(curptr->htext);
			SetFont(lpgw, hdc);
			/* recalculate shifting of rotated text */
			hshift = sin(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rr-rl, lpgw->xmax) / 2;
			vshift = cos(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rb-rt, lpgw->ymax) / 2;
			break;
		}

		case W_pointsize:
			if (curptr->x > 0) {
				double pointsize = curptr->x / 100.0;
				htic = pointsize * MulDiv(lpgw->htic, rr-rl, lpgw->xmax) + 1;
				vtic = pointsize * MulDiv(lpgw->vtic, rb-rt, lpgw->ymax) + 1;
			} else {
				htic = vtic = 0;
			}
			/* invalidate point symbol cache */
			last_symbol = 0;
			break;

		case W_line_width:
			/* HBB 20000813: this may look strange, but it ensures
			 * that linewidth is exactly 1 iff it's in default
			 * state */
			line_width = curptr->x == 100 ? 1 : (curptr->x / 100.0);
			line_width *= lpgw->sampling * lpgw->linewidth * lw_scale;
			/* invalidate point symbol cache */
			last_symbol = 0;
			break;

		case W_setcolor: {
			COLORREF color;

			if (curptr->htext != NULL) {	/* TC_LT */
				int pen = (int)curptr->x % WGNUMPENS;
				if (pen <= LT_NODRAW) {
					color = lpgw->background;
				} else {
					if (lpgw->color)
						color = lpgw->colorpen[pen + 2].lopnColor;
					else
						color = lpgw->monopen[pen + 2].lopnColor;
				}
				alpha_c = 1.;
			} else {						/* TC_RGB */
				rgb255_color rgb255;
				rgb255.r = (curptr->y & 0xff);
				rgb255.g = (curptr->x >> 8);
				rgb255.b = (curptr->x & 0xff);
				alpha_c = 1. - ((curptr->y >> 8) & 0xff) / 255.;

				if (lpgw->color || ((rgb255.r == rgb255.g) && (rgb255.r == rgb255.b))) {
					/* Use colors or this is already gray scale */
					color = RGB(rgb255.r, rgb255.g, rgb255.b);
				} else {
					/* convert to gray */
					unsigned luma = luma_from_color(rgb255.r, rgb255.g, rgb255.b);
					color = RGB(luma, luma, luma);
				}
			}

			/* solid fill brush */
			draw_new_brush(lpgw, hdc, color);
			solid_brush = lpgw->hcolorbrush;

			/* create new pen, too */
			cur_penstruct.lopnColor = color;
			draw_new_pens(lpgw, hdc, cur_penstruct);

			/* set text color, which is also used for pattern fill */
			SetTextColor(hdc, color);

			/* invalidate point symbol cache */
			if (last_color != color)
				last_symbol = 0;

			/* remember this color */
			last_color = color;
			fill_color = color;
			break;
		}

		case W_filled_polygon_pt: {
			/* a point of the polygon is coming */
			if (polyi >= polymax) {
				polymax += 200;
				ppt = (POINT *)LocalReAllocPtr(ppt, LHND, (polymax+1) * sizeof(POINT));
			}
			ppt[polyi].x = xdash;
			ppt[polyi].y = ydash;

			polyi++;
			break;
		}

		case W_filled_polygon_draw: {
			/* end of point series --> draw polygon now */
			if (!transparent) {
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing && ((fillstyle & 0x0f) == FS_SOLID)) {
					/* solid, antialiased fill */
					gdiplusSolidFilledPolygonEx(hdc, ppt, polyi, fill_color, alpha_c, lpgw->antialiasing && lpgw->polyaa);
				} else if (lpgw->antialiasing && lpgw->patternaa && ((fillstyle & 0x0f) == FS_PATTERN)) {
					gdiplusPatternFilledPolygonEx(hdc, ppt, polyi, fill_color, 1., lpgw->background, transparent, pattern);
				} else
#endif
				{
					/* fill area without border */
					SelectObject(hdc, lpgw->hnull);
					Polygon(hdc, ppt, polyi);
					SelectObject(hdc, lpgw->hapen); /* restore previous pen */
				}
			} else {
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing && (fillstyle & 0x0f) == FS_TRANSPARENT_SOLID) {
					gdiplusSolidFilledPolygonEx(hdc, ppt, polyi, fill_color, alpha, lpgw->polyaa);
				} else if (lpgw->antialiasing && lpgw->patternaa && ((fillstyle & 0x0f) == FS_TRANSPARENT_PATTERN)) {
					gdiplusPatternFilledPolygonEx(hdc, ppt, polyi, fill_color, 1., lpgw->background, transparent, pattern);
				} else
#endif
				{
				/* BM: To support transparent fill on Windows we draw the
				   polygon into a memory bitmap using a memory device context.

				   We then associate an alpha value to the bitmap and use
				   AlphaBlend() to copy the bitmap back.

				   Note: we could probably simplify and speed up the case of
				   pattern fill by using TransparentBlt() instead.
				*/
				HDC memdc;
				HBITMAP membmp, oldbmp;
				int minx, miny, maxx, maxy;
				UINT32 width, height;
				BITMAPINFO bmi;
				UINT32 *pvBits;
				int x, y, i;
				POINT * points;
				BLENDFUNCTION ftn;
				UINT32 uAlpha = (UCHAR)(0xff * alpha);
				/* make sure the indicator for transparency is different fill_color */
				UINT32 transparentColor = fill_color ^ 0x00ffffff;
				COLORREF bkColor = RGB((transparentColor >> 16) & 0xff, (transparentColor >> 8) & 0xff, transparentColor & 0xff);
				HBRUSH old_brush;
				HPEN old_pen;

				/* find minimum rectangle enclosing our polygon. */
				minx = maxx = ppt[0].x;
				miny = maxy = ppt[0].y;
				for (i = 1; i < polyi; i++) {
					minx = min(ppt[i].x, minx);
					miny = min(ppt[i].y, miny);
					maxx = max(ppt[i].x, maxx);
					maxy = max(ppt[i].y, maxy);
				}

				/* now shift polygon points to upper left corner */
				points = (POINT *)LocalAllocPtr(LHND, (polyi+1) * sizeof(POINT));
				for (i = 0; i < polyi; i++) {
					points[i].x = ppt[i].x - minx;
					points[i].y = ppt[i].y - miny;
				}

				/* create memory device context for bitmap */
				memdc = CreateCompatibleDC(hdc);

				/* create memory bitmap with alpha channel and minimal size */
				width  = maxx - minx;
				height = maxy - miny;
				ZeroMemory(&bmi, sizeof(BITMAPINFO));
				bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				bmi.bmiHeader.biWidth = width;
				bmi.bmiHeader.biHeight = height;
				bmi.bmiHeader.biPlanes = 1;
				bmi.bmiHeader.biBitCount = 32;
				bmi.bmiHeader.biCompression = BI_RGB;
				bmi.bmiHeader.biSizeImage = width * height * 4;
				membmp = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void **)&pvBits, NULL, 0x0);
				oldbmp = (HBITMAP)SelectObject(memdc, membmp);
				/* clear bitmap, could also do it via GDI */
				for (i = 0; i < width * height; i++)
					pvBits[i] = transparentColor;

				/* prepare the memory context */
				SetTextColor(memdc, fill_color);
				SetBkColor(memdc, bkColor);
				old_pen = SelectObject(memdc, lpgw->hnull);
				if ((fillstyle & 0x0f) == FS_TRANSPARENT_PATTERN)
					old_brush = SelectObject(memdc, pattern_brush[pattern]);
				else
					old_brush = SelectObject(memdc, solid_brush);

				/* finally, draw polygon */
				Polygon(memdc, points, polyi);

				/* add alpha channel to bitmap */
				/* Note: this is really a pre-scaled alpha channel, see MSDN.
				   To make life easy we only use global transparency, see below */
				for (y = 0; y < height; y++) {
					for (x = 0; x < width; x++) {
						UINT32 pixel = pvBits[x + y * width];
						if (pixel == transparentColor)
							pvBits[x + y * width]  = 0x00000000; /* completely transparent */
						else
							pvBits[x + y * width] |= 0xff000000; /* mark as completely opaque */
					}
				}

				/* copy to device with alpha blending */
				ftn.BlendOp = AC_SRC_OVER;
				ftn.BlendFlags = 0;
				ftn.AlphaFormat = AC_SRC_ALPHA; /* bitmap has an alpha channel */
				ftn.SourceConstantAlpha = uAlpha;
				AlphaBlend(hdc, minx, miny, width, height,
						   memdc, 0, 0, width, height, ftn);

				/* clean up */
				LocalFreePtr(points);
				SelectObject(memdc, old_pen);
				SelectObject(memdc, old_brush);
				SelectObject(memdc, oldbmp);
				DeleteObject(membmp);
				DeleteDC(memdc);
			}
			}
			polyi = 0;
			}
			break;

		case W_image: {
			/* Due to the structure of gwop 6 entries are needed in total. */
			if (seq == 0) {
				/* First OP contains only the color mode */
				color_mode = curptr->x;
			} else if (seq < 5) {
				/* Next four OPs contain the `corner` array */
				corners[seq-1].x = xdash;
				corners[seq-1].y = ydash;
			} else {
				/* The last OP contains the image and it's size */
				char * image = (char *) LocalLock(curptr->htext);
				unsigned int width = curptr->x;
				unsigned int height = curptr->y;
				draw_image(lpgw, hdc, image, corners, width, height, color_mode);
				LocalUnlock(curptr->htext);
			}
			seq = (seq + 1) % 6;
			}
			break;

		default: {
			int xofs, yofs;
			HDC dc;
			HBRUSH old_brush;
			HPEN old_pen;

			/* This covers only point symbols. All other codes should be
			   handled in the switch statement. */
			if ((curptr->op < W_dot) || (curptr->op > W_dot + WIN_POINT_TYPES))
				break;

			/* draw cached point symbol */
			if (ps_caching && (last_symbol == curptr->op) && (cb_memdc != NULL)) {
				TransparentBlt(hdc, xdash - cb_ofs.x, ydash - cb_ofs.y, 2*htic+2, 2*vtic+2,
					           cb_memdc, 0, 0, 2*htic+2, 2*vtic+2, 0x00ffffff);
				break;
			} else {
				if (cb_memdc != NULL) {
					SelectObject(cb_memdc, cb_old_bmp);
					DeleteObject(cb_membmp);
					DeleteDC(cb_memdc);
					cb_memdc = NULL;
				}
			}

			/* caching of point symbols? */
			if (ps_caching) {
				RECT rect;

				/* create memory device context for bitmap */
				dc = cb_memdc = CreateCompatibleDC(hdc);

				/* create standard bitmap, no alpha channel */
				cb_membmp = CreateCompatibleBitmap(hdc, 2*htic+2, 2*vtic+2);
				cb_old_bmp = (HBITMAP) SelectObject(dc, cb_membmp);

				/* prepare memory context */
				SetTextColor(dc, fill_color);
				old_brush = (HBRUSH) SelectObject(dc, solid_brush);
				old_pen = (HPEN) SelectObject(dc, lpgw->hapen);
				rect.left = rect.bottom = 0;
				rect.top = 2*vtic+2;
				rect.right = 2*htic+2;
				FillRect(dc, &rect, (HBRUSH) GetStockObject(WHITE_BRUSH));

				cb_ofs.x = xofs = htic+1;
				cb_ofs.y = yofs = vtic+1;
				last_symbol = curptr->op;
			} else {
				dc = hdc;
				xofs = xdash;
				yofs = ydash;
			}

			switch (curptr->op) {
			case W_dot:
				dot(dc, xofs, yofs);
				break;
			case W_plus: /* do plus */
			case W_star: /* do star: first plus, then cross */
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing) {
					POINT a, b;
					a.x = xofs - htic;
					a.y = yofs;
					b.x = xofs + htic;
					b.y = yofs;
					gdiplusLineEx(dc, a, b, PS_SOLID, line_width, last_color, alpha_c);
					a.x = xofs;
					a.y = yofs - vtic;
					b.x = xofs;
					b.y = yofs + vtic;
					gdiplusLineEx(dc, a, b, PS_SOLID, line_width, last_color, alpha_c);
				} else
#endif
				{
					SelectObject(dc, lpgw->hsolid);
					MoveTo(dc, xofs - htic, yofs);
					LineTo(dc, xofs + htic, yofs);
					MoveTo(dc, xofs, yofs - vtic);
					LineTo(dc, xofs, yofs + vtic);
					SelectObject(dc, lpgw->hapen);
				}
				if (curptr->op == W_plus) break;
			case W_cross: /* do X */
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing) {
					POINT a, b;
					a.x = xofs - htic;
					a.y = yofs - vtic;
					b.x = xofs + htic;
					b.y = yofs + vtic;
					gdiplusLineEx(dc, a, b, PS_SOLID, line_width, last_color, alpha_c);
					a.x = xofs - htic;
					a.y = yofs + vtic;
					b.x = xofs + htic;
					b.y = yofs - vtic;
					gdiplusLineEx(dc, a, b, PS_SOLID, line_width, last_color, alpha_c);
				} else
#endif
				{
					SelectObject(dc, lpgw->hsolid);
					MoveTo(dc, xofs - htic, yofs - vtic);
					LineTo(dc, xofs + htic, yofs + vtic);
					MoveTo(dc, xofs - htic, yofs + vtic);
					LineTo(dc, xofs + htic, yofs - vtic);
					SelectObject(dc, lpgw->hapen);
				}
				break;
			case W_circle: /* do open circle */
				SelectObject(dc, lpgw->hsolid);
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing) {
					POINT p;
					p.x = xofs;
					p.y = yofs;
					gdiplusCircleEx(dc, &p, htic, PS_SOLID, line_width, last_color, alpha_c);
				} else
#endif
					Arc(dc, xofs-htic, yofs-vtic, xofs+htic+1, yofs+vtic+1,
						xofs, yofs+vtic+1, xofs, yofs+vtic+1);
				dot(dc, xofs, yofs);
				SelectObject(dc, lpgw->hapen);
				break;
			case W_fcircle: /* do filled circle */
				SelectObject(dc, lpgw->hsolid);
				Ellipse(dc, xofs-htic, yofs-vtic,
				            xofs+htic+1, yofs+vtic+1);
				SelectObject(dc, lpgw->hapen);
#ifdef HAVE_GDIPLUS
				if (lpgw->antialiasing) {
					POINT p;
					p.x = xofs;
					p.y = yofs;
					gdiplusCircleEx(dc, &p, htic, PS_SOLID, line_width, last_color, alpha_c);
				}
#endif
				break;
			default: {	/* potentially closed figure */
				POINT p[6];
				int i;
				int shape = 0;
				int filled = 0;
				int index = 0;
				static float pointshapes[6][10] = {
					{-1, -1, +1, -1, +1, +1, -1, +1, 0, 0}, /* box */
					{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* dummy, circle */
					{ 0, -4./3, -4./3, 2./3,
						4./3,  2./3, 0, 0}, /* triangle */
					{ 0, 4./3, -4./3, -2./3,
						4./3,  -2./3, 0, 0}, /* inverted triangle */
					{ 0, +1, -1,  0,  0, -1, +1,  0, 0, 0}, /* diamond */
					{ 0, 1, 0.95106, 0.30902, 0.58779, -0.80902,
						-0.58779, -0.80902, -0.95106, 0.30902} /* pentagon */
				};

				/* This should never happen since all other codes should be
				   handled in the switch statement. */
				if ((curptr->op < W_box) || (curptr->op > W_fpentagon))
					break;

				/* Calculate index, instead of an ugly long switch statement;
				   Depends on definition of commands in wgnuplib.h.
				*/
				index = (curptr->op - W_box);
				shape = index / 2;
				filled = (index % 2) > 0;

				for (i = 0; i < 5; ++i) {
					if (pointshapes[shape][i * 2 + 1] == 0
						&& pointshapes[shape][i * 2] == 0)
						break;
					p[i].x = xofs + htic * pointshapes[shape][i * 2] + 0.5;
					p[i].y = yofs + vtic * pointshapes[shape][i * 2 + 1] + 0.5;
				}
				if (filled) {
					/* Filled polygon */
	#ifdef HAVE_GDIPLUS
					if (lpgw->antialiasing) {
						/* filled polygon with border */
						gdiplusSolidFilledPolygonEx(dc, p, i, last_color, alpha_c, TRUE);
					} else
	#endif
					{
						SelectObject(dc, lpgw->hsolid);
						Polygon(dc, p, i);
						SelectObject(dc, lpgw->hapen);
					}
				} else {
					/* Outline polygon */
					p[i].x = p[0].x;
					p[i].y = p[0].y;
	#ifdef HAVE_GDIPLUS
					if (lpgw->antialiasing) {
						gdiplusPolylineEx(dc, p, i + 1, PS_SOLID, line_width, last_color, alpha_c);
					} else
	#endif
					{
						SelectObject(dc, lpgw->hsolid);
						Polyline(dc, p, i + 1);
						SelectObject(dc, lpgw->hapen);
					}
					dot(dc, xofs, yofs);
				}
			} /* default case */
			} /* switch (point symbol) */

			if (ps_caching) {
				/* copy memory bitmap to screen */
				TransparentBlt(hdc, xdash - xofs, ydash - yofs, 2*htic+2, 2*vtic+2,
					           dc, 0, 0, 2*htic+2, 2*vtic+2, 0x00ffffff);
				/* partial clean up */
				SelectObject(dc, old_brush);
				SelectObject(dc, old_pen);
			}

			if (keysample) {
				draw_update_keybox(lpgw, plotno, xdash + htic, ydash + vtic);
				draw_update_keybox(lpgw, plotno, xdash - htic, ydash - vtic);
			}
			} /* default case */
		} /* switch(opcode) */
		} /* hide layer? */

		lastop = curptr->op;
		ngwop++;
		curptr++;
		if ((unsigned)(curptr - blkptr->gwop) >= GWOPMAX) {
			GlobalUnlock(blkptr->hblk);
			blkptr->gwop = (struct GWOP *)NULL;
			if ((blkptr = blkptr->next) == NULL)
				/* If exact multiple of GWOPMAX entries are queued,
				 * next will be NULL. Only the next GraphOp() call would
				 * have allocated a new block */
				return;
			if (!blkptr->gwop)
				blkptr->gwop = (struct GWOP *)GlobalLock(blkptr->hblk);
			if (!blkptr->gwop)
				return;
			curptr = (struct GWOP *)blkptr->gwop;
		}
    }
    if (polyi >= 2) {
#ifdef HAVE_GDIPLUS
		if (lpgw->antialiasing)
			gdiplusPolyline(hdc, ppt, polyi, &cur_penstruct, alpha_c);
		else
#endif
			Polyline(hdc, ppt, polyi);
	}
	/* cleanup */
	if (ps_caching && (cb_memdc != NULL)) {
		SelectObject(cb_memdc, cb_old_bmp);
		DeleteObject(cb_membmp);
		DeleteDC(cb_memdc);
		cb_memdc = NULL;
	}
	if (lpgw->hcolorbrush) {
		SelectObject(hdc, GetStockObject(BLACK_BRUSH));
		DeleteObject(lpgw->hcolorbrush);
		lpgw->hcolorbrush = NULL;
	}
	LocalFreePtr(ppt);
}

/* ================================== */

/* save graph windows as enhanced metafile
 * The code in here is very similar to what CopyClip does...
 */
static void
SaveAsEMF(LPGW lpgw)
{
    char *cwd;
    static OPENFILENAME Ofn;
    static char lpstrCustomFilter[256] = { '\0' };
    static char lpstrFileName[MAX_PATH] = { '\0' };
    static char lpstrFileTitle[MAX_PATH] = { '\0' };
    HWND hwnd = lpgw->hWndGraph;

    Ofn.lStructSize = sizeof(OPENFILENAME);
    Ofn.hwndOwner = hwnd;
    Ofn.lpstrInitialDir = (LPSTR)NULL;
    Ofn.lpstrFilter = (LPCTSTR) "Enhanced Metafile (*.EMF)\0*.EMF\0All Files (*.*)\0*.*\0";
    Ofn.lpstrCustomFilter = lpstrCustomFilter;
    Ofn.nMaxCustFilter = 255;
    Ofn.nFilterIndex = 1;   /* start with the *.emf filter */
    Ofn.lpstrFile = lpstrFileName;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrFileTitle = lpstrFileTitle;
    Ofn.nMaxFileTitle = MAX_PATH;
    Ofn.lpstrInitialDir = (LPSTR)NULL;
    Ofn.lpstrTitle = (LPSTR)NULL;
    Ofn.Flags = OFN_OVERWRITEPROMPT;
    Ofn.lpstrDefExt = (LPSTR) "emf";

    /* save cwd as GetSaveFileName apparently changes it */
    cwd = _getcwd(NULL, 0);

	if (GetSaveFileName(&Ofn) != 0) {
		RECT rect, mfrect;
		HDC hdc;
		HENHMETAFILE hemf;
		HDC hmf;
		TBOOLEAN antialiasing;

		/* get the context */
		hdc = GetDC(hwnd);
		GetPlotRect(lpgw, &rect);
		GetPlotRectInMM(lpgw, &mfrect, hdc);

		/* temporarily disable antialiasing */
		antialiasing = lpgw->antialiasing;
		lpgw->antialiasing = FALSE;

		hmf = CreateEnhMetaFile(hdc, Ofn.lpstrFile, &mfrect, (LPCTSTR)NULL);
		/* Always create EMF files using GDI only! */
		drawgraph(lpgw, hmf, (LPRECT) &rect);
		hemf = CloseEnhMetaFile(hmf);

		lpgw->antialiasing = antialiasing;

		DeleteEnhMetaFile(hemf);
		ReleaseDC(hwnd, hdc);

		/* restore cwd */
		if (cwd != NULL)
			_chdir( cwd );
    }

    /* free the cwd buffer allcoated by _getcwd */
    free(cwd);
}

/* ================================== */

/* copy graph window to clipboard --- note that the Metafile is drawn at the full
 * virtual resolution of the Windows terminal driver (24000 x 18000 pixels), to
 * preserve as much accuracy as remotely possible */
static void
CopyClip(LPGW lpgw)
{
	RECT rect, mfrect;
	HDC mem, hmf;
	HBITMAP bitmap;
	HENHMETAFILE hemf;
	HWND hwnd;
	HDC hdc;

	hwnd = lpgw->hWndGraph;

	/* view the window */
	if (IsIconic(hwnd))
		ShowWindow(hwnd, SW_SHOWNORMAL);
	BringWindowToTop(hwnd);
	UpdateWindow(hwnd);

	/* get the context */
	hdc = GetDC(hwnd);
	GetPlotRect(lpgw, &rect);

	/* make a bitmap and copy it there */
	mem = CreateCompatibleDC(hdc);
	bitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left,
			rect.bottom - rect.top);
	if (bitmap) {
		/* there is enough memory and the bitmaps OK */
		SelectObject(mem, bitmap);
		BitBlt(mem,0,0,rect.right - rect.left,
			rect.bottom - rect.top, hdc, rect.left,
			rect.top, SRCCOPY);
	} else {
		MessageBeep(MB_ICONHAND);
		MessageBox(hwnd, "Insufficient Memory to Copy Clipboard",
			lpgw->Title, MB_ICONHAND | MB_OK);
	}
	DeleteDC(mem);

	/* OK, bitmap done, now create an enhanced Metafile context
	 * and redraw the whole plot into that.
	 */
	{
		/* make copy of window's main status struct for modification */
		GW gwclip = *lpgw;

		/* disable antialiasing: do not mix GDI/GDI+ */
		gwclip.antialiasing = FALSE;

		gwclip.hfonth = gwclip.hfontv = 0;
		MakePens(&gwclip, hdc);
		MakeFonts(&gwclip, &rect, hdc);

		GetPlotRectInMM(lpgw, &mfrect, hdc);

		hmf = CreateEnhMetaFile(hdc, (LPCTSTR)NULL, &mfrect, (LPCTSTR)NULL);
		/* Always create EMF files using GDI only! */
		drawgraph(&gwclip, hmf, (LPRECT) &rect);
		hemf = CloseEnhMetaFile(hmf);

		DestroyFonts(&gwclip);
		DestroyPens(&gwclip);
	}

	/* Now we have the Metafile and Bitmap prepared, post their contents to
	 * the Clipboard */
	OpenClipboard(hwnd);
	EmptyClipboard();
	SetClipboardData(CF_ENHMETAFILE,hemf);
	SetClipboardData(CF_BITMAP, bitmap);
	CloseClipboard();
	ReleaseDC(hwnd, hdc);
	DeleteEnhMetaFile(hemf);
	return;
}


/* copy graph window to printer */
static void
CopyPrint(LPGW lpgw)
{
	DOCINFO docInfo;

	HDC printer;
	PRINTDLG pd;
	static DEVNAMES * pDevNames = NULL;
	static DEVMODE * pDevMode = NULL;
	LPCTSTR szDriver, szDevice, szOutput;
	HWND hwnd = lpgw->hWndGraph;
	RECT rect;
	GP_PRINT pr;

	/* Print Setup Dialog */

	/* See http://support.microsoft.com/kb/240082 */
	_fmemset (&pd, 0, sizeof pd);
	pd.lStructSize = sizeof pd;
	pd.hwndOwner = hwnd;
	pd.Flags = PD_PRINTSETUP;
	pd.hDevNames = pDevNames;
	pd.hDevMode = pDevMode;

	if (!PrintDlg(&pd))
		return;

	pDevNames = (DEVNAMES *) GlobalLock(pd.hDevNames);
	pDevMode = (DEVMODE *) GlobalLock(pd.hDevMode);

	szDriver = (LPCTSTR)pDevNames + pDevNames->wDriverOffset;
	szDevice = (LPCTSTR)pDevNames + pDevNames->wDeviceOffset;
	szOutput = (LPCTSTR)pDevNames + pDevNames->wOutputOffset;

	printer = CreateDC(szDriver, szDevice, szOutput, pDevMode);

	GlobalUnlock(pd.hDevMode);
	GlobalUnlock(pd.hDevNames);

	/* We no longer free these structures, but preserve them for the next time
	GlobalFree(pd.hDevMode);
	GlobalFree(pd.hDevNames);
	*/

	if (printer == NULL)
		return;	/* abort */


	/* Print Size Dialog */

	if (!PrintSize(printer, hwnd, &rect)) {
		DeleteDC(printer);
		return; /* abort */
	}

	pr.hdcPrn = printer;
	SetWindowLongPtr(hwnd, 4, (LONG_PTR)&pr);
	PrintRegister((GP_LPPRINT)&pr);

	EnableWindow(hwnd, FALSE);
	pr.bUserAbort = FALSE;
	pr.hDlgPrint = CreateDialogParam(hdllInstance, "CancelDlgBox",
					 hwnd, PrintDlgProc, (LPARAM)lpgw->Title);
	SetAbortProc(printer, PrintAbortProc);

	memset(&docInfo, 0, sizeof(DOCINFO));
	docInfo.cbSize = sizeof(DOCINFO);
	docInfo.lpszDocName = lpgw->Title;

	if (StartDoc(printer, &docInfo) > 0) {
		TBOOLEAN aa = lpgw->antialiasing;
		lpgw->sampling = 1;
		/* Mixing GDI/GDI+ does not seem to work properly on printer devices. */
		lpgw->antialiasing = FALSE;
		SetMapMode(printer, MM_TEXT);
		SetBkMode(printer, OPAQUE);
		StartPage(printer);
		DestroyFonts(lpgw);
		MakeFonts(lpgw, &rect, printer);
		DestroyPens(lpgw);	/* rebuild pens */
		MakePens(lpgw, printer);
		/* Always print using GDI only! */
		drawgraph(lpgw, printer, &rect);
		if (EndPage(printer) > 0)
			EndDoc(printer);
		lpgw->antialiasing = aa;
	}
	if (!pr.bUserAbort) {
		EnableWindow(hwnd, TRUE);
		DestroyWindow(pr.hDlgPrint);
	}
	DeleteDC(printer);
	SetWindowLong(hwnd, 4, (LONG)(0L));
	PrintUnregister((GP_LPPRINT)&pr);
	/* make certain that the screen pen set is restored */
	SendMessage(lpgw->hWndGraph, WM_COMMAND, M_REBUILDTOOLS, 0L);
	return;
}


/* ================================== */
/*  INI file stuff */
static void
WriteGraphIni(LPGW lpgw)
{
	RECT rect;
	LPSTR file = lpgw->IniFile;
	LPSTR section = lpgw->IniSection;
	char profile[80];
#ifdef WIN_CUSTOM_PENS
	int i;
#endif

	if ((file == (LPSTR)NULL) || (section == (LPSTR)NULL))
		return;
	if (IsIconic(lpgw->hWndGraph))
		ShowWindow(lpgw->hWndGraph, SW_SHOWNORMAL);
	GetWindowRect(lpgw->hWndGraph, &rect);
	wsprintf(profile, "%d %d", rect.left, rect.top);
	WritePrivateProfileString(section, "GraphOrigin", profile, file);
	if (lpgw->Canvas.x != 0) {
		wsprintf(profile, "%d %d", lpgw->Canvas.x, lpgw->Canvas.y);
		WritePrivateProfileString(section, "GraphSize", profile, file);
	} else if (lpgw->Size.x != CW_USEDEFAULT) {
		wsprintf(profile, "%d %d", lpgw->Size.x - lpgw->Decoration.x, lpgw->Size.y - lpgw->Decoration.y);
		WritePrivateProfileString(section, "GraphSize", profile, file);
	}
	wsprintf(profile, "%s,%d", lpgw->deffontname, lpgw->deffontsize);
	WritePrivateProfileString(section, "GraphFont", profile, file);
	strcpy(WIN_inifontname, lpgw->deffontname);
	WIN_inifontsize = lpgw->deffontsize;
	wsprintf(profile, "%d", lpgw->color);
	WritePrivateProfileString(section, "GraphColor", profile, file);
	wsprintf(profile, "%d", lpgw->graphtotop);
	WritePrivateProfileString(section, "GraphToTop", profile, file);
	wsprintf(profile, "%d", lpgw->doublebuffer);
	WritePrivateProfileString(section, "GraphDoublebuffer", profile, file);
	wsprintf(profile, "%d", lpgw->oversample);
	WritePrivateProfileString(section, "GraphOversampling", profile, file);
	wsprintf(profile, "%d", lpgw->gdiplus);
	WritePrivateProfileString(section, "GraphGDI+", profile, file);
	wsprintf(profile, "%d", lpgw->antialiasing);
	WritePrivateProfileString(section, "GraphAntialiasing", profile, file);
	wsprintf(profile, "%d", lpgw->polyaa);
	WritePrivateProfileString(section, "GraphPolygonAA", profile, file);
	wsprintf(profile, "%d", lpgw->patternaa);
	WritePrivateProfileString(section, "GraphPatternAA", profile, file);
	wsprintf(profile, "%d", lpgw->fastrotation);
	WritePrivateProfileString(section, "GraphFastRotation", profile, file);
	wsprintf(profile, "%d %d %d",GetRValue(lpgw->background),
			GetGValue(lpgw->background), GetBValue(lpgw->background));
	WritePrivateProfileString(section, "GraphBackground", profile, file);

#ifdef WIN_CUSTOM_PENS
	/* now save pens */
	for (i = 0; i < WGNUMPENS + 2; i++) {
		char entry[32];
		LPLOGPEN pc;
		LPLOGPEN pm;

		if (i == 0)
			_fstrcpy(entry, "Border");
		else if (i == 1)
			_fstrcpy(entry, "Axis");
		else
			 wsprintf(entry, "Line%d", i - 1);
		pc = &lpgw->colorpen[i];
		pm = &lpgw->monopen[i];
		wsprintf(profile, "%d %d %d %d %d", GetRValue(pc->lopnColor),
			GetGValue(pc->lopnColor), GetBValue(pc->lopnColor),
			(pc->lopnWidth.x != 1) ? -pc->lopnWidth.x : pc->lopnStyle,
			(pm->lopnWidth.x != 1) ? -pm->lopnWidth.x : pm->lopnStyle);
		WritePrivateProfileString(section, entry, profile, file);
	}
#endif
	return;
}


char *
GraphDefaultFont(void)
{
	if (GetACP() == 932) /* Japanese Shift-JIS */
		return WINJPFONT;
	else
		return WINFONT;
}


static void
ReadGraphIni(LPGW lpgw)
{
	LPSTR file = lpgw->IniFile;
	LPSTR section = lpgw->IniSection;
	char profile[81];
	LPSTR p;
	int r, g, b;
	BOOL bOKINI;
#ifdef WIN_CUSTOM_PENS
	int i;
	int colorstyle, monostyle;
#endif

	bOKINI = (file != (LPSTR)NULL) && (section != (LPSTR)NULL);
	if (!bOKINI)
		profile[0] = '\0';

	if (bOKINI)
		GetPrivateProfileString(section, "GraphOrigin", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->Origin.x)) == NULL)
		lpgw->Origin.x = CW_USEDEFAULT;
	if ( (p = GetInt(p, (LPINT)&lpgw->Origin.y)) == NULL)
		lpgw->Origin.y = CW_USEDEFAULT;
	if (bOKINI)
		GetPrivateProfileString(section, "GraphSize", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->Size.x)) == NULL)
		lpgw->Size.x = CW_USEDEFAULT;
	if ( (p = GetInt(p, (LPINT)&lpgw->Size.y)) == NULL)
		lpgw->Size.y = CW_USEDEFAULT;
	if ((lpgw->Size.x != CW_USEDEFAULT) && (lpgw->Size.y != CW_USEDEFAULT)) {
		lpgw->Canvas.x = lpgw->Size.x;
		lpgw->Canvas.y = lpgw->Size.y;
	}

	if (bOKINI)
	  GetPrivateProfileString(section, "GraphFont", "", profile, 80, file);
	{
		char *size;
		size = _fstrchr(profile,',');
		if (size) {
			*size++ = '\0';
			if ( (p = GetInt(size, (LPINT)&lpgw->fontsize)) == NULL)
				lpgw->fontsize = WINFONTSIZE;
		}
		_fstrcpy(lpgw->fontname, profile);
		if (lpgw->fontsize == 0)
			lpgw->fontsize = WINFONTSIZE;
		if (!(*lpgw->fontname))
			_fstrcpy(lpgw->fontname, GraphDefaultFont());
		/* set current font as default font */
		_fstrcpy(lpgw->deffontname, lpgw->fontname);
		lpgw->deffontsize = lpgw->fontsize;
		strcpy(WIN_inifontname, lpgw->deffontname);
		WIN_inifontsize = lpgw->deffontsize;
	}

	if (bOKINI)
		GetPrivateProfileString(section, "GraphColor", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->color)) == NULL)
		lpgw->color = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphToTop", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->graphtotop)) == NULL)
		lpgw->graphtotop = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphDoubleBuffer", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->doublebuffer)) == NULL)
		lpgw->doublebuffer = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphOversampling", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->oversample)) == NULL)
		lpgw->oversample = FALSE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphGDI+", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->gdiplus)) == NULL)
		lpgw->gdiplus = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphAntialiasing", "", profile, 80, file);
	if ( (p = GetInt(profile, (LPINT)&lpgw->antialiasing)) == NULL)
		lpgw->antialiasing = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphPolygonAA", "", profile, 80, file);
	if ((p = GetInt(profile, (LPINT)&lpgw->polyaa)) == NULL)
		lpgw->polyaa = FALSE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphPatternAA", "", profile, 80, file);
	if ((p = GetInt(profile, (LPINT)&lpgw->patternaa)) == NULL)
		lpgw->patternaa = TRUE;

	if (bOKINI)
		GetPrivateProfileString(section, "GraphFastRotation", "", profile, 80, file);
	if ((p = GetInt(profile, (LPINT)&lpgw->fastrotation)) == NULL)
		lpgw->fastrotation = TRUE;

	lpgw->background = RGB(255,255,255);
	if (bOKINI)
		GetPrivateProfileString(section, "GraphBackground", "", profile, 80, file);
	if ( ((p = GetInt(profile, (LPINT)&r)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&g)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&b)) != NULL) )
		lpgw->background = RGB(r,g,b);

#ifdef WIN_CUSTOM_PENS
	StorePen(lpgw, 0, RGB(0,0,0), PS_SOLID, PS_SOLID);
	if (bOKINI)
		GetPrivateProfileString(section, "Border", "", profile, 80, file);
	if ( ((p = GetInt(profile, (LPINT)&r)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&g)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&b)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&colorstyle)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&monostyle)) != NULL) )
		StorePen(lpgw, 0, RGB(r,g,b), colorstyle, monostyle);

	StorePen(lpgw, 1, RGB(192,192,192), PS_DOT, PS_DOT);
	if (bOKINI)
		GetPrivateProfileString(section, "Axis", "", profile, 80, file);
	if ( ((p = GetInt(profile, (LPINT)&r)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&g)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&b)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&colorstyle)) != NULL) &&
	     ((p = GetInt(p, (LPINT)&monostyle)) != NULL) )
		StorePen(lpgw, 1, RGB(r,g,b), colorstyle, monostyle);

	for (i = 0; i < WGNUMPENS; i++) {
		COLORREF ref;
		char entry[32];

		ref = wginitcolor[i % WGDEFCOLOR];
		colorstyle = wginitstyle[(i / WGDEFCOLOR) % WGDEFSTYLE];
		monostyle  = wginitstyle[i % WGDEFSTYLE];
		StorePen(lpgw, i + 2, ref, colorstyle, monostyle);
		wsprintf(entry, "Line%d", i + 1);
		if (bOKINI)
			GetPrivateProfileString(section, entry, "", profile, 80, file);
		if ( ((p = GetInt(profile, (LPINT)&r)) != NULL) &&
		     ((p = GetInt(p, (LPINT)&g)) != NULL) &&
		     ((p = GetInt(p, (LPINT)&b)) != NULL) &&
		     ((p = GetInt(p, (LPINT)&colorstyle)) != NULL) &&
		     ((p = GetInt(p, (LPINT)&monostyle)) != NULL) )
			StorePen(lpgw, i + 2, RGB(r,g,b), colorstyle, monostyle);
	}
#endif
}

/* ================================== */

/* Hypertext support functions */

void
add_tooltip(LPGW lpgw, PRECT rect, LPWSTR text)
{
	int idx = lpgw->numtooltips;

	/* Extend buffer, if necessary */
	if (lpgw->numtooltips >= lpgw->maxtooltips) {
		lpgw->maxtooltips += 10;
		lpgw->tooltips = (struct tooltips *) realloc(lpgw->tooltips, lpgw->maxtooltips * sizeof(struct tooltips));
	}

	rect->top += lpgw->ToolbarHeight;
	rect->bottom += lpgw->ToolbarHeight;
	lpgw->tooltips[idx].rect = *rect;
	lpgw->tooltips[idx].text = text;
	lpgw->numtooltips++;

	if (!lpgw->hTooltip) {
		TOOLINFO ti = { 0 };

		/* Create new tooltip. */
		HWND hwnd = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
									 WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
									 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
									 lpgw->hWndGraph, NULL, lpgw->hInstance, NULL);
		lpgw->hTooltip = hwnd;

		/* Associate the tooltip with the rect area.*/
		ti.cbSize   = sizeof(TOOLINFO);
		ti.uFlags   = TTF_SUBCLASS;
		ti.hwnd     = lpgw->hWndGraph;
		ti.hinst    = lpgw->hInstance;
		ti.uId      = 0;
		ti.rect     = * rect;
		ti.lpszText = (LPTSTR) text;
		SendMessage(hwnd, TTM_ADDTOOLW, 0, (LPARAM) (LPTOOLINFO) &ti);
		SendMessage(hwnd, TTM_SETDELAYTIME, TTDT_INITIAL, (LPARAM) 100);
		SendMessage(hwnd, TTM_SETDELAYTIME, TTDT_RESHOW, (LPARAM) 100);
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
}


void
clear_tooltips(LPGW lpgw)
{
	int i;

	for (i = 0; i < lpgw->numtooltips; i++)
		free(lpgw->tooltips[i].text);
	lpgw->numtooltips = 0;
	lpgw->maxtooltips = 0;
	free(lpgw->tooltips);
	lpgw->tooltips = NULL;
}


static void
track_tooltip(LPGW lpgw, int x, int y)
{
	static POINT p = {0, 0};
	int i;

	/* only update if mouse position changed */
	if ((p.x == x) && (p.y == y))
		return;
	p.x = x; p.y = y;

	for (i = 0; i < lpgw->numtooltips; i++) {
		if (PtInRect(&(lpgw->tooltips[i].rect), p)) {
			TOOLINFO ti = { 0 };
			int width;

			ti.cbSize   = sizeof(TOOLINFO);
			ti.hwnd     = lpgw->hWndGraph;
			ti.hinst    = lpgw->hInstance;
			ti.rect     = lpgw->tooltips[i].rect;
			ti.lpszText = (LPTSTR) lpgw->tooltips[i].text;
			SendMessage(lpgw->hTooltip, TTM_NEWTOOLRECT, 0, (LPARAM) (LPTOOLINFO) &ti);
			SendMessage(lpgw->hTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM) (LPTOOLINFO) &ti);
			/* Multi-line tooltip. */
			width = (wcschr(lpgw->tooltips[i].text, L'\n') == NULL) ? -1 : 200;
			SendMessage(lpgw->hTooltip, TTM_SETMAXTIPWIDTH, 0, (LPARAM) (INT) width);
		}
	}
}

/* ================================== */

#ifdef WIN_CUSTOM_PENS

/* the "Line Styles..." dialog and its support functions */
/* FIXME HBB 20010218: this might better be delegated to a separate source file */

#define LS_DEFLINE 2
typedef struct tagLS {
	LONG_PTR widtype;
	int		wid;
	HWND	hwnd;
	int		pen;			/* current pen number */
	LOGPEN	colorpen[WGNUMPENS+2];	/* logical color pens */
	LOGPEN	monopen[WGNUMPENS+2];	/* logical mono pens */
} LS;
typedef LS *  LPLS;
#endif

static COLORREF
GetColor(HWND hwnd, COLORREF ref)
{
	CHOOSECOLOR cc;
	COLORREF aclrCust[16];
	int i;

	for (i=0; i<16; i++) {
		aclrCust[i] = RGB(0,0,0);
	}
	_fmemset(&cc, 0, sizeof(CHOOSECOLOR));
	cc.lStructSize = sizeof(CHOOSECOLOR);
	cc.hwndOwner = hwnd;
	cc.lpCustColors = aclrCust;
	cc.rgbResult = ref;
	cc.Flags = CC_RGBINIT;
	if (ChooseColor(&cc))
		return cc.rgbResult;
	return ref;
}

#ifdef WIN_CUSTOM_PENS

/* force update of owner draw button */
static void
UpdateColorSample(HWND hdlg)
{
	RECT rect;
	POINT ptul, ptlr;
	GetWindowRect(GetDlgItem(hdlg, LS_COLORSAMPLE), &rect);
	ptul.x = rect.left;
	ptul.y = rect.top;
	ptlr.x = rect.right;
	ptlr.y = rect.bottom;
	ScreenToClient(hdlg, &ptul);
	ScreenToClient(hdlg, &ptlr);
	rect.left   = ptul.x;
	rect.top    = ptul.y;
	rect.right  = ptlr.x;
	rect.bottom = ptlr.y;
	InvalidateRect(hdlg, &rect, TRUE);
	UpdateWindow(hdlg);
}

/* Window handler function for the "Line Styles" dialog */
INT_PTR CALLBACK
LineStyleDlgProc(HWND hdlg, UINT wmsg, WPARAM wparam, LPARAM lparam)
{
	char buf[16];
	LPLS lpls;
	int i;
	UINT pen;
	LPLOGPEN plpm, plpc;
	lpls = (LPLS) GetWindowLongPtr(GetParent(hdlg), 4);

	switch (wmsg) {
		case WM_INITDIALOG:
			pen = 2;
			for (i=0; i<WGNUMPENS+2; i++) {
				if (i==0)
					_fstrcpy(buf,"Border");
				else if (i==1)
					_fstrcpy(buf,"Axis");
				else
			 		wsprintf(buf,"Line%d",i-1);
				SendDlgItemMessage(hdlg, LS_LINENUM, LB_ADDSTRING, 0,
					(LPARAM)((LPSTR)buf));
			}
			SendDlgItemMessage(hdlg, LS_LINENUM, LB_SETCURSEL, pen, 0L);

			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Solid"));
			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Dash"));
			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Dot"));
			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"DashDot"));
			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"DashDotDot"));

			plpm = &lpls->monopen[pen];
			SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_SETCURSEL,
				plpm->lopnStyle, 0L);
			wsprintf(buf,"%d",plpm->lopnWidth.x);
			SetDlgItemText(hdlg, LS_MONOWIDTH, buf);

			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Solid"));
			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Dash"));
			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"Dot"));
			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"DashDot"));
			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_ADDSTRING, 0,
				(LPARAM)((LPSTR)"DashDotDot"));

			plpc = &lpls->colorpen[pen];
			SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_SETCURSEL,
				plpc->lopnStyle, 0L);
			wsprintf(buf,"%d",plpc->lopnWidth.x);
			SetDlgItemText(hdlg, LS_COLORWIDTH, buf);

			return TRUE;
		case WM_COMMAND:
			pen = (UINT)SendDlgItemMessage(hdlg, LS_LINENUM, LB_GETCURSEL, 0, 0L);
			plpm = &lpls->monopen[pen];
			plpc = &lpls->colorpen[pen];
			switch (LOWORD(wparam)) {
				case LS_LINENUM:
					wsprintf(buf,"%d",plpm->lopnWidth.x);
					SetDlgItemText(hdlg, LS_MONOWIDTH, buf);
					SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_SETCURSEL,
						plpm->lopnStyle, 0L);
					wsprintf(buf,"%d",plpc->lopnWidth.x);
					SetDlgItemText(hdlg, LS_COLORWIDTH, buf);
					SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_SETCURSEL,
						plpc->lopnStyle, 0L);
					UpdateColorSample(hdlg);
					return FALSE;
				case LS_MONOSTYLE:
					plpm->lopnStyle =
						(UINT)SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_GETCURSEL, 0, 0L);
					if (plpm->lopnStyle != 0) {
						plpm->lopnWidth.x = 1;
						wsprintf(buf,"%d",plpm->lopnWidth.x);
						SetDlgItemText(hdlg, LS_MONOWIDTH, buf);
					}
					return FALSE;
				case LS_MONOWIDTH:
					GetDlgItemText(hdlg, LS_MONOWIDTH, buf, 15);
					GetInt(buf, (LPINT)&plpm->lopnWidth.x);
					if (plpm->lopnWidth.x != 1) {
						plpm->lopnStyle = 0;
						SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_SETCURSEL,
							plpm->lopnStyle, 0L);
					}
					return FALSE;
				case LS_CHOOSECOLOR:
					plpc->lopnColor = GetColor(hdlg, plpc->lopnColor);
					UpdateColorSample(hdlg);
					return FALSE;
				case LS_COLORSTYLE:
					plpc->lopnStyle =
						(UINT)SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_GETCURSEL, 0, 0L);
					if (plpc->lopnStyle != 0) {
						plpc->lopnWidth.x = 1;
						wsprintf(buf,"%d",plpc->lopnWidth.x);
						SetDlgItemText(hdlg, LS_COLORWIDTH, buf);
					}
					return FALSE;
				case LS_COLORWIDTH:
					GetDlgItemText(hdlg, LS_COLORWIDTH, buf, 15);
					GetInt(buf, (LPINT)&plpc->lopnWidth.x);
					if (plpc->lopnWidth.x != 1) {
						plpc->lopnStyle = 0;
						SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_SETCURSEL,
							plpc->lopnStyle, 0L);
					}
					return FALSE;
				case LS_DEFAULT:
					plpm = lpls->monopen;
					plpc = lpls->colorpen;
					/* border */
					plpc->lopnColor   = RGB(0,0,0);
					plpc->lopnStyle   = PS_SOLID;
					plpc->lopnWidth.x = 1;
					plpm->lopnStyle   = PS_SOLID;
					plpm->lopnWidth.x = 1;
					plpc++; plpm++;
					/* axis */
					plpc->lopnColor   = RGB(192,192,192);
					plpc->lopnStyle   = PS_DOT;
					plpc->lopnWidth.x = 1;
					plpm->lopnStyle   = PS_DOT;
					plpm->lopnWidth.x = 1;
					/* LineX */
					for (i=0; i<WGNUMPENS; i++) {
						plpc++; plpm++;
						plpc->lopnColor   = wginitcolor[ i%WGDEFCOLOR ];
						plpc->lopnStyle   = wginitstyle[ (i/WGDEFCOLOR) % WGDEFSTYLE ];
						plpc->lopnWidth.x = 1;
						plpm->lopnStyle   = wginitstyle[ i%WGDEFSTYLE ];
						plpm->lopnWidth.x = 1;
					}
					/* update window */
					plpm = &lpls->monopen[pen];
					plpc = &lpls->colorpen[pen];
					SendDlgItemMessage(hdlg, LS_LINENUM, LB_SETCURSEL, pen, 0L);
					wsprintf(buf,"%d",plpm->lopnWidth.x);
					SetDlgItemText(hdlg, LS_MONOWIDTH, buf);
					SendDlgItemMessage(hdlg, LS_MONOSTYLE, CB_SETCURSEL,
						plpm->lopnStyle, 0L);
					wsprintf(buf,"%d",plpc->lopnWidth.x);
					SetDlgItemText(hdlg, LS_COLORWIDTH, buf);
					SendDlgItemMessage(hdlg, LS_COLORSTYLE, CB_SETCURSEL,
						plpc->lopnStyle, 0L);
					UpdateColorSample(hdlg);
					return FALSE;
				case IDOK:
					EndDialog(hdlg, IDOK);
					return TRUE;
				case IDCANCEL:
					EndDialog(hdlg, IDCANCEL);
					return TRUE;
			}
			break;
		case WM_DRAWITEM:
			{
			HBRUSH hBrush;
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lparam;
			pen = (UINT)SendDlgItemMessage(hdlg, LS_LINENUM, LB_GETCURSEL, (WPARAM)0, (LPARAM)0);
			plpc = &lpls->colorpen[pen];
			hBrush = CreateSolidBrush(plpc->lopnColor);
			FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
			FrameRect(lpdis->hDC, &lpdis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
			DeleteObject(hBrush);
			}
			return FALSE;
	}
	return FALSE;
}


/* GetWindowLong(hwnd, 4) must be available for use */
static BOOL
LineStyle(LPGW lpgw)
{
	BOOL status = FALSE;
	LS ls;

	SetWindowLongPtr(lpgw->hWndGraph, 4, (LONG_PTR)((LPLS)&ls));
	_fmemcpy(&ls.colorpen, &lpgw->colorpen, (WGNUMPENS + 2) * sizeof(LOGPEN));
	_fmemcpy(&ls.monopen, &lpgw->monopen, (WGNUMPENS + 2) * sizeof(LOGPEN));

	if (DialogBox (hdllInstance, "LineStyleDlgBox", lpgw->hWndGraph, LineStyleDlgProc)
		== IDOK) {
		_fmemcpy(&lpgw->colorpen, &ls.colorpen, (WGNUMPENS + 2) * sizeof(LOGPEN));
		_fmemcpy(&lpgw->monopen, &ls.monopen, (WGNUMPENS + 2) * sizeof(LOGPEN));
		status = TRUE;
	}
	SetWindowLong(lpgw->hWndGraph, 4, (LONG)(0L));
	return status;
}

#endif /* WIN_CUSTOM_PENS */


#ifdef USE_MOUSE
/* ================================== */
/* helper functions: wrapper around gp_exec_event and DrawZoomBox. */

static void
Wnd_exec_event(LPGW lpgw, LPARAM lparam, char type, int par1)
{
	static unsigned long lastTimestamp = 0;
	unsigned long thisTimestamp = GetMessageTime();
	int mx, my, par2;
	TBOOLEAN old = FALSE;

	if (type != GE_keypress) /* no timestamp for key events */
		par2 = thisTimestamp - lastTimestamp;
	else
			par2 = 0;

	/* map events from inactive graph windows */
	if (lpgw != graphwin) {
		switch (type) {
		case GE_keypress:
			type = GE_keypress_old;
			old = TRUE;
			break;
		case GE_buttonpress:
			type = GE_buttonpress_old;
			old = TRUE;
			break;
		case GE_buttonrelease:
			type = GE_buttonrelease_old;
			old = TRUE;
			break;
		}
	}

	if ((term != NULL) && (strcmp(term->name, "windows") == 0) && ((lpgw == graphwin) || old)) {
		GetMousePosViewport(lpgw, &mx, &my);
		gp_exec_event(type, mx, my, par1, par2, 0);
		lastTimestamp = thisTimestamp;
	}

	/* FIXME: IMHO changing paused_for_mouse in terminal code is bad design. */
	/* end pause mouse? */
	if ((type == GE_buttonrelease) && (paused_for_mouse & PAUSE_CLICK) &&
		(((par1 == 1) && (paused_for_mouse & PAUSE_BUTTON1)) ||
		 ((par1 == 2) && (paused_for_mouse & PAUSE_BUTTON2)) ||
		 ((par1 == 3) && (paused_for_mouse & PAUSE_BUTTON3)))) {
		paused_for_mouse = 0;
	}
	if ((type == GE_keypress) && (paused_for_mouse & PAUSE_KEYSTROKE) && (par1 != NUL)) {
		paused_for_mouse = 0;
	}
}


static void
Wnd_refresh_zoombox(LPGW lpgw, LPARAM lParam)
{
	if (lpgw == graphwin) {
		int mx, my;

		GetMousePosViewport(lpgw, &mx, &my);
		DrawZoomBox(lpgw); /*  erase current zoom box */
		zoombox.to.x = mx; zoombox.to.y = my;
		DrawZoomBox(lpgw); /*  draw new zoom box */
	}
}


static void
Wnd_refresh_ruler_lineto(LPGW lpgw, LPARAM lParam)
{
	if (lpgw == graphwin) {
		int mx, my;

		GetMousePosViewport(lpgw, &mx, &my);
		DrawRulerLineTo(lpgw); /*  erase current line */
		ruler_lineto.x = mx; ruler_lineto.y = my;
		DrawRulerLineTo(lpgw); /*  draw new line box */
	}
}
#endif /* USE_MOUSE */

/* ================================== */

/* The toplevel function of this module: Window handler function of the graph window */
LRESULT CALLBACK
WndGraphProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;
	LPGW lpgw;
	HMENU sysmenu;
	int i;
#ifdef USE_MOUSE
	static unsigned int last_modifier_mask = -99;
#endif

	lpgw = (LPGW)GetWindowLongPtr(hwnd, 0);

#ifdef USE_MOUSE
	/*  mouse events first */
	if ((lpgw == graphwin) && mouse_setting.on) {
		switch (message) {
			case WM_MOUSEMOVE:
#if 1
				SetCursor(hptrCurrent);
#else
				SetCursor(hptrCrossHair);
#endif
				if (zoombox.on) {
					Wnd_refresh_zoombox(lpgw, lParam);
				}
				if (ruler.on && ruler_lineto.on) {
					Wnd_refresh_ruler_lineto(lpgw, lParam);
				}
				/* track hypertexts */
				track_tooltip(lpgw, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				/* track (show) mouse position -- send the event to gnuplot */
				Wnd_exec_event(lpgw, lParam,  GE_motion, wParam);
				return 0L; /* end of WM_MOUSEMOVE */

			case WM_LBUTTONDOWN: {
				int i;
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam) - lpgw->ToolbarHeight;
				for (i = 0; (i < lpgw->numplots) && (i < lpgw->maxkeyboxes) && (i < lpgw->maxhideplots); i++) {
					if ((lpgw->keyboxes[i].left != INT_MAX) &&
						(x >= lpgw->keyboxes[i].left) &&
						(x <= lpgw->keyboxes[i].right) &&
						(y <= lpgw->keyboxes[i].top) &&
						(y >= lpgw->keyboxes[i].bottom)) {
						lpgw->hideplot[i] = ! lpgw->hideplot[i];
						if (i < MAXPLOTSHIDE)
							SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEPLOT + i, (LPARAM)lpgw->hideplot[i]);
						lpgw->buffervalid = FALSE;
						GetClientRect(hwnd, &rect);
						InvalidateRect(hwnd, (LPRECT) &rect, 1);
						UpdateWindow(hwnd);
						return 0L;
					}
				}
				Wnd_exec_event(lpgw, lParam, GE_buttonpress, 1);
				return 0L;
			}
			case WM_RBUTTONDOWN:
				/* FIXME HBB 20010207: this collides with the right-click
				 * context menu !!! */
				Wnd_exec_event(lpgw, lParam, GE_buttonpress, 3);
				return 0L;

			case WM_MBUTTONDOWN:
				Wnd_exec_event(lpgw, lParam, GE_buttonpress, 2);
				return 0L;

			case WM_MOUSEWHEEL:	/* shige, BM : mouse wheel support */
			case WM_MOUSEHWHEEL: {
				WORD fwKeys;
				short int zDelta;
				int modifier_mask;

				fwKeys = LOWORD(wParam);
				zDelta = HIWORD(wParam);
				modifier_mask = ((fwKeys & MK_SHIFT)? Mod_Shift : 0) |
				                ((fwKeys & MK_CONTROL)? Mod_Ctrl : 0) |
				                ((fwKeys & MK_ALT)? Mod_Alt : 0);
				if (last_modifier_mask != modifier_mask) {
					Wnd_exec_event(lpgw, lParam, GE_modifier, modifier_mask);
					last_modifier_mask = modifier_mask;
				}
				if (message == WM_MOUSEWHEEL)
					Wnd_exec_event(lpgw, lParam, GE_buttonpress, zDelta > 0 ? 4 : 5);
				else
					Wnd_exec_event(lpgw, lParam, GE_buttonpress, zDelta > 0 ? 6 : 7);
				return 0L;
			}

			case WM_LBUTTONDBLCLK:
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 1);
				return 0L;

			case WM_RBUTTONDBLCLK:
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 3);
				return 0L;

			case WM_MBUTTONDBLCLK:
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 2);
				return 0L;

#if 1
			case WM_LBUTTONUP:
#else
			case WM_LBUTTONCLICK:
#endif
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 1);
				return 0L;

#if 1
			case WM_RBUTTONUP:
#else
			case WM_RBUTTONCLICK:
#endif
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 3);
				return 0L;

#if 1
			case WM_MBUTTONUP:
#else
			case WM_MBUTTONCLICK:
#endif
				Wnd_exec_event(lpgw, lParam, GE_buttonrelease, 2);
				return 0L;

		} /* switch over mouse events */
	}
#endif /* USE_MOUSE */



	switch(message)
	{
		case WM_SYSCOMMAND:
			switch(LOWORD(wParam))
			{
				case M_GRAPH_TO_TOP:
				case M_COLOR:
				case M_DOUBLEBUFFER:
				case M_OVERSAMPLE:
				case M_GDIPLUS:
				case M_ANTIALIASING:
				case M_POLYAA:
				case M_PATTERNAA:
				case M_FASTROTATE:
				case M_CHOOSE_FONT:
				case M_COPY_CLIP:
				case M_SAVE_AS_EMF:
				case M_LINESTYLE:
				case M_BACKGROUND:
				case M_PRINT:
				case M_WRITEINI:
				case M_REBUILDTOOLS:
					SendMessage(hwnd, WM_COMMAND, wParam, lParam);
					break;
#ifndef WGP_CONSOLE
				case M_ABOUT:
					if (lpgw->lptw)
						AboutBox(hwnd,lpgw->lptw->AboutText);
					return 0;
#endif
				case M_COMMANDLINE:
					sysmenu = GetSystemMenu(lpgw->hWndGraph, 0);
					i = GetMenuItemCount (sysmenu);
					DeleteMenu (sysmenu, --i, MF_BYPOSITION);
					DeleteMenu (sysmenu, --i, MF_BYPOSITION);
					if (lpgw->lptw)
						ShowWindow (lpgw->lptw->hWndParent, SW_SHOWNORMAL);
					break;
			}
			break;
		case WM_CHAR:
			/* All 'normal' keys (letters, digits and the likes) end up
			 * here... */
#ifndef DISABLE_SPACE_RAISES_CONSOLE
			if (wParam == VK_SPACE) {
				WinRaiseConsole();
				return 0L;
			}
#endif /* DISABLE_SPACE_RAISES_CONSOLE */
			if (wParam == 'q') {
				GraphClose(lpgw);
				return 0L;
			}
#ifdef USE_MOUSE
			Wnd_exec_event(lpgw, lParam, GE_keypress, (TCHAR)wParam);
#endif
			return 0L;
#ifdef USE_MOUSE
		/* "special" keys have to be caught from WM_KEYDOWN, as they
		 * don't generate WM_CHAR messages. */
		/* NB: It may not be possible to catch Alt-keys, this way */
		case WM_KEYUP:
			{
				/* First, look for a change in modifier status */
				unsigned int modifier_mask = 0;
				modifier_mask = ((GetKeyState(VK_SHIFT) < 0) ? Mod_Shift : 0 )
					| ((GetKeyState(VK_CONTROL) < 0) ? Mod_Ctrl : 0)
					| ((GetKeyState(VK_MENU) < 0) ? Mod_Alt : 0);
				if (modifier_mask != last_modifier_mask) {
					Wnd_exec_event(lpgw, lParam, GE_modifier, modifier_mask);
					last_modifier_mask = modifier_mask;
				}
			}
			/* Ignore Key-Up events other than those of modifier keys */
			break;
		case WM_KEYDOWN:
			if ((GetKeyState(VK_CONTROL) < 0)  && (wParam != VK_CONTROL)) {
				switch(wParam) {
				case 'C':
					/* Ctrl-C: Copy to Clipboard */
					SendMessage(hwnd, WM_COMMAND, M_COPY_CLIP, 0L);
					break;
				case 'S':
					/* Ctrl-S: Save As EMF */
					SendMessage(hwnd, WM_COMMAND, M_SAVE_AS_EMF, 0L);
					break;
				case VK_END:
					/* use CTRL-END as break key */
					ctrlc_flag = TRUE;
					PostMessage(graphwin->hWndGraph, WM_NULL, 0, 0);
					break;
				} /* switch(wparam) */
			} else {
				/* First, look for a change in modifier status */
				unsigned int modifier_mask = 0;
				modifier_mask = ((GetKeyState(VK_SHIFT) < 0) ? Mod_Shift : 0)
					| ((GetKeyState(VK_CONTROL) < 0) ? Mod_Ctrl : 0)
					| ((GetKeyState(VK_MENU) < 0) ? Mod_Alt : 0);
				if (modifier_mask != last_modifier_mask) {
					Wnd_exec_event(lpgw, lParam, GE_modifier, modifier_mask);
					last_modifier_mask = modifier_mask;
				}
			}
			switch (wParam) {
			case VK_BACK:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_BackSpace);
				break;
			case VK_TAB:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Tab);
				break;
			case VK_RETURN:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Return);
				break;
			case VK_PAUSE:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Pause);
				break;
			case VK_SCROLL:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Scroll_Lock);
				break;
#if 0 /* HOW_IS_THIS_FOR_WINDOWS */
/* HBB 20010215: not at all, AFAICS... :-( */
			case VK_SYSRQ:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Sys_Req);
				break;
#endif
			case VK_ESCAPE:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Escape);
				break;
			case VK_DELETE:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Delete);
				break;
			case VK_INSERT:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_KP_Insert);
				break;
			case VK_HOME:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Home);
				break;
			case VK_LEFT:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Left);
				break;
			case VK_UP:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Up);
				break;
			case VK_RIGHT:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Right);
				break;
			case VK_DOWN:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_Down);
				break;
			case VK_END:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_End);
				break;
			case VK_PRIOR:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_PageUp);
				break;
			case VK_NEXT:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_PageDown);
				break;
			case VK_F1:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F1);
				break;
			case VK_F2:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F2);
				break;
			case VK_F3:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F3);
				break;
			case VK_F4:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F4);
				break;
			case VK_F5:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F5);
				break;
			case VK_F6:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F6);
				break;
			case VK_F7:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F7);
				break;
			case VK_F8:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F8);
				break;
			case VK_F9:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F9);
				break;
			case VK_F10:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F10);
				break;
			case VK_F11:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F11);
				break;
			case VK_F12:
				Wnd_exec_event(lpgw, lParam, GE_keypress, GP_F12);
				break;
			case VK_CANCEL:
				ctrlc_flag = TRUE;
				PostMessage(graphwin->hWndGraph, WM_NULL, 0, 0);
				break;
			} /* switch (wParam) */

			return 0L;
#if 0 /* DO WE NEED THIS ??? */
		case WM_MOUSEMOVE:
			/* set default pointer: */
			SetCursor(hptrDefault);
			return 0L;
#endif
#endif /* USE_MOUSE */
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case M_GRAPH_TO_TOP:
					lpgw->graphtotop = !lpgw->graphtotop;
					SendMessage(hwnd,WM_COMMAND,M_REBUILDTOOLS,0L);
					return(0);
				case M_COLOR:
					lpgw->color = !lpgw->color;
					lpgw->dashed = !lpgw->color;
					SendMessage(hwnd,WM_COMMAND,M_REBUILDTOOLS,0L);
					WIN_update_options();
					return(0);
				case M_OVERSAMPLE:
					lpgw->oversample = !lpgw->oversample;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_DOUBLEBUFFER:
					lpgw->doublebuffer = !lpgw->doublebuffer;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_GDIPLUS:
					lpgw->gdiplus = !lpgw->gdiplus;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_ANTIALIASING:
					lpgw->antialiasing = !lpgw->antialiasing;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_POLYAA:
					lpgw->polyaa = !lpgw->polyaa;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_PATTERNAA:
					lpgw->patternaa = !lpgw->patternaa;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_FASTROTATE:
					lpgw->fastrotation = !lpgw->fastrotation;
					SendMessage(hwnd, WM_COMMAND, M_REBUILDTOOLS, 0L);
					return 0;
				case M_CHOOSE_FONT:
					SelFont(lpgw);
					WIN_update_options();
					return 0;
				case M_COPY_CLIP:
					CopyClip(lpgw);
					return 0;
				case M_SAVE_AS_EMF:
					SaveAsEMF(lpgw);
					return 0;
#ifdef WIN_CUSTOM_PENS
				case M_LINESTYLE:
					if (LineStyle(lpgw))
						SendMessage(hwnd,WM_COMMAND,M_REBUILDTOOLS,0L);
					return 0;
#endif
				case M_BACKGROUND:
					lpgw->background = GetColor(hwnd, lpgw->background);
					SendMessage(hwnd,WM_COMMAND,M_REBUILDTOOLS,0L);
					WIN_update_options();
					return 0;
				case M_PRINT:
					CopyPrint(lpgw);
					return 0;
				case M_HIDEGRID:
					lpgw->hidegrid = SendMessage(lpgw->hToolbar, TB_ISBUTTONCHECKED, LOWORD(wParam), (LPARAM)0);
					lpgw->buffervalid = FALSE;
					GetClientRect(hwnd, &rect);
					InvalidateRect(hwnd, (LPRECT) &rect, 1);
					UpdateWindow(hwnd);
					return 0;
				case M_WRITEINI:
					WriteGraphIni(lpgw);
#ifndef WGP_CONSOLE
					if (lpgw->lptw)
						WriteTextIni(lpgw->lptw);
#endif
					WIN_update_options();
					return 0;
				case M_REBUILDTOOLS:
					if (lpgw->color)
						CheckMenuItem(lpgw->hPopMenu, M_COLOR, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_COLOR, MF_BYCOMMAND | MF_UNCHECKED);
					if (lpgw->doublebuffer)
						CheckMenuItem(lpgw->hPopMenu, M_DOUBLEBUFFER, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_DOUBLEBUFFER, MF_BYCOMMAND | MF_UNCHECKED);
					if (lpgw->oversample)
						CheckMenuItem(lpgw->hPopMenu, M_OVERSAMPLE, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_OVERSAMPLE, MF_BYCOMMAND | MF_UNCHECKED);
#ifdef HAVE_GDIPLUS
					if (lpgw->gdiplus) {
						CheckMenuItem(lpgw->hPopMenu, M_GDIPLUS, MF_BYCOMMAND | MF_CHECKED);
						EnableMenuItem(lpgw->hPopMenu, M_PATTERNAA, MF_BYCOMMAND | MF_DISABLED);
					} else {
						CheckMenuItem(lpgw->hPopMenu, M_GDIPLUS, MF_BYCOMMAND | MF_UNCHECKED);
						EnableMenuItem(lpgw->hPopMenu, M_PATTERNAA, MF_BYCOMMAND | MF_ENABLED);
					}
					if (lpgw->antialiasing)
						CheckMenuItem(lpgw->hPopMenu, M_ANTIALIASING, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_ANTIALIASING, MF_BYCOMMAND | MF_UNCHECKED);
					if (lpgw->polyaa)
						CheckMenuItem(lpgw->hPopMenu, M_POLYAA, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_POLYAA, MF_BYCOMMAND | MF_UNCHECKED);
					if (lpgw->patternaa || lpgw->gdiplus)
						CheckMenuItem(lpgw->hPopMenu, M_PATTERNAA, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_PATTERNAA, MF_BYCOMMAND | MF_UNCHECKED);
					CheckMenuItem(lpgw->hPopMenu, M_FASTROTATE, MF_BYCOMMAND |
									(lpgw->fastrotation ? MF_CHECKED : MF_UNCHECKED));
#endif
					if (lpgw->graphtotop)
						CheckMenuItem(lpgw->hPopMenu, M_GRAPH_TO_TOP, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(lpgw->hPopMenu, M_GRAPH_TO_TOP, MF_BYCOMMAND | MF_UNCHECKED);
					lpgw->buffervalid = FALSE;
					DestroyPens(lpgw);
					DestroyFonts(lpgw);
					hdc = GetDC(hwnd);
					MakePens(lpgw, hdc);
					GetPlotRect(lpgw, &rect);
					MakeFonts(lpgw, (LPRECT)&rect, hdc);
					ReleaseDC(hwnd, hdc);
					GetClientRect(hwnd, &rect);
					InvalidateRect(hwnd, (LPRECT) &rect, 1);
					UpdateWindow(hwnd);
					return 0;
			}
			/* handle toolbar events  */
			if ((LOWORD(wParam) >= M_HIDEPLOT) && (LOWORD(wParam) < (M_HIDEPLOT + MAXPLOTSHIDE))) {
				unsigned button = LOWORD(wParam) - (M_HIDEPLOT);
				if (button < lpgw->maxhideplots)
					lpgw->hideplot[button] = SendMessage(lpgw->hToolbar, TB_ISBUTTONCHECKED, LOWORD(wParam), (LPARAM)0);
				lpgw->buffervalid = FALSE;
				GetClientRect(hwnd, &rect);
				InvalidateRect(hwnd, (LPRECT) &rect, 1);
				UpdateWindow(hwnd);
				return 0;
			}
			return 0;
		case WM_PARENTNOTIFY:
			/* Message from status bar (or another child window): */
#ifdef USE_MOUSE
			/* Cycle through mouse-mode on button 1 click */
			if (LOWORD(wParam) == WM_LBUTTONDOWN) {
				int y = HIWORD(lParam);
				RECT rect;
				GetClientRect(hwnd, &rect);
				if (y > rect.bottom - lpgw->StatusHeight)
					/* simulate keyboard event '1' */
					Wnd_exec_event(lpgw, lParam, GE_keypress, (TCHAR)'1');
				return 0;
			}
#endif
			/* Context menu is handled below, everything else is not */
			if (LOWORD(wParam) != WM_CONTEXTMENU)
				return 1;
			/* intentionally fall through */
		case WM_CONTEXTMENU:
		{	/* Note that this only works via mouse in `unset mouse`
			 * mode. You can access the popup via the System menu,
			 * status bar or keyboard (Shift-F10, Menu-Key) instead. */
			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			if (pt.x == -1) { /* keyboard activation */
				pt.x = pt.y = 0;
				ClientToScreen(hwnd, &pt);
			}
			TrackPopupMenu(lpgw->hPopMenu, TPM_LEFTALIGN,
				pt.x, pt.y, 0, hwnd, NULL);
			return 0;
		}
		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code) {
				case TBN_DROPDOWN: {
					RECT rc;
					TPMPARAMS tpm;
					LPNMTOOLBAR lpnmTB = (LPNMTOOLBAR)lParam;
					SendMessage(lpnmTB->hdr.hwndFrom, TB_GETRECT, (WPARAM)lpnmTB->iItem, (LPARAM)&rc);
					MapWindowPoints(lpnmTB->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&rc, 2);
					tpm.cbSize    = sizeof(TPMPARAMS);
					tpm.rcExclude = rc;
					TrackPopupMenuEx(lpgw->hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
						rc.left, rc.bottom, lpgw->hWndGraph, &tpm);
					return TBDDRET_DEFAULT;
				}
				case TTN_GETDISPINFO: {
					LPTOOLTIPTEXT lpttt = (LPTOOLTIPTEXT)lParam;
					UINT_PTR idButton = lpttt->hdr.idFrom;
					lpttt->hinst = 0;
					switch (idButton) {
						case M_COPY_CLIP:
							strcpy(lpttt->szText, "Copy graph to clipboard");
							break;
						case M_PRINT:
							strcpy(lpttt->szText, "Print graph");
							break;
						case M_SAVE_AS_EMF:
							strcpy(lpttt->szText, "Save graph as EMF");
							break;
						case M_HIDEGRID:
							strcpy(lpttt->szText, "Do not draw grid lines");
							break;
					}
					if ((idButton >= M_HIDEPLOT) && (idButton < (M_HIDEPLOT + MAXPLOTSHIDE))) {
						unsigned index = (unsigned)idButton - (M_HIDEPLOT) + 1;
						sprintf(lpttt->szText, "Hide graph #%i", index);
					}
					lpttt->uFlags |= TTF_DI_SETITEM;
					return TRUE;
				}
			}
			return FALSE;
		case WM_CREATE:
			lpgw = ((CREATESTRUCT *)lParam)->lpCreateParams;
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)lpgw);
			lpgw->hWndGraph = hwnd;
			hdc = GetDC(hwnd);
			MakePens(lpgw, hdc);
#ifdef USE_MOUSE
			LoadCursors(lpgw);
#endif
			GetPlotRect(lpgw, &rect);
			MakeFonts(lpgw, (LPRECT)&rect, hdc);
			ReleaseDC(hwnd, hdc);
			if ( lpgw->lptw && (lpgw->lptw->DragPre!=(LPSTR)NULL) && (lpgw->lptw->DragPost!=(LPSTR)NULL) )
			    DragAcceptFiles(hwnd, TRUE);
			return(0);

		case WM_ERASEBKGND:
			return(1); /* we erase the background ourselves */

		case WM_PAINT: {
			HDC memdc = NULL;
			HBITMAP oldbmp;
			LONG width, height;
			LONG wwidth, wheight;
			int sampling;
			RECT memrect;
			RECT wrect;

			hdc = BeginPaint(hwnd, &ps);
			SetMapMode(hdc, MM_TEXT);
			SetBkMode(hdc, OPAQUE);
			GetPlotRect(lpgw, &rect);
			SetViewportExtEx(hdc, rect.right, rect.bottom, NULL);
			/* double buffering */
			if (lpgw->doublebuffer) {
				/* choose oversampling */
				if (lpgw->oversample)
					sampling = 2; /* anything bigger than that makes the computer crawl ... */
				else
					sampling = 1;

				/* Was the window resized? */
				GetWindowRect(hwnd, &wrect);
				wwidth =  wrect.right - wrect.left;
				wheight = wrect.bottom - wrect.top;
				if ((lpgw->Size.x != wwidth) || (lpgw->Size.y != wheight)) {
					RECT rect;
					DestroyFonts(lpgw);
					GetPlotRect(lpgw, &rect);
					MakeFonts(lpgw, (LPRECT)&rect, hdc);
					lpgw->buffervalid = FALSE;
				}

				/* create memory device context for double buffering */
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
				memdc = CreateCompatibleDC(hdc);
				memrect.left = 0;
				memrect.right = width * sampling + sampling/2;
				memrect.top = 0;
				memrect.bottom = height * sampling + sampling/2;

				if (!lpgw->buffervalid || (lpgw->hBitmap == NULL)) {
					BOOL save_aa;

					if (lpgw->hBitmap != NULL)
						DeleteObject(lpgw->hBitmap);
					lpgw->hBitmap = CreateCompatibleBitmap(hdc, memrect.right, memrect.bottom);
					oldbmp = (HBITMAP)SelectObject(memdc, lpgw->hBitmap);
					/* Update window size */
					lpgw->Size.x = wwidth;
					lpgw->Size.y = wheight;

					/* TODO: we really should cache the results ... */
					if (sampling > 1) {
						lpgw->sampling = sampling;
						DestroyFonts(lpgw);
						MakeFonts(lpgw, &memrect, memdc);
					}

					/* Temporarily switch of antialiasing during rotation */
					save_aa = lpgw->antialiasing;
					if (lpgw->rotating && lpgw->fastrotation)
						lpgw->antialiasing = FALSE;

					/* draw into memdc, then copy to hdc */
#ifdef HAVE_GDIPLUS
					if (lpgw->gdiplus && !(lpgw->rotating && lpgw->fastrotation))
						drawgraph_gdiplus(lpgw, memdc, &memrect);
					else
#endif
						drawgraph(lpgw, memdc, &memrect);

					/* restore antialiasing */
					lpgw->antialiasing = save_aa;

					/* drawing by gnuplot still in progress... */
					lpgw->buffervalid = !lpgw->locked;
				} else {
					oldbmp = (HBITMAP) SelectObject(memdc, lpgw->hBitmap);
				}
				if (lpgw->buffervalid) {
					if (sampling == 1)
						BitBlt(hdc, rect.left, rect.top, width, height, memdc, 0, 0, SRCCOPY);
					else {
						int stretch = SetStretchBltMode(hdc, HALFTONE);
						StretchBlt(hdc, rect.left, rect.top, width, height,
								   memdc,0, 0, memrect.right, memrect.bottom,
								   SRCCOPY);
						SetStretchBltMode(hdc, stretch);
					}
				}
				lpgw->sampling = 1;

				/* select the old bitmap back into the device context */
				if (memdc != NULL) {
					SelectObject(memdc, oldbmp);
					DeleteDC(memdc);
				}
			} else {
#ifdef HAVE_GDIPLUS
				if (lpgw->gdiplus && !(lpgw->rotating && lpgw->fastrotation))
					drawgraph_gdiplus(lpgw, hdc, &rect);
				else
#endif
					drawgraph(lpgw, hdc, &rect);
			}
			EndPaint(hwnd, &ps);
#ifdef USE_MOUSE
			/* drawgraph() erases the plot window, so immediately after
			 * it has completed, all the 'real-time' stuff the gnuplot core
			 * doesn't know anything about has to be redrawn */
			DrawRuler(lpgw);
			DrawRulerLineTo(lpgw);
			DrawZoomBox(lpgw);
#endif
			/* Update in case the number of graphs has changed */
			UpdateToolbar(lpgw);

			return 0;
		}
		case WM_SIZE:
			SendMessage(lpgw->hStatusbar, WM_SIZE, wParam, lParam);
			if (lpgw->hToolbar) {
				RECT rect;
				SendMessage(lpgw->hToolbar, WM_SIZE, wParam, lParam);
				/* make room */
				GetClientRect(lpgw->hToolbar, &rect);
				lpgw->ToolbarHeight = rect.bottom - rect.top + 1;
			}
			if ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)) {
				RECT rect;
				unsigned width, height;

				GetWindowRect(hwnd, &rect);
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
				/* Ignore minimize / de-minize */
				if ((lpgw->Size.x != width) || (lpgw->Size.y != height)) {
					lpgw->Size.x = width;
					lpgw->Size.y = height;

					/* remake fonts */
					lpgw->buffervalid = FALSE;
					DestroyFonts(lpgw);
					GetPlotRect(lpgw, &rect);
					hdc = GetDC(hwnd);
					MakeFonts(lpgw, &rect, hdc);
					ReleaseDC(hwnd, hdc);

					GetClientRect(hwnd, &rect);
					InvalidateRect(hwnd, &rect, 1);
					UpdateWindow(hwnd);
				}
			}
			if (lpgw->Size.x == CW_USEDEFAULT) {
			    lpgw->Size.x = LOWORD(lParam);
			    lpgw->Size.y = HIWORD(lParam);
			}
			break;
#ifndef WGP_CONSOLE
		case WM_DROPFILES:
			if (lpgw->lptw)
				DragFunc(lpgw->lptw, (HDROP)wParam);
			break;
#endif
		case WM_DESTROY:
			lpgw->buffervalid = FALSE;
			DeleteObject(lpgw->hBitmap);
			lpgw->hBitmap = NULL;
			clear_tooltips(lpgw);
			DestroyWindow(lpgw->hTooltip);
			lpgw->hTooltip = NULL;
			DestroyPens(lpgw);
			DestroyFonts(lpgw);
#ifdef USE_MOUSE
			DestroyCursors(lpgw);
#endif
			DragAcceptFiles(hwnd, FALSE);
			lpgw->hWndGraph = NULL;
#ifndef WGP_CONSOLE
			WinPersistTextClose();
#endif
			return 0;
		case WM_CLOSE:
			GraphClose(lpgw);
			return 0;
		}
	return DefWindowProc(hwnd, message, wParam, lParam);
}


void
GraphChangeFont(LPGW lpgw, LPCSTR font, int fontsize, HDC hdc, RECT rect)
{
    int newfontsize;
    bool remakefonts = FALSE;
    bool font_is_not_empty = (font != NULL && *font != '\0');

    newfontsize = (fontsize != 0) ? fontsize : lpgw->deffontsize;
    if (font_is_not_empty) {
		remakefonts = (strcmp(lpgw->fontname, font) != 0) || (newfontsize != lpgw->fontsize);
    } else {
		remakefonts = (strcmp(lpgw->fontname, lpgw->deffontname) != 0) || (newfontsize != lpgw->fontsize);
    }

    if (remakefonts) {
        lpgw->fontsize = newfontsize;
        strcpy(lpgw->fontname, font_is_not_empty ? font : lpgw->deffontname);

        DestroyFonts(lpgw);
        MakeFonts(lpgw, &rect, hdc);
    }
}


/* close the terminal window */
void WDPROC
win_close_terminal_window(LPGW lpgw)
{
	if (GraphHasWindow(lpgw))
		SendMessage( lpgw->hWndGraph, WM_CLOSE, 0L, 0L);
}


#ifdef USE_MOUSE
/* Implemented by Petr Mikulik, February 2001 --- the best Windows solutions
 * come from OS/2 :-))
 */

/* ================================================================= */

/* Firstly: terminal calls from win.trm */

/* Note that these all take lpgw as their first argument. It's an OO-type
 * 'this' pointer, sort of: it stores all the status information of the graph
 * window that we need, in a single large structure. */

void WDPROC
Graph_set_cursor(LPGW lpgw, int c, int x, int y)
{
	switch (c) {
	case -4: /* switch off line between ruler and mouse cursor */
		DrawRulerLineTo(lpgw);
		ruler_lineto.on = FALSE;
		break;
	case -3: /* switch on line between ruler and mouse cursor */
		if (ruler.on && ruler_lineto.on)
		    break;
		ruler_lineto.x = x;
		ruler_lineto.y = y;
		ruler_lineto.on = TRUE;
		DrawRulerLineTo(lpgw);
		break;
	case -2:
		{ /* move mouse to the given point */
			RECT rc;
			POINT pt;

			GetPlotRect(lpgw, &rc);
			pt.x = MulDiv(x, rc.right - rc.left, lpgw->xmax);
			pt.y = rc.bottom - MulDiv(y, rc.bottom - rc.top, lpgw->ymax);

			MapWindowPoints(lpgw->hWndGraph, HWND_DESKTOP, &pt, 1);
			SetCursorPos(pt.x, pt.y);
		}
		break;
	case -1: /* start zooming; zooming cursor */
		zoombox.on = TRUE;
		zoombox.from.x = zoombox.to.x = x;
		zoombox.from.y = zoombox.to.y = y;
		break;
	case 0:  /* standard cross-hair cursor */
		SetCursor((hptrCurrent = mouse_setting.on ? hptrCrossHair : hptrDefault));
		/* Once done with rotation we have to redraw with aa once more. */
		if (lpgw->rotating && lpgw->fastrotation && lpgw->antialiasing) {
			lpgw->rotating = FALSE;
			GraphRedraw(lpgw);
		}
		break;
	case 1:  /* cursor during rotation */
		SetCursor(hptrCurrent = hptrRotating);
		lpgw->rotating = TRUE;
		break;
	case 2:  /* cursor during scaling */
		SetCursor(hptrCurrent = hptrScaling);
		break;
	case 3:  /* cursor during zooming */
		SetCursor(hptrCurrent = hptrZooming);
		break;
	}
	if (c>=0 && zoombox.on) { /* erase zoom box */
		DrawZoomBox(lpgw);
		zoombox.on = FALSE;
	}
	if (c>=0 && ruler_lineto.on) { /* erase ruler line */
		DrawRulerLineTo(lpgw);
		ruler_lineto.on = FALSE;
	}
}

/* set_ruler(int x, int y) term API: x<0 switches ruler off. */
void WDPROC
Graph_set_ruler (LPGW lpgw, int x, int y )
{
	DrawRuler(lpgw); /* remove previous drawing, if any */
	DrawRulerLineTo(lpgw);
	if (x < 0) {
		ruler.on = FALSE;
		return;
	}
	ruler.on = TRUE;
	ruler.x = x; ruler.y = y;
	DrawRuler(lpgw); /* draw ruler at new positions */
	DrawRulerLineTo(lpgw);
}

/* put_tmptext(int i, char c[]) term API
 * 	i: 0..at statusline
 *	1, 2: at corners of zoom box with \r separating text
 */
void WDPROC
Graph_put_tmptext (LPGW lpgw, int where, LPCSTR text )
{
    /* Position of the annotation string (mouse movement) or zoom box
     * text or whatever temporary text added...
     */
    switch (where) {
	case 0:
		UpdateStatusLine(lpgw, text);
		break;
	case 1:
		DrawZoomBox(lpgw);
		if (zoombox.text1) {
			free((char*)zoombox.text1);
		}
		zoombox.text1 = strdup(text);
		DrawZoomBox(lpgw);
		break;
	case 2:
		DrawZoomBox(lpgw);
		if (zoombox.text2) {
			free((char*)zoombox.text2);
		}
		zoombox.text2 = strdup(text);
		DrawZoomBox(lpgw);
		break;
	default:
		; /* should NEVER happen */
	}
}


void WDPROC
Graph_set_clipboard (LPGW lpgw, LPCSTR s)
{
	size_t length;
	HGLOBAL memHandle;
	LPSTR clipText;

	/* check: no string --> nothing to do */
	if (!s || !s[0])
		return;

	/* Transport the string into a system-global storage area. In case
	 * of (unlikely) allocation failures, fail silently */
	length = strlen(s);
	if ( (memHandle = GlobalAlloc(GHND, length + 1)) == NULL)
		return;
	if ( (clipText = GlobalLock(memHandle)) == NULL) {
		GlobalFree(memHandle);
		return;
	}
	strcpy(clipText, s);
	GlobalUnlock(memHandle);

	/* Now post that memory area to the Clipboard */
	OpenClipboard(lpgw->hWndGraph);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, memHandle);
	CloseClipboard();
}

/* ================================================================= */

/* Secondly, support routines that implement direct mouse reactions. */

/* This routine gets the mouse/pointer position in the current window and
 * recalculates it to the viewport coordinates. */
static void
GetMousePosViewport(LPGW lpgw, int *mx, int *my)
{
	POINT pt;
	RECT rc;

	GetPlotRect(lpgw, &rc);

	/* HBB: has to be done this way. The simpler method by taking apart LPARM
	 * only works for mouse, but not for keypress events. */
	GetCursorPos(&pt);
	ScreenToClient(lpgw->hWndGraph, &pt);

	/* px=px(mx); mouse=>gnuplot driver coordinates */
	/* FIXME: classically, this would use MulDiv() call, and no floating point */
	/* BM: protect against zero window size - Vista does that when minimizing windows */
	*mx = *my = 0;
	if ((rc.right - rc.left) != 0)
		*mx = (int)((pt.x - rc.left) * lpgw->xmax / (rc.right - rc.left) + 0.5);
	if ((rc.bottom -rc.top) != 0)
		*my = (int)((rc.bottom - pt.y) * lpgw->ymax / (rc.bottom -rc.top) + 0.5);
}

/* HBB 20010218: Newly separated function: Draw text string in XOR mode.
 * That is surprisingly difficult using the Windows API: have to draw text
 * into a background bitmap, first, and then blit that onto the screen.
 *
 * x and y give the bottom-left corner of the bounding box into which the
 * text will be drawn */
static void
Draw_XOR_Text(LPGW lpgw, const char *text, size_t length, int x, int y)
{
	HDC hdc, tempDC;
	HBITMAP bitmap;
	int cx, cy;

	if (!text || !text[0])
	       return; /* no text to be displayed */

	hdc = GetDC(lpgw->hWndGraph);

	/* Prepare background image buffer of the necessary size */
	Wnd_GetTextSize(hdc, text, length, &cx, &cy);
	bitmap = CreateCompatibleBitmap(hdc, cx, cy);
	/* ... and a DeviceContext to access it by */
	tempDC = CreateCompatibleDC(hdc);
	DeleteObject(SelectObject(tempDC, bitmap));

	TextOut(tempDC, 0, 0, text, length);

	/* Copy printed string to the screen window using
	   "target = target XOR (NOT source)" ROP, see "Ternary Raster Operations"
	   http://msdn.microsoft.com/en-us/library/dd145130%28VS.85%29.aspx
	*/
	BitBlt(hdc, x, y - cy, cx, cy, tempDC, 0, 0, 0x00990066);

	/* Clean up behind ourselves */
	DeleteDC(tempDC);
	DeleteObject(bitmap);
	ReleaseDC(lpgw->hWndGraph, hdc);
}

#endif

/* ================================== */

/*
 * Update the status line by the text; erase the previous text
 */
static void
UpdateStatusLine(LPGW lpgw, const char text[])
{
    SendMessage(lpgw->hStatusbar, WM_SETTEXT, (WPARAM)0, (LPARAM)text);
}


/*
 * Update the toolbar to reflect the current the number of active plots
 */
static void
UpdateToolbar(LPGW lpgw)
{
	unsigned i;

	SendMessage(lpgw->hToolbar, TB_HIDEBUTTON, M_HIDEGRID, (LPARAM)!lpgw->hasgrid);
	if (!lpgw->hasgrid) {
		lpgw->hidegrid = FALSE;
		SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEGRID, (LPARAM)FALSE);
	}
	for (i = 0; i < GPMAX(MAXPLOTSHIDE, lpgw->maxhideplots); i++) {
		if (i < lpgw->numplots) {
			if (i < MAXPLOTSHIDE)
				SendMessage(lpgw->hToolbar, TB_HIDEBUTTON, M_HIDEPLOT + i, (LPARAM)FALSE);
		} else {
			if (i < lpgw->maxhideplots)
				lpgw->hideplot[i] = FALSE;
			if (i < MAXPLOTSHIDE) {
				SendMessage(lpgw->hToolbar, TB_HIDEBUTTON, M_HIDEPLOT + i, (LPARAM)TRUE);
				SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEPLOT + i, (LPARAM)FALSE);
			}
		}
	}
}


/*
 * Toggle active plots
 */
void WDPROC
GraphModifyPlots(LPGW lpgw, unsigned int ops)
{
	int i;
	TBOOLEAN changed = FALSE;

	for (i = 0; i < GPMIN(lpgw->numplots, lpgw->maxhideplots); i++) {
		switch (ops) {
		case MODPLOTS_INVERT_VISIBILITIES:
			lpgw->hideplot[i] = !lpgw->hideplot[i];
			changed = TRUE;
			SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEPLOT + i, (LPARAM)lpgw->hideplot[i]);
			break;
		case MODPLOTS_SET_VISIBLE:
			if (lpgw->hideplot[i] == TRUE) {
				changed = TRUE;
				SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEPLOT + i, (LPARAM)FALSE);
			}
			lpgw->hideplot[i] = FALSE;
			break;
		case MODPLOTS_SET_INVISIBLE:
			if (lpgw->hideplot[i] == FALSE) {
				changed = TRUE;
				SendMessage(lpgw->hToolbar, TB_CHECKBUTTON, M_HIDEPLOT + i, (LPARAM)TRUE);
			}
			lpgw->hideplot[i] = TRUE;
			break;
		}
	}
	if (changed) {
		RECT rect;

		lpgw->buffervalid = FALSE;
		GetClientRect(lpgw->hWndGraph, &rect);
		InvalidateRect(lpgw->hWndGraph, (LPRECT) &rect, 1);
		UpdateToolbar(lpgw);
		UpdateWindow(lpgw->hWndGraph);
	}
}


#ifdef USE_MOUSE

/* Draw the ruler.
 */
static void
DrawRuler (LPGW lpgw)
{
	HDC hdc;
	int iOldRop;
	RECT rc;
	long rx, ry;

	if (!ruler.on || ruler.x < 0)
		return;

	hdc = GetDC(lpgw->hWndGraph);
	GetPlotRect(lpgw, &rc);

	rx = MulDiv(ruler.x, rc.right - rc.left, lpgw->xmax);
	ry = rc.bottom - MulDiv(ruler.y, rc.bottom - rc.top, lpgw->ymax);

	iOldRop = SetROP2(hdc, R2_NOT);
	MoveTo(hdc, rc.left, ry);
	LineTo(hdc, rc.right, ry);
	MoveTo(hdc, rx, rc.top);
	LineTo(hdc, rx, rc.bottom);
	SetROP2(hdc, iOldRop);
	ReleaseDC(lpgw->hWndGraph, hdc);
}

/* Draw the ruler line to cursor position.
 */
static void
DrawRulerLineTo (LPGW lpgw)
{
	HDC hdc;
	int iOldRop;
	RECT rc;
	long rx, ry, rlx, rly;

	if (!ruler.on || !ruler_lineto.on || ruler.x < 0 || ruler_lineto.x < 0)
		return;

	hdc = GetDC(lpgw->hWndGraph);
	GetPlotRect(lpgw, &rc);

	rx  = MulDiv(ruler.x, rc.right - rc.left, lpgw->xmax);
	ry  = rc.bottom - MulDiv(ruler.y, rc.bottom - rc.top, lpgw->ymax);
	rlx = MulDiv(ruler_lineto.x, rc.right - rc.left, lpgw->xmax);
	rly = rc.bottom - MulDiv(ruler_lineto.y, rc.bottom - rc.top, lpgw->ymax);

	iOldRop = SetROP2(hdc, R2_NOT);
	MoveTo(hdc, rx, ry);
	LineTo(hdc, rlx, rly);
	SetROP2(hdc, iOldRop);
	ReleaseDC(lpgw->hWndGraph, hdc);
}

/* Draw the zoom box.
 */
static void
DrawZoomBox (LPGW lpgw)
{
	HDC hdc;
	long fx, fy, tx, ty, text_y;
	int OldROP2;
	RECT rc;
	HPEN OldPen;

	if (!zoombox.on)
		return;

	hdc = GetDC(lpgw->hWndGraph);
	GetPlotRect(lpgw, &rc);

	fx = MulDiv(zoombox.from.x, rc.right - rc.left, lpgw->xmax);
	fy = rc.bottom - MulDiv(zoombox.from.y, rc.bottom - rc.top, lpgw->ymax);
	tx = MulDiv(zoombox.to.x, rc.right - rc.left, lpgw->xmax);
	ty = rc.bottom - MulDiv(zoombox.to.y, rc.bottom - rc.top, lpgw->ymax);
	text_y = MulDiv(lpgw->vchar, rc.bottom - rc.top, lpgw->ymax);

	OldROP2 = SetROP2(hdc, R2_NOTXORPEN);
	OldPen = SelectObject(hdc, CreatePenIndirect(
			(lpgw->color ? lpgw->colorpen : lpgw->monopen) + 1));
	Rectangle(hdc, fx, fy, tx, ty);
	DeleteObject(SelectObject(hdc, OldPen));
	SetROP2(hdc, OldROP2);

	ReleaseDC(lpgw->hWndGraph, hdc);

	if (zoombox.text1) {
		char *separator = strchr(zoombox.text1, '\r');

		if (separator) {
			Draw_XOR_Text(lpgw, zoombox.text1, separator - zoombox.text1, fx, fy);
			Draw_XOR_Text(lpgw, separator + 1, strlen(separator + 1), fx, fy + text_y);
		} else {
			Draw_XOR_Text(lpgw, zoombox.text1, strlen(zoombox.text1), fx, fy + lpgw->vchar / 2);
		}
	}
	if (zoombox.text2) {
		char *separator = strchr(zoombox.text2, '\r');

		if (separator) {
			Draw_XOR_Text(lpgw, zoombox.text2, separator - zoombox.text2, tx, ty);
			Draw_XOR_Text(lpgw, separator + 1, strlen(separator + 1), tx, ty + text_y);
		} else  {
			Draw_XOR_Text(lpgw, zoombox.text2, strlen(zoombox.text2), tx, ty + lpgw->vchar / 2);
		}
	}
}

#endif /* USE_MOUSE */

/* eof wgraph.c */

