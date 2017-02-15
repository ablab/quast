/*
 * $Id: wgdiplus.cpp,v 1.16.2.9 2016/09/12 15:14:15 markisch Exp $
 */

/*
Copyright (c) 2011-2014 Bastian Maerkisch. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

extern "C" {
# include "syscfg.h"
}
#include <windows.h>
#include <windowsx.h>
#define GDIPVER 0x0110
#include <gdiplus.h>

#include "wgdiplus.h"
#include "wgnuplib.h"
#include "wcommon.h"
using namespace Gdiplus;

static bool gdiplusInitialized = false;
static ULONG_PTR gdiplusToken;

#define GWOPMAX 4096
#define MINMAX(a,val,b) (((val) <= (a)) ? (a) : ((val) <= (b) ? (val) : (b)))
const int pattern_num = 8;


static Color gdiplusCreateColor(COLORREF color, double alpha);
static Pen * gdiplusCreatePen(UINT style, float width, COLORREF color, double alpha);
static void gdiplusPolyline(Graphics &graphics, Pen &pen, POINT *ppt, int polyi);
static void gdiplusFilledPolygon(Graphics &graphics, Brush &brush, POINT *ppt, int polyi);
static Brush * gdiplusPatternBrush(int style, COLORREF color, double alpha, COLORREF backcolor, BOOL transparent);
static void gdiplusDot(Graphics &graphics, Brush &brush, int x, int y);
static Font * SetFont_gdiplus(Graphics &graphics, LPRECT rect, LPGW lpgw, char * fontname, int size);


void
gdiplusInit(void)
{
	if (!gdiplusInitialized) {
		gdiplusInitialized = true;
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	}
}


void
gdiplusCleanup(void)
{
	if (gdiplusInitialized) {
		gdiplusInitialized = false;
		GdiplusShutdown(gdiplusToken);
	}
}


static Color
gdiplusCreateColor(COLORREF color, double alpha)
{
	ARGB argb = Color::MakeARGB(
		BYTE(255 * alpha),
		GetRValue(color), GetGValue(color), GetBValue(color));
	return Color(argb);
}


static Pen *
gdiplusCreatePen(UINT style, float width, COLORREF color, double alpha)
{
	// create GDI+ pen
	Color gdipColor = gdiplusCreateColor(color, alpha);
	Pen * pen = new Pen(gdipColor, width > 1 ? width : 1);
	if (style <= PS_DASHDOTDOT)
		// cast is save since GDI and GDI+ use same numbers
		pen->SetDashStyle(static_cast<DashStyle>(style));
	pen->SetLineCap(LineCapSquare, LineCapSquare, DashCapFlat);
	pen->SetLineJoin(LineJoinMiter);

	return pen;
}


/* ****************  Mixed mode GDI/GDI+ functions ************************* */


void
gdiplusLine(HDC hdc, POINT x, POINT y, const PLOGPEN logpen, double alpha)
{
	gdiplusLineEx(hdc, x, y, logpen->lopnStyle, (float)logpen->lopnWidth.x, logpen->lopnColor, 0);
}


void
gdiplusLineEx(HDC hdc, POINT x, POINT y, UINT style, float width, COLORREF color, double alpha)
{
	gdiplusInit();
	Graphics graphics(hdc);

	// Dash patterns get scaled with line width, in contrast to GDI.
	// Avoid smearing out caused by antialiasing for small line widths.
	if ((style == PS_SOLID) || (width >= 2.))
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	Pen * pen = gdiplusCreatePen(style, width, color, alpha);
	graphics.DrawLine(pen, (INT)x.x, (INT)x.y, (INT)y.x, (INT)y.y);
	delete pen;
}


void
gdiplusPolyline(HDC hdc, POINT *ppt, int polyi, const PLOGPEN logpen, double alpha)
{
	gdiplusPolylineEx(hdc, ppt, polyi, logpen->lopnStyle, (float)logpen->lopnWidth.x, logpen->lopnColor, alpha);
}


void
gdiplusPolylineEx(HDC hdc, POINT *ppt, int polyi, UINT style, float width, COLORREF color, double alpha)
{
	gdiplusInit();
	Graphics graphics(hdc);

	// Dash patterns get scaled with line width, in contrast to GDI.
	// Avoid smearing out caused by antialiasing for small line widths.
	if ((style == PS_SOLID) || (width >= 2.))
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	Pen * pen = gdiplusCreatePen(style, width, color, alpha);
	Point * points = new Point[polyi];
	for (int i = 0; i < polyi; i++) {
		points[i].X = ppt[i].x;
		points[i].Y = ppt[i].y;
	}
	if ((ppt[0].x != ppt[polyi - 1].x) || (ppt[0].y != ppt[polyi - 1].y))
		graphics.DrawLines(pen, points, polyi);
	else
		graphics.DrawPolygon(pen, points, polyi - 1);
	delete pen;
	delete [] points;
}


void
gdiplusSolidFilledPolygonEx(HDC hdc, POINT *ppt, int polyi, COLORREF color, double alpha, BOOL aa)
{
	gdiplusInit();
	Graphics graphics(hdc);
	if (aa)
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	Color gdipColor = gdiplusCreateColor(color, alpha);
	Point * points = new Point[polyi];
	for (int i = 0; i < polyi; i++) {
		points[i].X = ppt[i].x;
		points[i].Y = ppt[i].y;
	}
	SolidBrush brush(gdipColor);
	graphics.FillPolygon(&brush, points, polyi);
	delete [] points;
}


void
gdiplusPatternFilledPolygonEx(HDC hdc, POINT *ppt, int polyi, COLORREF color, double alpha, COLORREF backcolor, BOOL transparent, int style)
{
	gdiplusInit();
	Graphics graphics(hdc);
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	Color gdipColor = gdiplusCreateColor(color, alpha);
	Color gdipBackColor = gdiplusCreateColor(backcolor, transparent ? 0 : 1.);
	Brush * brush;
	style %= 8;
	const HatchStyle styles[] = { HatchStyleTotal, HatchStyleDiagonalCross,
		HatchStyleZigZag, HatchStyleTotal,
		HatchStyleForwardDiagonal, HatchStyleBackwardDiagonal,
		HatchStyleLightDownwardDiagonal, HatchStyleDarkUpwardDiagonal };
	switch (style) {
		case 0:
			brush = new SolidBrush(gdipBackColor);
			break;
		case 3:
			brush = new SolidBrush(gdipColor);
			break;
		default:
			brush = new HatchBrush(styles[style], gdipColor, gdipBackColor);
	}
	Point * points = new Point[polyi];
	for (int i = 0; i < polyi; i++) {
		points[i].X = ppt[i].x;
		points[i].Y = ppt[i].y;
	}
	graphics.FillPolygon(brush, points, polyi);
	delete [] points;
	delete brush;
}


