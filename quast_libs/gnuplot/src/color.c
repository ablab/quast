#ifndef lint
static char *RCSid() { return RCSid("$Id: color.c,v 1.114.2.2 2016/08/27 20:50:12 sfeam Exp $"); }
#endif

/* GNUPLOT - color.c */

/*[
 *
 * Petr Mikulik, since December 1998
 * Copyright: open source as much as possible
 *
 * What is here:
 *   - Global variables declared in .h are initialized here
 *   - Palette routines
 *   - Colour box drawing
 *
]*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "color.h"
#include "getcolor.h"

#include "axis.h"
#include "gadgets.h"
#include "graphics.h"
#include "plot.h"
#include "graph3d.h"
#include "pm3d.h"
#include "term_api.h"
#include "util3d.h"
#include "alloc.h"

/* COLOUR MODES - GLOBAL VARIABLES */

t_sm_palette sm_palette;  /* initialized in plot.c on program entry */

/* Copy of palette previously in use.
 * Exported so that change_term() can invalidate contents
 * FIXME: better naming 
 */
static t_sm_palette prev_palette = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	(rgb_color *) 0, -1
    };

/* Internal prototype declarations: */

static void draw_inside_color_smooth_box_postscript __PROTO((void));
static void draw_inside_color_smooth_box_bitmap __PROTO((void));
void cbtick_callback __PROTO((AXIS_INDEX axis, double place, char *text, int ticlevel,
			struct lp_style_type grid, struct ticmark *userlabels));



/* *******************************************************************
  ROUTINES
 */


void
init_color()
{
  /* initialize global palette */
  sm_palette.colorFormulae = 37;  /* const */
  sm_palette.formulaR = 7;
  sm_palette.formulaG = 5;
  sm_palette.formulaB = 15;
  sm_palette.positive = SMPAL_POSITIVE;
  sm_palette.use_maxcolors = 0;
  sm_palette.colors = 0;
  sm_palette.color = NULL;
  sm_palette.ps_allcF = FALSE;
  sm_palette.gradient_num = 0;
  sm_palette.gradient = NULL;
  sm_palette.cmodel = C_MODEL_RGB;
  sm_palette.Afunc.at = sm_palette.Bfunc.at = sm_palette.Cfunc.at = NULL;
  sm_palette.colorMode = SMPAL_COLOR_MODE_RGB;
  sm_palette.gamma = 1.5;
}


/*
   Make the colour palette. Return 0 on success
   Put number of allocated colours into sm_palette.colors
 */
int
make_palette()
{
    int i;
    double gray;

    if (!term->make_palette) {
	return 1;
    }

    /* ask for suitable number of colours in the palette */
    i = term->make_palette(NULL);
    sm_palette.colors = i;
    if (i == 0) {
	/* terminal with its own mapping (PostScript, for instance)
	   It will not change palette passed below, but non-NULL has to be
	   passed there to create the header or force its initialization
	 */
	if (memcmp(&prev_palette, &sm_palette, sizeof(t_sm_palette))) {
	    term->make_palette(&sm_palette);
	    prev_palette = sm_palette;
	    FPRINTF((stderr,"make_palette: calling term->make_palette for term with ncolors == 0\n"));
	} else {
	    FPRINTF((stderr,"make_palette: skipping duplicate palette for term with ncolors == 0\n"));
	}
	return 0;
    }

    /* set the number of colours to be used (allocated) */
    if (sm_palette.use_maxcolors > 0) {
	if (sm_palette.colorMode == SMPAL_COLOR_MODE_GRADIENT)
	    sm_palette.colors = i;	/* EAM Sep 2010 - could this be a constant? */
	else if (i > sm_palette.use_maxcolors)
	    sm_palette.colors = sm_palette.use_maxcolors;
    }

    if (prev_palette.colorFormulae < 0
	|| sm_palette.colorFormulae != prev_palette.colorFormulae
	|| sm_palette.colorMode != prev_palette.colorMode
	|| sm_palette.formulaR != prev_palette.formulaR
	|| sm_palette.formulaG != prev_palette.formulaG
	|| sm_palette.formulaB != prev_palette.formulaB
	|| sm_palette.positive != prev_palette.positive
	|| sm_palette.colors != prev_palette.colors) {
	/* print the message only if colors have changed */
	if (interactive)
	    fprintf(stderr, "smooth palette in %s: using %i of %i available color positions\n",
	    		term->name, sm_palette.colors, i);
    }

    prev_palette = sm_palette;

    if (sm_palette.color != NULL) {
	free(sm_palette.color);
	sm_palette.color = NULL;
    }
    sm_palette.color = gp_alloc( sm_palette.colors * sizeof(rgb_color),
				 "pm3d palette color");

    /*  fill sm_palette.color[]  */
    for (i = 0; i < sm_palette.colors; i++) {
	gray = (double) i / (sm_palette.colors - 1);	/* rescale to [0;1] */
	rgb1_from_gray( gray, &(sm_palette.color[i]) );
    }

    /* let the terminal make the palette from the supplied RGB triplets */
    term->make_palette(&sm_palette);

    return 0;
}

