#ifndef lint
static char *RCSid() { return RCSid("$Id: gadgets.c,v 1.115.2.7 2016/08/27 20:50:12 sfeam Exp $"); }
#endif

/* GNUPLOT - gadgets.c */

/*[
 * Copyright 2000, 2004   Thomas Williams, Colin Kelley
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

#include "gadgets.h"
#include "alloc.h"
#include "command.h"
#include "graph3d.h" /* for map3d_position_r() */
#include "graphics.h"
#include "plot3d.h" /* For is_plot_with_palette() */
#include "axis.h" /* For CB_AXIS */

#include "pm3d.h"

/* This file contains mainly a collection of global variables that
 * used to be in 'set.c', where they didn't really belong. They
 * describe the status of several parts of the gnuplot plotting engine
 * that are used by both 2D and 3D plots, and thus belong neither to
 * graphics.c nor graph3d.c, alone. This is not a very clean solution,
 * but better than mixing internal status and the user interface as we
 * used to have it, in set.c and setshow.h */

legend_key keyT = DEFAULT_KEY_PROPS;

/* Description of the color box associated with CB_AXIS */
color_box_struct color_box; /* initialized in init_color() */
color_box_struct default_color_box = {SMCOLOR_BOX_DEFAULT, 'v', 1, LT_BLACK, LAYER_FRONT, 0,
					{screen, screen, screen, 0.90, 0.2, 0.0},
					{screen, screen, screen, 0.05, 0.6, 0.0}, FALSE,
					{0,0,0,0} };

/* The graph box, in terminal coordinates, as calculated by boundary()
 * or boundary3d(): */
BoundingBox plot_bounds;

/* The bounding box for the entire drawable area  of current terminal */
BoundingBox canvas;

/* The bounding box against which clipping is to be done */
BoundingBox *clip_area = &plot_bounds;

/* 'set size', 'set origin' setttings */
float xsize = 1.0;		/* scale factor for size */
float ysize = 1.0;		/* scale factor for size */
float zsize = 1.0;		/* scale factor for size */
float xoffset = 0.0;		/* x origin */
float yoffset = 0.0;		/* y origin */
float aspect_ratio = 0.0;	/* don't attempt to force it */
int aspect_ratio_3D = 0;	/* 2 will put x and y on same scale, 3 for z also */

/* EAM Augest 2006 - 
   redefine margin as t_position so that absolute placement is possible */
/* space between left edge and plot_bounds.xleft in chars (-1: computed) */
t_position lmargin = DEFAULT_MARGIN_POSITION;
/* space between bottom and plot_bounds.ybot in chars (-1: computed) */
t_position bmargin = DEFAULT_MARGIN_POSITION;
/* space between right egde and plot_bounds.xright in chars (-1: computed) */
t_position rmargin = DEFAULT_MARGIN_POSITION;
/* space between top egde and plot_bounds.ytop in chars (-1: computed) */
t_position tmargin = DEFAULT_MARGIN_POSITION;

/* Pointer to first 'set dashtype' definition in linked list */
struct custom_dashtype_def *first_custom_dashtype = NULL;

/* Pointer to the start of the linked list of 'set label' definitions */
struct text_label *first_label = NULL;

/* Pointer to first 'set linestyle' definition in linked list */
struct linestyle_def *first_linestyle = NULL;
struct linestyle_def *first_perm_linestyle = NULL;
struct linestyle_def *first_mono_linestyle = NULL;

/* Pointer to first 'set style arrow' definition in linked list */
struct arrowstyle_def *first_arrowstyle = NULL;

/* Holds the properties from 'set style parallelaxis' */
struct pa_style parallel_axis_style = DEFAULT_PARALLEL_AXIS_STYLE;

/* set arrow */
struct arrow_def *first_arrow = NULL;

#ifdef EAM_OBJECTS
/* Pointer to first object instance in linked list */
struct object *first_object = NULL;
#endif