void
gdiplusCircleEx(HDC hdc, POINT * p, int radius, UINT style, float width, COLORREF color, double alpha)
{
	gdiplusInit();
	Graphics graphics(hdc);
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	Pen * pen = gdiplusCreatePen(style, width, color, alpha);
	graphics.DrawEllipse(pen, p->x - radius, p->y - radius, 2*radius, 2*radius);
	delete pen;
}


/* ****************  GDI+ only functions ********************************** */


void
gdiplusPolyline(Graphics &graphics, Pen &pen, POINT *ppt, int polyi)
{
	// Dash patterns get scaled with line width, in contrast to GDI.
	// Avoid smearing out caused by antialiasing for small line widths.
	SmoothingMode mode = graphics.GetSmoothingMode();
	if ((mode != SmoothingModeNone) && (pen.GetDashStyle() != DashStyleSolid) && (pen.GetWidth() < 2))
		graphics.SetSmoothingMode(SmoothingModeNone);

	Point * points = new Point[polyi];
	for (int i = 0; i < polyi; i++) {
		points[i].X = ppt[i].x;
		points[i].Y = ppt[i].y;
	}
	if ((ppt[0].x != ppt[polyi - 1].x) || (ppt[0].y != ppt[polyi - 1].y))
		graphics.DrawLines(&pen, points, polyi);
	else
		graphics.DrawPolygon(&pen, points, polyi - 1);
	delete [] points;

	/* restore */
	if (mode != SmoothingModeNone)
		graphics.SetSmoothingMode(mode);
}


static void
gdiplusFilledPolygon(Graphics &graphics, Brush &brush, POINT *ppt, int polyi)
{
	Point * points = new Point[polyi];
	for (int i = 0; i < polyi; i++) {
		points[i].X = ppt[i].x;
		points[i].Y = ppt[i].y;
	}
	graphics.SetCompositingQuality(CompositingQualityGammaCorrected);
	graphics.FillPolygon(&brush, points, polyi);
	graphics.SetCompositingQuality(CompositingQualityDefault);
	delete [] points;
}


static Brush *
gdiplusPatternBrush(int style, COLORREF color, double alpha, COLORREF backcolor, BOOL transparent)
{
	Color gdipColor = gdiplusCreateColor(color, alpha);
	Color gdipBackColor = gdiplusCreateColor(backcolor, transparent ? 0 : 1.);
	Brush * brush;
	style %= pattern_num;
	const HatchStyle styles[] = { HatchStyleTotal, HatchStyleDiagonalCross,
		HatchStyleZigZag, HatchStyleTotal,
		HatchStyleForwardDiagonal, HatchStyleBackwardDiagonal,
		HatchStyleLightDownwardDiagonal, HatchStyleDarkUpwardDiagonal };
	switch (style) {
		case 0:
			brush = new SolidBrush(gdipBackColor);
			break;
		case 3:
			brush = new SolidBrush(gdipColor);
			break;
		default:
			brush = new HatchBrush(styles[style], gdipColor, gdipBackColor);
	}
	return brush;
}


static void
gdiplusDot(Graphics &graphics, Brush &brush, int x, int y)
{
	/* no antialiasing in order to avoid blurred pixel */
	SmoothingMode mode = graphics.GetSmoothingMode();
	graphics.SetSmoothingMode(SmoothingModeNone);
	graphics.FillRectangle(&brush, x, y, 1, 1);
	graphics.SetSmoothingMode(mode);
}


static Font *
SetFont_gdiplus(Graphics &graphics, LPRECT rect, LPGW lpgw, char * fontname, int size)
{
	if ((fontname == NULL) || (*fontname == 0))
		fontname = lpgw->deffontname;
	if (size == 0)
		size = lpgw->deffontsize;

	/* make a local copy */
	fontname = strdup(fontname);

	/* save current font */
	strcpy(lpgw->fontname, fontname);
	lpgw->fontsize = size;

	/* extract font style */
	INT fontStyle = FontStyleRegular;
	char * italic, * bold, * underline, * strikeout;
	if ((italic = strstr(fontname, " Italic")) != NULL)
		fontStyle |= FontStyleItalic;
	else if ((italic = strstr(fontname, ":Italic")) != NULL)
		fontStyle |= FontStyleItalic;
	if ((bold = strstr(fontname, " Bold")) != NULL)
		fontStyle |= FontStyleBold;
	else if ((bold = strstr(fontname, ":Bold")) != NULL)
		fontStyle |= FontStyleBold;
	if ((underline = strstr(fontname, " Underline")) != NULL)
		fontStyle |= FontStyleUnderline;
	if ((strikeout = strstr(fontname, " Strikeout")) != NULL)
		fontStyle |= FontStyleStrikeout;
	if (italic) *italic = 0;
	if (strikeout) *strikeout = 0;
	if (bold) * bold = 0;
	if (underline) * underline = 0;

	LPWSTR family = UnicodeText(fontname, lpgw->encoding);
	free(fontname);
	const FontFamily * fontFamily = new FontFamily(family);
	free(family);
	Font * font;
	int fontHeight;
	if (fontFamily->GetLastStatus() != Ok) {
		delete fontFamily;
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
		// MinGW 4.8.1 does not have this
		fontFamily = FontFamily::GenericSansSerif();
#else
		family = UnicodeText(GraphDefaultFont(), S_ENC_DEFAULT); // should always be available
		fontFamily = new FontFamily(family);
		free(family);
#endif
		font = new Font(fontFamily, size * lpgw->sampling, fontStyle, UnitPoint);
		fontHeight = font->GetSize() / fontFamily->GetEmHeight(fontStyle) * graphics.GetDpiY() / 72. *
			(fontFamily->GetCellAscent(fontStyle) + fontFamily->GetCellDescent(fontStyle));
	} else {
		font = new Font(fontFamily, size * lpgw->sampling, fontStyle, UnitPoint);
		fontHeight = font->GetSize() / fontFamily->GetEmHeight(fontStyle) * graphics.GetDpiY() / 72. *
			(fontFamily->GetCellAscent(fontStyle) + fontFamily->GetCellDescent(fontStyle));
		delete fontFamily;
	}

	RectF box;
	graphics.MeasureString(L"0123456789", 10, font, PointF(0, 0), &box);
	// lpgw->vchar = MulDiv(box.Height, lpgw->ymax, rect->bottom - rect->top);
	lpgw->vchar = MulDiv(fontHeight, lpgw->ymax, rect->bottom - rect->top);
	lpgw->hchar = MulDiv(box.Width, lpgw->xmax, 10 * (rect->right - rect->left));
	lpgw->rotate = TRUE;
	lpgw->htic = MulDiv(lpgw->hchar, 2, 5);
	unsigned cy = MulDiv(box.Width, 2 * graphics.GetDpiY(), 50 * graphics.GetDpiX());
	lpgw->vtic = MulDiv(cy, lpgw->ymax, rect->bottom - rect->top);

	return font;
}