/*
 * Force a mismatch between the current palette and whatever is sent next,
 * so that the new one will always be loaded 
 */
void
invalidate_palette()
{
    prev_palette.colors = -1;
}

/*
   Set the colour on the terminal
   Each terminal takes care of remembering the current colour,
   so there is not much to do here.
   FIXME: NaN could alternatively map to LT_NODRAW or TC_RGB full transparency
 */
void
set_color(double gray)
{
    t_colorspec color;
    color.value = gray;
    color.lt = LT_BACKGROUND;
    color.type = (isnan(gray)) ? TC_LT : TC_FRAC;
    term->set_color(&color);
}

void
set_rgbcolor_var(unsigned int rgbvalue)
{
    t_colorspec color;
    color.type = TC_RGB;
    *(unsigned int *)(&color.lt) = rgbvalue;
    color.value = -1;	/* -1 flags that this came from "rgb variable" */
    apply_pm3dcolor(&color, term);
}

void
set_rgbcolor_const(unsigned int rgbvalue)
{
    t_colorspec color;
    color.type = TC_RGB;
    *(unsigned int *)(&color.lt) = rgbvalue;
    color.value = 0;	/* 0 flags that this is a constant color */
    apply_pm3dcolor(&color, term);
}

void
ifilled_quadrangle(gpiPoint* icorners)
{
    if (default_fillstyle.fillstyle == FS_EMPTY)
	icorners->style = FS_OPAQUE;
    else
	icorners->style = style_from_fill(&default_fillstyle);
    term->filled_polygon(4, icorners);

    if (pm3d.border.l_type != LT_NODRAW) {
	int i;

	/* It should be sufficient to set only the color, but for some */
	/* reason this causes the svg terminal to lose the fill type.  */
	term_apply_lp_properties(&pm3d_border_lp);

	term->move(icorners[0].x, icorners[0].y);
	for (i = 3; i >= 0; i--) {
	    term->vector(icorners[i].x, icorners[i].y);
	}
    }
}


/* The routine above for 4 points explicitly.
 * This is the only routine which supportes extended
 * color specs currently.
 */
#ifdef EXTENDED_COLOR_SPECS
void
filled_quadrangle(gpdPoint * corners, gpiPoint * icorners)
#else
void
filled_quadrangle(gpdPoint * corners)
#endif
{
    int i;
    double x, y;
#ifndef EXTENDED_COLOR_SPECS
    gpiPoint icorners[4];
#endif
    for (i = 0; i < 4; i++) {
	map3d_xy_double(corners[i].x, corners[i].y, corners[i].z, &x, &y);
	icorners[i].x = x;
	icorners[i].y = y;
    }

    ifilled_quadrangle(icorners);
}

#ifdef PM3D_CONTOURS
/*
   Makes mapping from real 3D coordinates, passed as coords array,
   to 2D terminal coordinates, then draws filled polygon
 */
