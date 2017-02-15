/*
 * $Id: boundary.c,v 1.15.2.5 2015/10/29 23:25:47 sfeam Exp $
 */

/* GNUPLOT - boundary.c */

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

#include "graphics.h"
#include "boundary.h"
#include "alloc.h"
#include "axis.h"
#include "misc.h"
#include "pm3d.h"	/* for is_plot_with_palette */

#define ERRORBARTIC GPMAX((t->h_tic/2),1)

/*{{{  local variables */
static int xlablin, x2lablin, ylablin, y2lablin, titlelin, xticlin, x2ticlin;

/*{{{  local and global variables */
static int key_sample_width;	/* width of line sample */
static int key_sample_left;	/* offset from x for left of line sample */
static int key_sample_right;	/* offset from x for right of line sample */
static int key_text_left;	/* offset from x for left-justified text */
static int key_text_right;	/* offset from x for right-justified text */
static int key_size_left;	/* size of left bit of key (text or sample, depends on key->reverse) */
static int key_size_right;	/* size of right part of key (including padding) */
static int key_xleft;		/* Amount of space on the left required by the key */
static int max_ptitl_len = 0;	/* max length of plot-titles (keys) */
static int ptitl_cnt;		/* count keys with len > 0  */

static int key_width;		/* calculate once, then everyone uses it */
static int key_height;		/* ditto */
static int key_title_height;	/* nominal number of lines * character height */
static int key_title_extra;	/* allow room for subscript/superscript */
static int time_y, time_x;
static int title_y;

/*
 * These quantities are needed in do_plot() e.g. for histogtram title layout
 */
int key_entry_height;		/* bigger of t->v_char, t->v_tic */
int key_point_offset;		/* offset from x for point sample */
int key_col_wth, yl_ref;
int ylabel_x, y2label_x, xlabel_y, x2label_y;
int ylabel_y, y2label_y, xtic_y, x2tic_y, ytic_x, y2tic_x;
int key_rows;
int key_cols;

/*{{{  boundary() */
/* borders of plotting area
 * computed once on every call to do_plot
 *
 * The order in which things is done is getting pretty critical:
 *  plot_bounds.ytop depends on title, x2label, ylabels (if no rotated text)
 *  plot_bounds.ybot depends on key, if "under"
 *  once we have these, we can setup the y1 and y2 tics and the
 *  only then can we calculate plot_bounds.xleft and plot_bounds.xright
 *  plot_bounds.xright depends also on key RIGHT
 *  then we can do x and x2 tics
 *
 * For set size ratio ..., everything depends on everything else...
 * not really a lot we can do about that, so we lose if the plot has to
 * be reduced vertically. But the chances are the
 * change will not be very big, so the number of tics will not
 * change dramatically.
 *
 * Margin computation redone by Dick Crawford (rccrawford@lanl.gov) 4/98
 */