/* 'set title' status */
text_label title = EMPTY_LABELSTRUCT;

/* 'set timelabel' status */
text_label timelabel = EMPTY_LABELSTRUCT;
int timelabel_rotate = FALSE;
int timelabel_bottom = TRUE;

/* flag for polar mode */
TBOOLEAN polar = FALSE;

/* zero threshold, may _not_ be 0! */
double zero = ZERO;

/* Status of 'set pointsize' and 'set pointintervalbox' commands */
double pointsize = 1.0;
double pointintervalbox = 1.0;

/* set border */
int draw_border = 31;	/* The current settings */
int user_border = 31;	/* What the user last set explicitly */
int border_layer = LAYER_FRONT;
# define DEFAULT_BORDER_LP { 0, LT_BLACK, 0, DASHTYPE_SOLID, 0, 1.0, 1.0, 0, BLACK_COLORSPEC, DEFAULT_DASHPATTERN }
struct lp_style_type border_lp = DEFAULT_BORDER_LP;
const struct lp_style_type default_border_lp = DEFAULT_BORDER_LP;
const struct lp_style_type background_lp = {0, LT_BACKGROUND, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, 0, BACKGROUND_COLORSPEC, DEFAULT_DASHPATTERN};

/* set clip */
TBOOLEAN clip_lines1 = TRUE;
TBOOLEAN clip_lines2 = FALSE;
TBOOLEAN clip_points = FALSE;

static int clip_line __PROTO((int *, int *, int *, int *));

/* set samples */
int samples_1 = SAMPLES;
int samples_2 = SAMPLES;

/* set angles */
double ang2rad = 1.0;		/* 1 or pi/180, tracking angles_format */

enum PLOT_STYLE data_style = POINTSTYLE;
enum PLOT_STYLE func_style = LINES;

TBOOLEAN parametric = FALSE;

/* If last plot was a 3d one. */
TBOOLEAN is_3d_plot = FALSE;

/* Flag to signal that the existing data is valid for a quick refresh */
TRefresh_Allowed refresh_ok = E_REFRESH_NOT_OK;
/* FIXME: do_plot should be able to figure this out on its own! */
int refresh_nplots = 0;
/* Flag to show that volatile input data is present */
TBOOLEAN volatile_data = FALSE;

fill_style_type default_fillstyle = { FS_EMPTY, 100, 0, DEFAULT_COLORSPEC } ;

#ifdef EAM_OBJECTS
/* Default rectangle style - background fill, black border */
struct object default_rectangle = DEFAULT_RECTANGLE_STYLE;
struct object default_circle = DEFAULT_CIRCLE_STYLE;
struct object default_ellipse = DEFAULT_ELLIPSE_STYLE;
#endif

/* filledcurves style options */
filledcurves_opts filledcurves_opts_data = EMPTY_FILLEDCURVES_OPTS;
filledcurves_opts filledcurves_opts_func = EMPTY_FILLEDCURVES_OPTS;

#if TRUE || defined(BACKWARDS_COMPATIBLE)
/* Prefer line styles over plain line types */
TBOOLEAN prefer_line_styles = FALSE;
#endif

/* If current terminal claims to be monochrome, don't try to send it colors */
#define monochrome_terminal ((t->flags & TERM_MONOCHROME) != 0)

histogram_style histogram_opts = DEFAULT_HISTOGRAM_STYLE;

boxplot_style boxplot_opts = DEFAULT_BOXPLOT_STYLE;

/* WINDOWID to be filled by terminals running on X11 (x11, wxt, qt, ...) */
int current_x11_windowid = 0;

#ifdef EAM_BOXED_TEXT
textbox_style textbox_opts = DEFAULT_TEXTBOX_STYLE;
#endif

/*****************************************************************/
/* Routines that deal with global objects defined in this module */
/*****************************************************************/

/* Clipping to the bounding box: */