void
filled_polygon_common(int points, struct coordinate GPHUGE * coords, TBOOLEAN fixed, double z)
{
    int i;
    double x, y;
    gpiPoint *icorners;
    icorners = gp_alloc(points * sizeof(gpiPoint), "filled_polygon3d corners");
    for (i = 0; i < points; i++) {
	if (fixed)
	    z = coords[i].z;
	map3d_xy_double(coords[i].x, coords[i].y, z, &x, &y);
	icorners[i].x = x;
	icorners[i].y = y;
    }
#ifdef EXTENDED_COLOR_SPECS
    if ((term->flags & TERM_EXTENDED_COLOR)) {
	icorners[0].spec.gray = -1;	/* force solid color */
    }
#endif
    if (default_fillstyle.fillstyle == FS_EMPTY)
	icorners->style = FS_OPAQUE;
    else
	icorners->style = style_from_fill(&default_fillstyle);
    term->filled_polygon(points, icorners);
    free(icorners);
}

void
filled_polygon_3dcoords(int points, struct coordinate GPHUGE * coords)
{
    filled_polygon_common(points, coords, FALSE, 0.0);
}

/*
   Makes mapping from real 3D coordinates, passed as coords array, but at z coordinate
   fixed (base_z, for instance) to 2D terminal coordinates, then draws filled polygon
 */
void
filled_polygon_3dcoords_zfixed(int points, struct coordinate GPHUGE * coords, double z)
{
    filled_polygon_common(points, coords, TRUE, z);
}

#endif /* PM3D_CONTOURS */


/*
   Draw colour smooth box

   Firstly two helper routines for plotting inside of the box
   for postscript and for other terminals, finally the main routine
 */


/* plot the colour smooth box for from terminal's integer coordinates
   This routine is for postscript files --- actually, it writes a small
   PS routine.
 */
static void
draw_inside_color_smooth_box_postscript()
{
    int scale_x = (color_box.bounds.xright - color_box.bounds.xleft), scale_y = (color_box.bounds.ytop - color_box.bounds.ybot);
    fputs("stroke gsave\t%% draw gray scale smooth box\n"
	  "maxcolors 0 gt {/imax maxcolors def} {/imax 1024 def} ifelse\n", gppsfile);

    /* nb. of discrete steps (counted in the loop) */
    fprintf(gppsfile, "%i %i translate %i %i scale 0 setlinewidth\n", color_box.bounds.xleft, color_box.bounds.ybot, scale_x, scale_y);
    /* define left bottom corner and scale of the box so that all coordinates
       of the box are from [0,0] up to [1,1]. Further, this normalization
       makes it possible to pass y from [0,1] as parameter to setgray */
    fprintf(gppsfile, "/ystep 1 imax div def /y0 0 def /ii 0 def\n");
    /* local variables; y-step, current y position and counter ii;  */
    if (sm_palette.positive == SMPAL_NEGATIVE)	/* inverted gray for negative figure */
	fputs("{ 0.99999 y0 sub g ", gppsfile); /* 1 > x > 1-1.0/1024 */
    else
	fputs("{ y0 g ", gppsfile);
    if (color_box.rotation == 'v')
	fputs("0 y0 N 1 0 V 0 ystep V -1 0 f\n", gppsfile);
    else
	fputs("y0 0 N 0 1 V ystep 0 V 0 -1 f\n", gppsfile);
    fputs("/y0 y0 ystep add def /ii ii 1 add def\n"
	  "ii imax ge {exit} if } loop\n"
	  "grestore 0 setgray\n", gppsfile);
}



/* plot a colour smooth box bounded by the terminal's integer coordinates
   [x_from,y_from] to [x_to,y_to].
   This routine is for non-postscript files, as it does an explicit loop
   over all thin rectangles
 */