void
drawgraph_gdiplus(LPGW lpgw, HDC hdc, LPRECT rect)
{
	/* draw ops */
	unsigned int ngwop = 0;
	struct GWOP *curptr;
	struct GWOPBLK *blkptr;

	/* layers and hypertext */
	unsigned plotno = 0;
	bool gridline = false;
	bool skipplot = false;
	bool keysample = false;
	bool interactive;
	LPWSTR hypertext = NULL;
	int hypertype = 0;

	/* colors */
	bool isColor;				/* use colors? */
	COLORREF last_color = 0;	/* currently selected color */
	double alpha_c = 1.;		/* alpha for transparency */

	/* text */
	Font * font;

	/* lines */
	double line_width = lpgw->sampling * lpgw->linewidth;	/* current line width */
	double lw_scale = 1.;
	LOGPEN cur_penstruct;		/* current pen settings */

	/* polylines and polygons */
	int polymax = 200;			/* size of ppt */
	int polyi = 0;				/* number of points in ppt */
	POINT * ppt;				/* storage of polyline/polygon-points */
	int last_polyi = 0;			/* number of points in last_poly */
	POINT * last_poly = NULL;	/* storage of last filled polygon */
	unsigned int lastop = -1;	/* used for plotting last point on a line */
	POINT cpoint;				/* current GDI location */

	/* filled polygons and boxes */
	int last_fillstyle = -1;
	COLORREF last_fillcolor = 0;
	double last_fill_alpha = 1.;
	bool transparent = false;	/* transparent fill? */
	Brush * pattern_brush = NULL;
	Brush * fill_brush = NULL;

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
	int last_symbol = 0;
	CachedBitmap *cb = NULL;
	POINT cb_ofs;
	bool ps_caching = false;

	/* coordinates and lengths */
	int xdash, ydash;			/* the transformed coordinates */
	int rr, rl, rt, rb;			/* coordinates of drawing area */
	int htic, vtic;				/* tic sizes */
	int hshift, vshift;			/* correction of text position */

	/* indices */
	int seq = 0;				/* sequence counter for W_image and W_boxedtext */

	if (lpgw->locked) return;

	/* clear hypertexts only in display sessions */
	interactive = (GetObjectType(hdc) == OBJ_MEMDC) ||
		((GetObjectType(hdc) == OBJ_DC) && (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASDISPLAY));
	if (interactive)
		clear_tooltips(lpgw);

	rr = rect->right;
	rl = rect->left;
	rt = rect->top;
	rb = rect->bottom;

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

	/* Need to scale line widths for raster printers so they are the same
	   as on screen */
	if ((GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER)) {
		HDC hdc_screen = GetDC(NULL);
		lw_scale = (double) GetDeviceCaps(hdc, VERTRES) /
		           (double) GetDeviceCaps(hdc_screen, VERTRES);
		line_width *= lw_scale;
		ReleaseDC(NULL, hdc_screen);
	}

	ps_caching = !((GetDeviceCaps(hdc, TECHNOLOGY) == DT_METAFILE)
	            || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_PLOTTER)
	            || (GetDeviceCaps(hdc, TECHNOLOGY) == DT_RASPRINTER));

	gdiplusInit();
	Graphics graphics(hdc);

	if (lpgw->antialiasing) {
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);
		graphics.SetSmoothingMode(SmoothingModeAntiAlias8x8);
		// graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
	}
	graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);

	/* background fill */
	if (isColor)
		graphics.Clear(gdiplusCreateColor(lpgw->background, 1.));
	else
		graphics.Clear(Color(255, 255, 255));

	/* Init brush and pens: need to be created after the Graphics object. */
	SolidBrush solid_brush(gdiplusCreateColor(lpgw->background, 1.));
	SolidBrush solid_fill_brush(gdiplusCreateColor(lpgw->background, 1.));
	cur_penstruct = (lpgw->color && isColor) ?  lpgw->colorpen[2] : lpgw->monopen[2];
	last_color = cur_penstruct.lopnColor;

	Pen pen(Color(0, 0, 0));
	Pen solid_pen(Color(0, 0, 0));
	pen.SetColor(gdiplusCreateColor(last_color, 1.));
	pen.SetLineCap(lpgw->rounded ? LineCapRound : LineCapSquare,
					lpgw->rounded ? LineCapRound : LineCapSquare,
					DashCapFlat);
	pen.SetLineJoin(lpgw->rounded ? LineJoinRound : LineJoinMiter);
	solid_pen.SetColor(gdiplusCreateColor(last_color, 1.));
	solid_pen.SetLineCap(lpgw->rounded ? LineCapRound : LineCapSquare,
					lpgw->rounded ? LineCapRound : LineCapSquare,
					DashCapFlat);
	solid_pen.SetLineJoin(lpgw->rounded ? LineJoinRound : LineJoinMiter);

	ppt = (POINT *) LocalAllocPtr(LHND, (polymax + 1) * sizeof(POINT));

	htic = (lpgw->org_pointsize * MulDiv(lpgw->htic, rr - rl, lpgw->xmax) + 1);
	vtic = (lpgw->org_pointsize * MulDiv(lpgw->vtic, rb - rt, lpgw->ymax) + 1);

	lpgw->angle = 0;
	lpgw->justify = LEFT;
	StringFormat stringFormat;
	stringFormat.SetAlignment(StringAlignmentNear);
	stringFormat.SetLineAlignment(StringAlignmentNear);
	font = SetFont_gdiplus(graphics, rect, lpgw, NULL, 0);

	/* calculate text shifting for horizontal text */
	hshift = 0;
	vshift = -MulDiv(lpgw->vchar, rb - rt, lpgw->ymax) / 2;

	/* init layer variables */
	lpgw->numplots = 0;
	lpgw->hasgrid = FALSE;
	for (unsigned i = 0; i < lpgw->maxkeyboxes; i++) {
		lpgw->keyboxes[i].left = INT_MAX;
		lpgw->keyboxes[i].right = 0;
		lpgw->keyboxes[i].bottom = INT_MAX;
		lpgw->keyboxes[i].top = 0;
	}