/* Test a single point to be within the BoundingBox.
 * Sets the returned integers 4 l.s.b. as follows:
 * bit 0 if to the left of xleft.
 * bit 1 if to the right of xright.
 * bit 2 if below of ybot.
 * bit 3 if above of ytop.
 * 0 is returned if inside.
 */
int
clip_point(unsigned int x, unsigned int y)
{
    int ret_val = 0;

    if (!clip_area)
	return 0;
    if ((int)x < clip_area->xleft)
	ret_val |= 0x01;
    if ((int)x > clip_area->xright)
	ret_val |= 0x02;
    if ((int)y < clip_area->ybot)
	ret_val |= 0x04;
    if ((int)y > clip_area->ytop)
	ret_val |= 0x08;

    return ret_val;
}

/* Clip the given line to drawing coords defined by BoundingBox.
 *   This routine uses the cohen & sutherland bit mapping for fast clipping -
 * see "Principles of Interactive Computer Graphics" Newman & Sproull page 65.
 */
void
draw_clip_line(int x1, int y1, int x2, int y2)
{
    struct termentry *t = term;

    if (!clip_line(&x1, &y1, &x2, &y2))
	/* clip_line() returns zero --> segment completely outside
	 * bounding box */
	return;

    (*t->move) (x1, y1);
    (*t->vector) (x2, y2);
}

/* Draw a contiguous line path which may be clipped. Compared to
 * draw_clip_line(), this routine moves to a coordinate only when
 * necessary.
 */
void 
draw_clip_polygon(int points, gpiPoint *p) 
{
    int i;
    int x1, y1, x2, y2;
    int pos1, pos2, clip_ret;
    struct termentry *t = term;

    if (points <= 1) 
	return;

    x1 = p[0].x;
    y1 = p[0].y;
    pos1 = clip_point(x1, y1);
    if (!pos1) /* move to first point if it is inside */
	(*t->move)(x1, y1);

    for (i = 1; i < points; i++) {
	x2 = p[i].x;
	y2 = p[i].y;
	pos2 = clip_point(x2, y2);
	clip_ret = clip_line(&x1, &y1, &x2, &y2);

	if (clip_ret) {
	    /* there is a line to draw */
	    if (pos1) /* first vertex was recalculated, move to new start point */
		(*t->move)(x1, y1);
	    (*t->vector)(x2, y2);
	}

	x1 = p[i].x;
	y1 = p[i].y;
	/* The end point and the line do not necessarily have the same
	 * status. The end point can be 'inside', but the whole line is
	 * 'outside'. Do not update pos1 in this case.  Bug #1268.
	 * FIXME: This is papering over an inconsistency in coordinate
	 * calculation somewhere else!
	 */
	if (!(clip_ret == 0 && pos2 == 0))
	    pos1 = pos2;
    }
}

void
draw_clip_arrow( int sx, int sy, int ex, int ey, int head)
{
    struct termentry *t = term;

    /* Don't draw head if the arrow itself is clipped */
    if (clip_point(sx,sy))
	head &= ~BACKHEAD;
    if (clip_point(ex,ey))
	head &= ~END_HEAD;
    clip_line(&sx, &sy, &ex, &ey);

    /* Call terminal routine to draw the clipped arrow */
    (*t->arrow)((unsigned int)sx, (unsigned int)sy,
		(unsigned int)ex, (unsigned int)ey, head);
}

/* Clip the given line to drawing coords defined by BoundingBox.
 *   This routine uses the cohen & sutherland bit mapping for fast clipping -
 * see "Principles of Interactive Computer Graphics" Newman & Sproull page 65.
 * Return 0: entire line segment is outside bounding box
 *        1: entire line segment is inside bounding box
 *       -1: line segment has been clipped to bounding box
 */