static void
draw_inside_color_smooth_box_bitmap()
{
    int steps = 128; /* I think that nobody can distinguish more colours drawn in the palette */
    int i, j, xy, xy2, xy_from, xy_to;
    int jmin = 0;
    double xy_step, gray, range;
    gpiPoint corners[4];

    if (color_box.rotation == 'v') {
	corners[0].x = corners[3].x = color_box.bounds.xleft;
	corners[1].x = corners[2].x = color_box.bounds.xright;
	xy_from = color_box.bounds.ybot;
	xy_to = color_box.bounds.ytop;
	xy_step = (color_box.bounds.ytop - color_box.bounds.ybot) / (double)steps;
    } else {
	corners[0].y = corners[1].y = color_box.bounds.ybot;
	corners[2].y = corners[3].y = color_box.bounds.ytop;
	xy_from = color_box.bounds.xleft;
	xy_to = color_box.bounds.xright;
	xy_step = (color_box.bounds.xright - color_box.bounds.xleft) / (double)steps;
    }
    range = (xy_to - xy_from);

    for (i = 0, xy2 = xy_from; i < steps; i++) {

	/* Start from one pixel beyond the previous box */
	xy = xy2;
	xy2 = xy_from + (int) (xy_step * (i + 1));

	/* Set the colour for the next range increment */
	/* FIXME - The "1 +" seems wrong, yet it improves the placement in gd */
	gray = (double)(1 + xy - xy_from) / range;
	if (sm_palette.positive == SMPAL_NEGATIVE)
	    gray = 1 - gray;
	set_color(gray);

	/* If this is a defined palette, make sure that the range increment */
	/* does not straddle a palette segment boundary. If it does, split  */
	/* it into two parts.                                               */
	if (sm_palette.colorMode == SMPAL_COLOR_MODE_GRADIENT)
	    for (j=jmin; j<sm_palette.gradient_num; j++) {
		int boundary = xy_from + (int)(sm_palette.gradient[j].pos * range);
		if (xy >= boundary) {
		    jmin = j;
		} else {
		    if (xy2 > boundary) {
			xy2 = boundary;
			i--;
			break;
		    }
		}
		if (xy2 < boundary)
		    break;
	    }

	if (color_box.rotation == 'v') {
	    corners[0].y = corners[1].y = xy;
	    corners[2].y = corners[3].y = GPMIN(xy_to,xy2+1);
	} else {
	    corners[0].x = corners[3].x = xy;
	    corners[1].x = corners[2].x = GPMIN(xy_to,xy2+1);
	}
#ifdef EXTENDED_COLOR_SPECS
	if ((term->flags & TERM_EXTENDED_COLOR))
	    corners[0].spec.gray = -1;	/* force solid color */
#endif
	/* print the rectangle with the given colour */
	if (default_fillstyle.fillstyle == FS_EMPTY)
	    corners->style = FS_OPAQUE;
	else
	    corners->style = style_from_fill(&default_fillstyle);
	term->filled_polygon(4, corners);
    }
}

/* Notice HBB 20010720: would be static, but HP-UX gcc bug forbids
 * this, for now */
void
cbtick_callback(
    AXIS_INDEX axis,
    double place,
    char *text,
    int ticlevel,
    struct lp_style_type grid, /* linetype or -2 for no grid */
    struct ticmark *userlabels)
{
    int len = TIC_SCALE(ticlevel, COLOR_AXIS)
	* (CB_AXIS.tic_in ? -1 : 1) * (term->h_tic);
    double cb_place = (place - CB_AXIS.min) / (CB_AXIS.max - CB_AXIS.min);
	/* relative z position along the colorbox axis */
    unsigned int x1, y1, x2, y2;

    /* calculate tic position */
    if (color_box.rotation == 'h') {
	x1 = x2 = color_box.bounds.xleft + cb_place * (color_box.bounds.xright - color_box.bounds.xleft);
	y1 = color_box.bounds.ybot;
	y2 = color_box.bounds.ybot - len;
    } else {
	x1 = color_box.bounds.xright;
	x2 = color_box.bounds.xright + len;
	y1 = y2 = color_box.bounds.ybot + cb_place * (color_box.bounds.ytop - color_box.bounds.ybot);
    }

    /* draw grid line */
    if (grid.l_type > LT_NODRAW) {
	term_apply_lp_properties(&grid);	/* grid linetype */
	if (color_box.rotation == 'h') {
	    (*term->move) (x1, color_box.bounds.ybot);
	    (*term->vector) (x1, color_box.bounds.ytop);
	} else {
	    (*term->move) (color_box.bounds.xleft, y1);
	    (*term->vector) (color_box.bounds.xright, y1);
	}
	term_apply_lp_properties(&border_lp);	/* border linetype */
    }

    /* draw tic */
    (*term->move) (x1, y1);
    (*term->vector) (x2, y2);

