/*
 * $Id: gp_cairo.c,v 1.87.2.9 2016/07/08 20:48:34 sfeam Exp $
 */

/* GNUPLOT - gp_cairo.c */

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
 * This code uses the cairo library, a 2D graphics library with
 * support for multiple output devices.
 * Cairo is distributed under the LGPL licence.
 *
 * See http://www.cairographics.org for details.

 * It also uses the pango library, a text-layout rendering library.
 * Pango is distributed under the LGPL licence.
 *
 * See http://www.pango.org for details.
 * -----------------------------------------------------*/

/* ------------------------------------------------------
 * This file implements all cairo related functions,
 * which provide drawing facilities.
 *
 * In particular, we have here :
 * - all the basic calls (lines, polygons for pm3d, custom patterns),
 * - image support,
 * - enhanced text mode
 *
 * The text rendering is done via pango.
 * ------------------------------------------------------*/

#include "gp_cairo.h"

#include "alloc.h"

#include <pango/pangocairo.h>
#include <glib.h>

#ifdef _MSC_VER
#define rint(x) floor((x)+0.5L)
#endif

/* undef this to see what happens without the Symbol-to-unicode processing */
#define MAP_SYMBOL

/* ========  enhanced text mode ======== */
/* copies of internal variables */
static char gp_cairo_enhanced_font[100] = "";
static const char* gp_cairo_enhanced_get_fontname(plot_struct *plot);
static double gp_cairo_enhanced_fontsize = 0;
static double gp_cairo_enhanced_base = 0;
static TBOOLEAN gp_cairo_enhanced_widthflag = TRUE;
static TBOOLEAN gp_cairo_enhanced_showflag = TRUE;
static int gp_cairo_enhanced_overprint = FALSE;
static TBOOLEAN gp_cairo_enhanced_opened_string  = FALSE;  /* try to cut out empty ()'s */
static char *gp_cairo_enhanced_string;
static char *gp_cairo_enhanced_char;
/* utf8 text to draw and its attributes */
static gchar gp_cairo_utf8[2048] = "";
static PangoAttrList *gp_cairo_enhanced_AttrList = NULL;
/* save/restore facilitiy */
static TBOOLEAN gp_cairo_enhanced_restore_now = FALSE;
static TBOOLEAN gp_cairo_enhanced_save = FALSE;
static gchar gp_cairo_save_utf8[2048] = "";
static PangoAttrList *gp_cairo_enhanced_save_AttrList = NULL;
/* underprint/overprint facility */
static gchar gp_cairo_underprinted_utf8[2048] = "";
static PangoAttrList *gp_cairo_enhanced_underprinted_AttrList = NULL;
/* converts text from symbol encoding to utf8 encoding */
static gchar* gp_cairo_convert_symbol_to_unicode(plot_struct *plot, const char* string);
/* add standard attributes (fontsize,fontfamily, rise) to
 * the specified characters in a PangoAttrList */
static void gp_cairo_add_attr(plot_struct *plot, PangoAttrList * AttrList, int start, int end );
/* add a blank character to the text string and an associated custom shape to the attribute list */
static void gp_cairo_add_shape( PangoRectangle rect,int position);

/* Average character height as reported back through term->v_char */
static int avg_vchar = 150;

/* set a cairo pattern or solid fill depending on parameters */
static void gp_cairo_fill(plot_struct *plot, int fillstyle, int fillpar);
static void gp_cairo_fill_pattern(plot_struct *plot, int fillstyle, int fillpar);

/* array of colors
 * FIXME could be shared with all gnuplot terminals */
static rgb_color gp_cairo_colorlist[12] = {
{1,1,1}, /* white */
{0,0,0}, /* black */
{0,0,0}, /* black */
{1,0,0}, /* red */
{0,1,0}, /* green */
{0,0,1}, /* blue */
{1,0,1}, /* magenta */
{0,1,1}, /* cyan */
{1,1,0}, /* yellow */
{0,0,0}, /* black */
{1,0.3,0}, /* orange */
{0.5,0.5,0.5} /* grey */
};

/* correspondance between gnuplot linetypes and terminal colors */
void gp_cairo_set_background( rgb_color background )
{
	gp_cairo_colorlist[0] = background;
}

rgb_color gp_cairo_linetype2color( int linetype )
{
	if (linetype<=LT_NODRAW)
		return gp_cairo_colorlist[ 0 ];
	else
		return gp_cairo_colorlist[ linetype%9 +3 ];
}

/* initialize all fields of the plot structure */
void gp_cairo_initialize_plot(plot_struct *plot)
{
	plot->xscale = 1.0; plot->yscale = 1.0;
	plot->device_xmax = 1; plot->device_ymax = 1;
	plot->xmax = 1; plot->ymax = 1;

	plot->justify_mode = LEFT;
	plot->linetype = 1;
	plot->linewidth = 1.0;
	plot->linestyle = GP_CAIRO_SOLID;
	plot->pointsize = 1.0;
	plot->dashlength = 1.0;
	plot->text_angle = 0.0;
	plot->color.r = 0.0; plot->color.g = 0.0; plot->color.b = 0.0;
	plot->background.r = 1.0; plot->background.g = 1.0; plot->background.b = 1.0;

	plot->opened_path = FALSE;

	plot->current_x = -1; plot->current_y = -1;

	strncpy(plot->fontname, "", sizeof(plot->fontname));
	plot->fontsize = 1.0;
	plot->encoding = S_ENC_DEFAULT;

	plot->success = FALSE;

	plot->antialiasing = TRUE;

	plot->oversampling = TRUE;
	plot->oversampling_scale = GP_CAIRO_SCALE;

	plot->linecap = BUTT;

	plot->hinting = 100;

	plot->polygons_saturate = TRUE;

	plot->cr = NULL;

	plot->polygon_path_last = NULL;

	plot->interrupt = FALSE;
}

#ifdef EAM_BOXED_TEXT
/* Boxed text support */
static int bounding_box[4];
static double bounding_xmargin = 1.0;
static double bounding_ymargin = 1.0;
#endif

/* set the transformation matrix of the context, and other details */
/* NOTE : depends on the setting of xscale and yscale */
void gp_cairo_initialize_context(plot_struct *plot)
{
	cairo_matrix_t matrix;

	if (plot->oversampling)
		plot->oversampling_scale = GP_CAIRO_SCALE;
	else
		plot->oversampling_scale = 1;

	if (plot->antialiasing)
		cairo_set_antialias(plot->cr,CAIRO_ANTIALIAS_DEFAULT);
	else
		cairo_set_antialias(plot->cr,CAIRO_ANTIALIAS_NONE);

	cairo_matrix_init(&matrix,
			plot->xscale/plot->oversampling_scale,
			0, 0,
			plot->yscale/plot->oversampling_scale,
			0.5, 0.5);
	cairo_set_matrix(plot->cr, &matrix);

	/* Default is square caps, mitered joins */
	if (plot->linecap == ROUNDED) {
	    cairo_set_line_cap  (plot->cr, CAIRO_LINE_CAP_ROUND);
	    cairo_set_line_join (plot->cr, CAIRO_LINE_JOIN_ROUND);
	} else if (plot->linecap == SQUARE) {
	    cairo_set_line_cap  (plot->cr, CAIRO_LINE_CAP_SQUARE);
	    cairo_set_line_join (plot->cr, CAIRO_LINE_JOIN_MITER);
	    cairo_set_miter_limit(plot->cr, 3.8);
	} else {
	    cairo_set_line_cap  (plot->cr, CAIRO_LINE_CAP_BUTT);
	    cairo_set_line_join (plot->cr, CAIRO_LINE_JOIN_MITER);
	    cairo_set_miter_limit(plot->cr, 3.8);
	}

}


void gp_cairo_set_color(plot_struct *plot, rgb_color color, double alpha)
{
	/*stroke any open path */
	gp_cairo_stroke(plot);

	FPRINTF((stderr,"set_color %lf %lf %lf\n",color.r, color.g, color.b));

	plot->color.r = color.r;
	plot->color.g = color.g;
	plot->color.b = color.b;
	plot->color.alpha = alpha;
}


void gp_cairo_set_linestyle(plot_struct *plot, int linestyle)
{
	/*stroke any open path */
	gp_cairo_stroke(plot);
	/* draw any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"set_linestyle %d\n",linestyle));

	plot->linestyle = linestyle;
}


void gp_cairo_set_linetype(plot_struct *plot, int linetype)
{
	/*stroke any open path */
	gp_cairo_stroke(plot);
	/* draw any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"set_linetype %d\n",linetype));

	plot->linetype = linetype;
}


void gp_cairo_set_pointsize(plot_struct *plot, double pointsize)
{
	FPRINTF((stderr,"set_pointsize %lf\n",pointsize));

	plot->pointsize = pointsize;
}


void gp_cairo_set_justify(plot_struct *plot, JUSTIFY mode)
{
	FPRINTF((stderr,"set_justify\n"));

	plot->justify_mode = mode;
}


void gp_cairo_set_font(plot_struct *plot, const char *name, int fontsize)
{
    char *c;
    char *fname;

	FPRINTF((stderr,"set_font \"%s\" %d\n", name,fontsize));

	/* Split out Bold and Italic attributes from font name */
	fname = strdup(name);
	for (c=fname; *c; c++) {
	    if (*c == '\\') {
		char *d = c;
		do { *d = *(d+1); } while (*d++);
	    } else {
		if (*c == '-') *c = ' ';
		if (*c == ':') *c = ' ';
	    }
	}
	if ((c = strstr(fname, " Bold"))) {
	    do { *c = *(c+5); } while (*c++);
	    plot->fontweight = PANGO_WEIGHT_BOLD;
	} else
	    plot->fontweight = PANGO_WEIGHT_NORMAL;
	if ((c = strstr(fname, " Italic"))) {
	    do { *c = *(c+7); } while (*c++);
	    plot->fontstyle = PANGO_STYLE_ITALIC;
	} else
	    plot->fontstyle = PANGO_STYLE_NORMAL;

	strncpy( plot->fontname, fname, sizeof(plot->fontname) );
	plot->fontsize = fontsize;
	free(fname);
}


void gp_cairo_set_linewidth(plot_struct *plot, double linewidth)
{
	FPRINTF((stderr,"set_linewidth %lf\n",linewidth));

	/*stroke any open path */
	gp_cairo_stroke(plot);
	/* draw any open polygon set */
	gp_cairo_end_polygon(plot);

	if (!strcmp(term->name,"pdfcairo"))
	    linewidth *= 2;
	if (linewidth < 0.20)	/* Admittedly arbitrary */
	    linewidth = 0.20;

	plot->linewidth = linewidth;
}


void gp_cairo_set_textangle(plot_struct *plot, double angle)
{
	FPRINTF((stderr,"set_textangle %lf\n",angle));

	plot->text_angle =angle;
}

/* By default, Cairo uses an antialiasing algorithm which may
 * leave a seam between polygons which share a common edge.
 * Several solutions allow to workaround this behaviour :
 * - don't antialias the polygons
 *   Problem : aliased lines are ugly
 * - stroke on each edge
 *   Problem : stroking is a very time-consuming operation
 * - draw without antialiasing to a separate context of a bigger size
 *   Problem : not really in the spirit of the rest of the drawing.
 * - enlarge the polygons so that they overlap slightly
 *   Problem : It is really more time-consuming that it may seem.
 *   It implies inspecting each corner to find which direction to move it
 *   (making the difference between the inside and the outside of the polygon).
 * - using CAIRO_OPERATOR_SATURATE
 *   Problem : for each set of polygons, we have to draw front-to-back
 *   on a separate context and then copy back to this one.
 *   Time-consuming but probably less than stroking all the edges.
 *
 * The last solution is implemented if plot->polygons_saturate is set to TRUE
 * Otherwise the default (antialiasing but may have seams) is used.
 */