int
clip_line(int *x1, int *y1, int *x2, int *y2)
{
    /* Apr 2014: This algorithm apparently assumed infinite precision
     * integer arithmetic. It was failing when passed coordinates that
     * were hugely out of bounds because tests for signedness of the
     * form (dx * dy > 0) would overflow rather than correctly evaluating
     * to (sign(dx) == sign(dy)).  Worse yet, the numerical values are
     * used to determine which end of the segment to update.
     * This is now addressed by making dx and dy (double) rather than (int)
     * but it might be better to hard-code the sign tests.
     */
    double dx, dy;
    double x, y;

    int x_intr[4], y_intr[4], count, pos1, pos2;
    int x_max, x_min, y_max, y_min;
    pos1 = clip_point(*x1, *y1);
    pos2 = clip_point(*x2, *y2);
    if (!pos1 && !pos2)
	return 1;		/* segment is totally in */
    if (pos1 & pos2)
	return 0;		/* segment is totally out. */
    /* Here part of the segment MAY be inside. test the intersection
     * of this segment with the 4 boundaries for hopefully 2 intersections
     * in. If none are found segment is totaly out.
     * Under rare circumstances there may be up to 4 intersections (e.g.
     * when the line passes directly through at least one corner).
     */
    count = 0;
    dx = *x2 - *x1;
    dy = *y2 - *y1;
    /* Find intersections with the x parallel bbox lines: */
    if (dy != 0) {
	x = (clip_area->ybot - *y2) * dx / dy + *x2;	/* Test for clip_area->ybot boundary. */
	if (x >= clip_area->xleft && x <= clip_area->xright) {
	    x_intr[count] = x;
	    y_intr[count++] = clip_area->ybot;
	}
	x = (clip_area->ytop - *y2) * dx / dy + *x2;	/* Test for clip_area->ytop boundary. */
	if (x >= clip_area->xleft && x <= clip_area->xright) {
	    x_intr[count] = x;
	    y_intr[count++] = clip_area->ytop;
	}
    }
    /* Find intersections with the y parallel bbox lines: */
    if (dx != 0) {
	y = (clip_area->xleft - *x2) * dy / dx + *y2;	/* Test for clip_area->xleft boundary. */
	if (y >= clip_area->ybot && y <= clip_area->ytop) {
	    x_intr[count] = clip_area->xleft;
	    y_intr[count++] = y;
	}
	y = (clip_area->xright - *x2) * dy / dx + *y2;	/* Test for clip_area->xright boundary. */
	if (y >= clip_area->ybot && y <= clip_area->ytop) {
	    x_intr[count] = clip_area->xright;
	    y_intr[count++] = y;
	}
    }
    if (count < 2)
	return 0;

    /* check which intersections to use, for more than two intersections the first two may be identical */
    if ((count > 2) && (x_intr[0] == x_intr[1]) && (y_intr[0] == y_intr[1])) {
	x_intr[1] = x_intr[2];
	y_intr[1] = y_intr[2];
    }	

    if (*x1 < *x2) {
	x_min = *x1;
	x_max = *x2;
    } else {
	x_min = *x2;
	x_max = *x1;
    }
    if (*y1 < *y2) {
	y_min = *y1;
	y_max = *y2;
    } else {
	y_min = *y2;
	y_max = *y1;
    }

    if (pos1 && pos2) {		/* Both were out - update both */
	/* EAM Sep 2008 - preserve direction of line segment */
	if ((dx*(x_intr[1]-x_intr[0]) < 0)
	||  (dy*(y_intr[1]-y_intr[0]) < 0)) {
	    *x1 = x_intr[1];
	    *y1 = y_intr[1];
	    *x2 = x_intr[0];
	    *y2 = y_intr[0];
	} else {
	    *x1 = x_intr[0];
	    *y1 = y_intr[0];
	    *x2 = x_intr[1];
	    *y2 = y_intr[1];
	}
    } else if (pos1) {		/* Only x1/y1 was out - update only it */
	/* Nov 2010: When clip_line() and draw_clip_line() were consolidated in */
	/* 2000, the test below was the only point of difference between them.  */
	/* Unfortunately, the wrong version was kept. Now I change it back.     */
	/* The effect of the wrong version (>= rather than >) was that a line   */
	/* from ymin to ymax+eps was clipped to ymin,ymin rather than ymin,ymax */
	if (dx * (*x2 - x_intr[0]) + dy * (*y2 - y_intr[0]) > 0) {
	    *x1 = x_intr[0];
	    *y1 = y_intr[0];
	} else {
	    *x1 = x_intr[1];
	    *y1 = y_intr[1];
	}
    } else {			/* Only x2/y2 was out - update only it */
	/* Same difference here, again */
	if (dx * (x_intr[0] - *x1) + dy * (y_intr[0] - *y1) > 0) {
	    *x2 = x_intr[0];
	    *y2 = y_intr[0];
	} else {
	    *x2 = x_intr[1];
	    *y2 = y_intr[1];
	}
    }

    if (*x1 < x_min || *x1 > x_max || *x2 < x_min || *x2 > x_max || *y1 < y_min || *y1 > y_max || *y2 < y_min || *y2 > y_max)
	return 0;

    return -1;
}