    /* draw label */
    if (text) {
	int just;
	int offsetx, offsety;

	/* Skip label if we've already written a user-specified one here */
#	define MINIMUM_SEPARATION 0.001
	while (userlabels) {
	    if (fabs((place - userlabels->position) / (CB_AXIS.max - CB_AXIS.min))
		<= MINIMUM_SEPARATION) {
		text = NULL;
		break;
	    }
	    userlabels = userlabels->next;
	}
#	undef MINIMUM_SEPARATION

	/* get offset */
	map3d_position_r(&(axis_array[axis].ticdef.offset),
			 &offsetx, &offsety, "cbtics");
	/* User-specified different color for the tics text */
	if (axis_array[axis].ticdef.textcolor.type != TC_DEFAULT)
	    apply_pm3dcolor(&(axis_array[axis].ticdef.textcolor), term);
	if (color_box.rotation == 'h') {
	    int y3 = color_box.bounds.ybot - (term->v_char);
	    int hrotate = 0;

	    if (axis_array[axis].tic_rotate
		&& (*term->text_angle)(axis_array[axis].tic_rotate))
		    hrotate = axis_array[axis].tic_rotate;
	    if (len > 0) y3 -= len; /* add outer tics len */
	    if (y3<0) y3 = 0;
	    just = hrotate ? LEFT : CENTRE;
	    if (axis_array[axis].manual_justify)
		just = axis_array[axis].label.pos;
	    write_multiline(x2+offsetx, y3+offsety, text,
			    just, JUST_CENTRE, hrotate,
			    axis_array[axis].ticdef.font);
	    if (hrotate)
		(*term->text_angle)(0);
	} else {
	    unsigned int x3 = color_box.bounds.xright + (term->h_char);
	    if (len > 0) x3 += len; /* add outer tics len */
	    just = LEFT;
	    if (axis_array[axis].manual_justify)
		just = axis_array[axis].label.pos;	    
	    write_multiline(x3+offsetx, y2+offsety, text,
			    just, JUST_CENTRE, 0.0,
			    axis_array[axis].ticdef.font);
	}
	term_apply_lp_properties(&border_lp);	/* border linetype */
    }

    /* draw tic on the mirror side */
    if (CB_AXIS.ticmode & TICS_MIRROR) {
	if (color_box.rotation == 'h') {
	    y1 = color_box.bounds.ytop;
	    y2 = color_box.bounds.ytop + len;
	} else {
	    x1 = color_box.bounds.xleft;
	    x2 = color_box.bounds.xleft - len;
	}
	(*term->move) (x1, y1);
	(*term->vector) (x2, y2);
    }
}

/*
   Finally the main colour smooth box drawing routine
 */