void gp_cairo_draw_polygon(plot_struct *plot, int n, gpiPoint *corners)
{
	/* begin by stroking any open path */
	gp_cairo_stroke(plot);

	if (plot->polygons_saturate) {
		int i;
		path_item *path;

		path = (path_item*) gp_alloc(sizeof(path_item), "gp_cairo : polygon path");

		path->n = n;
		path->corners = (gpiPoint*) gp_alloc(n*sizeof(gpiPoint), "gp_cairo : polygon corners");
		for(i=0;i<n;i++)
			*(path->corners + i) = *corners++;

		path->color = plot->color;

		if (plot->polygon_path_last == NULL) {
			FPRINTF((stderr,"creating a polygon path\n"));
			path->previous = NULL;
			plot->polygon_path_last = path;
		} else {
			FPRINTF((stderr,"adding a polygon to the polygon path\n"));
			path->previous = plot->polygon_path_last;
			plot->polygon_path_last = path;
		}
	} else {
		int i;
		/* draw the polygon directly */
		FPRINTF((stderr,"processing one polygon\n"));
		cairo_move_to(plot->cr, corners[0].x, corners[0].y);
		for (i=1;i<n;++i)
			cairo_line_to(plot->cr, corners[i].x, corners[i].y);
		cairo_close_path(plot->cr);
		gp_cairo_fill( plot, corners->style & 0xf, corners->style >> 4 );
		cairo_fill(plot->cr);
	}
}


void gp_cairo_end_polygon(plot_struct *plot)
{
	int i;
	path_item *path;
	path_item *path2;
	rgba_color color_sav;
	cairo_t *context;
	cairo_t *context_sav;
	cairo_surface_t *surface;
	cairo_matrix_t matrix;
	cairo_matrix_t matrix2;
	cairo_pattern_t *pattern;

	/* when we are not using OPERATOR_SATURATE, the polygons are drawn
	 * directly in gp_cairo_draw_polygon */
	if (!plot->polygons_saturate)
		return;

	if (plot->polygon_path_last == NULL)
		return;

	path = plot->polygon_path_last;
	color_sav = plot->color;

	/* if there's only one polygon, draw it directly */
	if (path->previous == NULL) {
		FPRINTF((stderr,"processing one polygon\n"));
		cairo_move_to(plot->cr, path->corners[0].x, path->corners[0].y);
		for (i=1;i<path->n;++i)
			cairo_line_to(plot->cr, path->corners[i].x, path->corners[i].y);
		cairo_close_path(plot->cr);
		plot->color = path->color;
		gp_cairo_fill( plot, path->corners->style & 0xf, path->corners->style >> 4 );
		cairo_fill(plot->cr);
		free(path->corners);
		free(path);
		plot->polygon_path_last = NULL;
		plot->color = color_sav;
		return;
	}

	FPRINTF((stderr,"processing several polygons\n"));

/* this is meant to test Full-Scene-Anti-Aliasing by supersampling,
 * in association with CAIRO_ANTIALIAS_NONE a few lines below */
#define SCALE 1

	/* otherwise, draw front-to-back to a separate context,
	 * using CAIRO_OPERATOR_SATURATE */
	context_sav = plot->cr;
	surface = cairo_surface_create_similar(cairo_get_target(plot->cr),
                                             CAIRO_CONTENT_COLOR_ALPHA,
                                             plot->device_xmax*SCALE,
                                             plot->device_ymax*SCALE);
	context = cairo_create(surface);
	cairo_set_operator(context,CAIRO_OPERATOR_SATURATE);
	if (plot->antialiasing)
		cairo_set_antialias(context,CAIRO_ANTIALIAS_DEFAULT);
	else
		cairo_set_antialias(context,CAIRO_ANTIALIAS_NONE);

	/* transformation matrix between gnuplot and cairo coordinates */
	cairo_matrix_init(&matrix,
			plot->xscale/SCALE/plot->oversampling_scale,
			0,0,
			plot->yscale/SCALE/plot->oversampling_scale,
			0.5,0.5);
	cairo_set_matrix(context, &matrix);

 	plot->cr = context;
	path = plot->polygon_path_last;

	while (path != NULL) {
		/* check for interrupt */
		if (plot->interrupt)
			break;
		/* build the path */
		cairo_move_to(plot->cr, path->corners[0].x, path->corners[0].y);
		for (i=1;i<(path->n);++i)
			cairo_line_to(plot->cr, path->corners[i].x, path->corners[i].y);
		cairo_close_path(plot->cr);
		/* set the fill pattern */
		plot->color = path->color;
		gp_cairo_fill( plot, path->corners->style & 0xf, path->corners->style >> 4 );
		cairo_fill(plot->cr);
		/* free the ressources, and go to the next point */
		free(path->corners);
		path2 = path->previous;
		free(path);
		path = path2;
	}

	plot->polygon_path_last = NULL;

	pattern = cairo_pattern_create_for_surface( surface );
	cairo_destroy( context );

	/* compensate the transformation matrix of the main context */
	cairo_matrix_init(&matrix2,
			plot->xscale*SCALE/plot->oversampling_scale,
			0,0,
			plot->yscale*SCALE/plot->oversampling_scale,
			0.5,0.5);
	cairo_pattern_set_matrix( pattern, &matrix2 );

	plot->cr = context_sav;
	plot->color = color_sav;
	cairo_surface_destroy( surface );
	cairo_set_source( plot->cr, pattern );
	cairo_pattern_destroy( pattern );
	cairo_paint( plot->cr );
}

void gp_cairo_set_dashtype(plot_struct *plot, int type, t_dashtype *custom_dash_type)
{
	static double dashpattern[4][8] =
	{
	    {5, 8, 5, 8, 5, 8, 5, 8},	/* Medium dash */
	    {1, 4, 1, 4, 1, 4, 1, 4},	/* dots */
	    {8, 4, 2, 4, 8, 4, 2, 4},	/* dash dot */
	    {9, 4, 1, 4, 1, 4, 0, 0}	/* dash dot dot */
	};
	int lt = (type) % 5;

	if (type == DASHTYPE_CUSTOM && custom_dash_type) {
		/* Convert to internal representation */
		int i;
		double empirical_scale;

		if (!strcmp(term->name,"pngcairo"))
			empirical_scale = 0.25;
		else
			empirical_scale = 0.55;

		if (plot->linewidth > 1)
			empirical_scale *= plot->linewidth;

		for (i=0; i<8; i++)
			plot->current_dashpattern[i] = custom_dash_type->pattern[i]
				* plot->dashlength
				* plot->oversampling_scale * empirical_scale;
		gp_cairo_set_linestyle(plot, GP_CAIRO_DASH);

	} else if (type > 0 && lt != 0) {
		/* Use old (version 4) set of linetype patterns */
		int i;
		double empirical_scale = 1.;
		if (plot->linewidth > 1)
			empirical_scale *= plot->linewidth;

		for (i=0; i<8; i++)
			plot->current_dashpattern[i] = dashpattern[lt-1][i]
				* plot->dashlength
				* plot->oversampling_scale
				* empirical_scale;
		gp_cairo_set_linestyle(plot, GP_CAIRO_DASH);

	} else {
		/* Every 5th pattern in the old set is solid */
		gp_cairo_set_linestyle(plot, GP_CAIRO_SOLID);
	}
}

void gp_cairo_stroke(plot_struct *plot)
{
	int lt = plot->linetype;
	double lw = plot->linewidth * plot->oversampling_scale;

	if (!plot->opened_path) {
		FPRINTF((stderr,"stroke with non-opened path !\n"));
		return;
	}

	/* add last point */
	cairo_line_to (plot->cr, plot->current_x, plot->current_y);


	cairo_save(plot->cr);

	if (plot->linetype == LT_NODRAW) {
		cairo_set_operator(plot->cr, CAIRO_OPERATOR_XOR);

	} else if (lt == LT_AXIS || plot->linestyle == GP_CAIRO_DOTS) {
		/* Grid lines (lt 0) */
		double dashes[2];
		double empirical_scale = 1.0;
		if (plot->linewidth > 1)
			empirical_scale *= plot->linewidth;
		dashes[0] = 0.4 * plot->oversampling_scale * plot->dashlength * empirical_scale;
		dashes[1] = 4.0 * plot->oversampling_scale * plot->dashlength * empirical_scale;
		cairo_set_dash(plot->cr, dashes, 2 /*num_dashes*/, 0 /*offset*/);
	}

	else if (plot->linestyle == GP_CAIRO_DASH) {
		cairo_set_dash(plot->cr, &(plot->current_dashpattern[0]), 8 /*num_dashes*/, 0 /*offset*/);
	}

	cairo_set_source_rgba(plot->cr, plot->color.r, plot->color.g, plot->color.b,
				1. - plot->color.alpha);
	cairo_set_line_width(plot->cr, lw);

	cairo_stroke(plot->cr);

	cairo_restore(plot->cr);

	plot->opened_path = FALSE;
}


void gp_cairo_move(plot_struct *plot, int x, int y)
{
	/* Dec 2014 - Do not let zero-length moves interrupt     */
	/* the current line/polyline context, e.g. dash pattern. */
	if (x == plot->current_x && y == plot->current_y)
		return;

	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);

	plot->current_x = x;
	plot->current_y = y;
	plot->orig_current_x = x;
	plot->orig_current_y = y;
}