void
boundary(struct curve_points *plots, int count)
{
    int yticlin = 0, y2ticlin = 0, timelin = 0;
    legend_key *key = &keyT;

    struct termentry *t = term;
    /* FIXME HBB 20000506: this line is the reason for the 'D0,1;D1,0'
     * bug in the HPGL terminal: we actually carry out the switch of
     * text orientation, just for finding out if the terminal can do
     * that. *But* we're not in graphical mode, yet, so this call
     * yields undesirable results */
    int can_rotate = (*t->text_angle) (TEXT_VERTICAL);

    int xtic_textheight;	/* height of xtic labels */
    int x2tic_textheight;	/* height of x2tic labels */
    int title_textheight;	/* height of title */
    int xlabel_textheight;	/* height of xlabel */
    int x2label_textheight;	/* height of x2label */
    int timetop_textheight;	/* height of timestamp (if at top) */
    int timebot_textheight;	/* height of timestamp (if at bottom) */
    int ylabel_textheight;	/* height of (unrotated) ylabel */
    int y2label_textheight;	/* height of (unrotated) y2label */
    int ylabel_textwidth;	/* width of (rotated) ylabel */
    int y2label_textwidth;	/* width of (rotated) y2label */
    int timelabel_textwidth;	/* width of timestamp */
    int ytic_textwidth;		/* width of ytic labels */
    int y2tic_textwidth;	/* width of y2tic labels */
    int x2tic_height;		/* 0 for tic_in or no x2tics, ticscale*v_tic otherwise */
    int xtic_textwidth=0;	/* amount by which the xtic label protrude to the right */
    int xtic_height;
    int ytic_width;
    int y2tic_width;

    /* figure out which rotatable items are to be rotated
     * (ylabel and y2label are rotated if possible) */
    int vertical_timelabel = can_rotate ? timelabel_rotate : 0;
    int vertical_xtics  = can_rotate ? axis_array[FIRST_X_AXIS].tic_rotate : 0;
    int vertical_x2tics = can_rotate ? axis_array[SECOND_X_AXIS].tic_rotate : 0;
    int vertical_ytics  = can_rotate ? axis_array[FIRST_Y_AXIS].tic_rotate : 0;
    int vertical_y2tics = can_rotate ? axis_array[SECOND_Y_AXIS].tic_rotate : 0;

    TBOOLEAN shift_labels_to_border = FALSE;

    xticlin = ylablin = y2lablin = xlablin = x2lablin = titlelin = 0;

    /*{{{  count lines in labels and tics */
    if (title.text)
	label_width(title.text, &titlelin);
    if (axis_array[FIRST_X_AXIS].label.text)
	label_width(axis_array[FIRST_X_AXIS].label.text, &xlablin);

    /* This should go *inside* label_width(), but it messes up the key title */
    /* Imperfect check for subscripts or superscripts */
    if ((term->flags & TERM_ENHANCED_TEXT) && axis_array[FIRST_X_AXIS].label.text
	&& strpbrk(axis_array[FIRST_X_AXIS].label.text, "_^"))
	    xlablin++;

    if (axis_array[SECOND_X_AXIS].label.text)
	label_width(axis_array[SECOND_X_AXIS].label.text, &x2lablin);
    if (axis_array[FIRST_Y_AXIS].label.text)
	label_width(axis_array[FIRST_Y_AXIS].label.text, &ylablin);
    if (axis_array[SECOND_Y_AXIS].label.text)
	label_width(axis_array[SECOND_Y_AXIS].label.text, &y2lablin);

    if (axis_array[FIRST_X_AXIS].ticmode) {
	label_width(axis_array[FIRST_X_AXIS].formatstring, &xticlin);
	/* Reserve room for user tic labels even if format of autoticks is "" */
	if (xticlin == 0 && axis_array[FIRST_X_AXIS].ticdef.def.user)
	    xticlin = 1;
    }

    if (axis_array[SECOND_X_AXIS].ticmode)
	label_width(axis_array[SECOND_X_AXIS].formatstring, &x2ticlin);
    if (axis_array[FIRST_Y_AXIS].ticmode)
	label_width(axis_array[FIRST_Y_AXIS].formatstring, &yticlin);
    if (axis_array[SECOND_Y_AXIS].ticmode)
	label_width(axis_array[SECOND_Y_AXIS].formatstring, &y2ticlin);
    if (timelabel.text)
	label_width(timelabel.text, &timelin);
    /*}}} */

    /*{{{  preliminary plot_bounds.ytop  calculation */

    /*     first compute heights of things to be written in the margin */

    /* title */
    if (titlelin) {
	double tmpx, tmpy;
	map_position_r(&(title.offset), &tmpx, &tmpy, "boundary");
	if (title.font)
	    t->set_font(title.font);
	title_textheight = (int) ((titlelin) * (t->v_char) + tmpy);
	if (title.font)
	    t->set_font("");
	title_textheight += (int)(t->v_char); /* Gap of one normal line height */
    } else
	title_textheight = 0;

    /* x2label */
    if (x2lablin) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[SECOND_X_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	if (axis_array[SECOND_X_AXIS].label.font)
	    t->set_font(axis_array[SECOND_X_AXIS].label.font);
	x2label_textheight = (int) (x2lablin * t->v_char + tmpy);
	if (!axis_array[SECOND_X_AXIS].ticmode)
	    x2label_textheight += 0.5 * t->v_char;
	if (axis_array[SECOND_X_AXIS].label.font)
	    t->set_font("");
    } else
	x2label_textheight = 0;

    /* tic labels */
    if (axis_array[SECOND_X_AXIS].ticmode & TICS_ON_BORDER) {
	/* ought to consider tics on axes if axis near border */
	x2tic_textheight = (int) (x2ticlin * t->v_char);
    } else
	x2tic_textheight = 0;

    /* tics */
    if (!axis_array[SECOND_X_AXIS].tic_in
	&& ((axis_array[SECOND_X_AXIS].ticmode & TICS_ON_BORDER)
	    || ((axis_array[FIRST_X_AXIS].ticmode & TICS_MIRROR)
		&& (axis_array[FIRST_X_AXIS].ticmode & TICS_ON_BORDER))))
	x2tic_height = (int) (t->v_tic * axis_array[SECOND_X_AXIS].ticscale);
    else
	x2tic_height = 0;

    /* timestamp */
    if (timelabel.text && !timelabel_bottom) {
	double tmpx, tmpy;
	map_position_r(&(timelabel.offset), &tmpx, &tmpy, "boundary");
	timetop_textheight = (int) ((timelin + 2) * t->v_char + tmpy);
    } else
	timetop_textheight = 0;

    /* horizontal ylabel */
    if (axis_array[FIRST_Y_AXIS].label.text && !can_rotate) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[FIRST_Y_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	if (axis_array[FIRST_Y_AXIS].label.font)
	    t->set_font(axis_array[FIRST_Y_AXIS].label.font);
	ylabel_textheight = (int) (ylablin * t->v_char + tmpy);
	if (axis_array[FIRST_Y_AXIS].label.font)
	    t->set_font("");
    } else
	ylabel_textheight = 0;

    /* horizontal y2label */
    if (axis_array[SECOND_Y_AXIS].label.text && !can_rotate) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[SECOND_Y_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	if (axis_array[SECOND_Y_AXIS].label.font)
	    t->set_font(axis_array[FIRST_Y_AXIS].label.font);
	y2label_textheight = (int) (y2lablin * t->v_char + tmpy);
	if (axis_array[SECOND_Y_AXIS].label.font)
	    t->set_font("");
    } else
	y2label_textheight = 0;

    /* compute plot_bounds.ytop from the various components
     *     unless tmargin is explicitly specified
     */

    plot_bounds.ytop = (int) (0.5 + (ysize + yoffset) * (t->ymax-1));

    /* Sanity check top and bottom margins, in case the user got confused */
    if (bmargin.scalex == screen && tmargin.scalex == screen)
	if (bmargin.x > tmargin.x) {
	    double tmp = bmargin.x;
	    bmargin.x = tmargin.x;
	    tmargin.x = tmp;
	}

    if (tmargin.scalex == screen) {
	/* Specified as absolute position on the canvas */
	plot_bounds.ytop = (tmargin.x) * (float)(t->ymax-1);
    } else if (tmargin.x >=0) {
	/* Specified in terms of character height */
	plot_bounds.ytop -= (int)(tmargin.x * (float)t->v_char + 0.5);
    } else {
	/* Auto-calculation of space required */
	int top_margin = x2label_textheight + title_textheight;

	if (timetop_textheight + ylabel_textheight > top_margin)
	    top_margin = timetop_textheight + ylabel_textheight;
	if (y2label_textheight > top_margin)
	    top_margin = y2label_textheight;

	top_margin += x2tic_height + x2tic_textheight;
	/* x2tic_height and x2tic_textheight are computed as only the
	 *     relevant heights, but they nonetheless need a blank
	 *     space above them  */
	if (top_margin > x2tic_height)
	    top_margin += (int) t->v_char;

	plot_bounds.ytop -= top_margin;
	if (plot_bounds.ytop == (int)(0.5 + (ysize + yoffset) * (t->ymax-1))) {
	    /* make room for the end of rotated ytics or y2tics */
	    plot_bounds.ytop -= (int) (t->h_char * 2);
	}
    }

    /*  end of preliminary plot_bounds.ytop calculation }}} */


    /*{{{  preliminary plot_bounds.xleft, needed for "under" */
    if (lmargin.scalex == screen)
	plot_bounds.xleft = lmargin.x * (float)t->xmax;
    else
	plot_bounds.xleft = xoffset * t->xmax
			  + t->h_char * (lmargin.x >= 0 ? lmargin.x : 1);
    /*}}} */


    /*{{{  tentative plot_bounds.xright, needed for "under" */
    if (rmargin.scalex == screen)
	plot_bounds.xright = rmargin.x * (float)(t->xmax - 1);
    else
	plot_bounds.xright = (xsize + xoffset) * (t->xmax - 1)
			   - t->h_char * (rmargin.x >= 0 ? rmargin.x : 2);
    /*}}} */


    /*{{{  preliminary plot_bounds.ybot calculation
     *     first compute heights of labels and tics */

    /* tic labels */
    shift_labels_to_border = FALSE;
    if (axis_array[FIRST_X_AXIS].ticmode & TICS_ON_AXIS) {
	/* FIXME: This test for how close the axis is to the border does not match */
	/*        the tests in axis_output_tics(), and assumes FIRST_Y_AXIS.       */
	if (!inrange(0.0, axis_array[FIRST_Y_AXIS].min, axis_array[FIRST_Y_AXIS].max))
	    shift_labels_to_border = TRUE;
	if (0.05 > fabs( axis_array[FIRST_Y_AXIS].min
		/ (axis_array[FIRST_Y_AXIS].max - axis_array[FIRST_Y_AXIS].min)))
	    shift_labels_to_border = TRUE;
    }
    if ((axis_array[FIRST_X_AXIS].ticmode & TICS_ON_BORDER)
    ||  shift_labels_to_border) {
	xtic_textheight = (int) (t->v_char * (xticlin + 1));
    } else
	xtic_textheight =  0;

    /* tics */
    if (!axis_array[FIRST_X_AXIS].tic_in
	&& ((axis_array[FIRST_X_AXIS].ticmode & TICS_ON_BORDER)
	    || ((axis_array[SECOND_X_AXIS].ticmode & TICS_MIRROR)
		&& (axis_array[SECOND_X_AXIS].ticmode & TICS_ON_BORDER))))
	xtic_height = (int) (t->v_tic * axis_array[FIRST_X_AXIS].ticscale);
    else
	xtic_height = 0;

    /* xlabel */
    if (xlablin) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[FIRST_X_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	/* offset is subtracted because if > 0, the margin is smaller */
	/* textheight is inflated by 0.2 to allow descenders to clear bottom of canvas */
	xlabel_textheight = (((float)xlablin + 0.2) * t->v_char - tmpy);
	if (!axis_array[FIRST_X_AXIS].ticmode)
	    xlabel_textheight += 0.5 * t->v_char;
    } else
	xlabel_textheight = 0;

    /* timestamp */
    if (timelabel.text && timelabel_bottom) {
	/* && !vertical_timelabel)
	 * DBT 11-18-98 resize plot for vertical timelabels too !
	 */
	double tmpx, tmpy;
	map_position_r(&(timelabel.offset), &tmpx, &tmpy, "boundary");
	/* offset is subtracted because if . 0, the margin is smaller */
	timebot_textheight = (int) (timelin * t->v_char - tmpy);
    } else
	timebot_textheight = 0;

    /* compute plot_bounds.ybot from the various components
     *     unless bmargin is explicitly specified  */

    plot_bounds.ybot = yoffset * (float)t->ymax;

    if (bmargin.scalex == screen) {
	/* Absolute position for bottom of plot */
	plot_bounds.ybot = bmargin.x * (float)t->ymax;
    } else if (bmargin.x >= 0) {
	/* Position based on specified character height */
	plot_bounds.ybot += bmargin.x * (float)t->v_char + 0.5;
    } else {
	plot_bounds.ybot += xtic_height + xtic_textheight;
	if (xlabel_textheight > 0)
	    plot_bounds.ybot += xlabel_textheight;
	if (timebot_textheight > 0)
	    plot_bounds.ybot += timebot_textheight;
	/* HBB 19990616: round to nearest integer, required to escape
	 * floating point inaccuracies */
	if (plot_bounds.ybot == (int) (t->ymax * yoffset)) {
	    /* make room for the end of rotated ytics or y2tics */
	    plot_bounds.ybot += (int) (t->h_char * 2);
	}
    }

    /*  end of preliminary plot_bounds.ybot calculation }}} */

    /* Determine the size and position of the key box */
    if (key->visible) {
	/* Count max_len key and number keys with len > 0 */
	max_ptitl_len = find_maxl_keys(plots, count, &ptitl_cnt);
	do_key_layout(key);
    }

    /*{{{  set up y and y2 tics */
    setup_tics(FIRST_Y_AXIS, 20);
    setup_tics(SECOND_Y_AXIS, 20);
    /*}}} */

    /* Adjust color axis limits if necessary. */
    if (is_plot_with_palette()) {
	/* June 2014 - moved outside do_plot so that it is not called during a refresh */
	/* set_cbminmax(); */
	axis_checked_extend_empty_range(COLOR_AXIS, "All points of color axis undefined.");
	if (color_box.where != SMCOLOR_BOX_NO)
	    setup_tics(COLOR_AXIS, 20);
    }

    /*{{{  recompute plot_bounds.xleft based on widths of ytics, ylabel etc
       unless it has been explicitly set by lmargin */

    /* tic labels */
    shift_labels_to_border = FALSE;
    if (axis_array[FIRST_Y_AXIS].ticmode & TICS_ON_AXIS) {
	/* FIXME: This test for how close the axis is to the border does not match */
	/*        the tests in axis_output_tics(), and assumes FIRST_X_AXIS.       */
	if (!inrange(0.0, axis_array[FIRST_X_AXIS].min, axis_array[FIRST_X_AXIS].max))
	    shift_labels_to_border = TRUE;
	if (0.1 > fabs( axis_array[FIRST_X_AXIS].min
	       /  (axis_array[FIRST_X_AXIS].max - axis_array[FIRST_X_AXIS].min)))
	    shift_labels_to_border = TRUE;
    }

    if ((axis_array[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER)
    ||  shift_labels_to_border) {
	if (vertical_ytics)
	    /* HBB: we will later add some white space as part of this, so
	     * reserve two more rows (one above, one below the text ...).
	     * Same will be done to similar calc.'s elsewhere */
	    ytic_textwidth = (int) (t->v_char * (yticlin + 2));
	else {
	    widest_tic_strlen = 0;	/* reset the global variable ... */
	    /* get gen_tics to call widest_tic_callback with all labels
	     * the latter sets widest_tic_strlen to the length of the widest
	     * one ought to consider tics on axis if axis near border...
	     */
	    gen_tics(FIRST_Y_AXIS, /* 0, */ widest_tic_callback);

	    ytic_textwidth = (int) (t->h_char * (widest_tic_strlen + 2));
	}
    } else {
	ytic_textwidth = 0;
    }

    /* tics */
    if (!axis_array[FIRST_Y_AXIS].tic_in
	&& ((axis_array[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER)
	    || ((axis_array[SECOND_Y_AXIS].ticmode & TICS_MIRROR)
		&& (axis_array[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER))))
	ytic_width = (int) (t->h_tic * axis_array[FIRST_Y_AXIS].ticscale);
    else
	ytic_width = 0;

    /* ylabel */
    if (axis_array[FIRST_Y_AXIS].label.text && can_rotate) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[FIRST_Y_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	ylabel_textwidth = (int) (ylablin * (t->v_char) - tmpx);
	if (!axis_array[FIRST_Y_AXIS].ticmode)
	    ylabel_textwidth += 0.5 * t->v_char;
    } else
	/* this should get large for NEGATIVE ylabel.xoffsets  DBT 11-5-98 */
	ylabel_textwidth = 0;

    /* timestamp */
    if (timelabel.text && vertical_timelabel) {
	double tmpx, tmpy;
	map_position_r(&(timelabel.offset), &tmpx, &tmpy, "boundary");
	timelabel_textwidth = (int) ((timelin + 1.5) * t->v_char - tmpx);
    } else
	timelabel_textwidth = 0;

    if (lmargin.x < 0) {
	/* Auto-calculation */
	double tmpx, tmpy;
	int space_to_left = key_xleft;

	if (space_to_left < timelabel_textwidth)
	    space_to_left = timelabel_textwidth;
	if (space_to_left < ylabel_textwidth)
	    space_to_left = ylabel_textwidth;
	plot_bounds.xleft = xoffset * t->xmax;
	plot_bounds.xleft += space_to_left;
	plot_bounds.xleft += ytic_width + ytic_textwidth;

	/* make sure plot_bounds.xleft is wide enough for a negatively
	 * x-offset horizontal timestamp
	 */
	map_position_r(&(timelabel.offset), &tmpx, &tmpy, "boundary");
	if (!vertical_timelabel
	    && plot_bounds.xleft - ytic_width - ytic_textwidth < -(int) (tmpx))
	    plot_bounds.xleft = ytic_width + ytic_textwidth - (int) (tmpx);
	if (plot_bounds.xleft == (int) (t->xmax * xoffset)) {
	    /* make room for end of xtic or x2tic label */
	    plot_bounds.xleft += (int) (t->h_char * 2);
	}
	/* DBT 12-3-98  extra margin just in case */
	plot_bounds.xleft += 0.5 * t->h_char;
    }
    /* Note: we took care of explicit 'set lmargin foo' at line 492 */

    /*  end of plot_bounds.xleft calculation }}} */

    /*{{{  recompute plot_bounds.xright based on widest y2tic. y2labels, key "outside"
       unless it has been explicitly set by rmargin */

    /* tic labels */
    if (axis_array[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER) {
	if (vertical_y2tics)
	    y2tic_textwidth = (int) (t->v_char * (y2ticlin + 2));
	else {
	    widest_tic_strlen = 0;	/* reset the global variable ... */
	    /* get gen_tics to call widest_tic_callback with all labels
	     * the latter sets widest_tic_strlen to the length of the widest
	     * one ought to consider tics on axis if axis near border...
	     */
	    gen_tics(SECOND_Y_AXIS, /* 0, */ widest_tic_callback);

	    y2tic_textwidth = (int) (t->h_char * (widest_tic_strlen + 2));
	}
    } else {
	y2tic_textwidth = 0;
    }

    /* EAM May 2009
     * Check to see if any xtic labels are so long that they extend beyond
     * the right boundary of the plot. If so, allow extra room in the margin.
     * If the labels are too long to fit even with a big margin, too bad.
     */
    if (axis_array[FIRST_X_AXIS].ticdef.def.user) {
	struct ticmark *tic = axis_array[FIRST_X_AXIS].ticdef.def.user;
	int maxrightlabel = plot_bounds.xright;

	/* We don't really know the plot layout yet, but try for an estimate */
	AXIS_SETSCALE(FIRST_X_AXIS, plot_bounds.xleft, plot_bounds.xright);
	axis_set_graphical_range(FIRST_X_AXIS, plot_bounds.xleft, plot_bounds.xright);

	while (tic) {
	    if (tic->label) {
		double xx;
		int length = estimate_strlen(tic->label)
			   * cos(DEG2RAD * (double)(axis_array[FIRST_X_AXIS].tic_rotate))
			   * term->h_char;

		if (inrange(tic->position,
		    axis_array[FIRST_X_AXIS].set_min,
		    axis_array[FIRST_X_AXIS].set_max)) {
			xx = axis_log_value_checked(FIRST_X_AXIS, tic->position, "xtic");
		        xx = AXIS_MAP(FIRST_X_AXIS, xx);
			xx += (axis_array[FIRST_X_AXIS].tic_rotate) ? length : length /2;
			if (maxrightlabel < xx)
			    maxrightlabel = xx;
		}
	    }
	    tic = tic->next;
	}
	xtic_textwidth = maxrightlabel - plot_bounds.xright;
	if (xtic_textwidth > term->xmax/4) {
	    xtic_textwidth = term->xmax/4;
	    int_warn(NO_CARET, "difficulty making room for xtic labels");
	}
    }

    /* tics */
    if (!axis_array[SECOND_Y_AXIS].tic_in
	&& ((axis_array[SECOND_Y_AXIS].ticmode & TICS_ON_BORDER)
	    || ((axis_array[FIRST_Y_AXIS].ticmode & TICS_MIRROR)
		&& (axis_array[FIRST_Y_AXIS].ticmode & TICS_ON_BORDER))))
	y2tic_width = (int) (t->h_tic * axis_array[SECOND_Y_AXIS].ticscale);
    else
	y2tic_width = 0;

    /* y2label */
    if (can_rotate && axis_array[SECOND_Y_AXIS].label.text) {
	double tmpx, tmpy;
	map_position_r(&(axis_array[SECOND_Y_AXIS].label.offset),
		       &tmpx, &tmpy, "boundary");
	y2label_textwidth = (int) (y2lablin * t->v_char + tmpx);
	if (!axis_array[SECOND_Y_AXIS].ticmode)
	    y2label_textwidth += 0.5 * t->v_char;
    } else
	y2label_textwidth = 0;

    /* Make room for the color box if needed. */
    if (rmargin.scalex != screen) {
	if (is_plot_with_colorbox()) {
#define COLORBOX_SCALE 0.100
#define WIDEST_COLORBOX_TICTEXT 3
	    if ((color_box.where != SMCOLOR_BOX_NO) && (color_box.where != SMCOLOR_BOX_USER)) {
		plot_bounds.xright -= (int) (plot_bounds.xright-plot_bounds.xleft)*COLORBOX_SCALE;
		plot_bounds.xright -= (int) ((t->h_char) * WIDEST_COLORBOX_TICTEXT);
	    }
	    color_box.xoffset = 0;
	}

	if (rmargin.x < 0) {
	    color_box.xoffset = plot_bounds.xright;
	    plot_bounds.xright -= y2tic_width + y2tic_textwidth;
	    if (y2label_textwidth > 0)
		plot_bounds.xright -= y2label_textwidth;

	    if (plot_bounds.xright > (xsize+xoffset)*(t->xmax-1) - (t->h_char * 2))
		plot_bounds.xright = (xsize+xoffset)*(t->xmax-1) - (t->h_char * 2);

	    color_box.xoffset -= plot_bounds.xright;
	    /* EAM 2009 - protruding xtic labels */
	    if (term->xmax - plot_bounds.xright < xtic_textwidth)
		plot_bounds.xright = term->xmax - xtic_textwidth;
	    /* DBT 12-3-98  extra margin just in case */
	    plot_bounds.xright -= 1.0 * t->h_char;
	}
	/* Note: we took care of explicit 'set rmargin foo' at line 502 */
    }

    /*  end of plot_bounds.xright calculation }}} */


    /* Set up x and x2 tics */
    /* we should base the guide on the width of the xtics, but we cannot
     * use widest_tics until tics are set up. Bit of a downer - let us
     * assume tics are 5 characters wide
     */
    /* HBB 20001205: moved this block to before aspect_ratio is
     * applied: setup_tics may extend the ranges, which would distort
     * the aspect ratio */

    setup_tics(FIRST_X_AXIS, 20);
    setup_tics(SECOND_X_AXIS, 20);

    if (polar)
	setup_tics(POLAR_AXIS, 10);


    /* Modify the bounding box to fit the aspect ratio, if any was
     * given. */
    if (aspect_ratio != 0.0) {
	double current_aspect_ratio;

	if (aspect_ratio < 0
	    && (X_AXIS.max - X_AXIS.min) != 0.0
	    ) {
	    current_aspect_ratio = - aspect_ratio
		* fabs((Y_AXIS.max - Y_AXIS.min) / (X_AXIS.max - X_AXIS.min));
	} else
	    current_aspect_ratio = aspect_ratio;

	/* Set aspect ratio if valid and sensible */
	/* EAM Mar 2008 - fixed borders take precedence over centering */
	if (current_aspect_ratio >= 0.01 && current_aspect_ratio <= 100.0) {
	    double current = ((double) (plot_bounds.ytop - plot_bounds.ybot))
			   / (plot_bounds.xright - plot_bounds.xleft);
	    double required = (current_aspect_ratio * t->v_tic) / t->h_tic;

	    if (current > required) {
		/* too tall */
		int old_height = plot_bounds.ytop - plot_bounds.ybot;
		int new_height = required * (plot_bounds.xright - plot_bounds.xleft);
		if (bmargin.scalex == screen)
		    plot_bounds.ytop = plot_bounds.ybot + new_height;
		else if (tmargin.scalex == screen)
		    plot_bounds.ybot = plot_bounds.ytop - new_height;
		else {
		    plot_bounds.ybot += (old_height - new_height) / 2;
		    plot_bounds.ytop -= (old_height - new_height) / 2;
		}

	    } else {
		int old_width = plot_bounds.xright - plot_bounds.xleft;
		int new_width = (plot_bounds.ytop - plot_bounds.ybot) / required;
		if (lmargin.scalex == screen)
		    plot_bounds.xright = plot_bounds.xleft + new_width;
		else if (rmargin.scalex == screen)
		    plot_bounds.xleft = plot_bounds.xright - new_width;
		else {
		    plot_bounds.xleft += (old_width - new_width) / 2;
		    plot_bounds.xright -= (old_width - new_width) / 2;
		}
	    }
	}
    }

    /*  Calculate space needed for tic label rotation.
     *  If [tb]margin is auto, move the plot boundary.
     *  Otherwise use textheight to adjust placement of various titles.
     */

    if (axis_array[SECOND_X_AXIS].ticmode & TICS_ON_BORDER && vertical_x2tics) {
	/* Assuming left justified tic labels. Correction below if they aren't */
	double projection = sin((double)axis_array[SECOND_X_AXIS].tic_rotate*DEG2RAD);
	if (axis_array[SECOND_X_AXIS].label.pos == RIGHT)
	    projection *= -1;
	else if (axis_array[SECOND_X_AXIS].label.pos == CENTRE)
	    projection = 0.5*fabs(projection);
	widest_tic_strlen = 0;		/* reset the global variable ... */
	gen_tics(SECOND_X_AXIS, /* 0, */ widest_tic_callback);
	if (tmargin.x < 0) /* Undo original estimate */
	    plot_bounds.ytop += x2tic_textheight;
	/* Adjust spacing for rotation */
	if (projection > 0.0)
	    x2tic_textheight += (int) (t->h_char * (widest_tic_strlen)) * projection;
	if (tmargin.x < 0)
	    plot_bounds.ytop -= x2tic_textheight;
    }
    if (axis_array[FIRST_X_AXIS].ticmode & TICS_ON_BORDER && vertical_xtics) {
	double projection;
	/* This adjustment will happen again in axis_output_tics but we need it now */
	if (axis_array[FIRST_X_AXIS].tic_rotate == TEXT_VERTICAL
	&& !axis_array[FIRST_X_AXIS].manual_justify)
	    axis_array[FIRST_X_AXIS].label.pos = RIGHT;
	if (axis_array[FIRST_X_AXIS].tic_rotate == 90)
	    projection = -1.0;
	else if (axis_array[FIRST_X_AXIS].tic_rotate == TEXT_VERTICAL)
	    projection = -1.0;
	else
	    projection = -sin((double)axis_array[FIRST_X_AXIS].tic_rotate*DEG2RAD);
	if (axis_array[FIRST_X_AXIS].label.pos == RIGHT)
	    projection *= -1;
	widest_tic_strlen = 0;		/* reset the global variable ... */
	gen_tics(FIRST_X_AXIS, /* 0, */ widest_tic_callback);

	if (bmargin.x < 0)
	    plot_bounds.ybot -= xtic_textheight;
	if (projection > 0.0)
	    xtic_textheight = (int) (t->h_char * widest_tic_strlen) * projection
			    + t->v_char;
	if (bmargin.x < 0)
	    plot_bounds.ybot += xtic_textheight;
    }

    /* EAM - FIXME
     * Notwithstanding all these fancy calculations, plot_bounds.ytop must always be above plot_bounds.ybot
     */
    if (plot_bounds.ytop < plot_bounds.ybot) {
	int i = plot_bounds.ytop;

	plot_bounds.ytop = plot_bounds.ybot;
	plot_bounds.ybot = i;
	FPRINTF((stderr,"boundary: Big problems! plot_bounds.ybot > plot_bounds.ytop\n"));
    }

    /*  compute coordinates for axis labels, title et al
     *     (some of these may not be used) */

    x2label_y = plot_bounds.ytop + x2tic_height + x2tic_textheight + x2label_textheight;
    if (x2tic_textheight && (title_textheight || x2label_textheight))
	x2label_y += t->v_char;

    title_y = x2label_y + title_textheight;

    ylabel_y = plot_bounds.ytop + x2tic_height + x2tic_textheight + ylabel_textheight;

    y2label_y = plot_bounds.ytop + x2tic_height + x2tic_textheight + y2label_textheight;

    /* Shift upward by 0.2 line to allow for descenders in xlabel text */
    xlabel_y = plot_bounds.ybot - xtic_height - xtic_textheight - xlabel_textheight
	+ ((float)xlablin+0.2) * t->v_char;
    ylabel_x = plot_bounds.xleft - ytic_width - ytic_textwidth;
    if (axis_array[FIRST_Y_AXIS].label.text && can_rotate)
	ylabel_x -= ylabel_textwidth;

    y2label_x = plot_bounds.xright + y2tic_width + y2tic_textwidth;
    if (axis_array[SECOND_Y_AXIS].label.text && can_rotate)
	y2label_x += y2label_textwidth - y2lablin * t->v_char;

    if (vertical_timelabel) {
	if (timelabel_bottom)
	    time_y = xlabel_y - timebot_textheight + xlabel_textheight;
	else {
	    time_y = title_y + timetop_textheight - title_textheight
		- x2label_textheight;
	}
    } else {
	if (timelabel_bottom)
	    time_y = plot_bounds.ybot - xtic_height - xtic_textheight - xlabel_textheight
		- timebot_textheight + t->v_char;
	else if (ylabel_textheight > 0)
	    time_y = ylabel_y + timetop_textheight;
	else
	    time_y = plot_bounds.ytop + x2tic_height + x2tic_textheight
		+ timetop_textheight + (int) t->h_char;
    }
    if (vertical_timelabel)
	time_x = plot_bounds.xleft - ytic_width - ytic_textwidth - timelabel_textwidth;
    else {
	double tmpx, tmpy;
	map_position_r(&(timelabel.offset), &tmpx, &tmpy, "boundary");
	time_x = plot_bounds.xleft - ytic_width - ytic_textwidth + (int) (tmpx);
    }

    xtic_y = plot_bounds.ybot - xtic_height
	- (int) (vertical_xtics ? t->h_char : t->v_char);

    x2tic_y = plot_bounds.ytop + x2tic_height
	+ (vertical_x2tics ? (int) t->h_char : x2tic_textheight);

    ytic_x = plot_bounds.xleft - ytic_width
	- (vertical_ytics
	   ? (ytic_textwidth - (int) t->v_char)
	   : (int) t->h_char);

    y2tic_x = plot_bounds.xright + y2tic_width
	+ (int) (vertical_y2tics ? t->v_char : t->h_char);

    /* restore text to horizontal [we tested rotation above] */
    (void) (*t->text_angle) (0);

    /* needed for map_position() below */
    AXIS_SETSCALE(FIRST_Y_AXIS, plot_bounds.ybot, plot_bounds.ytop);
    AXIS_SETSCALE(SECOND_Y_AXIS, plot_bounds.ybot, plot_bounds.ytop);
    AXIS_SETSCALE(FIRST_X_AXIS, plot_bounds.xleft, plot_bounds.xright);
    AXIS_SETSCALE(SECOND_X_AXIS, plot_bounds.xleft, plot_bounds.xright);
    /* HBB 20020122: moved here from do_plot, because map_position
     * needs these, too */
    axis_set_graphical_range(FIRST_X_AXIS, plot_bounds.xleft, plot_bounds.xright);
    axis_set_graphical_range(FIRST_Y_AXIS, plot_bounds.ybot, plot_bounds.ytop);
    axis_set_graphical_range(SECOND_X_AXIS, plot_bounds.xleft, plot_bounds.xright);
    axis_set_graphical_range(SECOND_Y_AXIS, plot_bounds.ybot, plot_bounds.ytop);

    /* Calculate limiting bounds of the key */
    do_key_bounds(key);


    /* Set default clipping to the plot boundary */
    clip_area = &plot_bounds;

    /* Sanity check. FIXME:  Stricter test? Fatal error? */
    if (plot_bounds.xright < plot_bounds.xleft
    ||  plot_bounds.ytop   < plot_bounds.ybot)
	int_warn(NO_CARET, "Terminal canvas area too small to hold plot."
			"\n\t    Check plot boundary and font sizes.");

}

/*}}} */

void
do_key_bounds(legend_key *key)
{
    struct termentry *t = term;

    key_height = key_title_height + key_title_extra
		+ key_rows * key_entry_height + key->height_fix * key_entry_height;
    key_width = key_col_wth * key_cols;

    /* Key inside plot boundaries */
    if (key->region == GPKEY_AUTO_INTERIOR_LRTBC
	|| (key->region == GPKEY_AUTO_EXTERIOR_LRTBC && key->vpos == JUST_CENTRE && key->hpos == CENTRE)) {
	if (key->vpos == JUST_TOP) {
	    key->bounds.ytop = plot_bounds.ytop - t->v_tic;
	    key->bounds.ybot = key->bounds.ytop - key_height;
	} else if (key->vpos == JUST_BOT) {
	    key->bounds.ybot = plot_bounds.ybot + t->v_tic;
	    key->bounds.ytop = key->bounds.ybot + key_height;
	} else /* (key->vpos == JUST_CENTRE) */ {
	    key->bounds.ybot = ((plot_bounds.ybot + plot_bounds.ytop) - key_height) / 2;
	    key->bounds.ytop = ((plot_bounds.ybot + plot_bounds.ytop) + key_height) / 2;
	}
	if (key->hpos == LEFT) {
	    key->bounds.xleft = plot_bounds.xleft + t->h_char;
	    key->bounds.xright = key->bounds.xleft + key_width;
	} else if (key->hpos == RIGHT) {
	    key->bounds.xright = plot_bounds.xright - t->h_char;
	    key->bounds.xleft = key->bounds.xright - key_width;
	} else /* (key->hpos == CENTER) */ {
	    key->bounds.xleft = ((plot_bounds.xright + plot_bounds.xleft) - key_width) / 2;
	    key->bounds.xright = ((plot_bounds.xright + plot_bounds.xleft) + key_width) / 2;
	}

    /* Key outside plot boundaries */
    } else if (key->region == GPKEY_AUTO_EXTERIOR_LRTBC || key->region == GPKEY_AUTO_EXTERIOR_MARGIN) {

	/* Vertical alignment */
	if (key->margin == GPKEY_TMARGIN) {
	    /* align top first since tmargin may be manual */
	    key->bounds.ytop = (ysize + yoffset) * t->ymax - t->v_tic;
	    key->bounds.ybot = key->bounds.ytop - key_height;
	} else if (key->margin == GPKEY_BMARGIN) {
	    /* align bottom first since bmargin may be manual */
	    key->bounds.ybot = yoffset * t->ymax + t->v_tic;
	    key->bounds.ytop = key->bounds.ybot + key_height;
	} else {
	    if (key->vpos == JUST_TOP) {
		/* align top first since tmargin may be manual */
		key->bounds.ytop = plot_bounds.ytop;
		key->bounds.ybot = key->bounds.ytop - key_height;
	    } else if (key->vpos == JUST_CENTRE) {
		key->bounds.ybot = ((plot_bounds.ybot + plot_bounds.ytop) - key_height) / 2;
		key->bounds.ytop = ((plot_bounds.ybot + plot_bounds.ytop) + key_height) / 2;
	    } else {
		/* align bottom first since bmargin may be manual */
		key->bounds.ybot = plot_bounds.ybot;
		key->bounds.ytop = key->bounds.ybot + key_height;
	    }
	}

	/* Horizontal alignment */
	if (key->margin == GPKEY_LMARGIN) {
	    /* align left first since lmargin may be manual */
	    key->bounds.xleft = xoffset * t->xmax + t->h_char;
	    key->bounds.xright = key->bounds.xleft + key_width;
	} else if (key->margin == GPKEY_RMARGIN) {
	    /* align right first since rmargin may be manual */
	    key->bounds.xright = (xsize + xoffset) * (t->xmax-1) - t->h_char;
	    key->bounds.xleft = key->bounds.xright - key_width;
	} else {
	    if (key->hpos == LEFT) {
		/* align left first since lmargin may be manual */
		key->bounds.xleft = plot_bounds.xleft;
		key->bounds.xright = key->bounds.xleft + key_width;
	    } else if (key->hpos == CENTRE) {
		key->bounds.xleft  = ((plot_bounds.xright + plot_bounds.xleft) - key_width) / 2;
		key->bounds.xright = ((plot_bounds.xright + plot_bounds.xleft) + key_width) / 2;
	    } else {
		/* align right first since rmargin may be manual */
		key->bounds.xright = plot_bounds.xright;
		key->bounds.xleft = key->bounds.xright - key_width;
	    }
	}

    /* Key at explicit position specified by user */
    } else {
	int x, y;

	/* FIXME!!!
	 * pm 22.1.2002: if key->user_pos.scalex or scaley == first_axes or second_axes,
	 * then the graph scaling is not yet known and the box is positioned incorrectly;
	 * you must do "replot" to avoid the wrong plot ... bad luck if output does not
	 * go to screen
	 */
	map_position(&key->user_pos, &x, &y, "key");

	/* Here top, bottom, left, right refer to the alignment with respect to point. */
	key->bounds.xleft = x;
	if (key->hpos == CENTRE)
	    key->bounds.xleft -= key_width/2;
	else if (key->hpos == RIGHT)
	    key->bounds.xleft -= key_width;
	key->bounds.xright = key->bounds.xleft + key_width;
	key->bounds.ytop = y;
	if (key->vpos == JUST_CENTRE)
	    key->bounds.ytop += key_height/2;
	else if (key->vpos == JUST_BOT)
	    key->bounds.ytop += key_height;
	key->bounds.ybot = key->bounds.ytop - key_height;
    }
}

/* Calculate positioning of components that make up the key box */
void
do_key_layout(legend_key *key)
{
    struct termentry *t = term;
    TBOOLEAN key_panic = FALSE;

    /* If there is a separate font for the key, use it for space calculations.	*/
    if (key->font)
	t->set_font(key->font);

    key_xleft = 0;

    if (key->swidth >= 0) {
	key_sample_width = key->swidth * t->h_char + t->h_tic;
    } else {
	key_sample_width = 0;
    }

    key_entry_height = t->v_tic * 1.25 * key->vert_factor;
    if (key_entry_height < t->v_char)
	key_entry_height = t->v_char * key->vert_factor;
    /* HBB 20020122: safeguard to prevent division by zero later */
    if (key_entry_height == 0)
	key_entry_height = 1;

    /* Key title length and height */
    key_title_height = 0;
    key_title_extra = 0;
    if (key->title.text) {
	int ytheight;
	(void) label_width(key->title.text, &ytheight);
	key_title_height = ytheight * t->v_char;
	if ((*key->title.text) && (t->flags & TERM_ENHANCED_TEXT)
	&&  (strchr(key->title.text,'^') || strchr(key->title.text,'_')))
	    key_title_extra = t->v_char;
    }

    if (key->reverse) {
	key_sample_left = -key_sample_width;
	key_sample_right = 0;
	/* if key width is being used, adjust right-justified text */
	key_text_left = t->h_char;
	key_text_right = t->h_char * (max_ptitl_len + 1 + key->width_fix);
	key_size_left = t->h_char - key_sample_left; /* sample left is -ve */
	key_size_right = key_text_right;
    } else {
	key_sample_left = 0;
	key_sample_right = key_sample_width;
	/* if key width is being used, adjust left-justified text */
	key_text_left = -(int) (t->h_char
				* (max_ptitl_len + 1 + key->width_fix));
	key_text_right = -(int) t->h_char;
	key_size_left = -key_text_left;
	key_size_right = key_sample_right + t->h_char;
    }
    key_point_offset = (key_sample_left + key_sample_right) / 2;

    /* advance width for cols */
    key_col_wth = key_size_left + key_size_right;

    key_rows = ptitl_cnt;
    key_cols = 1;

    /* calculate rows and cols for key */

    if (key->stack_dir == GPKEY_HORIZONTAL) {
	/* maximise no cols, limited by label-length */
	key_cols = (int) (plot_bounds.xright - plot_bounds.xleft) / key_col_wth;
	if (key->maxcols > 0 && key_cols > key->maxcols)
	    key_cols = key->maxcols;
	/* EAM Dec 2004 - Rather than turn off the key, try to squeeze */
	if (key_cols == 0) {
	    key_cols = 1;
	    key_panic = TRUE;
	    key_col_wth = (plot_bounds.xright - plot_bounds.xleft);
	}
	key_rows = (ptitl_cnt + key_cols - 1) / key_cols;
	/* now calculate actual no cols depending on no rows */
	key_cols = (key_rows == 0) ? 1 : (ptitl_cnt + key_rows - 1) / key_rows;
	if (key_cols == 0) {
	    key_cols = 1;
	}
    } else {
	/* maximise no rows, limited by plot_bounds.ytop-plot_bounds.ybot */
	int i = (plot_bounds.ytop - plot_bounds.ybot - key->height_fix * key_entry_height
		    - key_title_height - key_title_extra)
		/ key_entry_height;
	if (key->maxrows > 0 && i > key->maxrows)
	    i = key->maxrows;

	if (i == 0) {
	    i = 1;
	    key_panic = TRUE;
	}
	if (ptitl_cnt > i) {
	    key_cols = (ptitl_cnt + i - 1) / i;
	    /* now calculate actual no rows depending on no cols */
	    if (key_cols == 0) {
		key_cols = 1;
		key_panic = TRUE;
	    }
	    key_rows = (ptitl_cnt + key_cols - 1) / key_cols;
	}
    }

    /* If the key title is wider than the contents, try to make room for it */
    if (key->title.text) {
	int ytlen = label_width(key->title.text, NULL) - key->swidth + 2;
	ytlen *= t->h_char;
	if (ytlen > key_cols * key_col_wth)
	    key_col_wth = ytlen / key_cols;
    }

    /* Adjust for outside key, leave manually set margins alone */
    if ((key->region == GPKEY_AUTO_EXTERIOR_LRTBC && (key->vpos != JUST_CENTRE || key->hpos != CENTRE))
	|| key->region == GPKEY_AUTO_EXTERIOR_MARGIN) {
	int more = 0;
	if (key->margin == GPKEY_BMARGIN && bmargin.x < 0) {
	    more = key_rows * key_entry_height + key_title_height + key_title_extra
		    + key->height_fix * key_entry_height;
	    if (plot_bounds.ybot + more > plot_bounds.ytop)
		key_panic = TRUE;
	    else
		plot_bounds.ybot += more;
	} else if (key->margin == GPKEY_TMARGIN && tmargin.x < 0) {
	    more = key_rows * key_entry_height + key_title_height + key_title_extra
		    + key->height_fix * key_entry_height;
	    if (plot_bounds.ytop - more < plot_bounds.ybot)
		key_panic = TRUE;
	    else
		plot_bounds.ytop -= more;
	} else if (key->margin == GPKEY_LMARGIN && lmargin.x < 0) {
	    more = key_col_wth * key_cols;
	    if (plot_bounds.xleft + more > plot_bounds.xright)
		key_panic = TRUE;
	    else
		key_xleft = more;
	    plot_bounds.xleft += key_xleft;
	} else if (key->margin == GPKEY_RMARGIN && rmargin.x < 0) {
	    more = key_col_wth * key_cols;
	    if (plot_bounds.xright - more < plot_bounds.xleft)
		key_panic = TRUE;
	    else
		plot_bounds.xright -= more;
	}
    }

    /* Restore default font */
    if (key->font)
	t->set_font("");

    /* warn if we had to punt on key size calculations */
    if (key_panic)
	int_warn(NO_CARET, "Warning - difficulty fitting plot titles into key");
}

int
find_maxl_keys(struct curve_points *plots, int count, int *kcnt)
{
    int mlen, len, curve, cnt;
    int previous_plot_style = 0;
    struct curve_points *this_plot;

    mlen = cnt = 0;
    this_plot = plots;
    for (curve = 0; curve < count; this_plot = this_plot->next, curve++) {
	if (this_plot->title && !this_plot->title_is_suppressed) {
	    ignore_enhanced(this_plot->title_no_enhanced);
	    len = estimate_strlen(this_plot->title);
	    if (len != 0) {
		cnt++;
		if (len > mlen)
		    mlen = len;
	    }
	    ignore_enhanced(FALSE);
	}

	/* Check for new histogram here and save space for divider */
	if (this_plot->plot_style == HISTOGRAMS
	&&  previous_plot_style == HISTOGRAMS
	&&  this_plot->histogram_sequence == 0 && cnt > 1)
	    cnt++;
	/* Check for column-stacked histogram with key entries */
	if (this_plot->plot_style == HISTOGRAMS &&  this_plot->labels) {
	    text_label *key_entry = this_plot->labels->next;
	    for (; key_entry; key_entry=key_entry->next) {
		cnt++;
		len = key_entry->text ? estimate_strlen(key_entry->text) : 0;
		if (len > mlen)
		    mlen = len;
	    }
	}
	previous_plot_style = this_plot->plot_style;
    }

    if (kcnt != NULL)
	*kcnt = cnt;
    return (mlen);
}

/*
 * Make the key sample code a subroutine so that it can eventually be
 * shared by the 3d code also. As of now the two code sections are not
 * very parallel.  EAM Nov 2003
 */

void
do_key_sample(
    struct curve_points *this_plot,
    legend_key *key,
    char *title,
    int xl, int yl)
{
    struct termentry *t = term;

    /* Clip key box against canvas */
    BoundingBox *clip_save = clip_area;
    if (term->flags & TERM_CAN_CLIP)
	clip_area = NULL;
    else
	clip_area = &canvas;

    (*t->layer)(TERM_LAYER_BEGIN_KEYSAMPLE);

    if (key->textcolor.type == TC_VARIABLE)
	/* Draw key text in same color as plot */
	;
    else if (key->textcolor.type != TC_DEFAULT)
	/* Draw key text in same color as key title */
	apply_pm3dcolor(&key->textcolor, t);
    else
	/* Draw key text in black */
	(*t->linetype)(LT_BLACK);

    if (key->just == GPKEY_LEFT) {
	write_multiline(xl + key_text_left, yl, title, LEFT, JUST_CENTRE, 0, key->font);
    } else {
	if ((*t->justify_text) (RIGHT)) {
	    write_multiline(xl + key_text_right, yl, title, RIGHT, JUST_CENTRE, 0, key->font);
	} else {
	    int x = xl + key_text_right - t->h_char * estimate_strlen(title);
	    if (key->region == GPKEY_AUTO_EXTERIOR_LRTBC ||	/* HBB 990327 */
		key->region == GPKEY_AUTO_EXTERIOR_MARGIN ||
		inrange((x), (plot_bounds.xleft), (plot_bounds.xright)))
		write_multiline(x, yl, title, LEFT, JUST_CENTRE, 0, key->font);
	}
    }

    /* Draw sample in same style and color as the corresponding plot  */
    /* The variable color case uses the color of the first data point */
    if (!check_for_variable_color(this_plot, &this_plot->varcolor[0]))
	term_apply_lp_properties(&this_plot->lp_properties);

    /* draw sample depending on bits set in plot_style */
    if (this_plot->plot_style & PLOT_STYLE_HAS_FILL && t->fillbox) {
	struct fill_style_type *fs = &this_plot->fill_properties;
	int style = style_from_fill(fs);
	unsigned int x = xl + key_sample_left;
	unsigned int y = yl - key_entry_height/4;
	unsigned int w = key_sample_right - key_sample_left;
	unsigned int h = key_entry_height/2;

#ifdef EAM_OBJECTS
	if (this_plot->plot_style == CIRCLES && w > 0) {
	    do_arc(xl + key_point_offset, yl, key_entry_height/4, 0., 360., style, FALSE);
	    /* Retrace the border if the style requests it */
	    if (need_fill_border(fs)) {
	        do_arc(xl + key_point_offset, yl, key_entry_height/4, 0., 360., 0, FALSE);
	    }
	} else if (this_plot->plot_style == ELLIPSES && w > 0) {
	    t_ellipse *key_ellipse = (t_ellipse *) gp_alloc(sizeof(t_ellipse),
	        "cute little ellipse for the key sample");
	    key_ellipse->center.x = xl + key_point_offset;
	    key_ellipse->center.y = yl;
	    key_ellipse->extent.x = w * 2/3;
	    key_ellipse->extent.y = h;
	    key_ellipse->orientation = 0.0;
	    /* already in term coords, no need to map */
	    do_ellipse(2, key_ellipse, style, FALSE);
	    /* Retrace the border if the style requests it */
	    if (need_fill_border(fs)) {
		do_ellipse(2, key_ellipse, 0, FALSE);
	    }
	    free(key_ellipse);
	} else
#endif
	if (w > 0) {    /* All other plot types with fill */
	    if (style != FS_EMPTY)
		(*t->fillbox)(style,x,y,w,h);

	    /* need_fill_border will set the border linetype, but candlesticks don't want it */
	    if ((this_plot->plot_style == CANDLESTICKS && fs->border_color.type == TC_LT
							&& fs->border_color.lt == LT_NODRAW)
	    ||   style == FS_EMPTY
	    ||   need_fill_border(fs)) {
		newpath();
		draw_clip_line( xl + key_sample_left,  yl - key_entry_height/4,
			    xl + key_sample_right, yl - key_entry_height/4);
		draw_clip_line( xl + key_sample_right, yl - key_entry_height/4,
			    xl + key_sample_right, yl + key_entry_height/4);
		draw_clip_line( xl + key_sample_right, yl + key_entry_height/4,
			    xl + key_sample_left,  yl + key_entry_height/4);
		draw_clip_line( xl + key_sample_left,  yl + key_entry_height/4,
			    xl + key_sample_left,  yl - key_entry_height/4);
		closepath();
	    }
	    if (fs->fillstyle != FS_EMPTY && fs->fillstyle != FS_DEFAULT
	    && !(fs->border_color.type == TC_LT && fs->border_color.lt == LT_NODRAW)) {
		/* need_fill_border() might have changed our original linetype */
		term_apply_lp_properties(&this_plot->lp_properties);
	    }
	}

    } else if (this_plot->plot_style == VECTOR && t->arrow) {
	    apply_head_properties(&(this_plot->arrow_properties));
	    draw_clip_arrow(xl + key_sample_left, yl, xl + key_sample_right, yl,
			this_plot->arrow_properties.head);

    } else if ((this_plot->plot_style & PLOT_STYLE_HAS_LINE)
		   || ((this_plot->plot_style & PLOT_STYLE_HAS_ERRORBAR)
		       && this_plot->plot_type == DATA)) {
	if (this_plot->lp_properties.l_type != LT_NODRAW)
	    /* errors for data plots only */
	    draw_clip_line(xl + key_sample_left, yl, xl + key_sample_right, yl);
    }

    if ((this_plot->plot_type == DATA)
	&& (this_plot->plot_style & PLOT_STYLE_HAS_ERRORBAR)
	&& (this_plot->plot_style != CANDLESTICKS)
	&& (bar_size > 0.0)) {
	draw_clip_line( xl + key_sample_left, yl + ERRORBARTIC,
			xl + key_sample_left, yl - ERRORBARTIC);
	draw_clip_line( xl + key_sample_right, yl + ERRORBARTIC,
			xl + key_sample_right, yl - ERRORBARTIC);
    }

    /* oops - doing the point sample now would break the postscript
     * terminal for example, which changes current line style
     * when drawing a point, but does not restore it. We must wait to
     * draw the point sample at the end of do_plot (comment KEY SAMPLES).
     */

    (*t->layer)(TERM_LAYER_END_KEYSAMPLE);

    /* Restore previous clipping area */
    clip_area = clip_save;
}

void
do_key_sample_point(
    struct curve_points *this_plot,
    legend_key *key,
    int xl, int yl)
{
    struct termentry *t = term;

    (t->layer)(TERM_LAYER_BEGIN_KEYSAMPLE);

    if (this_plot->plot_style == LINESPOINTS
	 &&  this_plot->lp_properties.p_interval < 0) {
	t_colorspec background_fill = BACKGROUND_COLORSPEC;
	(*t->set_color)(&background_fill);
	(*t->pointsize)(pointsize * pointintervalbox);
	(*t->point)(xl + key_point_offset, yl, 6);
	term_apply_lp_properties(&this_plot->lp_properties);
    }

    if (this_plot->plot_style == BOXPLOT) {
	;	/* Don't draw a sample point in the key */

    } else if (this_plot->plot_style == DOTS) {
	if (on_page(xl + key_point_offset, yl))
	    (*t->point) (xl + key_point_offset, yl, -1);

    } else if (this_plot->plot_style & PLOT_STYLE_HAS_POINT) {
	if (this_plot->lp_properties.p_size == PTSZ_VARIABLE)
	    (*t->pointsize)(pointsize);
	if (on_page(xl + key_point_offset, yl)) {
	    if (this_plot->lp_properties.p_type == PT_CHARACTER) {
		apply_pm3dcolor(&(this_plot->labels->textcolor), t);
		(*t->put_text) (xl + key_point_offset, yl, 
				(char *)(&this_plot->lp_properties.p_char));
		apply_pm3dcolor(&(this_plot->lp_properties.pm3d_color), t);
	    } else {
		(*t->point) (xl + key_point_offset, yl, 
				this_plot->lp_properties.p_type);
	    }
	}
    }

    (t->layer)(TERM_LAYER_END_KEYSAMPLE);
}

/* Graph legend is now optionally done in two passes. The first pass calculates	*/
/* and reserves the necessary space.  Next the individual plots in the graph 	*/
/* are drawn. Then the reserved space for the legend is blanked out, and 	*/
/* finally the second pass through this code draws the legend.			*/
void
draw_key(legend_key *key, TBOOLEAN key_pass, int *xinkey, int *yinkey)
{
    struct termentry *t = term;

    /* In two-pass mode (set key opaque) we blank out the key box after	*/
    /* the graph is drawn and then redo the key in the blank area.	*/
    if (key_pass && t->fillbox && !(t->flags & TERM_NULL_SET_COLOR)) {
	t_colorspec background_fill = BACKGROUND_COLORSPEC;
	(*t->set_color)(&background_fill);
	(*t->fillbox)(FS_OPAQUE, key->bounds.xleft, key->bounds.ybot,
		key_width, key_height);
    }

    if (key->title.text) {
	int title_anchor;
	if (key->title.pos == CENTRE)
		title_anchor = (key->bounds.xleft + key->bounds.xright) / 2;
	else if (key->title.pos == RIGHT)
		title_anchor = key->bounds.xright - term->h_char;
	else
		title_anchor = key->bounds.xleft + term->h_char;

	/* Only draw the title once */
	if (key_pass || !key->front) {
	    /* FIXME: Now that there is a full text_label structure for the key title */
	    /*        maybe we should call write_label() to get the full processing?  */
	    if (key->textcolor.type == TC_RGB && key->textcolor.value < 0)
		apply_pm3dcolor(&(key->box.pm3d_color), t);
	    else
		apply_pm3dcolor(&(key->textcolor), t);
	    ignore_enhanced(key->title.noenhanced);
	    write_multiline(title_anchor, 
			key->bounds.ytop - (key_title_extra + key_entry_height)/2,
			key->title.text, key->title.pos, JUST_TOP, 0, 
			key->title.font ? key->title.font : key->font);
	    ignore_enhanced(FALSE);
	    (*t->linetype)(LT_BLACK);
	}
    }

    if (key->box.l_type > LT_NODRAW) {
	BoundingBox *clip_save = clip_area;
	if (term->flags & TERM_CAN_CLIP)
	    clip_area = NULL;
	else
	    clip_area = &canvas;
	term_apply_lp_properties(&key->box);
	newpath();
	draw_clip_line(key->bounds.xleft, key->bounds.ybot, key->bounds.xleft, key->bounds.ytop);
	draw_clip_line(key->bounds.xleft, key->bounds.ytop, key->bounds.xright, key->bounds.ytop);
	draw_clip_line(key->bounds.xright, key->bounds.ytop, key->bounds.xright, key->bounds.ybot);
	draw_clip_line(key->bounds.xright, key->bounds.ybot, key->bounds.xleft, key->bounds.ybot);
	closepath();
	/* draw a horizontal line between key title and first entry */
	if (key->title.text)
	    draw_clip_line( key->bounds.xleft,
	    		    key->bounds.ytop - (key_title_height + key_title_extra),
			    key->bounds.xright,
	    		    key->bounds.ytop - (key_title_height + key_title_extra));
	clip_area = clip_save;
    }

    yl_ref = key->bounds.ytop - (key_title_height + key_title_extra);
    yl_ref -= ((key->height_fix + 1) * key_entry_height) / 2;
    *xinkey = key->bounds.xleft + key_size_left;
    *yinkey = yl_ref;
}

/*
 * This routine draws the plot title, the axis labels, and an optional time stamp.
 */
void
draw_titles()
{
    struct termentry *t = term;

    /* YLABEL */
    if (axis_array[FIRST_Y_AXIS].label.text) {
	ignore_enhanced(axis_array[FIRST_Y_AXIS].label.noenhanced);
	apply_pm3dcolor(&(axis_array[FIRST_Y_AXIS].label.textcolor),t);
	/* we worked out x-posn in boundary() */
	if ((*t->text_angle) (axis_array[FIRST_Y_AXIS].label.rotate)) {
	    double tmpx, tmpy;
	    unsigned int x, y;
	    map_position_r(&(axis_array[FIRST_Y_AXIS].label.offset),
			   &tmpx, &tmpy, "doplot");

	    x = ylabel_x + (t->v_char / 2);
	    y = (plot_bounds.ytop + plot_bounds.ybot) / 2 + tmpy;

	    write_multiline(x, y, axis_array[FIRST_Y_AXIS].label.text,
			    CENTRE, JUST_TOP, axis_array[FIRST_Y_AXIS].label.rotate,
			    axis_array[FIRST_Y_AXIS].label.font);
	    (*t->text_angle) (0);
	} else {
	    /* really bottom just, but we know number of lines
	       so we need to adjust x-posn by one line */
	    unsigned int x = ylabel_x;
	    unsigned int y = ylabel_y;

	    write_multiline(x, y, axis_array[FIRST_Y_AXIS].label.text,
			    LEFT, JUST_TOP, 0,
			    axis_array[FIRST_Y_AXIS].label.font);
	}
	reset_textcolor(&(axis_array[FIRST_Y_AXIS].label.textcolor),t);
	ignore_enhanced(FALSE);
    }

    /* Y2LABEL */
    if (axis_array[SECOND_Y_AXIS].label.text) {
	ignore_enhanced(axis_array[SECOND_Y_AXIS].label.noenhanced);
	apply_pm3dcolor(&(axis_array[SECOND_Y_AXIS].label.textcolor),t);
	/* we worked out coordinates in boundary() */
	if ((*t->text_angle) (axis_array[SECOND_Y_AXIS].label.rotate)) {
	    double tmpx, tmpy;
	    unsigned int x, y;
	    map_position_r(&(axis_array[SECOND_Y_AXIS].label.offset),
			   &tmpx, &tmpy, "doplot");
	    x = y2label_x + (t->v_char / 2) - 1;
	    y = (plot_bounds.ytop + plot_bounds.ybot) / 2 + tmpy;

	    write_multiline(x, y, axis_array[SECOND_Y_AXIS].label.text,
			    CENTRE, JUST_TOP,
			    axis_array[SECOND_Y_AXIS].label.rotate,
			    axis_array[SECOND_Y_AXIS].label.font);
	    (*t->text_angle) (0);
	} else {
	    /* really bottom just, but we know number of lines */
	    unsigned int x = y2label_x;
	    unsigned int y = y2label_y;

	    write_multiline(x, y, axis_array[SECOND_Y_AXIS].label.text,
			    RIGHT, JUST_TOP, 0,
			    axis_array[SECOND_Y_AXIS].label.font);
	}
	reset_textcolor(&(axis_array[SECOND_Y_AXIS].label.textcolor),t);
	ignore_enhanced(FALSE);
    }

    /* XLABEL */
    if (axis_array[FIRST_X_AXIS].label.text) {
	double tmpx, tmpy;
	unsigned int x, y;
	map_position_r(&(axis_array[FIRST_X_AXIS].label.offset),
		       &tmpx, &tmpy, "doplot");

	x = (plot_bounds.xright + plot_bounds.xleft) / 2 +  tmpx;
	y = xlabel_y - t->v_char / 2;   /* HBB */

	ignore_enhanced(axis_array[FIRST_X_AXIS].label.noenhanced);
	apply_pm3dcolor(&(axis_array[FIRST_X_AXIS].label.textcolor), t);
	write_multiline(x, y, axis_array[FIRST_X_AXIS].label.text,
			CENTRE, JUST_TOP, 0,
			axis_array[FIRST_X_AXIS].label.font);
	reset_textcolor(&(axis_array[FIRST_X_AXIS].label.textcolor), t);
	ignore_enhanced(FALSE);
    }

    /* PLACE TITLE */
    if (title.text) {
	double tmpx, tmpy;
	unsigned int x, y;
	map_position_r(&(title.offset), &tmpx, &tmpy, "doplot");
	/* we worked out y-coordinate in boundary() */
	x = (plot_bounds.xleft + plot_bounds.xright) / 2 + tmpx;
	y = title_y - t->v_char / 2;

	ignore_enhanced(title.noenhanced);
	apply_pm3dcolor(&(title.textcolor), t);
	write_multiline(x, y, title.text, CENTRE, JUST_TOP, 0, title.font);
	reset_textcolor(&(title.textcolor), t);
	ignore_enhanced(FALSE);
    }

    /* X2LABEL */
    if (axis_array[SECOND_X_AXIS].label.text) {
	double tmpx, tmpy;
	unsigned int x, y;
	map_position_r(&(axis_array[SECOND_X_AXIS].label.offset),
		       &tmpx, &tmpy, "doplot");
	/* we worked out y-coordinate in boundary() */
	x = (plot_bounds.xright + plot_bounds.xleft) / 2 + tmpx;
	y = x2label_y - t->v_char / 2 - 1;
	ignore_enhanced(axis_array[SECOND_X_AXIS].label.noenhanced);
	apply_pm3dcolor(&(axis_array[SECOND_X_AXIS].label.textcolor),t);
	write_multiline(x, y, axis_array[SECOND_X_AXIS].label.text, CENTRE,
			JUST_TOP, 0, axis_array[SECOND_X_AXIS].label.font);
	reset_textcolor(&(axis_array[SECOND_X_AXIS].label.textcolor),t);
	ignore_enhanced(FALSE);
    }

    /* PLACE TIMEDATE */
    if (timelabel.text) {
	/* we worked out coordinates in boundary() */
	char *str;
	time_t now;
	unsigned int x = time_x;
	unsigned int y = time_y;
	time(&now);
	/* there is probably no way to find out in advance how many
	 * chars strftime() writes */
	str = gp_alloc(MAX_LINE_LEN + 1, "timelabel.text");
	strftime(str, MAX_LINE_LEN, timelabel.text, localtime(&now));

	apply_pm3dcolor(&(timelabel.textcolor), t);
	if (timelabel_rotate && (*t->text_angle) (TEXT_VERTICAL)) {
	    x += t->v_char / 2;	/* HBB */
	    if (timelabel_bottom)
		write_multiline(x, y, str, LEFT, JUST_TOP, TEXT_VERTICAL, timelabel.font);
	    else
		write_multiline(x, y, str, RIGHT, JUST_TOP, TEXT_VERTICAL, timelabel.font);
	    (*t->text_angle) (0);
	} else {
	    y -= t->v_char / 2;	/* HBB */
	    if (timelabel_bottom)
		write_multiline(x, y, str, LEFT, JUST_BOT, 0, timelabel.font);
	    else
		write_multiline(x, y, str, LEFT, JUST_TOP, 0, timelabel.font);
	}
	free(str);
    }
}