/* test if coordinates of a vertex are inside boundary box. The start
   and end points for the clip_boundary must be in correct order for
   this to work properly (see respective definitions in clip_polygon()). */
TBOOLEAN
vertex_is_inside(gpiPoint test_vertex, gpiPoint *clip_boundary)
{
    if (clip_boundary[1].x > clip_boundary[0].x)              /*bottom edge*/
	if (test_vertex.y >= clip_boundary[0].y) return TRUE;
    if (clip_boundary[1].x < clip_boundary[0].x)              /*top edge*/
	if (test_vertex.y <= clip_boundary[0].y) return TRUE;
    if (clip_boundary[1].y > clip_boundary[0].y)              /*right edge*/
	if (test_vertex.x <= clip_boundary[1].x) return TRUE;
    if (clip_boundary[1].y < clip_boundary[0].y)              /*left edge*/
	if (test_vertex.x >= clip_boundary[1].x) return TRUE;
    return FALSE;
} 

void
intersect_polyedge_with_boundary(gpiPoint first, gpiPoint second, gpiPoint *intersect, gpiPoint *clip_boundary)
{
    /* this routine is called only if one point is outside and the other
       is inside, which implies that clipping is needed at a horizontal
       boundary, that second.y is different from first.y and no division
       by zero occurs. Same for vertical boundary and x coordinates. */
    if (clip_boundary[0].y == clip_boundary[1].y) { /* horizontal */
	(*intersect).y = clip_boundary[0].y;
	(*intersect).x = first.x + (clip_boundary[0].y - first.y) * (second.x - first.x)/(second.y - first.y);
    } else { /* vertical */
	(*intersect).x = clip_boundary[0].x;
	(*intersect).y = first.y + (clip_boundary[0].x - first.x) * (second.y - first.y)/(second.x - first.x);
    }
}

/* Clip the given polygon to a single edge of the bounding box. */
void 
clip_polygon_to_boundary(gpiPoint *in, gpiPoint *out, int in_length, int *out_length, gpiPoint *clip_boundary)
{
    gpiPoint prev, curr; /* start and end point of current polygon edge. */
    int j;

    *out_length = 0;
    if (in_length <= 0)
	return;
    else
	prev = in[in_length - 1]; /* start with the last vertex */

    for (j = 0; j < in_length; j++) {
	curr = in[j];
	if (vertex_is_inside(curr, clip_boundary)) {
	    if (vertex_is_inside(prev, clip_boundary)) {
		/* both are inside, add current vertex */
		out[*out_length] = in[j];
		(*out_length)++;
	    } else {
		/* changed from outside to inside, add intersection point and current point */
		intersect_polyedge_with_boundary(prev, curr, out+(*out_length), clip_boundary);
		out[*out_length+1] = curr;
		*out_length += 2;
	    }
	} else {
	    if (vertex_is_inside(prev, clip_boundary)) {
		/* changed from inside to outside, add intersection point */
		intersect_polyedge_with_boundary(prev, curr, out+(*out_length), clip_boundary);
		(*out_length)++;
	    }
	}
	prev = curr;
    }
}