void gp_cairo_vector(plot_struct *plot, int x, int y)
{
	double x1 = x, y1 = y;
	double new_pos;
	double weight1 = (double) plot->hinting/100;
	double weight2 = 1.0 - weight1;

	/* begin by drawing any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"vector\n"));

	/* hinting magic when we are using antialiasing+oversampling */
	if (plot->antialiasing && plot->oversampling) {
		if (plot->hinting < 0 || plot->hinting > 100) {
			fprintf(stderr,"wxt terminal : hinting error, setting to default\n");
			plot->hinting = 100;
		}

		/* detect and handle vertical lines */
		/* the second test is there to avoid artefacts when you choose
		* a high sampling ('set samples 10000'), so that a smooth function
		* may be drawn as lines between very close points */
		if (plot->orig_current_x == x1 && fabs(plot->orig_current_y - y1)>plot->oversampling_scale) {
			new_pos = rint(plot->current_x*plot->xscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->xscale;
			plot->current_x = weight1*new_pos + weight2*plot->current_x;
			x1 = plot->current_x;
			new_pos = rint(plot->current_y*plot->yscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->yscale;
			plot->current_y = weight1*new_pos + weight2*plot->current_y;
			new_pos = rint(y1*plot->yscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->yscale;
			y1 = weight1*new_pos + weight2*y1;
		}
		/* do the same for horizontal lines */
		if (plot->orig_current_y == y1 && fabs(plot->orig_current_x - x1)>plot->oversampling_scale) {
			new_pos = rint(plot->current_y*plot->yscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->yscale;
			plot->current_y = weight1*new_pos + weight2*plot->current_y;
			y1 = plot->current_y;
			new_pos = rint(plot->current_x*plot->xscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->xscale;
			plot->current_x = weight1*new_pos + weight2*plot->current_x;
			new_pos = rint(x1*plot->xscale/plot->oversampling_scale);
			new_pos *= plot->oversampling_scale/plot->xscale;
			x1 = weight1*new_pos + weight2*x1;
		}
	}

	if (!plot->opened_path) {
		plot->opened_path = TRUE;
		cairo_move_to (plot->cr, plot->current_x, plot->current_y);
	} else
		cairo_line_to (plot->cr, plot->current_x, plot->current_y);

	plot->current_x = x1;
	plot->current_y = y1;
	plot->orig_current_x = x;
	plot->orig_current_y = y;
}

/* pango needs a string encoded in utf-8. We use g_convert from glib.
 * gp_cairo_get_encoding() gives the encoding set via 'set enconding'
 * memory allocated for the string has to be freed */
static gchar * gp_cairo_convert(plot_struct *plot, const char* string)
{
	gsize bytes_read;
	GError *error = NULL;
	const char *charset = NULL;
	gchar * string_utf8;

	if (g_utf8_validate(string, -1, NULL)) {
	    string_utf8 = g_strdup(string);
	} else {
	    charset = gp_cairo_get_encoding(plot);
	    string_utf8 = g_convert(string, -1, "UTF-8", charset, &bytes_read, NULL, &error);
	}

	/* handle error case */
	if (error != NULL) {
		/* fatal error in conversion */
		if (error->code != G_CONVERT_ERROR_ILLEGAL_SEQUENCE) {
			fprintf(stderr, "Unable to convert \"%s\": %s\n", string, error->message);
			g_error_free (error);
			return strdup("");
		}
		/* The sequence is invalid in the chosen charset.
		 * we will try to fall back to iso_8859_1, and if it doesn't work,
		 * we'll use bytes_read to convert up to the faulty character,
		 * and throw the rest. */
		g_error_free (error);
		error = NULL;
		string_utf8 = g_convert(string, -1, "UTF-8", "ISO-8859-1", NULL, NULL, &error);
		if (error != NULL) {
			fprintf(stderr, "Unable to convert \"%s\": the sequence is invalid "\
				"in the current charset (%s), %d bytes read out of %d\n",
				string, charset, (int)bytes_read, (int)strlen(string));
			string_utf8 = g_convert(string, bytes_read, "UTF-8", charset, NULL, NULL, NULL);
			g_error_free (error);
		} else
			fprintf(stderr, "Unable to convert \"%s\": the sequence is invalid "\
				"in the current charset (%s), falling back to iso_8859_1\n",
				string, charset);
	}

	return string_utf8;
}

/*
 * The following #ifdef WIN32 section is all to work around a bug in
 * the cairo/win32 backend for font rendering.  It has the effect of
 * testing for libfreetype support, and using that instead if possible.
 * Suggested by cairo developer Behdad Esfahbod.
 * Allin Cottrell suggests that this not necessary anymore for newer
 * versions of cairo.
 */
#if defined(WIN32) && (CAIRO_VERSION_MAJOR < 2) && (CAIRO_VERSION_MINOR < 10)
static PangoLayout *
gp_cairo_create_layout(cairo_t *cr)
{
    static PangoFontMap *fontmap;
    PangoContext *context;
    PangoLayout *layout;

    if (fontmap == NULL) {
        fontmap = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
        if (fontmap == NULL) {
	    fontmap = pango_cairo_font_map_get_default();
        }
    }

#if PANGO_VERSION_MAJOR > 1 || PANGO_VERSION_MINOR >= 22
    context = pango_font_map_create_context(fontmap);
#else
    context = pango_cairo_font_map_create_context((PangoCairoFontMap *) fontmap);
#endif

    layout = pango_layout_new(context);
    g_object_unref(context);

    return layout;
}
#else
static PangoLayout *
gp_cairo_create_layout(cairo_t *cr)
{
    return pango_cairo_create_layout(cr);
}
#endif

void gp_cairo_draw_text(plot_struct *plot, int x1, int y1, const char* string,
		    int *width, int *height)
{
	double x,y;
	double arg = plot->text_angle * M_PI/180;
	double vert_just, delta, deltax, deltay;
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gchar* string_utf8;
#ifdef MAP_SYMBOL
	TBOOLEAN symbol_font_parsed = FALSE;
#endif /*MAP_SYMBOL*/
	int baseline_offset;


	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"draw_text\n"));

#ifdef MAP_SYMBOL
	/* we have to treat Symbol font as a special case */
	if (!strcmp(plot->fontname,"Symbol")) {
		FPRINTF((stderr,"Parsing a Symbol string\n"));
		string_utf8 = gp_cairo_convert_symbol_to_unicode(plot, string);
		strncpy(plot->fontname, gp_cairo_default_font(), sizeof(plot->fontname));
		symbol_font_parsed = TRUE;
	} else
#endif /*MAP_SYMBOL*/
	{
		/* convert the input string to utf8 */
		string_utf8 = gp_cairo_convert(plot, string);
	}

	/* Create a PangoLayout, set the font and text */
	layout = gp_cairo_create_layout (plot->cr);

	pango_layout_set_text (layout, string_utf8, -1);
	g_free(string_utf8);
	desc = pango_font_description_new ();
	pango_font_description_set_family (desc, (const char*) plot->fontname);
#ifdef MAP_SYMBOL
	/* restore the Symbol font setting */
	if (symbol_font_parsed)
		strncpy(plot->fontname, "Symbol", sizeof(plot->fontname));
#endif /*MAP_SYMBOL*/
	pango_font_description_set_size (desc, (int) (plot->fontsize*PANGO_SCALE*plot->oversampling_scale) );

	pango_font_description_set_weight (desc, plot->fontweight);
	pango_font_description_set_style (desc,
		plot->fontstyle ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
	pango_layout_set_font_description (layout, desc);
	FPRINTF((stderr, "pango font description: %s\n", pango_font_description_to_string(desc)));
	pango_font_description_free (desc);

	pango_layout_get_extents(layout, &ink_rect, &logical_rect);
	if (width)
		*width = logical_rect.width / PANGO_SCALE;
	if (height)
		*height = logical_rect.height / PANGO_SCALE;

	/* EAM Mar 2009 - Adjusting the vertical position for every character fragment	*/
	/* leads to uneven baselines. Better to adjust to the "average" character height */
	/* EAM Dec 2012 - The problem is that avg_vchar is not kept in sync with the	*/
	/* font size.  It is changed when the set_font command is received, not when	*/
	/* it is executed in the display list. Try basing off plot->fontsize instead. 	*/

	baseline_offset = pango_layout_get_baseline(layout) / PANGO_SCALE;
	vert_just = 0.5 * (float)(plot->fontsize * plot->oversampling_scale);
	vert_just = baseline_offset - vert_just;

	x = (double) x1;
	y = (double) y1;

	x -= vert_just * sin(arg);
	y -= vert_just * cos(arg);

	delta = ((double)logical_rect.width/2) / PANGO_SCALE;

	deltax = delta * cos(arg);
	deltay = delta * sin(arg);

	switch (plot->justify_mode) {
	case LEFT :
		break;
	case CENTRE :
		x -= deltax;
		y += deltay;
		break;
	case RIGHT :
		x -= 2*deltax;
		y += 2*deltay;
		break;
	}

#if 0 /* helper point */
	gp_cairo_draw_point(plot, x1, y1, 0);
#endif /* helper point */

	cairo_save (plot->cr);
	cairo_set_source_rgba(plot->cr, plot->color.r, plot->color.g, plot->color.b,
				1. - plot->color.alpha);
	cairo_move_to (plot->cr, x-0.5, y-0.5);
	cairo_rotate(plot->cr, -arg);

	/* Inform Pango to re-layout the text with the new transformation */
	pango_cairo_update_layout (plot->cr, layout);
	pango_cairo_show_layout (plot->cr, layout);
	/* pango_cairo_show_layout does not clear the path (here a starting point)
	 * Do it by ourselves, or we can get spurious lines on future calls. */
	cairo_new_path(plot->cr);

#ifdef EAM_BOXED_TEXT
	cairo_set_line_width(plot->cr, plot->linewidth*plot->oversampling_scale);
	cairo_rotate(plot->cr, arg);
	cairo_translate(plot->cr, x, y);
	cairo_rotate(plot->cr, -arg);

	{
	PangoRectangle ink, logical;
	pango_layout_get_pixel_extents (layout, &ink, &logical);

	/* Auto-initialization */
	if (bounding_box[0] < 0 && bounding_box[1] < 0) {
	    bounding_box[0] = bounding_box[2] = x;
	    bounding_box[1] = bounding_box[3] = y;
	}

	/* Would it look better to use logical bounds rather than ink? */
	if (bounding_box[0] > x + ink.x)
	    bounding_box[0] = x + ink.x;
	if (bounding_box[2] < x + ink.x + ink.width)
	    bounding_box[2] = x + ink.x + ink.width;
	if (bounding_box[1] > y + ink.y)
	    bounding_box[1] = y + ink.y;
	if (bounding_box[3] < y + ink.y + ink.height)
	    bounding_box[3] = y + ink.y + ink.height;
	}
#endif

	/* free the layout object */
	g_object_unref (layout);
	cairo_restore (plot->cr);
}


void gp_cairo_draw_point(plot_struct *plot, int x1, int y1, int style)
{
	double x = x1;
	double y = y1;
	double new_pos;
	double weight1 = (double) plot->hinting/100;
	double weight2 = 1.0 - weight1;
	double size = plot->pointsize*3*plot->oversampling_scale;

	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"drawpoint\n"));

	/* hinting magic when we are using antialiasing+oversampling */
	if (plot->antialiasing && plot->oversampling) {
		if (plot->hinting < 0 || plot->hinting > 100) {
			fprintf(stderr,"wxt terminal : hinting error, setting to default\n");
			plot->hinting = 100;
		}

		new_pos = rint(x*plot->xscale/plot->oversampling_scale);
		new_pos *= plot->oversampling_scale/plot->xscale;
		x = weight1*new_pos + weight2*x;

		new_pos = rint(y*plot->yscale/plot->oversampling_scale);
		new_pos *= plot->oversampling_scale/plot->yscale;
		y = weight1*new_pos + weight2*y;
	}

	cairo_save(plot->cr);
	cairo_set_line_width(plot->cr, plot->linewidth*plot->oversampling_scale);
	cairo_set_source_rgba(plot->cr, plot->color.r, plot->color.g, plot->color.b,
				1. - plot->color.alpha);

	/* Dot	FIXME: because this is drawn as a filled circle, it's quite slow */
	if (style < 0) {
		cairo_arc (plot->cr, x, y, 0.5*plot->oversampling_scale, 0, 2*M_PI);
		cairo_fill (plot->cr);
	}


	style = style % 15;
	switch (style) {
	case 0: /* plus */
		cairo_move_to(plot->cr, x-size, y);
		cairo_line_to(plot->cr, x+size,y);
		cairo_stroke(plot->cr);
		cairo_move_to(plot->cr, x, y-size);
		cairo_line_to(plot->cr, x,y+size);
		cairo_stroke(plot->cr);
		break;
	case 1: /* plot->cross */
		cairo_move_to(plot->cr, x-size, y-size);
		cairo_line_to(plot->cr, x+size,y+size);
		cairo_stroke(plot->cr);
		cairo_move_to(plot->cr, x-size, y+size);
		cairo_line_to(plot->cr, x+size,y-size);
		cairo_stroke(plot->cr);
		break;
	case 2: /* star */
		cairo_move_to(plot->cr, x-size, y);
		cairo_line_to(plot->cr, x+size,y);
		cairo_stroke(plot->cr);
		cairo_move_to(plot->cr, x, y-size);
		cairo_line_to(plot->cr, x,y+size);
		cairo_stroke(plot->cr);
		cairo_move_to(plot->cr, x-size, y-size);
		cairo_line_to(plot->cr, x+size,y+size);
		cairo_stroke(plot->cr);
		cairo_move_to(plot->cr, x-size, y+size);
		cairo_line_to(plot->cr, x+size,y-size);
		cairo_stroke(plot->cr);
		break;
	case 3: /* box */
	case 4: /* filled box */
		cairo_move_to(plot->cr, x-size, y-size);
		cairo_line_to(plot->cr, x-size,y+size);
		cairo_line_to(plot->cr, x+size,y+size);
		cairo_line_to(plot->cr, x+size,y-size);
		cairo_close_path(plot->cr);
		if (style == 4)
			cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;
	case 5: /* circle */
		cairo_arc (plot->cr, x, y, size, 0, 2*M_PI);
		cairo_stroke (plot->cr);
		break;
	case 6: /* filled circle */
		cairo_arc (plot->cr, x, y, size, 0, 2*M_PI);
		cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;
	case 7: /* triangle */
	case 8: /* filled triangle */
		cairo_move_to(plot->cr, x-size, y+size-plot->oversampling_scale);
		cairo_line_to(plot->cr, x,y-size);
		cairo_line_to(plot->cr, x+size,y+size-plot->oversampling_scale);
		cairo_close_path(plot->cr);
		if (style == 8)
			cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;
	case 9: /* upside down triangle */
	case 10: /* filled upside down triangle */
		cairo_move_to(plot->cr, x-size, y-size+plot->oversampling_scale);
		cairo_line_to(plot->cr, x,y+size);
		cairo_line_to(plot->cr, x+size,y-size+plot->oversampling_scale);
		cairo_close_path(plot->cr);
		if (style == 10)
			cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;
	case 11: /* diamond */
	case 12: /* filled diamond */
		cairo_move_to(plot->cr, x-size, y);
		cairo_line_to(plot->cr, x,y+size);
		cairo_line_to(plot->cr, x+size,y);
		cairo_line_to(plot->cr, x,y-size);
		cairo_close_path(plot->cr);
		if (style == 12)
			cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;
	case 13: /* pentagon */
	case 14: /* filled pentagon */
		cairo_move_to(plot->cr, x+size*0.5878, y-size*0.8090);
		cairo_line_to(plot->cr, x-size*0.5878, y-size*0.8090);
		cairo_line_to(plot->cr, x-size*0.9511, y+size*0.3090);
		cairo_line_to(plot->cr, x,             y+size);
		cairo_line_to(plot->cr, x+size*0.9511, y+size*0.3090);
		cairo_close_path(plot->cr);
		if (style == 14)
			cairo_fill_preserve(plot->cr);
		cairo_stroke(plot->cr);
		break;				
	default :
		break;
	}
	cairo_restore(plot->cr);
}



void gp_cairo_draw_fillbox(plot_struct *plot, int x, int y, int width, int height, int style)
{
	int fillpar = style >> 4;
	int fillstyle = style & 0xf;

	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);

	FPRINTF((stderr,"fillbox fillpar = %d, fillstyle = %d\n",fillpar, fillstyle));
	gp_cairo_fill( plot, fillstyle, fillpar);

	cairo_move_to(plot->cr, x, y);
	cairo_rel_line_to(plot->cr, 0, -height);
	cairo_rel_line_to(plot->cr, width, 0);
	cairo_rel_line_to(plot->cr, 0, height);
	cairo_rel_line_to(plot->cr, -width, 0);
	cairo_close_path(plot->cr);
	cairo_fill(plot->cr);
}


/*	corner[0] = (x1,y1) is the upper left corner (in terms of plot location) of
 *	the outer edge of the image.  Similarly, corner[1] = (x2,y2) is the lower
 *	right corner of the outer edge of the image.  (Outer edge means the
 *	outer extent of the corner pixels, not the middle of the corner pixels).
 *	corner[2] and corner[3] = (x3,y3) and (x4,y4) define a clipping box in
 *	the primary plot into which all or part of the image will be rendered.
 */
void gp_cairo_draw_image(plot_struct *plot, unsigned int * image, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int M, int N)
{
	double scale_x, scale_y;
	cairo_surface_t *image_surface;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;

	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);

	image_surface = cairo_image_surface_create_for_data((unsigned char*) image,
				CAIRO_FORMAT_ARGB32, M, N, 4*M);

	scale_x = (double)M/(double)abs( x2 - x1 );
	scale_y = (double)N/(double)abs( y2 - y1 );

	FPRINTF((stderr,"M %d N %d x1 %d y1 %d\n", M, N, x1, y1));
	cairo_save( plot->cr );

	/* Set clipping boundaries for image copy.
	 * The bounds were originally possed in corners[2] and corners[3]
	 */
	cairo_move_to(plot->cr, x3, y3);
	cairo_line_to(plot->cr, x3, y4);
	cairo_line_to(plot->cr, x4, y4);
	cairo_line_to(plot->cr, x4, y3);
	cairo_close_path(plot->cr);
	cairo_clip(plot->cr);

	pattern = cairo_pattern_create_for_surface( image_surface );
	/* scale and keep sharp edges */
	cairo_pattern_set_filter( pattern, CAIRO_FILTER_FAST );
	cairo_matrix_init_scale( &matrix, scale_x, scale_y );
	/* x1 and y1 give the user-space coordinate
	 * at which the surface origin should appear.
	 * (The surface origin is its upper-left corner
	 * before any transformation has been applied.) */
	cairo_matrix_translate( &matrix, -x1, -y1 );
	cairo_pattern_set_matrix( pattern, &matrix );
	cairo_set_source( plot->cr, pattern );

	cairo_paint( plot->cr );

	cairo_restore( plot->cr );

	cairo_pattern_destroy( pattern );
	cairo_surface_destroy( image_surface );
}