void
draw_color_smooth_box(int plot_mode)
{
    if (color_box.where == SMCOLOR_BOX_NO)
	return;
    if (!term->filled_polygon)
	return;

    /*
       firstly, choose some good position of the color box

       user's position like that (?):
       else {
       x_from = color_box.xlow;
       x_to   = color_box.xhigh;
       }
     */
    if (color_box.where == SMCOLOR_BOX_USER) {
	if (!is_3d_plot) {
	    double xtemp, ytemp;
	    map_position(&color_box.origin, &color_box.bounds.xleft, &color_box.bounds.ybot, "cbox");
	    map_position_r(&color_box.size, &xtemp, &ytemp, "cbox");
	    color_box.bounds.xright = xtemp;
	    color_box.bounds.ytop = ytemp;
	} else if (splot_map && is_3d_plot) {
	    /* In map view mode we allow any coordinate system for placement */
	    double xtemp, ytemp;
	    map3d_position_double(&color_box.origin, &xtemp, &ytemp, "cbox");
	    color_box.bounds.xleft = xtemp;
	    color_box.bounds.ybot = ytemp;
	    map3d_position_r(&color_box.size, &color_box.bounds.xright, &color_box.bounds.ytop, "cbox");
	} else {
	    /* But in full 3D mode we only allow screen coordinates */
	    color_box.bounds.xleft = color_box.origin.x * (term->xmax) + 0.5;
	    color_box.bounds.ybot = color_box.origin.y * (term->ymax) + 0.5;
	    color_box.bounds.xright = color_box.size.x * (term->xmax-1) + 0.5;
	    color_box.bounds.ytop = color_box.size.y * (term->ymax-1) + 0.5;
	}
	color_box.bounds.xright += color_box.bounds.xleft;
	color_box.bounds.ytop += color_box.bounds.ybot;

    } else { /* color_box.where == SMCOLOR_BOX_DEFAULT */
	if (plot_mode == MODE_SPLOT && !splot_map) {
	    /* HBB 20031215: new code.  Constants fixed to what the result
	     * of the old code in default view (set view 60,30,1,1)
	     * happened to be. Somebody fix them if they're not right! */
	    color_box.bounds.xleft = xmiddle + 0.709 * xscaler;
	    color_box.bounds.xright   = xmiddle + 0.778 * xscaler;
	    color_box.bounds.ybot = ymiddle - 0.147 * yscaler;
	    color_box.bounds.ytop   = ymiddle + 0.497 * yscaler;

	} else if (is_3d_plot) {
	    /* MWS 09-Dec-05, make color box full size for splot maps. */
	    double dx = (X_AXIS.max - X_AXIS.min);
	    map3d_xy(X_AXIS.max + dx * 0.025, Y_AXIS.min, base_z, &color_box.bounds.xleft, &color_box.bounds.ybot);
	    map3d_xy(X_AXIS.max + dx * 0.075, Y_AXIS.max, ceiling_z, &color_box.bounds.xright, &color_box.bounds.ytop);
	} else { /* 2D plot */
	    struct position default_origin = {graph,graph,graph, 1.025, 0, 0};
	    struct position default_size = {graph,graph,graph, 0.05, 1.0, 0};
	    double xtemp, ytemp;
	    map_position(&default_origin, &color_box.bounds.xleft, &color_box.bounds.ybot, "cbox");
	    color_box.bounds.xleft += color_box.xoffset;
	    map_position_r(&default_size, &xtemp, &ytemp, "cbox");
	    color_box.bounds.xright = xtemp + color_box.bounds.xleft;
	    color_box.bounds.ytop = ytemp + color_box.bounds.ybot;
	}

	/* now corrections for outer tics */
	if (color_box.rotation == 'v') {
	    int cblen = (CB_AXIS.tic_in ? -1 : 1) * CB_AXIS.ticscale * 
		(term->h_tic); /* positive for outer tics */
	    int ylen = (Y_AXIS.tic_in ? -1 : 1) * Y_AXIS.ticscale * 
		(term->h_tic); /* positive for outer tics */
	    if ((cblen > 0) && (CB_AXIS.ticmode & TICS_MIRROR)) {
		color_box.bounds.xleft += cblen;
		color_box.bounds.xright += cblen;
	    }
	    if ((ylen > 0) && 
		(axis_array[FIRST_Y_AXIS].ticmode & TICS_MIRROR)) {
		color_box.bounds.xleft += ylen;
		color_box.bounds.xright += ylen;
	    }
	}
    }

    if (color_box.bounds.ybot > color_box.bounds.ytop) {
	double tmp = color_box.bounds.ytop;
	color_box.bounds.ytop = color_box.bounds.ybot;
	color_box.bounds.ybot = tmp;
    }
    if (color_box.invert && color_box.rotation == 'v') {
	double tmp = color_box.bounds.ytop;
	color_box.bounds.ytop = color_box.bounds.ybot;
	color_box.bounds.ybot = tmp;
    }

    /* The PostScript terminal has an Optimized version */
    if ((term->flags & TERM_IS_POSTSCRIPT) != 0)
	draw_inside_color_smooth_box_postscript();
    else
	draw_inside_color_smooth_box_bitmap();

    if (color_box.border) {
	/* now make boundary around the colour box */
	if (color_box.border_lt_tag >= 0) {
	    /* user specified line type */
	    struct lp_style_type lp = border_lp;
	    lp_use_properties(&lp, color_box.border_lt_tag);
	    term_apply_lp_properties(&lp);
	} else {
	    /* black solid colour should be chosen, so it's border linetype */
	    term_apply_lp_properties(&border_lp);
	}
	newpath();
	(term->move) (color_box.bounds.xleft, color_box.bounds.ybot);
	(term->vector) (color_box.bounds.xright, color_box.bounds.ybot);
	(term->vector) (color_box.bounds.xright, color_box.bounds.ytop);
	(term->vector) (color_box.bounds.xleft, color_box.bounds.ytop);
	(term->vector) (color_box.bounds.xleft, color_box.bounds.ybot);
	closepath();

	/* Set line properties to some value, this also draws lines in postscript terminals. */
	    term_apply_lp_properties(&border_lp);
	}

    /* draw tics */
    if (axis_array[COLOR_AXIS].ticmode) {
	term_apply_lp_properties(&border_lp); /* border linetype */
	gen_tics(COLOR_AXIS, cbtick_callback );
    }

    /* write the colour box label */
    if (CB_AXIS.label.text) {
	int x, y;
	apply_pm3dcolor(&(CB_AXIS.label.textcolor),term);
	if (color_box.rotation == 'h') {
	    int len = CB_AXIS.ticscale * (CB_AXIS.tic_in ? 1 : -1) * 
		(term->v_tic);

	    map3d_position_r(&(CB_AXIS.label.offset), &x, &y, "smooth_box");
	    x += (color_box.bounds.xleft + color_box.bounds.xright) / 2;

#define DEFAULT_Y_DISTANCE 1.0
	    y += color_box.bounds.ybot + (- DEFAULT_Y_DISTANCE - 1.7) * term->v_char;
#undef DEFAULT_Y_DISTANCE
	    if (len < 0) y += len;
	    if (x<0) x = 0;
	    if (y<0) y = 0;
	    write_multiline(x, y, CB_AXIS.label.text, CENTRE, JUST_CENTRE, 0,
			    CB_AXIS.label.font);
	} else {
	    int len = CB_AXIS.ticscale * (CB_AXIS.tic_in ? -1 : 1) *
		(term->h_tic);
	    /* calculate max length of cb-tics labels */
	    widest_tic_strlen = 0;
	    if (CB_AXIS.ticmode & TICS_ON_BORDER) {
	      	widest_tic_strlen = 0; /* reset the global variable */
		gen_tics(COLOR_AXIS, /* 0, */ widest_tic_callback);
	    }
	    map3d_position_r(&(CB_AXIS.label.offset), &x, &y, "smooth_box");
#define DEFAULT_X_DISTANCE 0.0
	    x += color_box.bounds.xright + (widest_tic_strlen + DEFAULT_X_DISTANCE + 1.5) * term->h_char;
#undef DEFAULT_X_DISTANCE
	    if (len > 0) x += len;
	    y += (color_box.bounds.ybot + color_box.bounds.ytop) / 2;
	    if (x<0) x = 0;
	    if (y<0) y = 0;
	    if ((*term->text_angle)(CB_AXIS.label.rotate)) {
		write_multiline(x, y, CB_AXIS.label.text, CENTRE, JUST_TOP,
				CB_AXIS.label.rotate, CB_AXIS.label.font);
		(*term->text_angle)(0);
	    } else {
		write_multiline(x, y, CB_AXIS.label.text, LEFT, JUST_TOP, 0, CB_AXIS.label.font);
	    }
	}
	reset_textcolor(&(CB_AXIS.label.textcolor),term);
    }

}

/*
 * User-callable builtin color conversion 
 */
void
f_hsv2rgb(union argument *arg)
{
    struct value h, s, v, result;
    rgb_color color = {0., 0., 0.};

    (void) arg;
    (void) pop(&v);
    (void) pop(&s);
    (void) pop(&h);

    if (h.type == INTGR)
	color.r = h.v.int_val;
    else if (h.type == CMPLX)
	color.r = h.v.cmplx_val.real;
    if (s.type == INTGR)
	color.g = s.v.int_val;
    else if (s.type == CMPLX)
	color.g = s.v.cmplx_val.real;
    if (v.type == INTGR)
	color.b = v.v.int_val;
    else if (v.type == CMPLX)
	color.b = v.v.cmplx_val.real;

    if (color.r < 0)
	color.r = 0;
    if (color.g < 0)
	color.g = 0;
    if (color.b < 0)
	color.b = 0;
    if (color.r > 1.)
	color.r = 1.;
    if (color.g > 1.)
	color.g = 1.;
    if (color.b > 1.)
	color.b = 1.;

    (void) Ginteger(&result, hsv2rgb(&color));
    push(&result);
}