/* Clip the given polygon to drawing coords defined by BoundingBox.
 * This routine uses the Sutherland-Hodgman algorithm.  When calling
 * this function, you must make sure that you reserved enough
 * memory for the output polygon. out_length can be as big as
 * 2*(in_length - 1)
 */
void
clip_polygon(gpiPoint *in, gpiPoint *out, int in_length, int *out_length)
{
    int i;
    gpiPoint clip_boundary[5];
    static gpiPoint *tmp_corners = NULL;

    if (!clip_area) {
	memcpy(out, in, in_length * sizeof(gpiPoint));
	*out_length = in_length;
	return;
    }
    tmp_corners = gp_realloc(tmp_corners, 2 * in_length * sizeof(gpiPoint), "clip_polygon");

    /* vertices of the rectangular clipping window starting from
       top-left in counterclockwise direction */
    clip_boundary[0].x = clip_area->xleft;  /* top left */
    clip_boundary[0].y = clip_area->ytop;
    clip_boundary[1].x = clip_area->xleft;  /* bottom left */
    clip_boundary[1].y = clip_area->ybot;
    clip_boundary[2].x = clip_area->xright; /* bottom right */
    clip_boundary[2].y = clip_area->ybot;
    clip_boundary[3].x = clip_area->xright; /* top right */
    clip_boundary[3].y = clip_area->ytop;
    clip_boundary[4] = clip_boundary[0];

    memcpy(tmp_corners, in, in_length * sizeof(gpiPoint));
    for(i = 0; i < 4; i++) {
	clip_polygon_to_boundary(tmp_corners, out, in_length, out_length, clip_boundary+i);
	memcpy(tmp_corners, out, *out_length * sizeof(gpiPoint));
	in_length = *out_length;
    }
}

/* Two routines to emulate move/vector sequence using line drawing routine. */
static unsigned int move_pos_x, move_pos_y;

void
clip_move(unsigned int x, unsigned int y)
{
    move_pos_x = x;
    move_pos_y = y;
}

void
clip_vector(unsigned int x, unsigned int y)
{
    draw_clip_line(move_pos_x, move_pos_y, x, y);
    move_pos_x = x;
    move_pos_y = y;
}

/* Common routines for setting text or line color from t_colorspec */

void
apply_pm3dcolor(struct t_colorspec *tc, const struct termentry *t)
{
    /* V5 - term->linetype(LT_BLACK) would clobber the current	*/
    /* dashtype so instead we use term->set_color(black).	*/
    static t_colorspec black = BLACK_COLORSPEC; 
    double cbval;

    /* Replace colorspec with that of the requested line style */
    struct lp_style_type style;
    if (tc->type == TC_LINESTYLE) {
	lp_use_properties(&style, tc->lt);
	tc = &style.pm3d_color;
    }

    if (tc->type == TC_DEFAULT) {
	t->set_color(&black);
	return;
    }
    if (tc->type == TC_LT) {
	/* Removed Jan 2015 
	if (!monochrome_terminal)
	 */
	    t->set_color(tc);
	return;
    }
    if (tc->type == TC_RGB) {
	/* FIXME: several plausible ways for monochrome terminals to handle color request
	 * (1) Allow all color requests despite the label "monochrome"
	 * (2) Choose any color you want so long as it is black
	 * (3) Convert colors to gray scale (NTSC?)
	 */
	/* Monochrome terminals are still allowed to display rgb variable colors */
	if (monochrome_terminal && tc->value >= 0)
	    t->set_color(&black);
	else
	    t->set_color(tc);
	return;
    }
    if (!is_plot_with_palette()) {
	t->set_color(&black);
	return;
    }
    switch (tc->type) {
	case TC_Z:
		set_color(cb2gray(z2cb(tc->value)));
		break;
	case TC_CB:
		if (CB_AXIS.log)
		    cbval = (tc->value <= 0) ? CB_AXIS.min : (log(tc->value) / CB_AXIS.log_base);
		else
		    cbval = tc->value;
		set_color(cb2gray(cbval));
		break;
	case TC_FRAC:
		set_color(sm_palette.positive == SMPAL_POSITIVE ?  tc->value : 1-tc->value);
		break;
    }
}

