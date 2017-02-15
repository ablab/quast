/*
 * $Id: graphics.h,v 1.61.2.4 2016/05/11 18:38:51 sfeam Exp $
 */

/* GNUPLOT - graphics.h */

/*[
 * Copyright 1999, 2004
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

#ifndef GNUPLOT_GRAPHICS_H
# define GNUPLOT_GRAPHICS_H

#include "syscfg.h"
#include "gp_types.h"

#include "gadgets.h"
#include "term_api.h"

/* types defined for 2D plotting */

typedef struct curve_points {
    struct curve_points *next;	/* pointer to next plot in linked list */
    int token;			/* last token used, for second parsing pass */
    enum PLOT_TYPE plot_type;	/* DATA2D? DATA3D? FUNC2D FUNC3D? NODATA? */
    enum PLOT_STYLE plot_style;	/* style set by "with" or by default */
    char *title;		/* plot title, a.k.a. key entry */
    int title_position;		/* -1 for beginning; +1 for end */
    TBOOLEAN title_no_enhanced;	/* don't typeset title in enhanced mode */
    TBOOLEAN title_is_filename;	/* TRUE if title was auto-generated from filename */
    TBOOLEAN title_is_suppressed;/* TRUE if 'notitle' was specified */
    TBOOLEAN noautoscale;	/* ignore data from this plot during autoscaling */
    struct lp_style_type lp_properties;
    struct arrow_style_type arrow_properties;
    struct fill_style_type fill_properties;
    struct text_label *labels;	/* Only used if plot_style == LABELPOINTS */
    struct t_image image_properties;	/* only used if plot_style is IMAGE or RGB_IMAGE */
    struct udvt_entry *sample_var;	/* Only used if plot has private sampling range */

    /* 2D and 3D plot structure fields overlay only to this point */
    filledcurves_opts filledcurves_options;
    int base_linetype;		/* before any calls to load_linetype(), lc variable */
				/* analogous to hidden3d_top_linetype in graph3d.h  */
    int ellipseaxes_units;              /* Only used if plot_style == ELLIPSES */    
    struct histogram_style *histogram;	/* Only used if plot_style == HISTOGRAM */
    int histogram_sequence;	/* Ordering of this dataset within the histogram */
    enum PLOT_SMOOTH plot_smooth; /* which "smooth" method to be used? */
    double smooth_parameter;	/* e.g. optional bandwidth for smooth kdensity */
    int boxplot_factors;	/* Only used if plot_style == BOXPLOT */
    int p_max;			/* how many points are allocated */
    int p_count;		/* count of points in points */
    int x_axis;			/* FIRST_X_AXIS or SECOND_X_AXIS */
    int y_axis;			/* FIRST_Y_AXIS or SECOND_Y_AXIS */
    int z_axis;			/* same as either x_axis or y_axis, for 5-column plot types */
    int n_par_axes;		/* Only used for parallel axis plots */
    double **z_n;		/* Only used for parallel axis plots */
    double *varcolor;		/* Only used if plot has variable color */
    struct coordinate GPHUGE *points;
} curve_points;

/* externally visible variables of graphics.h */

/* 'set offset' status variables */
extern t_position loff, roff, toff, boff;

/* 'set bar' status */
extern double bar_size;
extern int bar_layer;

/* function prototypes */

void do_plot __PROTO((struct curve_points *, int));
void map_position __PROTO((struct position * pos, int *x, int *y, const char *what));
void map_position_r __PROTO((struct position* pos, double* x, double* y,
			     const char* what));

void init_histogram __PROTO((struct histogram_style *hist, text_label *title));
void free_histlist __PROTO((struct histogram_style *hist));

void plot_image_or_update_axes __PROTO((void *plot, TBOOLEAN update_axes));
TBOOLEAN check_for_variable_color __PROTO((struct curve_points *plot, double *colorvalue));


#ifdef EAM_OBJECTS
void place_objects __PROTO((struct object *listhead, int layer, int dimensions));
void do_ellipse __PROTO((int dimensions, t_ellipse *e, int style, TBOOLEAN do_own_mapping ));
void do_polygon __PROTO((int dimensions, t_polygon *p, int style, t_clip_object clip ));
#else
#define place_objects(listhead,layer,dimensions) /* void() */
#endif

int filter_boxplot __PROTO((struct curve_points *));

#endif /* GNUPLOT_GRAPHICS_H */