/* =======================================================================
 * Enhanced text mode support
 * =====================================================================*/

void gp_cairo_add_attr(plot_struct *plot, PangoAttrList * AttrList, int start, int end )
{
	PangoAttribute *p_attr_rise, *p_attr_size, *p_attr_family;
	PangoAttribute *p_attr_weight, *p_attr_style;

	p_attr_size = pango_attr_size_new ((int) (gp_cairo_enhanced_fontsize*PANGO_SCALE));
	p_attr_size->start_index = start;
	p_attr_size->end_index = end;
	pango_attr_list_insert (AttrList, p_attr_size);

	p_attr_rise = pango_attr_rise_new ((int) (gp_cairo_enhanced_base*PANGO_SCALE));
	p_attr_rise->start_index = start;
	p_attr_rise->end_index = end;
	pango_attr_list_insert (AttrList, p_attr_rise);

	p_attr_family = pango_attr_family_new (gp_cairo_enhanced_get_fontname(plot));
	p_attr_family->start_index = start;
	p_attr_family->end_index = end;
	pango_attr_list_insert (AttrList, p_attr_family);

	p_attr_weight = pango_attr_weight_new (plot->fontweight);
	p_attr_weight->start_index = start;
	p_attr_weight->end_index = end;
	pango_attr_list_insert (AttrList, p_attr_weight);

	p_attr_style = pango_attr_style_new (plot->fontstyle);
	p_attr_style->start_index = start;
	p_attr_style->end_index = end;
	pango_attr_list_insert (AttrList, p_attr_style);
}

/* add a blank character to the text string with a custom shape */
void gp_cairo_add_shape( PangoRectangle rect,int position)
{
	PangoAttribute *p_attr_shape;

	FPRINTF((stderr, "adding blank custom shape\n"));

	strncat(gp_cairo_utf8, " ", sizeof(gp_cairo_utf8)-strlen(gp_cairo_utf8)-1);
	p_attr_shape = pango_attr_shape_new (&rect,&rect);
	p_attr_shape->start_index = position;
	p_attr_shape->end_index = position+1;
	pango_attr_list_insert (gp_cairo_enhanced_AttrList, p_attr_shape);
}