void
reset_textcolor(const struct t_colorspec *tc, const struct termentry *t)
{
    if (tc->type != TC_DEFAULT)
	(*t->linetype)(LT_BLACK);
}


void
default_arrow_style(struct arrow_style_type *arrow)
{
    static const struct lp_style_type tmp_lp_style = DEFAULT_LP_STYLE_TYPE;

    arrow->tag = -1;
    arrow->layer = LAYER_BACK;
    arrow->lp_properties = tmp_lp_style;
    arrow->head = 1;
    arrow->head_length = 0.0;
    arrow->head_lengthunit = first_axes;
    arrow->head_angle = 15.0;
    arrow->head_backangle = 90.0;
    arrow->headfill = AS_NOFILL;
    arrow->head_fixedsize = FALSE;
}

void
apply_head_properties(struct arrow_style_type *arrow_properties)
{
    curr_arrow_headfilled = arrow_properties->headfill;
    curr_arrow_headfixedsize = arrow_properties->head_fixedsize;
    curr_arrow_headlength = 0;
    if (arrow_properties->head_length > 0) {
	/* set head length+angle for term->arrow */
	double xtmp, ytmp;
	struct position headsize = {first_axes,graph,graph,0.,0.,0.};

	headsize.x = arrow_properties->head_length;
	headsize.scalex = arrow_properties->head_lengthunit;

	map_position_r(&headsize, &xtmp, &ytmp, "arrow");

	curr_arrow_headangle = arrow_properties->head_angle;
	curr_arrow_headbackangle = arrow_properties->head_backangle;
	curr_arrow_headlength = xtmp;
    }
}

void
free_labels(struct text_label *label)
{
struct text_label *temp;
char *master_font = label->font;

    /* Labels generated by 'plot with labels' all use the same font */
    if (master_font)
    	free(master_font);

    while (label) {
	if (label->text)
	    free(label->text);
	if (label->font && label->font != master_font)
	    free(label->font);
	temp=label->next;
	free(label);
	label = temp;
    }

}

void
get_offsets(
    struct text_label *this_label,
    struct termentry *t,
    int *htic, int *vtic)
{
    if ((this_label->lp_properties.flags & LP_SHOW_POINTS)) {
	*htic = (pointsize * t->h_tic * 0.5);
	*vtic = (pointsize * t->v_tic * 0.5);
    } else {
	*htic = 0;
	*vtic = 0;
    }
    if (is_3d_plot) {
	int htic2, vtic2;
	map3d_position_r(&(this_label->offset), &htic2, &vtic2, "get_offsets");
	*htic += htic2;
	*vtic += vtic2;
    } else {
	double htic2, vtic2;
	map_position_r(&(this_label->offset), &htic2, &vtic2, "get_offsets");
	*htic += (int)htic2;
	*vtic += (int)vtic2;
    }
}


/*
 * Write one label, with all the trimmings.
 * This routine is used for both 2D and 3D plots.
 */