#ifdef EAM_BOXED_TEXT
	boxedtext.boxing = FALSE;
#endif

	/* do the drawing */
	blkptr = lpgw->gwopblk_head;
	curptr = NULL;
	if (blkptr != NULL) {
		if (!blkptr->gwop)
			blkptr->gwop = (struct GWOP *) GlobalLock(blkptr->hblk);
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
				gdiplusPolyline(graphics, pen, ppt, polyi);
				/* move internal state to last point */
				cpoint = ppt[polyi - 1];
			} else if (polyi == 1) {
				/* degenerate case e.g. when using 'linecolor variable' */
				graphics.DrawLine(&pen, (INT) cpoint.x, cpoint.y, ppt[0].x, ppt[0].y);
				cpoint = ppt[0];
			}
			polyi = 0;
		}

		/* finish last filled polygon */
		if ((last_poly != NULL) &&
			(((lastop == W_filled_polygon_draw) && (curptr->op != W_fillstyle)) ||
			 ((curptr->op == W_fillstyle) && (curptr->x != unsigned(last_fillstyle))))) {
			SmoothingMode mode = graphics.GetSmoothingMode();
			if (lpgw->antialiasing && !lpgw->polyaa)
				graphics.SetSmoothingMode(SmoothingModeNone);
			gdiplusFilledPolygon(graphics, *fill_brush, last_poly, last_polyi);
			graphics.SetSmoothingMode(mode);
			last_polyi = 0;
			free(last_poly);
			last_poly = NULL;
		}

		/* handle layer commands first */
		if (curptr->op == W_layer) {
			t_termlayer layer = (t_termlayer) curptr->x;
			switch (layer) {
				case TERM_LAYER_BEFORE_PLOT:
					plotno++;
					lpgw->numplots = plotno;
					if (plotno >= lpgw->maxhideplots) {
						unsigned int idx;
						lpgw->maxhideplots += 10;
						lpgw->hideplot = (BOOL *) realloc(lpgw->hideplot, lpgw->maxhideplots * sizeof(BOOL));
						for (idx = plotno; idx < lpgw->maxhideplots; idx++)
							lpgw->hideplot[idx] = FALSE;
					}
					if (plotno <= lpgw->maxhideplots)
						skipplot = (lpgw->hideplot[plotno - 1] == TRUE);
					break;
				case TERM_LAYER_AFTER_PLOT:
					skipplot = false;
					break;
#if 0
				case TERM_LAYER_BACKTEXT:
				case TERM_LAYER_FRONTTEXT
				case TERM_LAYER_END_TEXT:
					break;
#endif
				case TERM_LAYER_BEGIN_GRID:
					gridline = true;
					lpgw->hasgrid = TRUE;
					break;
				case TERM_LAYER_END_GRID:
					gridline = false;
					break;
				case TERM_LAYER_BEGIN_KEYSAMPLE:
					keysample = true;
					break;
				case TERM_LAYER_END_KEYSAMPLE:
					/* grey out keysample if graph is hidden */
					if ((plotno <= lpgw->maxhideplots) && lpgw->hideplot[plotno - 1]) {
						ARGB argb = Color::MakeARGB(128, 192, 192, 192);
						Color transparentgrey(argb);
						SolidBrush greybrush(transparentgrey);
						LPRECT bb = lpgw->keyboxes + plotno - 1;
						graphics.FillRectangle(&greybrush, INT(bb->left), INT(bb->bottom),
						                       bb->right - bb->left, bb->top - bb->bottom);
					}
					keysample = false;
					break;
				case TERM_LAYER_RESET:
				case TERM_LAYER_RESET_PLOTNO:
					plotno = 0;
					break;
				case TERM_LAYER_BEGIN_PM3D_MAP:
				case TERM_LAYER_BEGIN_IMAGE:
					// antialiasing is not supported properly for pm3d polygons
					// and failsafe images
					if (lpgw->antialiasing)
						graphics.SetSmoothingMode(SmoothingModeNone);
					break;
				case TERM_LAYER_END_PM3D_MAP:
				case TERM_LAYER_END_IMAGE:
					if (lpgw->antialiasing)
						graphics.SetSmoothingMode(SmoothingModeAntiAlias8x8);
					break;
				default:
					break;
			};
		}

		/* Hide this layer? Do not skip commands which could affect key samples: */
		if (!(skipplot || (gridline && lpgw->hidegrid)) ||
			keysample || (curptr->op == W_line_type) || (curptr->op == W_setcolor)
			          || (curptr->op == W_pointsize) || (curptr->op == W_line_width)
			          || (curptr->op == W_dash_type)) {

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
				gdiplusPolyline(graphics, pen, ppt, polyi);
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
				cur_penstruct.lopnWidth =
					(lpgw->color && isColor) ? lpgw->colorpen[cur_pen].lopnWidth : lpgw->monopen[cur_pen].lopnWidth;
				cur_penstruct.lopnColor = lpgw->colorpen[cur_pen].lopnColor;
				if (!lpgw->color || !isColor) {
					COLORREF color = cur_penstruct.lopnColor;
					unsigned luma = luma_from_color(GetRValue(color), GetGValue(color), GetBValue(color));
					cur_penstruct.lopnColor = RGB(luma, luma, luma);
				}
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

			Color color = gdiplusCreateColor(cur_penstruct.lopnColor, 1.);
			solid_brush.SetColor(color);

			solid_pen.SetColor(color);
			solid_pen.SetWidth(cur_penstruct.lopnWidth.x);

			pen.SetColor(color);
			pen.SetWidth(cur_penstruct.lopnWidth.x);
			if (cur_penstruct.lopnStyle <= PS_DASHDOTDOT)
				// cast is save since GDI and GDI+ use the same numbers
				pen.SetDashStyle(static_cast<DashStyle>(cur_penstruct.lopnStyle));
			else
				pen.SetDashStyle(DashStyleSolid);

			/* remember this color */
			last_color = cur_penstruct.lopnColor;
			alpha_c = 1.;
			break;
		}

		case W_dash_type: {
			int dt = static_cast<int>(curptr->x);

			if (dt >= 0) {
				dt %= WGNUMPENS;
				dt += 2;
				cur_penstruct.lopnStyle = lpgw->monopen[dt].lopnStyle;
				pen.SetDashStyle(static_cast<DashStyle>(cur_penstruct.lopnStyle));
			} else if (dt == DASHTYPE_SOLID) {
				cur_penstruct.lopnStyle = PS_SOLID;
				pen.SetDashStyle(static_cast<DashStyle>(cur_penstruct.lopnStyle));
			} else if (dt == DASHTYPE_AXIS) {
				dt = 1;
				cur_penstruct.lopnStyle =
					lpgw->dashed ? lpgw->monopen[dt].lopnStyle : lpgw->colorpen[dt].lopnStyle;
				pen.SetDashStyle(static_cast<DashStyle>(cur_penstruct.lopnStyle));
			} else if (dt == DASHTYPE_CUSTOM) {
				t_dashtype * dash = static_cast<t_dashtype *>(LocalLock(curptr->htext));
				INT count = 0;
				while ((dash->pattern[count] != 0.) && (count < DASHPATTERN_LENGTH)) count++;
				pen.SetDashPattern(dash->pattern, count);
				LocalUnlock(curptr->htext);
			}
			break;
		}

		case W_text_encoding:
			lpgw->encoding = (set_encoding_id) curptr->x;
			break;

		case W_put_text: {
			char * str;
			str = (char *) LocalLock(curptr->htext);
			if (str) {
				LPWSTR textw = UnicodeText(str, lpgw->encoding);
				if (textw) {
					if (lpgw->angle == 0) {
						PointF pointF(xdash + hshift, ydash + vshift);
						graphics.DrawString(textw, -1, font, pointF, &stringFormat, &solid_brush);
					} else {
						/* shift rotated text correctly */
						graphics.TranslateTransform(xdash + hshift, ydash + vshift);
						graphics.RotateTransform(-lpgw->angle);
						graphics.DrawString(textw, -1, font, PointF(0,0), &stringFormat, &solid_brush);
						graphics.ResetTransform();
					}
					RectF size;
					int dxl, dxr;
					graphics.MeasureString(textw, -1, font, PointF(0,0), &size);
					if (lpgw->justify == LEFT) {
						dxl = 0;
						dxr = size.Width;
					} else if (lpgw->justify == CENTRE) {
						dxl = dxr = size.Width / 2;
					} else {
						dxl = size.Width;
						dxr = 0;
					}
					if (keysample) {
						draw_update_keybox(lpgw, plotno, xdash - dxl, ydash - size.Height / 2);
						draw_update_keybox(lpgw, plotno, xdash + dxr, ydash + size.Height / 2);
					}
#ifdef EAM_BOXED_TEXT
					if (boxedtext.boxing) {
						if (boxedtext.box.left > (xdash - boxedtext.start.x - dxl))
							boxedtext.box.left = xdash - boxedtext.start.x - dxl;
						if (boxedtext.box.right < (xdash - boxedtext.start.x + dxr))
							boxedtext.box.right = xdash - boxedtext.start.x + dxr;
						if (boxedtext.box.top > (ydash - boxedtext.start.y - size.Height / 2))
							boxedtext.box.top = ydash - boxedtext.start.y - size.Height / 2;
						if (boxedtext.box.bottom < (ydash - boxedtext.start.y + size.Height / 2))
							boxedtext.box.bottom = ydash - boxedtext.start.y + size.Height / 2;
						/* We have to remember the text angle as well. */
						boxedtext.angle = lpgw->angle;
					}
#endif
					free(textw);
				}
			}
			LocalUnlock(curptr->htext);
			break;
		}

		case W_enhanced_text: {
			/* TODO: This section still uses GDI. Convert to GDI+. */
			HDC hdc = graphics.GetHDC();
			char * str;
			str = (char *) LocalLock(curptr->htext);
			if (str) {
				RECT extend;

				/* Setup GDI fonts: force re-make */
				int save_fontsize = lpgw->fontsize;
				lpgw->fontsize = -1;
				GraphChangeFont(lpgw, lpgw->fontname, save_fontsize, hdc, *rect);
				SetFont(lpgw, hdc);
				lpgw->fontsize = save_fontsize;

				/* Set GDI text color */
				SetTextColor(hdc, last_color);

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
			graphics.ReleaseHDC(hdc);
			break;
		}

		case W_hypertext:
			if (interactive) {
				/* Make a copy for future reference */
				char * str = (char *) LocalLock(curptr->htext);
				free(hypertext);
				hypertext = UnicodeText(str, lpgw->encoding);
				hypertype = curptr->x;
				LocalUnlock(curptr->htext);
			}
			break;

#ifdef EAM_BOXED_TEXT
		case W_boxedtext:
			if (seq == 0) {
				boxedtext.option = (t_textbox_options) curptr->x;
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
				if ((boxedtext.angle % 90) == 0) {
					Rect rect;

					switch (boxedtext.angle) {
					case 0:
						rect.X      = + boxedtext.box.left;
						rect.Y      = + boxedtext.box.top;
						rect.Width  = boxedtext.box.right - boxedtext.box.left;
						rect.Height = boxedtext.box.bottom - boxedtext.box.top;
						rect.Inflate(dx, dy);
						break;
					case 90:
						rect.X      = + boxedtext.box.top;
						rect.Y      = - boxedtext.box.right;
						rect.Height = boxedtext.box.right - boxedtext.box.left;
						rect.Width  = boxedtext.box.bottom - boxedtext.box.top;
						rect.Inflate(dy, dx);
						break;
					case 180:
						rect.X      = - boxedtext.box.right;
						rect.Y      = - boxedtext.box.bottom;
						rect.Width  = boxedtext.box.right - boxedtext.box.left;
						rect.Height = boxedtext.box.bottom - boxedtext.box.top;
						rect.Inflate(dx, dy);
						break;
					case 270:
						rect.X      = - boxedtext.box.bottom;
						rect.Y      = + boxedtext.box.left;
						rect.Height = boxedtext.box.right - boxedtext.box.left;
						rect.Width  = boxedtext.box.bottom - boxedtext.box.top;
						rect.Inflate(dy, dx);
						break;
					}
					rect.Offset(boxedtext.start.x, boxedtext.start.y);
					if (boxedtext.option == TEXTBOX_OUTLINE) {
						/* FIXME: Shouldn't we use the current color brush lpgw->hcolorbrush? */
						Pen * pen = gdiplusCreatePen(PS_SOLID, line_width, RGB(0,0,0), 1.);
						graphics.DrawRectangle(pen, rect);
						delete pen;
					} else {
						/* Fill bounding box with background color. */
						SolidBrush brush(gdiplusCreateColor(lpgw->background, 1.));
						graphics.FillRectangle(&brush, rect);
					}
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
					for (int i = 0; i < 4; i++) {
						rect[i].x += boxedtext.start.x;
						rect[i].y += boxedtext.start.y;
					}
					if (boxedtext.option == TEXTBOX_OUTLINE) {
						rect[4].x = rect[0].x;
						rect[4].y = rect[0].y;
						Pen * pen = gdiplusCreatePen(PS_SOLID, line_width, RGB(0,0,0), 1.);
						gdiplusPolyline(graphics, *pen, rect, 5);
						delete pen;
					} else {
						gdiplusSolidFilledPolygonEx(hdc, rect, 4, lpgw->background, 1., TRUE);
					}
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

		case W_fillstyle: {
			/* HBB 20010916: new entry, needed to squeeze the many
			 * parameters of a filled box call through the bottleneck
			 * of the fixed number of parameters in GraphOp() and
			 * struct GWOP, respectively. */
			polyi = 0; /* start new sequence */
			int fillstyle = curptr->x;

			/* Eliminate duplicate fillstyle requests. */
			if ((fillstyle == last_fillstyle) &&
				(last_color == last_fillcolor) &&
				(last_fill_alpha == alpha_c))
				break;

			transparent = false;
			switch (fillstyle & 0x0f) {
				case FS_TRANSPARENT_SOLID: {
					double alpha = (fillstyle >> 4) / 100.;
					solid_fill_brush.SetColor(gdiplusCreateColor(last_color, alpha));
					fill_brush = &solid_fill_brush;
					break;
				}
				case FS_SOLID: {
					if (alpha_c < 1.) {
						solid_fill_brush.SetColor(gdiplusCreateColor(last_color, alpha_c));
					} else if ((int)(fillstyle >> 4) == 100) {
						/* special case this common choice */
						solid_fill_brush.SetColor(gdiplusCreateColor(last_color, 1.));
					} else {
						double density = MINMAX(0, (int)(fillstyle >> 4), 100) * 0.01;
						COLORREF color =
							RGB(255 - density * (255 - GetRValue(last_color)),
								255 - density * (255 - GetGValue(last_color)),
								255 - density * (255 - GetBValue(last_color)));
						solid_fill_brush.SetColor(gdiplusCreateColor(color, 1.));
					}
					fill_brush = &solid_fill_brush;
					break;
				}
				case FS_TRANSPARENT_PATTERN:
					transparent = true;
					/* intentionally fall through */
				case FS_PATTERN: {
					/* style == 2 --> use fill pattern according to
							 * fillpattern. Pattern number is enumerated */
					int pattern = GPMAX(fillstyle >> 4, 0) % pattern_num;
					if (pattern_brush) delete pattern_brush;
					pattern_brush = gdiplusPatternBrush(pattern,
									last_color, 1., lpgw->background, transparent);
					fill_brush = pattern_brush;
					break;
				}
				case FS_EMPTY:
					/* FIXME: Instead of filling with background color, we should not fill at all in this case! */
					/* fill with background color */
					solid_fill_brush.SetColor(gdiplusCreateColor(lpgw->background, 1.));
					fill_brush = &solid_fill_brush;
					break;
				case FS_DEFAULT:
				default:
					/* Leave the current brush and color in place */
					solid_fill_brush.SetColor(gdiplusCreateColor(last_color, 1.));
					fill_brush = &solid_fill_brush;
					break;
			}
			last_fillstyle = fillstyle;
			last_fillcolor = last_color;
			last_fill_alpha = alpha_c;
			break;
		}

		case W_boxfill: {
			/* NOTE: the x and y passed with this call are the coordinates of the
			 * lower right corner of the box. The upper left corner was stored into
			 * ppt[0] by a preceding W_move, and the style was set
			 * by a W_fillstyle call. */
			POINT p;
			UINT  height, width;

			p.x = GPMIN(ppt[0].x, xdash);
			p.y = GPMIN(ppt[0].y, ydash);
			width = abs(xdash - ppt[0].x);
			height = abs(ppt[0].y - ydash);

			SmoothingMode mode = graphics.GetSmoothingMode();
			graphics.SetSmoothingMode(SmoothingModeNone);
			graphics.FillRectangle(fill_brush, (INT) p.x, p.y, width, height);
			graphics.SetSmoothingMode(mode);
			if (keysample)
				draw_update_keybox(lpgw, plotno, xdash + 1, ydash);
			polyi = 0;
			break;
		}

		case W_text_angle:
			if (lpgw->angle != (int)curptr->x) {
				lpgw->angle = (int)curptr->x;
				/* recalculate shifting of rotated text */
				hshift = - sin(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rr-rl, lpgw->xmax) / 2;
				vshift = - cos(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rb-rt, lpgw->ymax) / 2;
			}
			break;

		case W_justify:
			switch (curptr->x) {
				case LEFT:
					stringFormat.SetAlignment(StringAlignmentNear);
					break;
				case RIGHT:
					stringFormat.SetAlignment(StringAlignmentFar);
					break;
				case CENTRE:
					stringFormat.SetAlignment(StringAlignmentCenter);
					break;
			}
			lpgw->justify = curptr->x;
			break;

		case W_font: {
			int size = curptr->x;
			char * fontname = (char *) LocalLock(curptr->htext);
			delete font;
			font = SetFont_gdiplus(graphics, rect, lpgw, fontname, size);
			LocalUnlock(curptr->htext);
			/* recalculate shifting of rotated text */
			hshift = - sin(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rr-rl, lpgw->xmax) / 2;
			vshift = - cos(M_PI/180. * lpgw->angle) * MulDiv(lpgw->vchar, rb-rt, lpgw->ymax) / 2;
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
			solid_pen.SetWidth(line_width);
			pen.SetWidth(line_width);
			/* invalidate point symbol cache */
			last_symbol = 0;
			break;

		case W_setcolor: {
			COLORREF color;

			/* distinguish gray values and RGB colors */
			if (curptr->htext != NULL) {	/* TC_LT */
				int pen = (int)curptr->x % WGNUMPENS;
				color = (pen <= LT_NODRAW) ? lpgw->background : lpgw->colorpen[pen + 2].lopnColor;
				if (!lpgw->color || !isColor) {
					unsigned luma = luma_from_color(GetRValue(color), GetGValue(color), GetBValue(color));
					color = RGB(luma, luma, luma);
				}
				alpha_c = 1.;
			} else {					/* TC_RGB */
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

			/* update brushes and pens */
			Color pcolor = gdiplusCreateColor(color, alpha_c);
			solid_brush.SetColor(pcolor);
			pen.SetColor(pcolor);
			solid_pen.SetColor(pcolor);

			/* invalidate point symbol cache */
			if (last_color != color)
				last_symbol = 0;

			/* remember this color */
			cur_penstruct.lopnColor = color;
			last_color = color;
			break;
		}

		case W_filled_polygon_pt: {
			/* a point of the polygon is coming */
			if (polyi >= polymax) {
				polymax += 200;
				ppt = (POINT *) LocalReAllocPtr(ppt, LHND, (polymax + 1) * sizeof(POINT));
			}
			ppt[polyi].x = xdash;
			ppt[polyi].y = ydash;
			polyi++;
			break;
		}

		case W_filled_polygon_draw: {
			bool found = false;
			int i, k;
			bool same_rot = true;

			// Test if successive polygons share a common edge:
			if ((last_poly != NULL) && (polyi > 2)) {
				// Check for a common edge with previous filled polygon.
				for (i = 0; (i < polyi) && !found; i++) {
					for (k = 0; (k < last_polyi) && !found; k++) {
						if ((ppt[i].x == last_poly[k].x) && (ppt[i].y == last_poly[k].y)) {
							if ((ppt[(i + 1) % polyi].x == last_poly[(k + 1) % last_polyi].x) &&
							    (ppt[(i + 1) % polyi].y == last_poly[(k + 1) % last_polyi].y)) {
								//found = true;
								same_rot = true;
							}
							// This is the dominant case for filling between curves,
							// see fillbetween.dem and polar.dem.
							if ((ppt[(i + 1) % polyi].x == last_poly[(k + last_polyi - 1) % last_polyi].x) &&
							    (ppt[(i + 1) % polyi].y == last_poly[(k + last_polyi - 1) % last_polyi].y)) {
								found = true;
								same_rot = false;
							}
						}
					}
				}
			}

			if (found) { // merge polygons
				// rewind
				i--; k--;

				int extra = polyi - 2;
				// extend buffer to make room for extra points
				last_poly = (POINT *) realloc(last_poly, (last_polyi + extra + 1) * sizeof(POINT));
				/* TODO: should use memmove instead */
				for (int n = last_polyi - 1; n >= k; n--) {
					last_poly[n + extra].x = last_poly[n].x;
					last_poly[n + extra].y = last_poly[n].y;
				}
				// copy new points
				for (int n = 0; n < extra; n++) {
					last_poly[k + n].x = ppt[(i + 2 + n) % polyi].x;
					last_poly[k + n].y = ppt[(i + 2 + n) % polyi].y;
				}
				last_polyi += extra;
			} else {
				if (last_poly != NULL) {
					SmoothingMode mode = graphics.GetSmoothingMode();
					if (lpgw->antialiasing && !lpgw->polyaa)
						graphics.SetSmoothingMode(SmoothingModeNone);
					gdiplusFilledPolygon(graphics, *fill_brush, last_poly, last_polyi);
					graphics.SetSmoothingMode(mode);
					free(last_poly);
				}
				// save the current polygon
				last_poly = (POINT *) malloc(sizeof(POINT) * (polyi + 1));
				memcpy(last_poly, ppt, sizeof(POINT) * (polyi + 1));
				last_polyi = polyi;
			}

			polyi = 0;
			break;
		}

		case W_image:	{
			/* Due to the structure of gwop 6 entries are needed in total. */
			if (seq == 0) {
				/* First OP contains only the color mode */
				color_mode = curptr->x;
			} else if (seq < 5) {
				/* Next four OPs contain the `corner` array */
				corners[seq - 1].x = xdash;
				corners[seq - 1].y = ydash;
			} else {
				/* The last OP contains the image and it's size */
				char * image = (char *) LocalLock(curptr->htext);
				unsigned int width = curptr->x;
				unsigned int height = curptr->y;
#ifndef USE_GDIP_IMAGES
		 		HDC hdc = graphics.GetHDC(); /* switch back to GDI */
				draw_image(lpgw, hdc, image, corners, width, height, color_mode);
				graphics.ReleaseHDC(hdc); /* switch back to GDI+ */
#else
				if (image) {
					Bitmap * bitmap;

					/* With GDI+ interpolation of images cannot be avoided.
					Try to keep it simple at least: */
					graphics.SetInterpolationMode(InterpolationModeBilinear);
					SmoothingMode mode = graphics.GetSmoothingMode();
					graphics.SetSmoothingMode(SmoothingModeNone);

					/* create clip region */
					Rect clipRect(
						GPMIN(corners[2].x, corners[3].x), GPMIN(corners[2].y, corners[3].y),
						GPMAX(corners[2].x, corners[3].x) + 1, GPMAX(corners[2].y, corners[3].y) + 1);
					graphics.SetClip(clipRect);

					if (color_mode != IC_RGBA) {
						int pad_bytes = (4 - (3 * width) % 4) % 4; /* scan lines start on ULONG boundaries */
						int stride = width * 3 + pad_bytes;
						bitmap = new Bitmap(width, height, stride, PixelFormat24bppRGB, (BYTE *) image);
					} else {
						int stride = width * 4;
						bitmap = new Bitmap(width, height, stride, PixelFormat32bppARGB, (BYTE *) image);
					}

					if (bitmap) {
						/* image is upside-down */
						bitmap->RotateFlip(RotateNoneFlipY);
						if (lpgw->color) {
							graphics.DrawImage(bitmap,
								(INT) GPMIN(corners[0].x, corners[1].x),
								(INT) GPMIN(corners[0].y, corners[1].y),
								abs(corners[1].x - corners[0].x),
								abs(corners[1].y - corners[0].y));
						} else {
							/* convert to grayscale */
							ColorMatrix cm = {{{0.30f, 0.30f, 0.30f, 0, 0},
											   {0.59f, 0.59f, 0.59f, 0, 0},
											   {0.11f, 0.11f, 0.11f, 0, 0},
											   {0, 0, 0, 1, 0},
											   {0, 0, 0, 0, 1}
											 }};
							ImageAttributes ia;
							ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
							graphics.DrawImage(bitmap,
								RectF((INT) GPMIN(corners[0].x, corners[1].x),
									(INT) GPMIN(corners[0].y, corners[1].y),
									abs(corners[1].x - corners[0].x),
									abs(corners[1].y - corners[0].y)),
								0, 0, width, height,
								UnitPixel, &ia);
						}
						delete bitmap;
					}
					graphics.ResetClip();
					graphics.SetSmoothingMode(mode);
				}
#endif
				LocalUnlock(curptr->htext);
			}
			seq = (seq + 1) % 6;
			break;
		}

		default: {
			/* This covers only point symbols. All other codes should be
			   handled in the switch statement. */
			if ((curptr->op < W_dot) || (curptr->op > W_dot + WIN_POINT_TYPES))
				break;

			// draw cached point symbol
			if ((last_symbol == curptr->op) && (cb != NULL)) {
				graphics.DrawCachedBitmap(cb, xdash - cb_ofs.x, ydash - cb_ofs.y);
				break;
			} else {
				if (cb != NULL) {
					delete cb;
					cb = NULL;
				}
			}

			Bitmap *b = 0;
			Graphics *g = 0;
			int xofs;
			int yofs;

			// Switch between cached and direct drawing
			if (ps_caching) {
				// Create a compatible bitmap
				b = new Bitmap(2*htic+3, 2*vtic+3);
				g = Graphics::FromImage(b);
				if (lpgw->antialiasing)
					g->SetSmoothingMode(SmoothingModeAntiAlias8x8);
				cb_ofs.x = xofs = htic+1;
				cb_ofs.y = yofs = vtic+1;
				last_symbol = curptr->op;
			} else {
				g = &graphics;
				xofs = xdash;
				yofs = ydash;
			}

			switch (curptr->op) {
			case W_dot:
				gdiplusDot(*g, solid_brush, xofs, yofs);
				break;
			case W_plus: /* do plus */
			case W_star: /* do star: first plus, then cross */
				g->DrawLine(&solid_pen, xofs - htic, yofs, xofs + htic, yofs);
				g->DrawLine(&solid_pen, xofs, yofs - vtic, xofs, yofs + vtic);
				if (curptr->op == W_plus) break;
			case W_cross: /* do X */
				g->DrawLine(&solid_pen, xofs - htic, yofs - vtic, xofs + htic - 1, yofs + vtic);
				g->DrawLine(&solid_pen, xofs - htic, yofs + vtic, xofs + htic - 1, yofs - vtic);
				break;
			case W_circle: /* do open circle */
				g->DrawEllipse(&solid_pen, xofs - htic, yofs - htic, 2 * htic, 2 * htic);
				break;
			case W_fcircle: /* do filled circle */
				g->FillEllipse(&solid_brush, xofs - htic, yofs - htic, 2 * htic, 2 * htic);
				break;
			default: {	/* potentially closed figure */
				POINT p[6];
				int i;
				int shape = 0;
				int filled = 0;
				int index = 0;
				const float pointshapes[6][10] = {
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

				// This should never happen since all other codes should be
				// handled in the switch statement.
				if ((curptr->op < W_box) || (curptr->op > W_fpentagon))
					break;

				// Calculate index, instead of an ugly long switch statement;
				// Depends on definition of commands in wgnuplib.h.
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
					/* filled polygon with border */
					gdiplusFilledPolygon(*g, solid_brush, p, i);
				} else {
					/* Outline polygon */
					p[i].x = p[0].x;
					p[i].y = p[0].y;
					gdiplusPolyline(*g, solid_pen, p, i + 1);
					gdiplusDot(*g, solid_brush, xofs, yofs);
				}
			} /* default case */
			} /* switch (point symbol) */

			if (b != NULL) {
				// create a chached bitmap for faster redrawing
				cb = new CachedBitmap(b, &graphics);
				// display bitmap
				graphics.DrawCachedBitmap(cb, xdash - xofs, ydash - yofs);
				delete b;
				delete g;
			}

			if (keysample) {
				draw_update_keybox(lpgw, plotno, xdash + htic, ydash + vtic);
				draw_update_keybox(lpgw, plotno, xdash - htic, ydash - vtic);
			}
			break;
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
				break;
			if (!blkptr->gwop)
				blkptr->gwop = (struct GWOP *)GlobalLock(blkptr->hblk);
			if (!blkptr->gwop)
				break;
			curptr = (struct GWOP *)blkptr->gwop;
		}
	}
	if (polyi >= 2) {
		gdiplusPolyline(graphics, pen, ppt, polyi);
	}
	/* clean-up */
	if (pattern_brush)
		delete pattern_brush;
	if (cb)
		delete cb;
	if (font)
		delete font;
	LocalFreePtr(ppt);
}