/* gp_cairo_enhanced_flush() draws enhanced_text, which has been filled by _writec()*/
void gp_cairo_enhanced_flush(plot_struct *plot)
{
	PangoRectangle save_logical_rect;
	PangoLayout *save_layout;

	PangoLayout *current_layout;
	PangoRectangle current_ink_rect;
	PangoRectangle current_logical_rect;
	PangoFontDescription *current_desc;

	PangoRectangle underprinted_logical_rect;
	int overprinted_width = 0;
	PangoLayout *underprinted_layout;
	int start, end;
	PangoLayout *hide_layout;

	PangoRectangle hide_ink_rect;
	PangoRectangle hide_logical_rect;
	PangoFontDescription *hide_desc;
	PangoLayout *zerowidth_layout;
	PangoFontDescription *zerowidth_desc;
	PangoRectangle zerowidth_logical_rect;
	/* PangoRectangle zerowidth_ink_rect; */

	int save_start, save_end;
	int underprinted_start, underprinted_end;

	gchar* enhanced_text_utf8;

#ifdef MAP_SYMBOL
	TBOOLEAN symbol_font_parsed = FALSE;
#endif /*MAP_SYMBOL*/

	if (!gp_cairo_enhanced_opened_string)
		return;

	FPRINTF((stderr, "enhanced flush str=\"%s\" font=%s op=%d sf=%d wf=%d base=%f os=%d wt=%d sl=%d\n",
		gp_cairo_enhanced_string,
		gp_cairo_enhanced_font,
		gp_cairo_enhanced_overprint,
		gp_cairo_enhanced_showflag,
		gp_cairo_enhanced_widthflag,
		gp_cairo_enhanced_base,
		gp_cairo_enhanced_opened_string,
		plot->fontweight,
		plot->fontstyle ));

	gp_cairo_enhanced_opened_string = FALSE;

#ifdef MAP_SYMBOL
	/* we have to treat Symbol font as a special case */
	if (!strcmp(gp_cairo_enhanced_font,"Symbol")) {
		FPRINTF((stderr,"Parsing a Symbol string\n"));

		enhanced_text_utf8 = gp_cairo_convert_symbol_to_unicode(plot, gp_cairo_enhanced_string);

		if (!strcmp(plot->fontname,"Symbol")) {
			strncpy(gp_cairo_enhanced_font,
				plot->fontname,
				sizeof(gp_cairo_enhanced_font));
		} else {
			strncpy(gp_cairo_enhanced_font,
				gp_cairo_default_font(), sizeof(gp_cairo_enhanced_font));
		}
		symbol_font_parsed = TRUE;
	} else
#endif /*MAP_SYMBOL*/
	{
		/* convert the input string to utf8 */
		enhanced_text_utf8 = gp_cairo_convert(plot, gp_cairo_enhanced_string);
	}

	start = strlen(gp_cairo_utf8);

	if (gp_cairo_enhanced_restore_now) {
		/* restore saved position */
		/* the idea is to use a space character, drawn with a negative width */

		/* we first compute the size of the text drawn since the 'save' command */

		/* Create a PangoLayout, set the font and text
		 * with the saved attributes list, get extents */
		save_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (save_layout, gp_cairo_save_utf8, -1);
		pango_layout_set_attributes (save_layout, gp_cairo_enhanced_save_AttrList);
		pango_layout_get_extents(save_layout, NULL, &save_logical_rect);
		g_object_unref (save_layout);
		pango_attr_list_unref( gp_cairo_enhanced_save_AttrList );
		/* invert the size, so we will go back to the saved state */
		save_logical_rect.width = -save_logical_rect.width;
		/* EAM FIXME:  Zero height necessary but I don't understand why */
		save_logical_rect.height = 0;
		/* adding a blank character with the corresponding shape */
		gp_cairo_add_shape(save_logical_rect,start);

		strncpy(gp_cairo_save_utf8, "", sizeof(gp_cairo_save_utf8));
		gp_cairo_enhanced_restore_now = FALSE;
		start++;
	}

	if (gp_cairo_enhanced_overprint==2) {
		/* the idea is first to use a space character, drawn with an appropriate negative width */

		/* we first compute the size of the text drawn since overprint==1 was used */

		/* Create a PangoLayout, set the font and text with
		 * the saved attributes list, get extents */
		underprinted_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (underprinted_layout, gp_cairo_underprinted_utf8, -1);
		if (!gp_cairo_enhanced_underprinted_AttrList)
			fprintf(stderr,"uninitialized gp_cairo_enhanced_underprinted_AttrList!\n");
		else
			pango_layout_set_attributes (underprinted_layout, gp_cairo_enhanced_underprinted_AttrList);
		pango_layout_get_extents(underprinted_layout, NULL, &underprinted_logical_rect);
		g_object_unref (underprinted_layout);

		/* compute the size of the text to overprint*/

		/* Create a PangoLayout, set the font and text */
		current_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (current_layout, enhanced_text_utf8, -1);
		current_desc = pango_font_description_new ();
		pango_font_description_set_family (current_desc, gp_cairo_enhanced_get_fontname(plot));
		pango_font_description_set_size(current_desc,(int) gp_cairo_enhanced_fontsize*PANGO_SCALE);
		pango_font_description_set_weight (current_desc, plot->fontweight);
		pango_font_description_set_style (current_desc,
			plot->fontstyle ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

		pango_layout_set_font_description (current_layout, current_desc);
		pango_font_description_free (current_desc);
		pango_layout_get_extents(current_layout, &current_ink_rect, &current_logical_rect);
		g_object_unref (current_layout);

		/* calculate the distance to remove to center the overprinted text */
		underprinted_logical_rect.width = -(underprinted_logical_rect.width + current_logical_rect.width)/2;
		overprinted_width = current_logical_rect.width;

		/* adding a blank character with the corresponding shape */
		gp_cairo_add_shape(underprinted_logical_rect, start);

		strncpy(gp_cairo_underprinted_utf8, "", sizeof(gp_cairo_underprinted_utf8));
		/* increment the position as we added a character */
		start++;
	}

	if (gp_cairo_enhanced_showflag) {
		strncat(gp_cairo_utf8, enhanced_text_utf8, sizeof(gp_cairo_utf8)-strlen(gp_cairo_utf8)-1);
		end = strlen(gp_cairo_utf8);

		/* add text attributes to the main list */
		gp_cairo_add_attr(plot, gp_cairo_enhanced_AttrList, start, end);

	} else {
		/* position must be modified, but text not actually drawn */
		/* the idea is to use a blank character, drawn with the width of the text*/

		current_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (current_layout, gp_cairo_utf8, -1);
		pango_layout_set_attributes (current_layout, gp_cairo_enhanced_AttrList);
		pango_layout_get_extents(current_layout, &current_ink_rect, &current_logical_rect);
		g_object_unref (current_layout);

		/* we first compute the size of the text */
		/* Create a PangoLayout, set the font and text */
		hide_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (hide_layout, enhanced_text_utf8, -1);
		hide_desc = pango_font_description_new ();
		pango_font_description_set_family (hide_desc, gp_cairo_enhanced_get_fontname(plot));
		pango_font_description_set_size(hide_desc,(int) gp_cairo_enhanced_fontsize*PANGO_SCALE);
		pango_font_description_set_weight (hide_desc, plot->fontweight);
		pango_font_description_set_style (hide_desc,
			plot->fontstyle ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
		pango_layout_set_font_description (hide_layout, hide_desc);
		pango_font_description_free (hide_desc);

		pango_layout_get_extents(hide_layout, &hide_ink_rect, &hide_logical_rect);
		g_object_unref (hide_layout);

		/* rect.y must be reworked to take previous text into account, which may be smaller */
		/* hide_logical_rect.y is always initialized at zero, but should be : */
		if (current_logical_rect.height<hide_logical_rect.height)
			hide_logical_rect.y = current_logical_rect.height - hide_logical_rect.height;

		/* adding a blank character with the corresponding shape */
		gp_cairo_add_shape(hide_logical_rect, start);

		end = start+1; /* end *must* be defined, as it is used if widthflag is false */
	}

	if (!gp_cairo_enhanced_widthflag) {
		/* the idea is to use a blank character, drawn with the inverted width of the text*/
		/* we first compute the size of the text */

		/* Create a PangoLayout, set the font and text */
		zerowidth_layout = gp_cairo_create_layout (plot->cr);
		pango_layout_set_text (zerowidth_layout, enhanced_text_utf8, -1);
		zerowidth_desc = pango_font_description_new ();
		pango_font_description_set_family (zerowidth_desc, gp_cairo_enhanced_get_fontname(plot));
		pango_font_description_set_size(zerowidth_desc,(int) gp_cairo_enhanced_fontsize*PANGO_SCALE);
		pango_font_description_set_weight (zerowidth_desc, plot->fontweight);
		pango_font_description_set_style (zerowidth_desc,
			plot->fontstyle ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
		pango_layout_set_font_description (zerowidth_layout, zerowidth_desc);
		pango_font_description_free (zerowidth_desc);
		pango_layout_get_extents(zerowidth_layout, NULL, &zerowidth_logical_rect);
		g_object_unref (zerowidth_layout);

		/* invert the size, so we will go back to the start of the string */
		zerowidth_logical_rect.width = -zerowidth_logical_rect.width;

		/* adding a blank character with the corresponding shape */
		gp_cairo_add_shape(zerowidth_logical_rect,end);
		end++;
	}

	if (gp_cairo_enhanced_overprint==2) {
		/* revert the previous negative space to go back to starting point.
		*  Take centered overprinted text width into account  */
		underprinted_logical_rect.width = -underprinted_logical_rect.width - overprinted_width/2;
		/* adding a blank character with the corresponding shape */
		gp_cairo_add_shape(underprinted_logical_rect,end);
	}

	if (gp_cairo_enhanced_save) /* we aim at restoring position later */ {
		save_start = strlen( gp_cairo_save_utf8);
		strncat(gp_cairo_save_utf8, enhanced_text_utf8, sizeof(gp_cairo_utf8)-strlen(gp_cairo_utf8)-1);
		save_end = strlen( gp_cairo_save_utf8);

		/* add text attributes to the save list */
		gp_cairo_add_attr(plot, gp_cairo_enhanced_save_AttrList, save_start, save_end);
	}

	if (gp_cairo_enhanced_overprint==1) /* save underprinted text with its attributes */{
		underprinted_start = strlen(gp_cairo_underprinted_utf8);
		strncat(gp_cairo_underprinted_utf8,
			enhanced_text_utf8,
			sizeof(gp_cairo_underprinted_utf8)-strlen(gp_cairo_underprinted_utf8)-1);
		underprinted_end = strlen(gp_cairo_underprinted_utf8);

		if (gp_cairo_enhanced_underprinted_AttrList)
			pango_attr_list_unref( gp_cairo_enhanced_underprinted_AttrList );
		gp_cairo_enhanced_underprinted_AttrList = pango_attr_list_new();

		/* add text attributes to the underprinted list */
		gp_cairo_add_attr(plot, gp_cairo_enhanced_underprinted_AttrList,
			underprinted_start, underprinted_end);
	}

#ifdef MAP_SYMBOL
	if (symbol_font_parsed)
		strncpy(gp_cairo_enhanced_font, "Symbol", sizeof(gp_cairo_enhanced_font));
#endif /* MAP_SYMBOL */

	g_free(enhanced_text_utf8);
}

/* brace is TRUE to keep processing to },
 *         FALSE to do one character only
 * fontname & fontsize are obvious
 * base is the current baseline
 * widthflag is TRUE if the width of this should count,
 *              FALSE for zero width boxes
 * showflag is TRUE if this should be shown,
 *             FALSE if it should not be shown (like TeX \phantom)
 * overprint is 0 for normal operation,
 *              1 for the underprinted text (included in width calculation),
 *              2 for the overprinted text (not included in width calc, through widhtflag=false),
 *              (overprinted text is centered horizontally on underprinted text)
 *              3 means "save current position",
 *              4 means "restore saved position" */
void gp_cairo_enhanced_open(plot_struct *plot, char* fontname, double fontsize, double base, TBOOLEAN widthflag, TBOOLEAN showflag, int overprint)
{
	if (overprint == 3) {
		gp_cairo_enhanced_save = TRUE;
		gp_cairo_enhanced_restore_now = FALSE;
		gp_cairo_enhanced_save_AttrList = pango_attr_list_new();
		return;
	}

	if (overprint == 4) {
		gp_cairo_enhanced_save = FALSE;
		gp_cairo_enhanced_restore_now = TRUE;
		return;
	}

	if (!gp_cairo_enhanced_opened_string) {
		// Strip off Bold or Italic and apply immediately
		// Is it really necessary to preserve plot->fontname?
		char *save_plot_font = strdup(plot->fontname);
		gp_cairo_set_font(plot, fontname, plot->fontsize);
		strncpy(gp_cairo_enhanced_font, plot->fontname, sizeof(gp_cairo_enhanced_font));
		strcpy(plot->fontname, save_plot_font);
		free(save_plot_font);

		gp_cairo_enhanced_opened_string = TRUE;
		gp_cairo_enhanced_char = gp_cairo_enhanced_string;
		gp_cairo_enhanced_fontsize = fontsize*plot->oversampling_scale;
		gp_cairo_enhanced_base = base*plot->oversampling_scale;
		gp_cairo_enhanced_showflag = showflag;
		gp_cairo_enhanced_overprint = overprint;
		gp_cairo_enhanced_widthflag = widthflag;
	}
}

void gp_cairo_enhanced_writec(plot_struct *plot, int character)
{
	*gp_cairo_enhanced_char++ = character;
	*gp_cairo_enhanced_char = '\0';
}

void gp_cairo_enhanced_init(plot_struct *plot, int len)
{
	/* begin by stroking any open path */
	gp_cairo_stroke(plot);
	/* also draw any open polygon set */
	gp_cairo_end_polygon(plot);


	gp_cairo_enhanced_string = (char*) malloc(len+1);
	gp_cairo_enhanced_opened_string = FALSE;
	gp_cairo_enhanced_overprint = FALSE;
	gp_cairo_enhanced_showflag = TRUE;
	gp_cairo_enhanced_fontsize = plot->fontsize*plot->oversampling_scale;
	strncpy(gp_cairo_enhanced_font, plot->fontname, sizeof(gp_cairo_enhanced_font));
	gp_cairo_enhanced_AttrList = pango_attr_list_new();
}

void gp_cairo_enhanced_finish(plot_struct *plot, int x, int y)
{
	PangoRectangle ink_rect, logical_rect;
	PangoLayout *layout;
	double vert_just, arg, enh_x, enh_y, delta, deltax, deltay;
	int baseline_offset;

	/* Create a PangoLayout, set the font and text */
	layout = gp_cairo_create_layout (plot->cr);

	pango_layout_set_text (layout, gp_cairo_utf8, -1);

	pango_layout_set_attributes (layout, gp_cairo_enhanced_AttrList);

	pango_layout_get_extents(layout, &ink_rect, &logical_rect);

	/* NB: See explanatory comments in gp_cairo_draw_text() */
	baseline_offset = pango_layout_get_baseline(layout) / PANGO_SCALE;
	vert_just = 0.5 * (float)(plot->fontsize * plot->oversampling_scale);
	vert_just = baseline_offset - vert_just;
	
	arg = plot->text_angle * M_PI/180;
	enh_x = x - vert_just * sin(arg);
	enh_y = y - vert_just * cos(arg);

	delta = ((double)logical_rect.width/2) / PANGO_SCALE;

	deltax = delta * cos(arg);
	deltay = delta * sin(arg);

	switch (plot->justify_mode) {
	case LEFT :
		break;
	case CENTRE :
		enh_x -= deltax;
		enh_y += deltay;
		break;
	case RIGHT :
		enh_x -= 2*deltax;
		enh_y += 2*deltay;
		break;
	}

	cairo_save(plot->cr);
	cairo_move_to (plot->cr, enh_x, enh_y);
	/* angle in radians */
	cairo_rotate(plot->cr, -arg);

	cairo_set_source_rgba(plot->cr, plot->color.r, plot->color.g, plot->color.b,
				1. - plot->color.alpha);
	/* Inform Pango to re-layout the text with the new transformation */
	pango_cairo_update_layout (plot->cr, layout);
	pango_cairo_show_layout (plot->cr, layout);
	/* pango_cairo_show_layout does not clear the path (here a starting point)
	 * Do it by ourselves, or we can get spurious lines on future calls. */
	cairo_new_path(plot->cr);

#ifdef EAM_BOXED_TEXT
	/* Update bounding box for boxed label text */
	pango_layout_get_pixel_extents (layout, &ink_rect, &logical_rect);

	/* Auto-initialization */
	if (bounding_box[0] < 0 && bounding_box[1] < 0) {
	    bounding_box[0] = bounding_box[2] = x;
	    bounding_box[1] = bounding_box[3] = y;
	}

	/* Would it look better to use logical bounds rather than ink_rect? */
	if (bounding_box[0] > enh_x + ink_rect.x)
	    bounding_box[0] = enh_x + ink_rect.x;
	if (bounding_box[2] < enh_x + ink_rect.x + ink_rect.width)
	    bounding_box[2] = enh_x + ink_rect.x + ink_rect.width;
	if (bounding_box[1] > enh_y + ink_rect.y)
	    bounding_box[1] = enh_y + ink_rect.y;
	if (bounding_box[3] < enh_y + ink_rect.y + ink_rect.height)
	    bounding_box[3] = enh_y + ink_rect.y + ink_rect.height;
#endif
	
	/* free the layout object */
	pango_attr_list_unref( gp_cairo_enhanced_AttrList );
	g_object_unref (layout);
	cairo_restore(plot->cr);
	strncpy(gp_cairo_utf8, "", sizeof(gp_cairo_utf8));
	free(gp_cairo_enhanced_string);
}

/* obtain the right pattern or solid fill from fillstyle and fillpar.
 * Used to draw fillboxes and polygons */
void gp_cairo_fill(plot_struct *plot, int fillstyle, int fillpar)
{
	double red = 0, green = 0, blue = 0, fact = 0;

	switch (fillstyle) {
	case FS_SOLID: /* solid fill */
		if (plot->color.alpha > 0) {
			fillpar = 100. * (1. - plot->color.alpha);
			/* Fall through to FS_TRANSPARENT_SOLID */
		} else if (fillpar==100)
			/* treated as a special case to accelerate common situation */ {
			cairo_set_source_rgb(plot->cr, plot->color.r, plot->color.g, plot->color.b);
			FPRINTF((stderr,"solid %lf %lf %lf\n",plot->color.r, plot->color.g, plot->color.b));
			return;
		} else {
			red   = plot->color.r;
			green = plot->color.g;
			blue  = plot->color.b;

			fact = (double)(100 - fillpar) /100;

			if (fact >= 0 && fact <= 1) {
				red   += (1 - red) * fact;
				green += (1 - green) * fact;
				blue  += (1 - blue) * fact;
			}
			cairo_set_source_rgb(plot->cr, red, green, blue);
			FPRINTF((stderr,"transparent solid %lf %lf %lf\n",red, green, blue));
			return;
		}
	case FS_TRANSPARENT_SOLID:
		red   = plot->color.r;
		green = plot->color.g;
		blue  = plot->color.b;
		cairo_set_source_rgba(plot->cr, red, green, blue, (double)fillpar/100.);
		return;
	case FS_PATTERN: /* pattern fill */
	case FS_TRANSPARENT_PATTERN:
		gp_cairo_fill_pattern(plot, fillstyle, fillpar);
		FPRINTF((stderr,"pattern fillpar = %d %lf %lf %lf\n",fillpar, plot->color.r, plot->color.g, plot->color.b));
		return;
	case FS_EMPTY: /* fill with background plot->color */
		cairo_set_source_rgb(plot->cr, plot->background.r, plot->background.g, plot->background.b);
		FPRINTF((stderr,"empty\n"));
		return;
	default:
		cairo_set_source_rgb(plot->cr, plot->color.r, plot->color.g, plot->color.b);
		FPRINTF((stderr,"default %lf %lf %lf\n",plot->color.r, plot->color.g, plot->color.b));
		return;
	}
}

#ifdef EAM_BOXED_TEXT
void gp_cairo_boxed_text(plot_struct *plot, int x, int y, int option)
{
	int dx, dy;

	switch (option) {
	case TEXTBOX_INIT:
		/* Initialize bounding box for this text string */
		bounding_box[0] = bounding_box[2] = x;
		bounding_box[1] = bounding_box[3] = y;
		break;

	case TEXTBOX_OUTLINE:
		/* Stroke the outline of the bounding box for previous text */
	case TEXTBOX_BACKGROUNDFILL:
	case TEXTBOX_GREY:
		/* Fill the bounding box with background color */
		/* begin by stroking any open path */
		gp_cairo_stroke(plot);
		gp_cairo_end_polygon(plot);

		cairo_save(plot->cr);
		dx = 0.25 * bounding_xmargin * (float)(plot->fontsize * plot->oversampling_scale);
		dy = 0.25 * bounding_ymargin * (float)(plot->fontsize * plot->oversampling_scale);
		if (option == TEXTBOX_GREY)
		    dy = 0;
		gp_cairo_move(plot,   bounding_box[0]-dx, bounding_box[1]-dy); 
		gp_cairo_vector(plot, bounding_box[0]-dx, bounding_box[3]+dy); 
		gp_cairo_vector(plot, bounding_box[2]+dx, bounding_box[3]+dy); 
		gp_cairo_vector(plot, bounding_box[2]+dx, bounding_box[1]-dy); 
		gp_cairo_vector(plot, bounding_box[0]+dx, bounding_box[1]-dy); 
		cairo_close_path(plot->cr);
		if (option == TEXTBOX_BACKGROUNDFILL) {
		    rgb_color *background = &gp_cairo_colorlist[0];
		    cairo_set_source_rgb(plot->cr, background->r, background->g, background->b);
		    cairo_fill(plot->cr);
		} else if (option == TEXTBOX_GREY) {
		    cairo_set_source_rgba(plot->cr, 0.75, 0.75, 0.75, 0.50);
		    cairo_fill(plot->cr);
		} else {
		    cairo_set_line_width(plot->cr, 0.5*plot->oversampling_scale);
		    cairo_set_source_rgb(plot->cr,
			plot->color.r, plot->color.g, plot->color.b);
		    cairo_stroke(plot->cr);
		}
		cairo_restore(plot->cr);
		break;

	case TEXTBOX_MARGINS: /* Change the margin between text and box */
		bounding_xmargin = (double)x/100.;
		bounding_ymargin = (double)y/100.;
		break;

	default:
		break;
	}
}
#endif

#define PATTERN_SIZE 8

/* return a pattern used for fillboxes and polygons */
void gp_cairo_fill_pattern(plot_struct *plot, int fillstyle, int fillpar)
{
	cairo_surface_t *pattern_surface;
	cairo_t *pattern_cr;
	cairo_pattern_t *pattern;
	cairo_matrix_t context_matrix;
	cairo_matrix_t matrix;

	pattern_surface = cairo_surface_create_similar(cairo_get_target(plot->cr),
                                             CAIRO_CONTENT_COLOR_ALPHA,
                                             PATTERN_SIZE,
                                             PATTERN_SIZE);
	pattern_cr = cairo_create(pattern_surface);

	cairo_matrix_init_scale(&context_matrix,
		PATTERN_SIZE,
		PATTERN_SIZE);
	cairo_set_matrix(pattern_cr,&context_matrix);

	if (fillstyle == FS_TRANSPARENT_PATTERN)
	    cairo_set_source_rgba( pattern_cr, 1.0, 1.0, 1.0, 0.0);
	else
	    cairo_set_source_rgb( pattern_cr, 1.0, 1.0, 1.0);

	cairo_paint(pattern_cr);

	if (!strcmp(term->name,"pdfcairo")) /* Work-around for poor scaling in cairo */
	    cairo_set_line_width(pattern_cr, PATTERN_SIZE/150.);
	else
	    cairo_set_line_width(pattern_cr, PATTERN_SIZE/50.);
	cairo_set_line_cap  (pattern_cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_source_rgb(pattern_cr, plot->color.r, plot->color.g, plot->color.b);

	switch (fillpar%8) {
	case 0: /* no fill */
	default:
		break;
	case 1: /* cross-hatch */
	case 2: /* double cross-hatch */
		cairo_move_to(pattern_cr, 0,0);
		cairo_line_to(pattern_cr, 1.0,1.0);
		cairo_stroke(pattern_cr);
		cairo_move_to(pattern_cr, 0,1.0);
		cairo_line_to(pattern_cr, 1.0,0);
		cairo_stroke(pattern_cr);
		break;
	case 3: /* solid */
		cairo_paint(pattern_cr);
		break;
	case 4: /* diagonal hatch */
	case 5:
	case 6:
	case 7:
		cairo_move_to(pattern_cr, 0.5,0.);
		cairo_line_to(pattern_cr, 0.5,1.);
		cairo_stroke(pattern_cr);
		break;
	}

	pattern = cairo_pattern_create_for_surface( pattern_surface );
	cairo_pattern_set_extend( pattern, CAIRO_EXTEND_REPEAT );

	/* compensate the transformation matrix of the main context */
	cairo_matrix_init_scale(&matrix,
		1.0/plot->oversampling_scale,
		1.0/plot->oversampling_scale);

	switch (fillpar%8) {
	case 0: /* no fill */
	case 1: /* cross-hatch */
	case 3: /* solid */
	default:
		break;
	case 2: /* double cross-hatch */
		cairo_matrix_scale( &matrix, 2.,2.);
		break;
	case 4: /* diagonal hatch */
		cairo_matrix_rotate( &matrix, M_PI/4);
		break;
	case 5:
		cairo_matrix_rotate( &matrix, -M_PI/4);
		break;
	case 6:
		cairo_matrix_rotate( &matrix, M_PI/4);
		cairo_matrix_scale( &matrix, 2.,2.);
		break;
	case 7:
		cairo_matrix_rotate( &matrix, -M_PI/4);
		cairo_matrix_scale( &matrix, 2.,2.);
		break;
	}

	cairo_pattern_set_matrix(pattern,&matrix);

	cairo_destroy( pattern_cr );
	cairo_set_source( plot->cr, pattern );
	cairo_pattern_destroy( pattern );
	cairo_surface_destroy( pattern_surface );
}


/* Sets term vars v_char, h_char, v_tic, h_tic
 * Depends on plot->fontsize and fontname */
void gp_cairo_set_termvar(plot_struct *plot, unsigned int *v_char,
                                             unsigned int *h_char)
{
	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;
	unsigned int tmp_v_char, tmp_h_char;

	/* Create a PangoLayout, set the font and text */
	layout = gp_cairo_create_layout (plot->cr);
	pango_layout_set_text (layout, "0123456789", -1);
	desc = pango_font_description_new ();
	pango_font_description_set_family (desc, plot->fontname);
	pango_font_description_set_size(desc,(int) (plot->fontsize*PANGO_SCALE*plot->oversampling_scale));
	pango_font_description_set_weight (desc, plot->fontweight);
	pango_font_description_set_style (desc,
		plot->fontstyle ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	pango_layout_get_extents(layout, &ink_rect, &logical_rect);
	g_object_unref (layout);

	/* we don't use gnuplot_x() and gnuplot_y() in the following
	 * as the scale should have just been updated to 1.
	 * Although PANGO works with integer, it scales them via a huge number (usually ~1000).
	 * That's why I use ceil() instead of direct division result */
	tmp_v_char = (int) ceil( (double) logical_rect.height/PANGO_SCALE) - 1;
	tmp_h_char = (int) ceil( (double) logical_rect.width/(10*PANGO_SCALE));

	if (v_char)
		*v_char = tmp_v_char;
	if (h_char)
		*h_char = tmp_h_char;

/* FIXME!!! So far, so good. But now we have a problem. This routine	*/
/* is called synchronously with the set_font command, but avg_vchar is	*/
/* needed asynchronously during execution of the display list. 		*/
	avg_vchar = tmp_v_char;
}

void gp_cairo_solid_background(plot_struct *plot)
{
	if (cairo_status (plot->cr)) {
		fprintf(stderr, "Cairo is unhappy: %s\n",
			cairo_status_to_string (cairo_status (plot->cr)));
		gp_exit(EXIT_FAILURE);
	}
	cairo_set_source_rgb(plot->cr, plot->background.r, plot->background.g, plot->background.b);
	cairo_paint(plot->cr);
}

void gp_cairo_clear_background(plot_struct *plot)
{
	if (cairo_status (plot->cr)) {
		fprintf(stderr, "Cairo is unhappy: %s\n",
			cairo_status_to_string (cairo_status (plot->cr)));
		gp_exit(EXIT_FAILURE);
	}
	cairo_set_source_rgba(plot->cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(plot->cr);
}

/*----------------------------------------------------------------------------
   font functions
----------------------------------------------------------------------------*/


/* in enhanced text mode, look if enhanced mode has set the font,
 * otherwise return the default */
const char* gp_cairo_enhanced_get_fontname(plot_struct *plot)
{
	if ( strlen(gp_cairo_enhanced_font)==0 )
		return plot->fontname;
	else
		return gp_cairo_enhanced_font;
}

/* BM: New function to determine the default font.
 * On Windows, the "Sans" alias normally is equivalent to
 * "Tahoma" but the resolution fails on some systems. */
const char *
gp_cairo_default_font(void)
{
#ifdef WIN32
	return "Tahoma";
#else
	return "Sans";
#endif
}

/*----------------------------------------------------------------------------
   coordinates functions
----------------------------------------------------------------------------*/

#define OFFSET 0

double device_x(plot_struct *plot, double x)
{
	double scaled_x;
	scaled_x = plot->xscale*x/plot->oversampling_scale ;
	return scaled_x + OFFSET;
}

double device_y(plot_struct *plot, double y)
{
	double scaled_and_mirrored_y;
	scaled_and_mirrored_y = (plot->ymax - y)*plot->yscale/plot->oversampling_scale;
	return scaled_and_mirrored_y + OFFSET;
}

double gnuplot_x(plot_struct *plot, double x)
{
	double scaled_x;
	scaled_x = (x + OFFSET)/plot->xscale*plot->oversampling_scale ;
	return scaled_x;
}

double gnuplot_y(plot_struct *plot, double y)
{
	double scaled_and_mirrored_y;
	scaled_and_mirrored_y = plot->ymax +(-y + OFFSET)/plot->yscale*plot->oversampling_scale;
	return scaled_and_mirrored_y;
}


/* return the charset as a string accepted by glib routines,
 * default to the locale charset,
 * the returned char* doesn't have to be freed. */
const char* gp_cairo_get_encoding(plot_struct *plot)
{
	const char * charset;

	switch (plot->encoding) {
	case S_ENC_ISO8859_2 : return "ISO-8859-2";
	case S_ENC_ISO8859_15 : return "ISO-8859-15";
	case S_ENC_CP437 : return "cp437";
	case S_ENC_CP850 :  return "cp850";
	case S_ENC_CP852 :  return "cp852";
	case S_ENC_CP1250 : return "windows-1250";
	case S_ENC_CP1252 : return "windows-1252";
	case S_ENC_KOI8_R : return "KOI8-R";
	case S_ENC_KOI8_U :  return "KOI8-U";
	case S_ENC_ISO8859_1 : return "ISO-8859-1";
	case S_ENC_UTF8 : return "UTF-8";
	case S_ENC_DEFAULT :
	case S_ENC_INVALID :
	default :
 		g_get_charset(&charset);
		return charset;
	}
}

/* Symbol font handling.
 * To ensure compatibility with other terminals,
 * use the map provided by http://www.unicode.org/ to
 * translate character codes to their unicode counterparts.
 * The returned string has te be freed by the calling function. */
gchar* gp_cairo_convert_symbol_to_unicode(plot_struct *plot, const char* string)
{
	gchar *string_utf8;
	gchar *output;
	gchar *iter;
	gchar *iter_mod;
	int i;
	int imax;
	GError *error = NULL;

	/* first step, get a valid utf8 string, without taking care of Symbol.
	 * The input string is likely to be encoded in iso_8859_1, with characters
	 * going from 1 to 255. Try this first. If it's not the case, fall back to
	 * routine based on the encoding variable. */
	string_utf8 = g_convert(string, -1, "UTF-8", "ISO-8859-1", NULL, NULL, &error);
	if (error != NULL) {
		fprintf(stderr,"Symbol font : fallback to iso_8859_1 did not work\n");
		g_error_free(error);
		string_utf8 = gp_cairo_convert(plot, string);
	}

	iter = string_utf8;
	/* Assume that the output string in utf8 won't use more than 8 bytes per character/
	 * The utf8 specification fixes the limit to 4 bytes per character, but here we can also
	 * composite two characters */
	output = (gchar*) gp_alloc((4*strlen(string)+1)*sizeof(gchar),"Symbol to unicode");
	iter_mod = output;
	imax = g_utf8_strlen(string_utf8,-1) + 1;

	for (i=0; i<imax; ++i) {
		switch(g_utf8_get_char(iter)) {
#define SYMB_UNICODE(symbol_char,unicode) case symbol_char : g_unichar_to_utf8(unicode, iter_mod); break;
		/* not modifying ASCII characters */
		/* SYMB_UNICODE(0x20,0x0020); */ /* SPACE */
		/* SYMB_UNICODE(0x21,0x0021); */ /* EXCLAMATION MARK */
		SYMB_UNICODE(0x22,0x2200); /* FOR ALL */
		/* SYMB_UNICODE(0x23,0x0023); */ /* NUMBER SIGN */
		SYMB_UNICODE(0x24,0x2203); /* THERE EXISTS */
		/* SYMB_UNICODE(0x25,0x0025); */ /* PERCENT SIGN */
		/* SYMB_UNICODE(0x26,0x0026); */ /* AMPERSAND */
		SYMB_UNICODE(0x27,0x220D); /* SMALL CONTAINS AS MEMBER */
		/* SYMB_UNICODE(0x28,0x0028); */ /* LEFT PARENTHESIS */
		/* SYMB_UNICODE(0x29,0x0029); */ /* RIGHT PARENTHESIS */
		/* SYMB_UNICODE(0x2A,0x2217); */ /* ASTERISK OPERATOR */
		/* SYMB_UNICODE(0x2B,0x002B); */ /* PLUS SIGN */
		/* SYMB_UNICODE(0x2C,0x002C); */ /* COMMA */
		/* SYMB_UNICODE(0x2D,0x2212); */ /* MINUS SIGN */
		/* SYMB_UNICODE(0x2E,0x002E); */ /* FULL STOP */
		/* SYMB_UNICODE(0x2F,0x002F); */ /* SOLIDUS */
		/* SYMB_UNICODE(0x30,0x0030); */ /* DIGIT ZERO */
		/* SYMB_UNICODE(0x31,0x0031); */ /* DIGIT ONE */
		/* SYMB_UNICODE(0x32,0x0032); */ /* DIGIT TWO */
		/* SYMB_UNICODE(0x33,0x0033); */ /* DIGIT THREE */
		/* SYMB_UNICODE(0x34,0x0034); */ /* DIGIT FOUR */
		/* SYMB_UNICODE(0x35,0x0035); */ /* DIGIT FIVE */
		/* SYMB_UNICODE(0x36,0x0036); */ /* DIGIT SIX */
		/* SYMB_UNICODE(0x37,0x0037); */ /* DIGIT SEVEN */
		/* SYMB_UNICODE(0x38,0x0038); */ /* DIGIT EIGHT */
		/* SYMB_UNICODE(0x39,0x0039); */ /* DIGIT NINE */
		/* SYMB_UNICODE(0x3A,0x003A); */ /* COLON */
		/* SYMB_UNICODE(0x3B,0x003B); */ /* SEMICOLON */
		/* SYMB_UNICODE(0x3C,0x003C); */ /* LESS-THAN SIGN */
		/* SYMB_UNICODE(0x3D,0x003D); */ /* EQUALS SIGN */
		/* SYMB_UNICODE(0x3E,0x003E); */ /* GREATER-THAN SIGN */
		/* SYMB_UNICODE(0x3F,0x003F); */ /* QUESTION MARK */
		SYMB_UNICODE(0x40,0x2245); /* APPROXIMATELY EQUAL TO */
		SYMB_UNICODE(0x41,0x0391); /* GREEK CAPITAL LETTER ALPHA */
		SYMB_UNICODE(0x42,0x0392); /* GREEK CAPITAL LETTER BETA */
		SYMB_UNICODE(0x43,0x03A7); /* GREEK CAPITAL LETTER CHI */
		SYMB_UNICODE(0x44,0x0394); /* GREEK CAPITAL LETTER DELTA */
		SYMB_UNICODE(0x45,0x0395); /* GREEK CAPITAL LETTER EPSILON */
		SYMB_UNICODE(0x46,0x03A6); /* GREEK CAPITAL LETTER PHI */
		SYMB_UNICODE(0x47,0x0393); /* GREEK CAPITAL LETTER GAMMA */
		SYMB_UNICODE(0x48,0x0397); /* GREEK CAPITAL LETTER ETA */
		SYMB_UNICODE(0x49,0x0399); /* GREEK CAPITAL LETTER IOTA */
		SYMB_UNICODE(0x4A,0x03D1); /* GREEK THETA SYMBOL */
		SYMB_UNICODE(0x4B,0x039A); /* GREEK CAPITAL LETTER KAPPA */
		SYMB_UNICODE(0x4C,0x039B); /* GREEK CAPITAL LETTER LAMDA */
		SYMB_UNICODE(0x4D,0x039C); /* GREEK CAPITAL LETTER MU */
		SYMB_UNICODE(0x4E,0x039D); /* GREEK CAPITAL LETTER NU */
		SYMB_UNICODE(0x4F,0x039F); /* GREEK CAPITAL LETTER OMICRON */
		SYMB_UNICODE(0x50,0x03A0); /* GREEK CAPITAL LETTER PI */
		SYMB_UNICODE(0x51,0x0398); /* GREEK CAPITAL LETTER THETA */
		SYMB_UNICODE(0x52,0x03A1); /* GREEK CAPITAL LETTER RHO */
		SYMB_UNICODE(0x53,0x03A3); /* GREEK CAPITAL LETTER SIGMA */
		SYMB_UNICODE(0x54,0x03A4); /* GREEK CAPITAL LETTER TAU */
		SYMB_UNICODE(0x55,0x03A5); /* GREEK CAPITAL LETTER UPSILON */
		SYMB_UNICODE(0x56,0x03C2); /* GREEK SMALL LETTER FINAL SIGMA */
		SYMB_UNICODE(0x57,0x03A9); /* GREEK CAPITAL LETTER OMEGA */
		SYMB_UNICODE(0x58,0x039E); /* GREEK CAPITAL LETTER XI */
		SYMB_UNICODE(0x59,0x03A8); /* GREEK CAPITAL LETTER PSI */
		SYMB_UNICODE(0x5A,0x0396); /* GREEK CAPITAL LETTER ZETA */
		SYMB_UNICODE(0x5B,0x005B); /* LEFT SQUARE BRACKET */
		SYMB_UNICODE(0x5C,0x2234); /* THEREFORE */
		SYMB_UNICODE(0x5D,0x005D); /* RIGHT SQUARE BRACKET */
		SYMB_UNICODE(0x5E,0x22A5); /* UP TACK */
		SYMB_UNICODE(0x5F,0x005F); /* LOW LINE */
		SYMB_UNICODE(0x60,0xF8E5); /* radical extender corporate char */
		SYMB_UNICODE(0x61,0x03B1); /* GREEK SMALL LETTER ALPHA */
		SYMB_UNICODE(0x62,0x03B2); /* GREEK SMALL LETTER BETA */
		SYMB_UNICODE(0x63,0x03C7); /* GREEK SMALL LETTER CHI */
		SYMB_UNICODE(0x64,0x03B4); /* GREEK SMALL LETTER DELTA */
		SYMB_UNICODE(0x65,0x03B5); /* GREEK SMALL LETTER EPSILON */
		SYMB_UNICODE(0x66,0x03C6); /* GREEK SMALL LETTER PHI */
		SYMB_UNICODE(0x67,0x03B3); /* GREEK SMALL LETTER GAMMA */
		SYMB_UNICODE(0x68,0x03B7); /* GREEK SMALL LETTER ETA */
		SYMB_UNICODE(0x69,0x03B9); /* GREEK SMALL LETTER IOTA */
		SYMB_UNICODE(0x6A,0x03D5); /* GREEK PHI SYMBOL */
		SYMB_UNICODE(0x6B,0x03BA); /* GREEK SMALL LETTER KAPPA */
		SYMB_UNICODE(0x6C,0x03BB); /* GREEK SMALL LETTER LAMDA */
		/* SYMB_UNICODE(0x6D,0x03BC); */ /* GREEK SMALL LETTER MU */
		SYMB_UNICODE(0x6D,0x00B5); /* GREEK SMALL LETTER MU */
		SYMB_UNICODE(0x6E,0x03BD); /* GREEK SMALL LETTER NU */
		SYMB_UNICODE(0x6F,0x03BF); /* GREEK SMALL LETTER OMICRON */
		SYMB_UNICODE(0x70,0x03C0); /* GREEK SMALL LETTER PI */
		SYMB_UNICODE(0x71,0x03B8); /* GREEK SMALL LETTER THETA */
		SYMB_UNICODE(0x72,0x03C1); /* GREEK SMALL LETTER RHO */
		SYMB_UNICODE(0x73,0x03C3); /* GREEK SMALL LETTER SIGMA */
		SYMB_UNICODE(0x74,0x03C4); /* GREEK SMALL LETTER TAU */
		SYMB_UNICODE(0x75,0x03C5); /* GREEK SMALL LETTER UPSILON */
		SYMB_UNICODE(0x76,0x03D6); /* GREEK PI SYMBOL */
		SYMB_UNICODE(0x77,0x03C9); /* GREEK SMALL LETTER OMEGA */
		SYMB_UNICODE(0x78,0x03BE); /* GREEK SMALL LETTER XI */
		SYMB_UNICODE(0x79,0x03C8); /* GREEK SMALL LETTER PSI */
		SYMB_UNICODE(0x7A,0x03B6); /* GREEK SMALL LETTER ZETA */
		SYMB_UNICODE(0x7B,0x007B); /* LEFT CURLY BRACKET */
		SYMB_UNICODE(0x7C,0x007C); /* VERTICAL LINE */
		SYMB_UNICODE(0x7D,0x007D); /* RIGHT CURLY BRACKET */
		SYMB_UNICODE(0x7E,0x223C); /* TILDE OPERATOR */

		SYMB_UNICODE(0xA0,0x20AC); /* EURO SIGN */
		SYMB_UNICODE(0xA1,0x03D2); /* GREEK UPSILON WITH HOOK SYMBOL */
		SYMB_UNICODE(0xA2,0x2032); /* PRIME minute */
		SYMB_UNICODE(0xA3,0x2264); /* LESS-THAN OR EQUAL TO */
		SYMB_UNICODE(0xA4,0x2044); /* FRACTION SLASH */
		SYMB_UNICODE(0xA5,0x221E); /* INFINITY */
		SYMB_UNICODE(0xA6,0x0192); /* LATIN SMALL LETTER F WITH HOOK */
		SYMB_UNICODE(0xA7,0x2663); /* BLACK CLUB SUIT */
		SYMB_UNICODE(0xA8,0x2666); /* BLACK DIAMOND SUIT */
		SYMB_UNICODE(0xA9,0x2665); /* BLACK HEART SUIT */
		SYMB_UNICODE(0xAA,0x2660); /* BLACK SPADE SUIT */
		SYMB_UNICODE(0xAB,0x2194); /* LEFT RIGHT ARROW */
		SYMB_UNICODE(0xAC,0x2190); /* LEFTWARDS ARROW */
		SYMB_UNICODE(0xAD,0x2191); /* UPWARDS ARROW */
		SYMB_UNICODE(0xAE,0x2192); /* RIGHTWARDS ARROW */
		SYMB_UNICODE(0xAF,0x2193); /* DOWNWARDS ARROW */
		SYMB_UNICODE(0xB0,0x00B0); /* DEGREE SIGN */
		SYMB_UNICODE(0xB1,0x00B1); /* PLUS-MINUS SIGN */
		SYMB_UNICODE(0xB2,0x2033); /* DOUBLE PRIME second */
		SYMB_UNICODE(0xB3,0x2265); /* GREATER-THAN OR EQUAL TO */
		SYMB_UNICODE(0xB4,0x00D7); /* MULTIPLICATION SIGN */
		SYMB_UNICODE(0xB5,0x221D); /* PROPORTIONAL TO */
		SYMB_UNICODE(0xB6,0x2202); /* PARTIAL DIFFERENTIAL */
		SYMB_UNICODE(0xB7,0x2022); /* BULLET */
		SYMB_UNICODE(0xB8,0x00F7); /* DIVISION SIGN */
		SYMB_UNICODE(0xB9,0x2260); /* NOT EQUAL TO */
		SYMB_UNICODE(0xBA,0x2261); /* IDENTICAL TO */
		SYMB_UNICODE(0xBB,0x2248); /* ALMOST EQUAL TO */
		SYMB_UNICODE(0xBC,0x2026); /* HORIZONTAL ELLIPSIS */
		SYMB_UNICODE(0xBD,0x23D0); /* VERTICAL LINE EXTENSION (for arrows) */
		SYMB_UNICODE(0xBE,0x23AF); /* HORIZONTAL LINE EXTENSION (for arrows) */
		SYMB_UNICODE(0xBF,0x21B5); /* DOWNWARDS ARROW WITH CORNER LEFTWARDS */
		SYMB_UNICODE(0xC0,0x2135); /* ALEF SYMBOL */
		SYMB_UNICODE(0xC1,0x2111); /* BLACK-LETTER CAPITAL I */
		SYMB_UNICODE(0xC2,0x211C); /* BLACK-LETTER CAPITAL R */
		SYMB_UNICODE(0xC3,0x2118); /* SCRIPT CAPITAL P */
		SYMB_UNICODE(0xC4,0x2297); /* CIRCLED TIMES */
		SYMB_UNICODE(0xC5,0x2295); /* CIRCLED PLUS */
		SYMB_UNICODE(0xC6,0x2205); /* EMPTY SET */
		SYMB_UNICODE(0xC7,0x2229); /* INTERSECTION */
		SYMB_UNICODE(0xC8,0x222A); /* UNION */
		SYMB_UNICODE(0xC9,0x2283); /* SUPERSET OF */
		SYMB_UNICODE(0xCA,0x2287); /* SUPERSET OF OR EQUAL TO */
		SYMB_UNICODE(0xCB,0x2284); /* NOT A SUBSET OF */
		SYMB_UNICODE(0xCC,0x2282); /* SUBSET OF */
		SYMB_UNICODE(0xCD,0x2286); /* SUBSET OF OR EQUAL TO */
		SYMB_UNICODE(0xCE,0x2208); /* ELEMENT OF */
		SYMB_UNICODE(0xCF,0x2209); /* NOT AN ELEMENT OF */
		SYMB_UNICODE(0xD0,0x2220); /* ANGLE */
		SYMB_UNICODE(0xD1,0x2207); /* NABLA */
		SYMB_UNICODE(0xD2,0x00AE); /* REGISTERED SIGN serif */
		SYMB_UNICODE(0xD3,0x00A9); /* COPYRIGHT SIGN serif */
		SYMB_UNICODE(0xD4,0x2122); /* TRADE MARK SIGN serif */
		SYMB_UNICODE(0xD5,0x220F); /* N-ARY PRODUCT */
		SYMB_UNICODE(0xD6,0x221A); /* SQUARE ROOT */
		SYMB_UNICODE(0xD7,0x22C5); /* DOT OPERATOR */
		SYMB_UNICODE(0xD8,0x00AC); /* NOT SIGN */
		SYMB_UNICODE(0xD9,0x2227); /* LOGICAL AND */
		SYMB_UNICODE(0xDA,0x2228); /* LOGICAL OR */
		SYMB_UNICODE(0xDB,0x21D4); /* LEFT RIGHT DOUBLE ARROW */
		SYMB_UNICODE(0xDC,0x21D0); /* LEFTWARDS DOUBLE ARROW */
		SYMB_UNICODE(0xDD,0x21D1); /* UPWARDS DOUBLE ARROW */
		SYMB_UNICODE(0xDE,0x21D2); /* RIGHTWARDS DOUBLE ARROW */
		SYMB_UNICODE(0xDF,0x21D3); /* DOWNWARDS DOUBLE ARROW */
		SYMB_UNICODE(0xE0,0x25CA); /* LOZENGE previously mapped to 0x22C4 DIAMOND OPERATOR */
		SYMB_UNICODE(0xE1,0x3008); /* LEFT ANGLE BRACKET */
		SYMB_UNICODE(0xE5,0x2211); /* N-ARY SUMMATION */
		SYMB_UNICODE(0xE6,0x239B); /* LEFT PARENTHESIS UPPER HOOK */
		SYMB_UNICODE(0xE7,0x239C); /* LEFT PARENTHESIS EXTENSION */
		SYMB_UNICODE(0xE8,0x239D); /* LEFT PARENTHESIS LOWER HOOK */
		SYMB_UNICODE(0xE9,0x23A1); /* LEFT SQUARE BRACKET UPPER CORNER */
		SYMB_UNICODE(0xEA,0x23A2); /* LEFT SQUARE BRACKET EXTENSION */
		SYMB_UNICODE(0xEB,0x23A3); /* LEFT SQUARE BRACKET LOWER CORNER */
		SYMB_UNICODE(0xEC,0x23A7); /* LEFT CURLY BRACKET UPPER HOOK */
		SYMB_UNICODE(0xED,0x23A8); /* LEFT CURLY BRACKET MIDDLE PIECE */
		SYMB_UNICODE(0xEE,0x23A9); /* LEFT CURLY BRACKET LOWER HOOK */
		SYMB_UNICODE(0xEF,0x23AA); /* CURLY BRACKET EXTENSION */
		SYMB_UNICODE(0xF0,0xF8FF); /* Apple logo */
		SYMB_UNICODE(0xF1,0x3009); /* RIGHT ANGLE BRACKET */
		SYMB_UNICODE(0xF2,0x222B); /* INTEGRAL */
		SYMB_UNICODE(0xF3,0x2320); /* TOP HALF INTEGRAL */
		SYMB_UNICODE(0xF4,0x23AE); /* INTEGRAL EXTENSION */
		SYMB_UNICODE(0xF5,0x2321); /* BOTTOM HALF INTEGRAL */
		SYMB_UNICODE(0xF6,0x239E); /* RIGHT PARENTHESIS UPPER HOOK */
		SYMB_UNICODE(0xF7,0x239F); /* RIGHT PARENTHESIS EXTENSION */
		SYMB_UNICODE(0xF8,0x23A0); /* RIGHT PARENTHESIS LOWER HOOK */
		SYMB_UNICODE(0xF9,0x23A4); /* RIGHT SQUARE BRACKET UPPER CORNER */
		SYMB_UNICODE(0xFA,0x23A5); /* RIGHT SQUARE BRACKET EXTENSION */
		SYMB_UNICODE(0xFB,0x23A6); /* RIGHT SQUARE BRACKET LOWER CORNER */
		SYMB_UNICODE(0xFC,0x23AB); /* RIGHT CURLY BRACKET UPPER HOOK */
		SYMB_UNICODE(0xFD,0x23AC); /* RIGHT CURLY BRACKET MIDDLE PIECE */
		SYMB_UNICODE(0xFE,0x23AD); /* RIGHT CURLY BRACKET LOWER HOOK */

		/* to be treated specifically : composed characters */
		case 0xE2 : /* REGISTERED SIGN, alternate: sans serif */
			g_unichar_to_utf8(0x00AE,iter_mod);
			iter_mod = g_utf8_next_char(iter_mod);
			g_unichar_to_utf8(0xF87F,iter_mod);
			break;
		case 0xE3 : /* COPYRIGHT SIGN, alternate: sans serif */
			g_unichar_to_utf8(0x00A9,iter_mod);
			iter_mod = g_utf8_next_char(iter_mod);
			g_unichar_to_utf8(0xF87F,iter_mod);
			break;
		case 0xE4 : /* TRADE MARK SIGN, alternate: sans serif */
			g_unichar_to_utf8(0x2122,iter_mod);
			iter_mod = g_utf8_next_char(iter_mod);
			g_unichar_to_utf8(0xF87F,iter_mod);
			break;
		default : g_unichar_to_utf8( g_utf8_get_char(iter), iter_mod); break;
		}
		iter = g_utf8_next_char(iter);
		iter_mod = g_utf8_next_char(iter_mod);
	}

	g_free(string_utf8);
	return output;
}