void
write_label(unsigned int x, unsigned int y, struct text_label *this_label)
{
	int htic, vtic;
	int justify = JUST_TOP;	/* This was the 2D default; 3D had CENTRE */

	apply_pm3dcolor(&(this_label->textcolor),term);
	ignore_enhanced(this_label->noenhanced);

	/* The text itself */
	if (this_label->hypertext) {
	    /* Treat text as hypertext */
	    char *font = this_label->font;
	    if (font)
		term->set_font(font);
	    if (term->hypertext)
	        term->hypertext(TERM_HYPERTEXT_TOOLTIP, this_label->text);
	    if (font)
		term->set_font("");
	} else {
	    /* A normal label (always print text) */
	    get_offsets(this_label, term, &htic, &vtic);
#ifdef EAM_BOXED_TEXT
	    /* Initialize the bounding box accounting */
	    if (this_label->boxed && term->boxed_text)
		(*term->boxed_text)(x + htic, y + vtic, TEXTBOX_INIT);
#endif
	    if (this_label->rotate && (*term->text_angle) (this_label->rotate)) {
		write_multiline(x + htic, y + vtic, this_label->text,
				this_label->pos, justify, this_label->rotate,
				this_label->font);
		(*term->text_angle) (0);
	    } else {
		write_multiline(x + htic, y + vtic, this_label->text,
				this_label->pos, justify, 0, this_label->font);
	    }
	}
#ifdef EAM_BOXED_TEXT
	if (this_label->boxed && term->boxed_text) {

	    /* Adjust the bounding box margins */
	    (*term->boxed_text)((int)(textbox_opts.xmargin * 100.),
		(int)(textbox_opts.ymargin * 100.), TEXTBOX_MARGINS);

	    /* Blank out the box and reprint the label */
	    if (textbox_opts.opaque) {
		(*term->boxed_text)(0,0, TEXTBOX_BACKGROUNDFILL);
		if (this_label->rotate && (*term->text_angle) (this_label->rotate)) {
		    write_multiline(x + htic, y + vtic, this_label->text,
				this_label->pos, justify, this_label->rotate,
				this_label->font);
		    (*term->text_angle) (0);
		} else {
		    write_multiline(x + htic, y + vtic, this_label->text,
				this_label->pos, justify, 0, this_label->font);
		}
	    }

	    /* Draw the bounding box - FIXME should set line properties first */
	    if (!textbox_opts.noborder)
		(*term->boxed_text)(0,0, TEXTBOX_OUTLINE);

	    (*term->boxed_text)(0,0, TEXTBOX_FINISH);
	}
#endif

	/* The associated point, if any */
	/* write_multiline() clips text to on_page; do the same for any point */
	if ((this_label->lp_properties.flags & LP_SHOW_POINTS) && on_page(x,y)) {
	    term_apply_lp_properties(&this_label->lp_properties);
	    (*term->point) (x, y, this_label->lp_properties.p_type);
	    /* the default label color is that of border */
	    term_apply_lp_properties(&border_lp);
	}

	ignore_enhanced(FALSE);
}


/* STR points to a label string, possibly with several lines separated
   by \n.  Return the number of characters in the longest line.  If
   LINES is not NULL, set *LINES to the number of lines in the
   label. */
int
label_width(const char *str, int *lines)
{
    char *lab = NULL, *s, *e;
    int mlen, len, l;

    if (!str || *str == '\0') {
	if (lines)
	    *lines = 0;
	return (0);
    }

    l = mlen = len = 0;
    lab = gp_alloc(strlen(str) + 2, "in label_width");
    strcpy(lab, str);
    strcat(lab, "\n");
    s = lab;
    while ((e = (char *) strchr(s, '\n')) != NULL) {
	*e = '\0';
	len = estimate_strlen(s);	/* = e-s ? */
	if (len > mlen)
	    mlen = len;
	if (len || l || *str == '\n')
	    l++;
	s = ++e;
    }
    /* lines = NULL => not interested - div */
    if (lines)
	*lines = l;

    free(lab);
    return (mlen);
}

