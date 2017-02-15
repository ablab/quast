#ifndef lint
static char *RCSid() { return RCSid("$Id: plot3d.c,v 1.231.2.7 2016/06/18 05:59:25 sfeam Exp $"); }
#endif

/* GNUPLOT - plot3d.c */

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

#include "plot3d.h"
#include "gp_types.h"

#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "contour.h"
#include "datafile.h"
#include "eval.h"
#include "graph3d.h"
#include "misc.h"
#include "parse.h"
#include "pm3d.h"
#include "setshow.h"
#include "term_api.h"
#include "tabulate.h"
#include "util.h"
#include "variable.h" /* For locale handling */

#include "plot2d.h" /* Only for store_label() */

#include "matrix.h" /* Used by thin-plate-splines in dgrid3d */

#ifndef _Windows
# include "help.h"
#endif

/* Sep 2013 - moved from axis.h in the process of doing away with it altogether */
#if (1)	/* FIXME:  I'm 99% sure TRUE is fine, i.e. NEED_PALETTE is unnecessary. */
	/* In any event, the alternative breaks splot with rgb variable */
#define NEED_PALETTE(plot) TRUE
#else
#define NEED_PALETTE(plot) \
    (  (pm3d.implicit == PM3D_IMPLICIT) \
    || ((plot)->plot_style == PM3DSURFACE) \
    || ((plot)->plot_style == IMAGE) \
    || (plot)->lp_properties.pm3d_color.type == TC_CB \
    || (plot)->lp_properties.pm3d_color.type == TC_FRAC \
    || (plot)->lp_properties.pm3d_color.type == TC_Z \
    )
#endif

/* global variables exported by this module */

t_data_mapping mapping3d = MAP3D_CARTESIAN;

int dgrid3d_row_fineness = 10;
int dgrid3d_col_fineness = 10;
int dgrid3d_norm_value = 1;
int dgrid3d_mode = DGRID3D_QNORM;
double dgrid3d_x_scale = 1.0;
double dgrid3d_y_scale = 1.0;
TBOOLEAN dgrid3d = FALSE;
TBOOLEAN dgrid3d_kdensity = FALSE;

/* static prototypes */

static void calculate_set_of_isolines __PROTO((AXIS_INDEX value_axis, TBOOLEAN cross, struct iso_curve **this_iso,
					       AXIS_INDEX iso_axis, double iso_min, double iso_step, int num_iso_to_use,
					       AXIS_INDEX sam_axis, double sam_min, double sam_step, int num_sam_to_use,
					       TBOOLEAN need_palette));
static int get_3ddata __PROTO((struct surface_points * this_plot));
static void eval_3dplots __PROTO((void));
static void grid_nongrid_data __PROTO((struct surface_points * this_plot));
static void parametric_3dfixup __PROTO((struct surface_points * start_plot, int *plot_num));
static struct surface_points * sp_alloc __PROTO((int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2));
static void sp_replace __PROTO((struct surface_points *sp, int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2));

/* helper functions for grid_nongrid_data() */
static double splines_kernel __PROTO((double h));
static void thin_plate_splines_setup __PROTO(( struct iso_curve *old_iso_crvs, double **p_xx, int *p_numpoints ));
static double qnorm __PROTO(( double dist_x, double dist_y, int q ));
static double pythag __PROTO(( double dx, double dy ));

/* helper functions for parsing */
static void load_contour_label_options __PROTO((struct text_label *contour_label));

/* the curves/surfaces of the plot */
struct surface_points *first_3dplot = NULL;
static struct udft_entry plot_func;

int plot3d_num=0;

/* HBB 20000508: moved these functions to the only module that uses them
 * so they can be turned 'static' */
/*
 * sp_alloc() allocates a surface_points structure that can hold 'num_iso_1'
 * iso-curves with 'num_samp_2' samples and 'num_iso_2' iso-curves with
 * 'num_samp_1' samples.
 * If, however num_iso_2 or num_samp_1 is zero no iso curves are allocated.
 */
static struct surface_points *
sp_alloc(int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2)
{
    struct lp_style_type default_lp_properties = DEFAULT_LP_STYLE_TYPE;

    struct surface_points *sp = gp_alloc(sizeof(*sp), "surface");
    memset(sp,0,sizeof(struct surface_points));

    /* Initialize various fields */
    sp->lp_properties = default_lp_properties;
    default_arrow_style(&(sp->arrow_properties));

    if (num_iso_2 > 0 && num_samp_1 > 0) {
	int i;
	struct iso_curve *icrv;

	for (i = 0; i < num_iso_1; i++) {
	    icrv = iso_alloc(num_samp_2);
	    icrv->next = sp->iso_crvs;
	    sp->iso_crvs = icrv;
	}
	for (i = 0; i < num_iso_2; i++) {
	    icrv = iso_alloc(num_samp_1);
	    icrv->next = sp->iso_crvs;
	    sp->iso_crvs = icrv;
	}
    }

    return (sp);
}

/*
 * sp_replace() updates a surface_points structure so it can hold 'num_iso_1'
 * iso-curves with 'num_samp_2' samples and 'num_iso_2' iso-curves with
 * 'num_samp_1' samples.
 * If, however num_iso_2 or num_samp_1 is zero no iso curves are allocated.
 */
static void
sp_replace(
    struct surface_points *sp,
    int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2)
{
    int i;
    struct iso_curve *icrv, *icrvs = sp->iso_crvs;

    while (icrvs) {
	icrv = icrvs;
	icrvs = icrvs->next;
	iso_free(icrv);
    }
    sp->iso_crvs = NULL;

    if (num_iso_2 > 0 && num_samp_1 > 0) {
	for (i = 0; i < num_iso_1; i++) {
	    icrv = iso_alloc(num_samp_2);
	    icrv->next = sp->iso_crvs;
	    sp->iso_crvs = icrv;
	}
	for (i = 0; i < num_iso_2; i++) {
	    icrv = iso_alloc(num_samp_1);
	    icrv->next = sp->iso_crvs;
	    sp->iso_crvs = icrv;
	}
    } else
	sp->iso_crvs = NULL;
}

/*
 * sp_free() releases any memory which was previously malloc()'d to hold
 *   surface points.
 */
/* HBB 20000506: don't risk stack havoc by recursion, use iterative list
 * cleanup instead */
void
sp_free(struct surface_points *sp)
{
    while (sp) {
	struct surface_points *next = sp->next_sp;
	if (sp->title)
	    free(sp->title);

	while (sp->contours) {
	    struct gnuplot_contours *next_cntrs = sp->contours->next;

	    free(sp->contours->coords);
	    free(sp->contours);
	    sp->contours = next_cntrs;
	}

	while (sp->iso_crvs) {
	    struct iso_curve *next_icrvs = sp->iso_crvs->next;

	    iso_free(sp->iso_crvs);
	    sp->iso_crvs = next_icrvs;
	}

	if (sp->labels) {
	    free_labels(sp->labels);
	    sp->labels = (struct text_label *)NULL;
	}

	free(sp);
	sp = next;
    }
}


void
plot3drequest()
/*
 * in the parametric case we would say splot [u= -Pi:Pi] [v= 0:2*Pi] [-1:1]
 * [-1:1] [-1:1] sin(v)*cos(u),sin(v)*cos(u),sin(u) in the non-parametric
 * case we would say only splot [x= -2:2] [y= -5:5] sin(x)*cos(y)
 *
 */
{
    int dummy_token0 = -1, dummy_token1 = -1;
    AXIS_INDEX u_axis, v_axis;

    is_3d_plot = TRUE;

    if (parametric && strcmp(set_dummy_var[0], "t") == 0) {
	strcpy(set_dummy_var[0], "u");
	strcpy(set_dummy_var[1], "v");
    }

    /* put stuff into arrays to simplify access */
    AXIS_INIT3D(FIRST_X_AXIS, 0, 0);
    AXIS_INIT3D(FIRST_Y_AXIS, 0, 0);
    AXIS_INIT3D(FIRST_Z_AXIS, 0, 1);
    AXIS_INIT3D(U_AXIS, 1, 0);
    AXIS_INIT3D(V_AXIS, 1, 0);
    AXIS_INIT3D(COLOR_AXIS, 0, 1);

    if (!term)			/* unknown */
	int_error(c_token, "use 'set term' to set terminal type first");

    /* Range limits for the entire plot are optional but must be given	*/
    /* in a fixed order. The keyword 'sample' terminates range parsing.	*/
    u_axis = (parametric ? U_AXIS : FIRST_X_AXIS);
    v_axis = (parametric ? V_AXIS : FIRST_Y_AXIS);

    dummy_token0 = parse_range(u_axis);
    dummy_token1 = parse_range(v_axis);

    if (parametric) {
	parse_range(FIRST_X_AXIS);
	parse_range(FIRST_Y_AXIS);
    }
    parse_range(FIRST_Z_AXIS);
    check_axis_reversed(FIRST_X_AXIS);
    check_axis_reversed(FIRST_Y_AXIS);
    check_axis_reversed(FIRST_Z_AXIS);
    if (equals(c_token,"sample") && equals(c_token+1,"["))
	c_token++;

    /* Clear out any tick labels read from data files in previous plot */
    for (u_axis=0; u_axis<AXIS_ARRAY_SIZE; u_axis++) {
	struct ticdef *ticdef = &axis_array[u_axis].ticdef;
	if (ticdef->def.user)
	    ticdef->def.user = prune_dataticks(ticdef->def.user);
	if (!ticdef->def.user && ticdef->type == TIC_USER)
	    ticdef->type = TIC_COMPUTED;
    }

    /* use the default dummy variable unless changed */
    if (dummy_token0 > 0)
	copy_str(c_dummy_var[0], dummy_token0, MAX_ID_LEN);
    else
	strcpy(c_dummy_var[0], set_dummy_var[0]);

    if (dummy_token1 > 0)
	copy_str(c_dummy_var[1], dummy_token1, MAX_ID_LEN);
    else
	strcpy(c_dummy_var[1], set_dummy_var[1]);

    /* In "set view map" mode the x2 and y2 axes are legal */
    /* but must be linked to the respective primary axis. */
    if (splot_map) {
	if ((axis_array[SECOND_X_AXIS].ticmode && !axis_array[SECOND_X_AXIS].linked_to_primary)
	||  (axis_array[SECOND_Y_AXIS].ticmode && !axis_array[SECOND_Y_AXIS].linked_to_primary))
	    int_error(NO_CARET,
		"Secondary axis must be linked to primary axis in order to draw tics");
    }

    eval_3dplots();
}


/* Helper function for refresh command.  Reexamine each data point and update the
 * flags for INRANGE/OUTRANGE/UNDEFINED based on the current limits for that axis.
 * Normally the axis limits are already known at this point. But if the user has
 * forced "set autoscale" since the previous plot or refresh, we need to reset the
 * axis limits and try to approximate the full auto-scaling behaviour.
 */
void
refresh_3dbounds(struct surface_points *first_plot, int nplots)
{
    struct surface_points *this_plot = first_plot;
    int iplot;		/* plot index */

    for (iplot = 0;  iplot < nplots; iplot++, this_plot = this_plot->next_sp) {
	int i;		/* point index */
	struct axis *x_axis = &axis_array[FIRST_X_AXIS];
	struct axis *y_axis = &axis_array[FIRST_Y_AXIS];
	struct axis *z_axis = &axis_array[FIRST_Z_AXIS];
	struct iso_curve *this_curve;

	/* IMAGE clipping is done elsewhere, so we don't need INRANGE/OUTRANGE
	 * checks.  
	 */
	if (this_plot->plot_style == IMAGE 
	||  this_plot->plot_style == RGBIMAGE
	||  this_plot->plot_style == RGBA_IMAGE) {
	    if (x_axis->set_autoscale)
		plot_image_or_update_axes(this_plot,TRUE);
	    continue;
	}

	for ( this_curve = this_plot->iso_crvs; this_curve; this_curve = this_curve->next) {

	for (i=0; i<this_curve->p_count; i++) {
	    struct coordinate GPHUGE *point = &this_curve->points[i];

	    if (point->type == UNDEFINED)
		continue;
	    else
		point->type = INRANGE;

	    /* If the state has been set to autoscale since the last plot,
	     * mark everything INRANGE and re-evaluate the axis limits now.
	     * Otherwise test INRANGE/OUTRANGE against previous axis limits.
	     */

	    /* This autoscaling logic is identical to that in
	     * refresh_bounds() in plot2d.c
	     */
	    if (!this_plot->noautoscale) {
		if (x_axis->set_autoscale & AUTOSCALE_MIN && point->x < x_axis->min)
		     x_axis->min = point->x;
		if (x_axis->set_autoscale & AUTOSCALE_MAX && point->x > x_axis->max)
		     x_axis->max = point->x;
	    }
	    if (!inrange(point->x, x_axis->min, x_axis->max)) {
		point->type = OUTRANGE;
		continue;
	    }
	    if (!this_plot->noautoscale) {
		if (y_axis->set_autoscale & AUTOSCALE_MIN && point->y < y_axis->min)
		     y_axis->min = point->y;
		if (y_axis->set_autoscale & AUTOSCALE_MAX && point->y > y_axis->max)
		     y_axis->max = point->y;
	    }
	    if (!inrange(point->y, y_axis->min, y_axis->max)) {
		point->type = OUTRANGE;
		continue;
	    }
	    if (!this_plot->noautoscale) {
		if (z_axis->set_autoscale & AUTOSCALE_MIN && point->z < z_axis->min)
		     z_axis->min = point->z;
		if (z_axis->set_autoscale & AUTOSCALE_MAX && point->z > z_axis->max)
		     z_axis->max = point->z;
	    }
	    if (!inrange(point->z, z_axis->min, z_axis->max)) {
		point->type = OUTRANGE;
		continue;
	    }
	}	/* End of this curve */
	}

    }	/* End of this plot */

    /* handle 'reverse' ranges */
    axis_revert_range(FIRST_X_AXIS);
    axis_revert_range(FIRST_Y_AXIS);
    axis_revert_range(FIRST_Z_AXIS);

    /* Make sure the bounds are reasonable, and tweak them if they aren't */
    axis_checked_extend_empty_range(FIRST_X_AXIS, NULL);
    axis_checked_extend_empty_range(FIRST_Y_AXIS, NULL);
    axis_checked_extend_empty_range(FIRST_Z_AXIS, NULL);
}


static double
splines_kernel(double h)
{
    if (h > 0.0) { return h * h * log(h); }
    return 0.0;
}

/* PKJ:
   This function has been hived off out of the original grid_nongrid_data().
   No changes have been made, but variables only needed locally have moved
   out of grid_nongrid_data() into this functin. */
static void 
thin_plate_splines_setup( struct iso_curve *old_iso_crvs,
			  double **p_xx, int *p_numpoints )
{
    int i, j, k;
    double *xx, *yy, *zz, *b, **K, d;
    int numpoints, *indx;
    struct iso_curve *oicrv;
    
    numpoints = 0;
    for (oicrv = old_iso_crvs; oicrv != NULL; oicrv = oicrv->next) {
        numpoints += oicrv->p_count;
    }
    xx = gp_alloc(sizeof(xx[0]) * (numpoints + 3) * (numpoints + 8),
                  "thin plate splines in dgrid3d");
    /* the memory needed is not really (n+3)*(n+8) for now,
       but might be if I take into account errors ... */
    K = gp_alloc(sizeof(K[0]) * (numpoints + 3),
                 "matrix : thin plate splines 2d");
    yy = xx + numpoints;
    zz = yy + numpoints;
    b = zz + numpoints;
    
    /* HBB 20010424: Count actual input points without the UNDEFINED
     * ones, as we copy them */
    numpoints = 0;
    for (oicrv = old_iso_crvs; oicrv != NULL; oicrv = oicrv->next) {
        struct coordinate GPHUGE *opoints = oicrv->points;
        
        for (k = 0; k < oicrv->p_count; k++, opoints++) {
            /* HBB 20010424: avoid crashing for undefined input */
            if (opoints->type == UNDEFINED)
                continue;
            xx[numpoints] = opoints->x;
            yy[numpoints] = opoints->y;
            zz[numpoints] = opoints->z;
            numpoints++;
        }
    }
    
    for (i = 0; i < numpoints + 3; i++) {
        K[i] = b + (numpoints + 3) * (i + 1);
    }
    
    for (i = 0; i < numpoints; i++) {
        for (j = i + 1; j < numpoints; j++) {
            double dx = xx[i] - xx[j], dy = yy[i] - yy[j];
            K[i][j] = K[j][i] = -splines_kernel(sqrt(dx * dx + dy * dy));
        }
        K[i][i] = 0.0;		/* here will come the weights for errors */
        b[i] = zz[i];
    }
    for (i = 0; i < numpoints; i++) {
        K[i][numpoints] = K[numpoints][i] = 1.0;
        K[i][numpoints + 1] = K[numpoints + 1][i] = xx[i];
        K[i][numpoints + 2] = K[numpoints + 2][i] = yy[i];
    }
    b[numpoints] = 0.0;
    b[numpoints + 1] = 0.0;
    b[numpoints + 2] = 0.0;
    K[numpoints][numpoints] = 0.0;
    K[numpoints][numpoints + 1] = 0.0;
    K[numpoints][numpoints + 2] = 0.0;
    K[numpoints + 1][numpoints] = 0.0;
    K[numpoints + 1][numpoints + 1] = 0.0;
    K[numpoints + 1][numpoints + 2] = 0.0;
    K[numpoints + 2][numpoints] = 0.0;
    K[numpoints + 2][numpoints + 1] = 0.0;
    K[numpoints + 2][numpoints + 2] = 0.0;
    indx = gp_alloc(sizeof(indx[0]) * (numpoints + 3), "indexes lu");
    /* actually, K is *not* positive definite, but
       has only non zero real eigenvalues ->
       we can use an lu_decomp safely */
    lu_decomp(K, numpoints + 3, indx, &d);
    lu_backsubst(K, numpoints + 3, indx, b);
    
    free( K );
    free( indx );
    
    *p_xx = xx;
    *p_numpoints = numpoints;
}

static double
qnorm( double dist_x, double dist_y, int q ) 
{
    double dist = 0.0;
    switch (q) {
    case 1:
        dist = dist_x + dist_y;
        break;
    case 2:
        dist = dist_x * dist_x + dist_y * dist_y;
        break;
    case 4:
        dist = dist_x * dist_x + dist_y * dist_y;
        dist *= dist;
        break;
    case 8:
        dist = dist_x * dist_x + dist_y * dist_y;
        dist *= dist;
        dist *= dist;
        break;
    case 16:
        dist = dist_x * dist_x + dist_y * dist_y;
        dist *= dist;
        dist *= dist;
        dist *= dist;
        break;
    default:
        dist = pow(dist_x, (double)q ) + pow(dist_y, (double)q );
        break;
    }
    return dist;
}

/* This is from Numerical Recipes in C, 2nd ed, p70 */
static double
pythag( double dx, double dy )
{
    double x, y;
    x = fabs(dx);
    y = fabs(dy);
    
    if( x > y  ) { return x*sqrt(1.0 + (y*y)/(x*x)); }
    if( y==0.0 ) { return 0.0; }
    return y*sqrt(1.0 + (x*x)/(y*y));
}

static void
grid_nongrid_data(struct surface_points *this_plot)
{
    int i, j, k;
    double x, y, z, w, dx, dy, xmin, xmax, ymin, ymax;
    struct iso_curve *old_iso_crvs = this_plot->iso_crvs;
    struct iso_curve *icrv, *oicrv, *oicrvs;
    
    /* these are only needed for thin_plate_splines */
    double *xx, *yy, *zz, *b;
    int numpoints = 0;

    xx = NULL; /* save to call free() on NULL if xx has never been used */
    
    /* Compute XY bounding box on the original data. */
    /* FIXME HBB 20010424: Does this make any sense? Shouldn't we just
     * use whatever the x and y ranges have been found to be, and
     * that's that? The largest difference this is going to make is if
     * we plot a datafile that doesn't span the whole x/y range
     * used. Do we want a dgrid3d over the actual data rectangle, or
     * over the xrange/yrange area? */
    xmin = xmax = old_iso_crvs->points[0].x;
    ymin = ymax = old_iso_crvs->points[0].y;
    for (icrv = old_iso_crvs; icrv != NULL; icrv = icrv->next) {
	struct coordinate GPHUGE *points = icrv->points;
        
	for (i = 0; i < icrv->p_count; i++, points++) {
	    /* HBB 20010424: avoid crashing for undefined input */
	    if (points->type == UNDEFINED)
		continue;
	    if (xmin > points->x)
		xmin = points->x;
	    if (xmax < points->x)
		xmax = points->x;
	    if (ymin > points->y)
		ymin = points->y;
	    if (ymax < points->y)
		ymax = points->y;
	}
    }

    dx = (xmax - xmin) / (dgrid3d_col_fineness - 1);
    dy = (ymax - ymin) / (dgrid3d_row_fineness - 1);

    /* Create the new grid structure, and compute the low pass filtering from
     * non grid to grid structure.
     */
    this_plot->iso_crvs = NULL;
    this_plot->num_iso_read = dgrid3d_col_fineness;
    this_plot->has_grid_topology = TRUE;
    
    if( dgrid3d_mode == DGRID3D_SPLINES ) {
        thin_plate_splines_setup( old_iso_crvs, &xx, &numpoints );
        yy = xx + numpoints;
        zz = yy + numpoints;
        b  = zz + numpoints;
    }
    
    for (i = 0, x = xmin; i < dgrid3d_col_fineness; i++, x += dx) {
	struct coordinate GPHUGE *points;

	icrv = iso_alloc(dgrid3d_row_fineness + 1);
	icrv->p_count = dgrid3d_row_fineness;
	icrv->next = this_plot->iso_crvs;
	this_plot->iso_crvs = icrv;
	points = icrv->points;

	for(j=0, y=ymin; j<dgrid3d_row_fineness; j++, y+=dy, points++) {
	    z = w = 0.0;

	    /* as soon as ->type is changed to UNDEFINED, break out of
	     * two inner loops! */
	    points->type = INRANGE;

	    if( dgrid3d_mode == DGRID3D_SPLINES ) {
                z = b[numpoints];
                for (k = 0; k < numpoints; k++) {
                    double dx = xx[k] - x, dy = yy[k] - y;
                    z = z - b[k] * splines_kernel(sqrt(dx * dx + dy * dy));
                }
                z = z + b[numpoints + 1] * x + b[numpoints + 2] * y;
	    } else { /* everything, except splines */
                for(oicrv = old_iso_crvs; oicrv != NULL; oicrv = oicrv->next) {
                    struct coordinate GPHUGE *opoints = oicrv->points;
                    for (k = 0; k < oicrv->p_count; k++, opoints++) {
                        
                        if( dgrid3d_mode == DGRID3D_QNORM ) {
                            double dist = qnorm( fabs(opoints->x - x),
                                                 fabs(opoints->y - y),
                                                 dgrid3d_norm_value );

                            if( dist == 0.0 ) {
                                /* HBB 981209: revised flagging as undefined */
                                /* Supporting all those infinities on various
                                 * platforms becomes tiresome, 
                                 * to say the least :-(
                                 * Let's just return the first z where this 
                                 * happens unchanged, and be done with this,
                                 * period. */
                                points->type = UNDEFINED;
                                z = opoints->z;
                                w = 1.0;
                                break;	/* out of inner loop */
                            } else {
                                z += opoints->z / dist;
                                w += 1.0/dist;
                            }

                        } else { /* ALL else: not spline, not qnorm! */
                            double weight = 0.0;                       
                            double dist=pythag((opoints->x-x)/dgrid3d_x_scale, 
                                               (opoints->y-y)/dgrid3d_y_scale);

                            if( dgrid3d_mode == DGRID3D_GAUSS ) {
                                weight = exp( -dist*dist );
                            } else if( dgrid3d_mode == DGRID3D_CAUCHY ) {
                                weight = 1.0/(1.0 + dist*dist );
                            } else if( dgrid3d_mode == DGRID3D_EXP ) {
                                weight = exp( -dist );
                            } else if( dgrid3d_mode == DGRID3D_BOX ) {
                                weight = (dist<1.0) ? 1.0 : 0.0;
                            } else if( dgrid3d_mode == DGRID3D_HANN ) {
                                if( dist < 1.0 ) {
                                    weight = 0.5*(1-cos(2.0*M_PI*dist));
                                }
                            }
                            z += opoints->z * weight;
                            w += weight;
                        }
                    }
                    
                    /* PKJ: I think this is only relevant for qnorm */
                    if (points->type != INRANGE)
                        break;	/* out of the second-inner loop as well ... */
                }
	    } /* endif( dgrid3d_mode == DGRID3D_SPLINES ) */
                 
              /* Now that we've escaped the loops safely, we know that we
               * do have a good value in z and w, so we can proceed just as
               * if nothing had happened at all. Nice, isn't it? */
	    points->type = INRANGE;
            
	    /* HBB 20010424: if log x or log y axis, we don't want to
	     * log() the value again --> just store it, and trust that
	     * it's always inrange */
	    points->x = x;
	    points->y = y;

	    /* Honor requested x and y limits */
	    /* Historical note: This code was not in 4.0 or 4.2. It imperfectly */
	    /* restores the clipping behaviour of version 3.7 and earlier. */
	    if ((x < axis_array[x_axis].min && !(axis_array[x_axis].autoscale & AUTOSCALE_MIN))
	    ||  (x > axis_array[x_axis].max && !(axis_array[x_axis].autoscale & AUTOSCALE_MAX))
	    ||  (y < axis_array[y_axis].min && !(axis_array[y_axis].autoscale & AUTOSCALE_MIN))
	    ||  (y > axis_array[y_axis].max && !(axis_array[y_axis].autoscale & AUTOSCALE_MAX)))
		points->type = OUTRANGE;

	    if (dgrid3d_mode != DGRID3D_SPLINES && !dgrid3d_kdensity)
               z = z / w;
            
	    STORE_WITH_LOG_AND_UPDATE_RANGE(points->z, z, 
					    points->type, z_axis,
					    this_plot->noautoscale,
					    NOOP, continue);
            
	    if (this_plot->pm3d_color_from_column)
		int_error(NO_CARET, 
			  "Gridding of the color column is not implemented");
	    else {
		COLOR_STORE_WITH_LOG_AND_UPDATE_RANGE(points->CRD_COLOR, z, 
						      points->type, 
						      COLOR_AXIS, 
						      this_plot->noautoscale,
						      NOOP, continue);
	    }
	}
    }
    
    free(xx); /* save to call free on NULL pointer if splines not used */
    
    /* Delete the old non grid data. */
    for (oicrvs = old_iso_crvs; oicrvs != NULL;) {
	oicrv = oicrvs;
	oicrvs = oicrvs->next;
	iso_free(oicrv);
    }
}

/* Get 3D data from file, and store into this_plot data
 * structure. Takes care of 'set mapping' and 'set dgrid3d'.
 *
 * Notice: this_plot->token is end of datafile spec, before title etc
 * will be moved past title etc after we return */
static int
get_3ddata(struct surface_points *this_plot)
{
    int xdatum = 0;
    int ydatum = 0;
    int j;
    double v[MAXDATACOLS];
    int pt_in_iso_crv = 0;
    struct iso_curve *this_iso;
    int retval = 0;

    if (mapping3d == MAP3D_CARTESIAN) {
	/* do this check only, if we have PM3D / PM3D-COLUMN not compiled in */
	if (df_no_use_specs == 2)
	    int_error(this_plot->token, "Need 1 or 3 columns for cartesian data");
	/* HBB NEW 20060427: if there's only one, explicit using
	 * column, it's z data.  df_axis[] has to reflect that, so
	 * df_readline() will expect time/date input. */
	if (df_no_use_specs == 1)
	    df_axis[0] = FIRST_Z_AXIS;
    } else {
	if (df_no_use_specs == 1)
	    int_error(this_plot->token, "Need 2 or 3 columns for polar data");
    }

    this_plot->num_iso_read = 0;
    this_plot->has_grid_topology = TRUE;
    this_plot->pm3d_color_from_column = FALSE;

    /* we ought to keep old memory - most likely case
     * is a replot, so it will probably exactly fit into
     * memory already allocated ?
     */
    if (this_plot->iso_crvs != NULL) {
	struct iso_curve *icrv, *icrvs = this_plot->iso_crvs;

	while (icrvs) {
	    icrv = icrvs;
	    icrvs = icrvs->next;
	    iso_free(icrv);
	}
	this_plot->iso_crvs = NULL;
    }
    /* data file is already open */

    if (df_matrix)
	this_plot->has_grid_topology = TRUE;

    {
	/*{{{  read surface from text file */
	struct iso_curve *local_this_iso = iso_alloc(samples_1);
	struct coordinate GPHUGE *cp;
	struct coordinate GPHUGE *cphead = NULL; /* Only for VECTOR plots */
	double x, y, z;
	double xtail, ytail, ztail;
	double color = VERYLARGE;
	int pm3d_color_from_column = FALSE;
#define color_from_column(x) pm3d_color_from_column = x

	if (this_plot->plot_style == LABELPOINTS)
	    expect_string( 4 );

	if (this_plot->plot_style == VECTOR) {
	    local_this_iso->next = iso_alloc(samples_1);
	    local_this_iso->next->p_count = 0;
	}

	/* If the user has set an explicit locale for numeric input, apply it */
	/* here so that it affects data fields read from the input file.      */
	set_numeric_locale();

	/* Initial state */
	df_warn_on_missing_columnheader = TRUE;

	while ((retval = df_readline(v,MAXDATACOLS)) != DF_EOF) {
	    j = retval;

	    if (j == 0) /* not blank line, but df_readline couldn't parse it */
		int_warn(NO_CARET, "Bad data on line %d of file %s",
			  df_line_number, df_filename ? df_filename : ""); 

	    if (j == DF_SECOND_BLANK)
		break;		/* two blank lines */
	    if (j == DF_FIRST_BLANK) {

		/* Images are in a sense similar to isocurves.
		 * However, the routine for images is written to
		 * compute the two dimensions of coordinates by
		 * examining the data alone.  That way it can be used
		 * in the 2D plots, for which there is no isoline
		 * record.  So, toss out isoline information for
		 * images.
		 */
		if ((this_plot->plot_style == IMAGE)
		||  (this_plot->plot_style == RGBIMAGE)
		||  (this_plot->plot_style == RGBA_IMAGE))
		    continue;

		if (this_plot->plot_style == VECTOR)
		    continue;

		/* one blank line */
		if (pt_in_iso_crv == 0) {
		    if (xdatum == 0)
			continue;
		    pt_in_iso_crv = xdatum;
		}
		if (xdatum > 0) {
		    local_this_iso->p_count = xdatum;
		    local_this_iso->next = this_plot->iso_crvs;
		    this_plot->iso_crvs = local_this_iso;
		    this_plot->num_iso_read++;

		    if (xdatum != pt_in_iso_crv)
			this_plot->has_grid_topology = FALSE;

		    local_this_iso = iso_alloc(pt_in_iso_crv);
		    xdatum = 0;
		    ydatum++;
		}
		continue;
	    }

	    else if (j == DF_FOUND_KEY_TITLE){
		/* only the shared part of the 2D and 3D headers is used */
		df_set_key_title((struct curve_points *)this_plot);
		continue;
	    }
	    else if (j == DF_KEY_TITLE_MISSING){
		fprintf(stderr,
			"get_data: key title not found in requested column\n"
		    );
		continue;
	    }

	    else if (j == DF_COLUMN_HEADERS) {
		continue;
	    }

	    /* its a data point or undefined */
	    if (xdatum >= local_this_iso->p_max) {
		/* overflow about to occur. Extend size of points[]
		 * array. Double the size, and add 1000 points, to
		 * avoid needlessly small steps. */
		iso_extend(local_this_iso, xdatum + xdatum + 1000);
		if (this_plot->plot_style == VECTOR) {
		    iso_extend(local_this_iso->next, xdatum + xdatum + 1000);
		    local_this_iso->next->p_count = 0;
		}
	    }
	    cp = local_this_iso->points + xdatum;

	    if (this_plot->plot_style == VECTOR) {
		if (j < 6) {
		    cp->type = UNDEFINED;
		    continue;
		}
		cphead = local_this_iso->next->points + xdatum;
	    }

	    if (j == DF_UNDEFINED || j == DF_MISSING) {
		cp->type = UNDEFINED;
		goto come_here_if_undefined;
	    }
	    cp->type = INRANGE;	/* unless we find out different */

	    /* EAM Oct 2004 - Substantially rework this section */
	    /* now that there are many more plot types.         */

	    x = y = z = 0.0;
	    xtail = ytail = ztail = 0.0;

	    /* The x, y, z coordinates depend on the mapping type */
	    switch (mapping3d) {

	    case MAP3D_CARTESIAN:
		if (j == 1) {
		    x = xdatum;
		    y = ydatum;
		    z = v[0];
		    j = 3;
		    break;
		}

		if (j == 2) {
		    if (PM3DSURFACE != this_plot->plot_style)
			int_error(this_plot->token,
				  "2 columns only possible with explicit pm3d style (line %d)",
				  df_line_number);
		    x = xdatum;
		    y = ydatum;
		    z = v[0];
		    color_from_column(TRUE);
		    color = v[1];
		    j = 3;
		    break;
		}

		/* Assume everybody agrees that x,y,z are the first three specs */
		if (j >= 3) {
		    x = v[0];
		    y = v[1];
		    z = v[2];
		    break;
		}

		break;

	    case MAP3D_SPHERICAL:
		if (j < 2)
		    int_error(this_plot->token, "Need 2 or 3 columns");
		if (j < 3) {
		    v[2] = 1;	/* default radius */
		    j = 3;
		}

		/* Convert to radians. */
		v[0] *= ang2rad;
		v[1] *= ang2rad;

		x = v[2] * cos(v[0]) * cos(v[1]);
		y = v[2] * sin(v[0]) * cos(v[1]);
		z = v[2] * sin(v[1]);

		break;

	    case MAP3D_CYLINDRICAL:
		if (j < 2)
		    int_error(this_plot->token, "Need 2 or 3 columns");
		if (j < 3) {
		    v[2] = 1;	/* default radius */
		    j = 3;
		}

		/* Convert to radians. */
		v[0] *= ang2rad;

		x = v[2] * cos(v[0]);
		y = v[2] * sin(v[0]);
		z = v[1];

		break;

	    default:
		int_error(NO_CARET, "Internal error: Unknown mapping type");
		return retval;
	    }

	    if (j < df_no_use_specs)
		int_error(this_plot->token,
			"Wrong number of columns in input data - line %d",
			df_line_number);

	    /* Work-around for hidden3d, which otherwise would use the */
	    /* color of the vector midpoint rather than the endpoint. */
	    if (this_plot->plot_style == IMPULSES) {
		if (this_plot->lp_properties.pm3d_color.type == TC_Z) {
		    color = z;
		    color_from_column(TRUE);
		}
	    }

	    /* After the first three columns it gets messy because */
	    /* different plot styles assume different contents in the columns */
	    if (j >= 4) {
		if (( this_plot->plot_style == POINTSTYLE
		   || this_plot->plot_style == LINESPOINTS)
		&&  this_plot->lp_properties.p_size == PTSZ_VARIABLE) {
		    cp->CRD_PTSIZE = v[3];
		    color = z;
		    color_from_column(FALSE);
		}

		else if (this_plot->plot_style == LABELPOINTS) {
		/* 4th column holds label text rather than color */
		/* text = df_tokens[3]; */
		    color = z;
		    color_from_column(FALSE);
		}

		else {
		    color = v[3];
		    color_from_column(TRUE);
		}
	    }

	    if (j >= 5) {
		if ((this_plot->plot_style == POINTSTYLE
		   || this_plot->plot_style == LINESPOINTS)
		&&  this_plot->lp_properties.p_size == PTSZ_VARIABLE) {
		    color = v[4];
		    color_from_column(TRUE);
		}

		if (this_plot->plot_style == LABELPOINTS) {
		    /* take color from an explicitly given 5th column */
		    color = v[4];
		    color_from_column(TRUE);
		}

	    }

	    if (j >= 6) {
		if (this_plot->plot_style == VECTOR) {
		    xtail = x + v[3];
		    ytail = y + v[4];
		    ztail = z + v[5];
		    if (j >= 7) {
			color = v[6];
			color_from_column(TRUE);
		    } else {
			color = z;
			color_from_column(FALSE);
		    }
		}
	    }
#undef color_from_column


	    /* Adjust for logscales. Set min/max and point types. Store in cp.
	     * The macro cannot use continue, as it is wrapped in a loop.
	     * I regard this as correct goto use
	     */
	    cp->type = INRANGE;
	    STORE_WITH_LOG_AND_UPDATE_RANGE(cp->x, x, cp->type, x_axis, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
	    STORE_WITH_LOG_AND_UPDATE_RANGE(cp->y, y, cp->type, y_axis, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
	    if (this_plot->plot_style == VECTOR) {
		cphead->type = INRANGE;
		STORE_WITH_LOG_AND_UPDATE_RANGE(cphead->x, xtail, cphead->type, x_axis, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
		STORE_WITH_LOG_AND_UPDATE_RANGE(cphead->y, ytail, cphead->type, y_axis, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
	    }

	    if (dgrid3d) {
		/* HBB 20010424: in dgrid3d mode, delay log() taking
		 * and scaling until after the dgrid process. Only for
		 * z, not for x and y, so we can layout the newly
		 * created created grid more easily. */
		cp->z = z;
		if (this_plot->plot_style == VECTOR)
		    cphead->z = ztail;
	    } else {

		/* EAM Sep 2008 - Otherwise z=Nan or z=Inf or DF_MISSING fails	*/
		/* to set CRD_COLOR at all, since the z test bails to a goto. 	*/
		if (this_plot->plot_style == IMAGE) {
			cp->CRD_COLOR = (pm3d_color_from_column) ? color : z;
	        }

		/* Version 5: cp->z=0 in the UNDEF_ACTION recovers what	version 4 did */
		STORE_WITH_LOG_AND_UPDATE_RANGE(cp->z, z, cp->type, z_axis,
				this_plot->noautoscale, NOOP,
				cp->z=0;goto come_here_if_undefined);

		if (this_plot->plot_style == VECTOR)
		    STORE_WITH_LOG_AND_UPDATE_RANGE(cphead->z, ztail, cphead->type, z_axis, this_plot->noautoscale, NOOP, goto come_here_if_undefined);

		if (this_plot->lp_properties.l_type == LT_COLORFROMCOLUMN)
		    cp->CRD_COLOR = color;

		if (NEED_PALETTE(this_plot)) {
		    if (pm3d_color_from_column) {
			COLOR_STORE_WITH_LOG_AND_UPDATE_RANGE(cp->CRD_COLOR, color, cp->type, COLOR_AXIS, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
			if (this_plot->plot_style == VECTOR)
			    cphead->CRD_COLOR = color;
		    } else {
			COLOR_STORE_WITH_LOG_AND_UPDATE_RANGE(cp->CRD_COLOR, z, cp->type, COLOR_AXIS, this_plot->noautoscale, NOOP, goto come_here_if_undefined);
		    }
		}
	    }

	    /* At this point we have stored the point coordinates. Now we need to copy */
	    /* x,y,z into the text_label structure and add the actual text string.     */
	    if (this_plot->plot_style == LABELPOINTS)
		store_label(this_plot->labels, cp, xdatum, df_tokens[3], color);

	    if (this_plot->plot_style == RGBIMAGE || this_plot->plot_style == RGBA_IMAGE) {
		/* We will autoscale the RGB components to  a total range [0:255]
		 * so we don't need to do any fancy scaling here.
		 */
		cp->CRD_R = v[3];
		cp->CRD_G = v[4];
		cp->CRD_B = v[5];
		cp->CRD_A = v[6];	/* Alpha channel */
	    }

	come_here_if_undefined:
	    /* some may complain, but I regard this as the correct use of goto */
	    ++xdatum;
	}			/* end of whileloop - end of surface */

	/* We are finished reading user input; return to C locale for internal use */
	reset_numeric_locale();

	if (pm3d_color_from_column) {
	    this_plot->pm3d_color_from_column = pm3d_color_from_column;
	}

	if (xdatum > 0) {
	    this_plot->num_iso_read++;	/* Update last iso. */
	    local_this_iso->p_count = xdatum;

	    /* If this is a VECTOR plot then iso->next is already */
	    /* occupied by the vector tail coordinates.           */
	    if (this_plot->plot_style != VECTOR)
		local_this_iso->next = this_plot->iso_crvs;
	    this_plot->iso_crvs = local_this_iso;

	    if (xdatum != pt_in_iso_crv)
		this_plot->has_grid_topology = FALSE;

	} else { /* Free last allocation */
	    if (this_plot->plot_style == VECTOR)
		iso_free(local_this_iso->next);
	    iso_free(local_this_iso);
	}

	/*}}} */
    }

    if (dgrid3d && this_plot->num_iso_read > 0)
	grid_nongrid_data(this_plot);

    /* This check used to be done in graph3d */
    if (X_AXIS.min == VERYLARGE || X_AXIS.max == -VERYLARGE ||
	Y_AXIS.min == VERYLARGE || Y_AXIS.max == -VERYLARGE ||
	Z_AXIS.min == VERYLARGE || Z_AXIS.max == -VERYLARGE) {
	    /* FIXME: Should we set plot type to NODATA? */
	    /* But in the case of 'set view map' we may not care about Z */
	    int_warn(NO_CARET,
		"No usable data in this plot to auto-scale axis range");
	    }

    if (this_plot->num_iso_read <= 1)
	this_plot->has_grid_topology = FALSE;

    if (this_plot->has_grid_topology && !hidden3d 
    &&   (implicit_surface || this_plot->plot_style == SURFACEGRID)) {
	struct iso_curve *new_icrvs = NULL;
	int num_new_iso = this_plot->iso_crvs->p_count;
	int len_new_iso = this_plot->num_iso_read;
	int i;

	/* Now we need to set the other direction (pseudo) isolines. */
	for (i = 0; i < num_new_iso; i++) {
	    struct iso_curve *new_icrv = iso_alloc(len_new_iso);

	    new_icrv->p_count = len_new_iso;

	    for (j = 0, this_iso = this_plot->iso_crvs;
		 this_iso != NULL;
		 j++, this_iso = this_iso->next) {
		/* copy whole point struct to get type too.
		 * wasteful for windows, with padding */
		/* more efficient would be extra pointer to same struct */
		new_icrv->points[j] = this_iso->points[i];
	    }

	    new_icrv->next = new_icrvs;
	    new_icrvs = new_icrv;
	}

	/* Append the new iso curves after the read ones. */
	for (this_iso = this_plot->iso_crvs;
	     this_iso->next != NULL;
	     this_iso = this_iso->next);
	this_iso->next = new_icrvs;
    }

    return retval;
}

/* HBB 20000501: code isolated from eval_3dplots(), where practically
 * identical code occured twice, for direct and crossing isolines,
 * respectively.  The latter only are done for in non-hidden3d
 * mode. */
static void
calculate_set_of_isolines(
    AXIS_INDEX value_axis,
    TBOOLEAN cross,
    struct iso_curve **this_iso,
    AXIS_INDEX iso_axis,
    double iso_min, double iso_step,
    int num_iso_to_use,
    AXIS_INDEX sam_axis,
    double sam_min, double sam_step,
    int num_sam_to_use,
    TBOOLEAN need_palette)
{
    int i, j;
    struct coordinate GPHUGE *points = (*this_iso)->points;
    int do_update_color = need_palette && (!parametric || (parametric && value_axis == FIRST_Z_AXIS));

    for (j = 0; j < num_iso_to_use; j++) {
	double iso = iso_min + j * iso_step;
	/* HBB 20000501: with the new code, it should
	 * be safe to rely on the actual 'v' axis not
	 * to be improperly logscaled... */
	(void) Gcomplex(&plot_func.dummy_values[cross ? 0 : 1],
			AXIS_DE_LOG_VALUE(iso_axis, iso), 0.0);

	for (i = 0; i < num_sam_to_use; i++) {
	    double sam = sam_min + i * sam_step;
	    struct value a;
	    double temp;

	    (void) Gcomplex(&plot_func.dummy_values[cross ? 1 : 0],
			    AXIS_DE_LOG_VALUE(sam_axis, sam), 0.0);

	    if (cross) {
		points[i].x = iso;
		points[i].y = sam;
	    } else {
		points[i].x = sam;
		points[i].y = iso;
	    }

	    evaluate_at(plot_func.at, &a);

	    if (undefined || (fabs(imag(&a)) > zero)) {
		points[i].type = UNDEFINED;
		continue;
	    }

	    temp = real(&a);
	    points[i].type = INRANGE;
	    STORE_WITH_LOG_AND_UPDATE_RANGE(points[i].z, temp, points[i].type,
					    value_axis, FALSE, NOOP, NOOP);
	    if (do_update_color) {
		COLOR_STORE_WITH_LOG_AND_UPDATE_RANGE(points[i].CRD_COLOR, temp, points[i].type, 
					    COLOR_AXIS, FALSE, NOOP, NOOP);
	    }
	}
	(*this_iso)->p_count = num_sam_to_use;
	*this_iso = (*this_iso)->next;
	points = (*this_iso) ? (*this_iso)->points : NULL;
    }
}


/*
 * This parses the splot command after any range specifications. To support
 * autoscaling on the x/z axis, we want any data files to define the x/y
 * range, then to plot any functions using that range. We thus parse the
 * input twice, once to pick up the data files, and again to pick up the
 * functions. Definitions are processed twice, but that won't hurt.
 * div - okay, it doesn't hurt, but every time an option as added for
 * datafiles, code to parse it has to be added here. Change so that
 * we store starting-token in the plot structure.
 */
static void
eval_3dplots()
{
    int i;
    struct surface_points **tp_3d_ptr;
    int start_token=0, end_token;
    int highest_iteration = 0;	/* last index reached in iteration [i=start:*] */
    TBOOLEAN eof_during_iteration = FALSE;  /* set when for [n=start:*] hits NODATA */
    int begin_token;
    TBOOLEAN some_data_files = FALSE, some_functions = FALSE;
    TBOOLEAN was_definition = FALSE;
    int df_return = 0;
    int plot_num, line_num;
    /* part number of parametric function triplet: 0 = z, 1 = y, 2 = x */
    int crnt_param = 0;
    char *xtitle;
    char *ytitle;
    legend_key *key = &keyT;

    /* Free memory from previous splot.
     * If there is an error within this function, the memory is left allocated,
     * since we cannot call sp_free if the list is incomplete
     */
    if (first_3dplot && plot3d_num>0)
      sp_free(first_3dplot);
    plot3d_num=0;
    first_3dplot = NULL;

    x_axis = FIRST_X_AXIS;
    y_axis = FIRST_Y_AXIS;
    z_axis = FIRST_Z_AXIS;

    tp_3d_ptr = &(first_3dplot);
    plot_num = 0;
    line_num = 0;		/* default line type */

    /* Assume that the input data can be re-read later */
    volatile_data = FALSE;

    xtitle = NULL;
    ytitle = NULL;

    begin_token = c_token;

/*** First Pass: Read through data files ***/
    /*
     * This pass serves to set the x/yranges and to parse the command, as
     * well as filling in every thing except the function data. That is done
     * after the x/yrange is defined.
     */
    plot_iterator = check_for_iteration();

    while (TRUE) {

	/* Forgive trailing comma on a multi-element plot command */
	if (END_OF_COMMAND) {
	    if (plot_num == 0)
		int_error(c_token, "function to plot expected");
	    break;
	}

	if (crnt_param == 0 && !was_definition)
	    start_token = c_token;

	if (is_definition(c_token)) {
	    define();
	    if (equals(c_token, ","))
		c_token++;
	    was_definition = TRUE;
	    continue;

	} else {
	    int specs = -1;
	    struct surface_points *this_plot;

	    char *name_str;
	    TBOOLEAN duplication = FALSE;
	    TBOOLEAN set_title = FALSE, set_with = FALSE;
	    TBOOLEAN set_lpstyle = FALSE;
	    TBOOLEAN checked_once = FALSE;
	    TBOOLEAN set_labelstyle = FALSE;
	    int sample_range_token;

	    if (!was_definition && (!parametric || crnt_param == 0))
		start_token = c_token;
	    was_definition = FALSE;

	    /* Check for a sampling range */
	    sample_range_token = parse_range(SAMPLE_AXIS);
	    if (sample_range_token > 0)
		axis_array[SAMPLE_AXIS].range_flags |= RANGE_SAMPLED;

	    /* Should this be saved in this_plot? */
	    dummy_func = &plot_func;
	    name_str = string_or_express(NULL);
	    dummy_func = NULL;
	    if (name_str) {
		/*{{{  data file to plot */
		if (parametric && crnt_param != 0)
		    int_error(c_token, "previous parametric function not fully specified");

		if (!some_data_files) {
		    if (axis_array[FIRST_X_AXIS].autoscale & AUTOSCALE_MIN) {
			axis_array[FIRST_X_AXIS].min = VERYLARGE;
		    }
		    if (axis_array[FIRST_X_AXIS].autoscale & AUTOSCALE_MAX) {
			axis_array[FIRST_X_AXIS].max = -VERYLARGE;
		    }
		    if (axis_array[FIRST_Y_AXIS].autoscale & AUTOSCALE_MIN) {
			axis_array[FIRST_Y_AXIS].min = VERYLARGE;
		    }
		    if (axis_array[FIRST_Y_AXIS].autoscale & AUTOSCALE_MAX) {
			axis_array[FIRST_Y_AXIS].max = -VERYLARGE;
		    }
		    some_data_files = TRUE;
		}
		if (*tp_3d_ptr)
		    this_plot = *tp_3d_ptr;
		else {		/* no memory malloc()'d there yet */
		    /* Allocate enough isosamples and samples */
		    this_plot = sp_alloc(0, 0, 0, 0);
		    *tp_3d_ptr = this_plot;
		}

		this_plot->plot_type = DATA3D;
		this_plot->plot_style = data_style;
		eof_during_iteration = FALSE;

		df_set_plot_mode(MODE_SPLOT);
		specs = df_open(name_str, MAXDATACOLS, (struct curve_points *)this_plot);

		if (df_matrix)
		    this_plot->has_grid_topology = TRUE;

		/* EAM FIXME - this seems to work but I am uneasy that c_dummy_var[]	*/
		/*             is not being loaded with the variable name.		*/
		if (sample_range_token > 0) {
		    this_plot->sample_var = add_udv(sample_range_token);
		    this_plot->sample_var->udv_undef = FALSE;
		} else {
		    /* FIXME: This has the side effect of creating a named variable x */
		    /* or overwriting an existing variable x.  Maybe it should save   */
		    /* and restore the pre-existing variable in this case?            */
		    this_plot->sample_var = add_udv_by_name(c_dummy_var[0]);
		    if (this_plot->sample_var->udv_undef) {
			this_plot->sample_var->udv_undef = FALSE;
			Gcomplex(&(this_plot->sample_var->udv_value), 0.0, 0.0);
		    }
		}

		/* for capture to key */
		this_plot->token = end_token = c_token - 1;
		/* FIXME: Is this really needed? */
		this_plot->iteration = plot_iterator ? plot_iterator->iteration : 0;

		/* this_plot->token is temporary, for errors in get_3ddata() */

		if (specs < 3) {
		    if (axis_array[FIRST_X_AXIS].datatype == DT_TIMEDATE) {
			int_error(c_token, "Need full using spec for x time data");
		    }
		    if (axis_array[FIRST_Y_AXIS].datatype == DT_TIMEDATE) {
			int_error(c_token, "Need full using spec for y time data");
		    }
		}
		df_axis[0] = FIRST_X_AXIS;
		df_axis[1] = FIRST_Y_AXIS;
		df_axis[2] = FIRST_Z_AXIS;

		/*}}} */

	    } else {		/* function to plot */

		/*{{{  function */
		++plot_num;
		if (parametric) {
		    /* Rotate between x/y/z axes */
		    /* +2 same as -1, but beats -ve problem */
		    crnt_param = (crnt_param + 2) % 3;
		}
		if (*tp_3d_ptr) {
		    this_plot = *tp_3d_ptr;
		    if (!hidden3d)
			sp_replace(this_plot, samples_1, iso_samples_1,
				   samples_2, iso_samples_2);
		    else
			sp_replace(this_plot, iso_samples_1, 0,
				   0, iso_samples_2);

		} else {	/* no memory malloc()'d there yet */
		    /* Allocate enough isosamples and samples */
		    if (!hidden3d)
			this_plot = sp_alloc(samples_1, iso_samples_1,
					     samples_2, iso_samples_2);
		    else
			this_plot = sp_alloc(iso_samples_1, 0,
					     0, iso_samples_2);
		    *tp_3d_ptr = this_plot;
		}

		this_plot->plot_type = FUNC3D;
		this_plot->has_grid_topology = TRUE;
		this_plot->plot_style = func_style;
		this_plot->num_iso_read = iso_samples_2;
		/* ignore it for now */
		some_functions = TRUE;
		end_token = c_token - 1;
		/*}}} */

	    }			/* end of IS THIS A FILE OR A FUNC block */

	    /* clear current title, if exist */
	    if (this_plot->title) {
		free(this_plot->title);
		this_plot->title = NULL;
	    }

	    /* default line and point types */
	    this_plot->lp_properties.l_type = line_num;
	    this_plot->lp_properties.p_type = line_num;
		this_plot->lp_properties.d_type = line_num;

	    /* user may prefer explicit line styles */
	    this_plot->hidden3d_top_linetype = line_num;
	    if (prefer_line_styles)
		lp_use_properties(&this_plot->lp_properties, line_num+1);
	    else
		load_linetype(&this_plot->lp_properties, line_num+1);

	    /* pm 25.11.2001 allow any order of options */
	    while (!END_OF_COMMAND || !checked_once) {

		/* deal with title */
		if (almost_equals(c_token, "t$itle") || almost_equals(c_token, "not$itle")) {
		    if (set_title) {
			duplication=TRUE;
			break;
		    }
		    set_title = TRUE;
		    if (almost_equals(c_token++, "not$itle")) {
			this_plot->title_is_suppressed = TRUE;
			if (equals(c_token,","))
			    continue;
		    }

		    if (parametric || this_plot->title_is_suppressed) {
			if (xtitle != NULL)
			    xtitle[0] = NUL;	/* Remove default title . */
			if (ytitle != NULL)
			    ytitle[0] = NUL;	/* Remove default title . */
		    }

		    /* title can be enhanced if not explicitly disabled */
		    this_plot->title_no_enhanced = !key->enhanced;

		    if (almost_equals(c_token,"col$umnheader")) {
			df_set_key_title_columnhead((struct curve_points *)this_plot);
		    } else {
			char *temp;
			temp = try_to_get_string();
			if (!this_plot->title_is_suppressed && !(this_plot->title = temp))
			    int_error(c_token, "expecting \"title\" for plot");
		    }
		    continue;
		}

		if (almost_equals(c_token, "enh$anced")) {
		    c_token++;
		    this_plot->title_no_enhanced = FALSE;
		    continue;
		} else if (almost_equals(c_token, "noenh$anced")) {
		    c_token++;
		    this_plot->title_no_enhanced = TRUE;
		    continue;
		}

		/* deal with style */
		if (almost_equals(c_token, "w$ith")) {
		    if (set_with) {
			duplication=TRUE;
			break;
		    }
		    this_plot->plot_style = get_style();
		    if ((this_plot->plot_type == FUNC3D) &&
			((this_plot->plot_style & PLOT_STYLE_HAS_ERRORBAR)
			|| (this_plot->plot_style == LABELPOINTS && !draw_contour)
			)) {
			int_warn(c_token-1, "This style cannot be used to plot a surface defined by a function");
			this_plot->plot_style = POINTSTYLE;
			this_plot->plot_type = NODATA;
		    }

		    if (this_plot->plot_style == IMAGE
		    ||  this_plot->plot_style == RGBA_IMAGE
		    ||  this_plot->plot_style == RGBIMAGE) {
			if (this_plot->plot_type == FUNC3D)
			    int_error(c_token-1, "a function cannot be plotted as an image");
			else
			    get_image_options(&this_plot->image_properties);
		    }

		    if ((this_plot->plot_style | data_style) & PM3DSURFACE) {
			if (equals(c_token, "at")) {
			/* option 'with pm3d [at ...]' is explicitly specified */
			c_token++;
			if (get_pm3d_at_option(&this_plot->pm3d_where[0]))
			    return; /* error */
			}
		    }

		    if (this_plot->plot_style == TABLESTYLE)
			int_error(NO_CARET, "use `plot with table` rather than `splot with table`"); 

		    set_with = TRUE;
		    continue;
		}

		/* Hidden3D code by default includes points, labels and vectors	*/
		/* in the hidden3d processing. Check here if this particular	*/
		/* plot wants to be excluded.					*/
		if (almost_equals(c_token, "nohidden$3d")) {
		    c_token++;
		    this_plot->opt_out_of_hidden3d = TRUE;
		    continue;
		}

		/* "set contour" is global.  Allow individual plots to opt out */
		if (almost_equals(c_token, "nocon$tours")) {
		    c_token++;
		    this_plot->opt_out_of_contours = TRUE;
		    continue;
		}

		/* "set surface" is global.  Allow individual plots to opt out */
		if (almost_equals(c_token, "nosur$face")) {
		    c_token++;
		    this_plot->opt_out_of_surface = TRUE;
		    continue;
		}

		/* Most plot styles accept line and point properties but do not */
		/* want font or text properties.				*/
		if (this_plot->plot_style == VECTOR) {
		    int stored_token = c_token;

		    if (!checked_once) {
			default_arrow_style(&this_plot->arrow_properties);
			load_linetype(&(this_plot->arrow_properties.lp_properties),
					line_num+1);
			checked_once = TRUE;
		    }
		    arrow_parse(&this_plot->arrow_properties, TRUE);
		    if (stored_token != c_token) {
			 if (set_lpstyle) {
			    duplication = TRUE;
			    break;
			 } else {
			    set_lpstyle = TRUE;
			    this_plot->lp_properties = this_plot->arrow_properties.lp_properties;
			    continue;
			}
		    }

		}

		if (this_plot->plot_style == PM3DSURFACE) {
		    /* both previous and subsequent line properties override pm3d default border */
		    int stored_token = c_token;
		    if (!set_lpstyle)
			this_plot->lp_properties = pm3d.border;
 		    lp_parse(&this_plot->lp_properties, LP_ADHOC, FALSE);
		    if (stored_token != c_token) {
			set_lpstyle = TRUE;
			continue;
		    }

		}

		if (this_plot->plot_style != LABELPOINTS) {
		    int stored_token = c_token;
		    struct lp_style_type lp = DEFAULT_LP_STYLE_TYPE;
		    int new_lt = 0;

		    lp.l_type = line_num;
		    lp.p_type = line_num;
		    lp.d_type = line_num;

		    /* user may prefer explicit line styles */
		    if (prefer_line_styles)
			lp_use_properties(&lp, line_num+1);
		    else
			load_linetype(&lp, line_num+1);

 		    new_lt = lp_parse(&lp, LP_ADHOC,
			     this_plot->plot_style & PLOT_STYLE_HAS_POINT);

		    checked_once = TRUE;
		    if (stored_token != c_token) {
			if (set_lpstyle) {
			    duplication=TRUE;
			    break;
			} else {
			    this_plot->lp_properties = lp;
			    set_lpstyle = TRUE;
			    if (new_lt)
				this_plot->hidden3d_top_linetype = new_lt - 1;
			    if (this_plot->lp_properties.p_type != PT_CHARACTER)
				continue;
			}
		    }
		}

		/* Labels can have font and text property info as plot options */
		/* In any case we must allocate one instance of the text style */
		/* that all labels in the plot will share.                     */
		if ((this_plot->plot_style == LABELPOINTS)
		||  (this_plot->plot_style & PLOT_STYLE_HAS_POINT
			&& this_plot->lp_properties.p_type == PT_CHARACTER)) {
		    int stored_token = c_token;
		    if (this_plot->labels == NULL) {
			this_plot->labels = new_text_label(-1);
			this_plot->labels->pos = CENTRE;
			this_plot->labels->layer = LAYER_PLOTLABELS;
		    }
		    parse_label_options(this_plot->labels, TRUE);
		    if (draw_contour)
			load_contour_label_options(this_plot->labels);
		    checked_once = TRUE;
		    if (stored_token != c_token) {
			if (set_labelstyle) {
			    duplication = TRUE;
			    break;
			} else {
			    set_labelstyle = TRUE;
			    continue;
			}
		    }
		}

		break; /* unknown option */

	    }  /* while (!END_OF_COMMAND)*/

	    if (duplication)
		int_error(c_token, "duplicated or contradicting arguments in plot options");

	    /* set default values for title if this has not been specified */
	    this_plot->title_is_filename = FALSE;
	    if (!set_title) {
		this_plot->title_no_enhanced = TRUE; /* filename or function cannot be enhanced */
		if (key->auto_titles == FILENAME_KEYTITLES) {
		    m_capture(&(this_plot->title), start_token, end_token);
		    if (crnt_param == 2)
			xtitle = this_plot->title;
		    else if (crnt_param == 1)
			ytitle = this_plot->title;
		    this_plot->title_is_filename = TRUE;
		} else {
		    if (xtitle != NULL)
			xtitle[0] = '\0';
		    if (ytitle != NULL)
			ytitle[0] = '\0';
		    /*   this_plot->title = NULL;   */
		}
	    }

	    /* No line/point style given. As lp_parse also supplies
	     * the defaults for linewidth and pointsize, call it now
	     * to define them. */
	    if (! set_lpstyle) {
		if (this_plot->plot_style == VECTOR) {
		    this_plot->arrow_properties.lp_properties.l_type = line_num;
		    arrow_parse(&this_plot->arrow_properties, TRUE);
		    this_plot->lp_properties = this_plot->arrow_properties.lp_properties;

		} else if (this_plot->plot_style == PM3DSURFACE) {
		    /* Use default pm3d border unless we see explicit line properties */
		    this_plot->lp_properties = pm3d.border;
 		    lp_parse(&this_plot->lp_properties, LP_ADHOC, FALSE);

		} else {
		    int new_lt = 0;
		    this_plot->lp_properties.l_type = line_num;
		    this_plot->lp_properties.l_width = 1.0;
		    this_plot->lp_properties.p_type = line_num;
			this_plot->lp_properties.d_type = line_num;
		    this_plot->lp_properties.p_size = pointsize;

		    /* user may prefer explicit line styles */
		    if (prefer_line_styles)
			lp_use_properties(&this_plot->lp_properties, line_num+1);
		    else
			load_linetype(&this_plot->lp_properties, line_num+1);

		    new_lt = lp_parse(&this_plot->lp_properties, LP_ADHOC,
			 this_plot->plot_style & PLOT_STYLE_HAS_POINT);
		    if (new_lt)
			this_plot->hidden3d_top_linetype = new_lt - 1;
		    else
			this_plot->hidden3d_top_linetype = line_num;
		}
	    }

	    /* Some low-level routines expect to find the pointflag attribute */
	    /* in lp_properties (they don't have access to the full header).  */
	    if (this_plot->plot_style & PLOT_STYLE_HAS_POINT)
		this_plot->lp_properties.flags |= LP_SHOW_POINTS;

	    /* Rule out incompatible line/point/style options */
	    if (this_plot->plot_type == FUNC3D) {
		if ((this_plot->plot_style & PLOT_STYLE_HAS_POINT)
		&&  (this_plot->lp_properties.p_size == PTSZ_VARIABLE))
		    this_plot->lp_properties.p_size = 1;
	    }
	    if (this_plot->plot_style == LINES) {
		this_plot->opt_out_of_hidden3d = FALSE;
	    }

	    if (crnt_param == 0
		&& this_plot->plot_style != PM3DSURFACE
		/* don't increment the default line/point properties if
		 * this_plot is an EXPLICIT pm3d surface plot */
		&& this_plot->plot_style != IMAGE
		&& this_plot->plot_style != RGBIMAGE
		&& this_plot->plot_style != RGBA_IMAGE
		/* same as above, for an (rgb)image plot */
		) {
		line_num += 1 + (draw_contour != 0) + (hidden3d != 0);
	    }

	    if (this_plot->plot_style == RGBIMAGE || this_plot->plot_style == RGBA_IMAGE) {
		if (CB_AXIS.autoscale & AUTOSCALE_MIN)
		    CB_AXIS.min = 0;
		if (CB_AXIS.autoscale & AUTOSCALE_MAX)
		    CB_AXIS.max = 255;
	    }

	    /* now get the data... having to think hard here...
	     * first time through, we fill in this_plot. For second
	     * surface in file, we have to allocate another surface
	     * struct. BUT we may allocate this store only to
	     * find that it is merely some blank lines at end of file
	     * tp_3d_ptr is still pointing at next field of prev. plot,
	     * before :    prev_or_first -> this_plot -> possible_preallocated_store
	     *                tp_3d_ptr--^
	     * after  :    prev_or_first -> first -> second -> last -> possibly_more_store
	     *                                        tp_3d_ptr ----^
	     * if file is empty, tp_3d_ptr is not moved. this_plot continues
	     * to point at allocated storage, but that will be reused later
	     */

	    assert(this_plot == *tp_3d_ptr);

	    if (this_plot->plot_type == DATA3D) {
		/*{{{  read data */
		/* pointer to the plot of the first dataset (surface) in the file */
		struct surface_points *first_dataset = this_plot;
		int this_token = this_plot->token;

		do {
		    this_plot = *tp_3d_ptr;
		    assert(this_plot != NULL);

		    /* dont move tp_3d_ptr until we are sure we
		     * have read a surface
		     */

		    /* used by get_3ddata() */
		    this_plot->token = this_token;

		    df_return = get_3ddata(this_plot);
		    /* for second pass */
		    this_plot->token = c_token;
		    this_plot->iteration = plot_iterator ? plot_iterator->iteration : 0;

		    if (this_plot->num_iso_read == 0)
			this_plot->plot_type = NODATA;

		    if (this_plot != first_dataset)
			/* copy (explicit) "with pm3d at ..." option from the first dataset in the file */
			strcpy(this_plot->pm3d_where, first_dataset->pm3d_where);

		    /* okay, we have read a surface */
		    ++plot_num;
		    tp_3d_ptr = &(this_plot->next_sp);
		    if (df_return == DF_EOF)
			break;

		    /* there might be another surface so allocate
		     * and prepare another surface structure
		     * This does no harm if in fact there are
		     * no more surfaces to read
		     */

		    if ((this_plot = *tp_3d_ptr) != NULL) {
			if (this_plot->title) {
			    free(this_plot->title);
			    this_plot->title = NULL;
			}
		    } else {
			/* Allocate enough isosamples and samples */
			this_plot = *tp_3d_ptr = sp_alloc(0, 0, 0, 0);
		    }

		    this_plot->plot_type = DATA3D;
		    this_plot->iteration = plot_iterator ? plot_iterator->iteration : 0;
		    this_plot->plot_style = first_dataset->plot_style;
		    this_plot->lp_properties = first_dataset->lp_properties;
		    if ((this_plot->plot_style == LABELPOINTS)
		    ||  (this_plot->plot_style & PLOT_STYLE_HAS_POINT
			    && this_plot->lp_properties.p_type == PT_CHARACTER)) {
			this_plot->labels = new_text_label(-1);
			*(this_plot->labels) = *(first_dataset->labels);
			this_plot->labels->next = NULL;
		    }
		} while (df_return != DF_EOF);

		df_close();

		/* Plot-type specific range-fiddling */
		if (this_plot->plot_style == IMPULSES && !axis_array[FIRST_Z_AXIS].log) {
		    if (axis_array[FIRST_Z_AXIS].autoscale & AUTOSCALE_MIN) {
			if (axis_array[FIRST_Z_AXIS].min > 0)
			    axis_array[FIRST_Z_AXIS].min = 0;
		    }
		    if (axis_array[FIRST_Z_AXIS].autoscale & AUTOSCALE_MAX) {
			if (axis_array[FIRST_Z_AXIS].max < 0)
			    axis_array[FIRST_Z_AXIS].max = 0;
		    }
		}

		/*}}} */

	    } else {		/* not a data file */
		tp_3d_ptr = &(this_plot->next_sp);
		this_plot->token = c_token;	/* store for second pass */
		this_plot->iteration = plot_iterator ? plot_iterator->iteration : 0;
	    }

	    if (empty_iteration(plot_iterator))
		this_plot->plot_type = NODATA;
	    if (forever_iteration(plot_iterator) && (this_plot->plot_type == NODATA)) {
		highest_iteration = plot_iterator->iteration;
		eof_during_iteration = TRUE;
	    }
	    if (forever_iteration(plot_iterator) && (this_plot->plot_type == FUNC3D)) {
		int_error(NO_CARET, "unbounded iteration in function plot");
	    }

	}			/* !is_definition() : end of scope of this_plot */

	if (crnt_param != 0) {
	    if (equals(c_token, ",")) {
		c_token++;
		continue;
	    } else
		break;
	}

	/* Iterate-over-plot mechanisms */
	if (eof_during_iteration) {
	    /* Nothing to do */ ;
	} else if (next_iteration(plot_iterator)) {
	    c_token = start_token;
	    highest_iteration = plot_iterator->iteration;
	    continue;
	}

	plot_iterator = cleanup_iteration(plot_iterator);
	if (equals(c_token, ",")) {
	    c_token++;
	    plot_iterator = check_for_iteration();
	} else
	    break;

    }				/* while(TRUE), ie first pass */


    if (parametric && crnt_param != 0)
	int_error(NO_CARET, "parametric function not fully specified");


/*** Second Pass: Evaluate the functions ***/
    /*
     * Everything is defined now, except the function data. We expect no
     * syntax errors, etc, since the above parsed it all. This makes the code
     * below simpler. If axis_array[FIRST_Y_AXIS].autoscale, the yrange may still change.
     * - eh ?  - z can still change.  x/y/z can change if we are parametric ??
     */

    if (some_functions) {
	/* I've changed the controlled variable in fn plots to u_min etc since
	 * it's easier for me to think parametric - 'normal' plot is after all
	 * a special case. I was confused about x_min being both minimum of
	 * x values found, and starting value for fn plots.
	 */
	double u_min, u_max, u_step, v_min, v_max, v_step;
	double u_isostep, v_isostep;
	AXIS_INDEX u_axis, v_axis;
	struct surface_points *this_plot;

	/* Make these point out the right 'u' and 'v' axis. In
	 * non-parametric mode, x is used as u, and y as v */
	u_axis = parametric ? U_AXIS : FIRST_X_AXIS;
	v_axis = parametric ? V_AXIS : FIRST_Y_AXIS;

	if (!parametric) {
	    /*{{{  check ranges */
	    /* give error if xrange badly set from missing datafile error
	     * parametric fn can still set ranges
	     * if there are no fns, we'll report it later as 'nothing to plot'
	     */

	    /* check that xmin -> xmax is not too small */
	    axis_checked_extend_empty_range(FIRST_X_AXIS, "x range is invalid");
	    axis_checked_extend_empty_range(FIRST_Y_AXIS, "y range is invalid");
	    /*}}} */
	}
	if (parametric && !some_data_files) {
	    /*{{{  set ranges */
	    /* parametric fn can still change x/y range */
	    if (axis_array[FIRST_X_AXIS].autoscale & AUTOSCALE_MIN)
		axis_array[FIRST_X_AXIS].min = VERYLARGE;
	    if (axis_array[FIRST_X_AXIS].autoscale & AUTOSCALE_MAX)
		axis_array[FIRST_X_AXIS].max = -VERYLARGE;
	    if (axis_array[FIRST_Y_AXIS].autoscale & AUTOSCALE_MIN)
		axis_array[FIRST_Y_AXIS].min = VERYLARGE;
	    if (axis_array[FIRST_Y_AXIS].autoscale & AUTOSCALE_MAX)
		axis_array[FIRST_Y_AXIS].max = -VERYLARGE;
	    /*}}} */
	}

	/*{{{  figure ranges, taking logs etc into account */
	u_min = axis_log_value_checked(u_axis, axis_array[u_axis].min, "x range");
	u_max = axis_log_value_checked(u_axis, axis_array[u_axis].max, "x range");
	v_min = axis_log_value_checked(v_axis, axis_array[v_axis].min, "y range");
	v_max = axis_log_value_checked(v_axis, axis_array[v_axis].max, "y range");
	/*}}} */


	if (samples_1 < 2 || samples_2 < 2 || iso_samples_1 < 2 ||
	    iso_samples_2 < 2) {
	    int_error(NO_CARET, "samples or iso_samples < 2. Must be at least 2.");
	}

	/* start over */
	this_plot = first_3dplot;
	c_token = begin_token;
	plot_iterator = check_for_iteration();

	if (hidden3d) {
	    u_step = (u_max - u_min) / (iso_samples_1 - 1);
	    v_step = (v_max - v_min) / (iso_samples_2 - 1);
	} else {
	    u_step = (u_max - u_min) / (samples_1 - 1);
	    v_step = (v_max - v_min) / (samples_2 - 1);
	}

	u_isostep = (u_max - u_min) / (iso_samples_1 - 1);
	v_isostep = (v_max - v_min) / (iso_samples_2 - 1);


	/* Read through functions */
	while (TRUE) {
	    if (crnt_param == 0 && !was_definition)
		start_token = c_token;

	    if (is_definition(c_token)) {
		define();
		if (equals(c_token,","))
		    c_token++;
		was_definition = TRUE;
		continue;

	    } else {
		struct at_type *at_ptr;
		char *name_str;
		was_definition = FALSE;

		/* Forgive trailing comma on a multi-element plot command */
		if (END_OF_COMMAND || this_plot == NULL) {
		    int_warn(c_token, "ignoring trailing comma in plot command");
		    break;
		}

		/* Check for a sampling range */
		/* Currently we are supporting only sampling of pseudofile '+' and   */
		/* this loop is for functions only, so the sampling range is ignored */
		parse_range(SAMPLE_AXIS);

		dummy_func = &plot_func;
		name_str = string_or_express(&at_ptr);

		if (!name_str) {                /* func to plot */
		    /*{{{  evaluate function */
		    struct iso_curve *this_iso = this_plot->iso_crvs;
		    int num_sam_to_use, num_iso_to_use;

		    /* crnt_param is used as the axis number.  As the
		     * axis array indices are ordered z, y, x, we have
		     * to count *backwards*, starting starting at 2,
		     * to properly store away contents to x, y and
		     * z. The following little gimmick does that. */
		    if (parametric)
			crnt_param = (crnt_param + 2) % 3;

		    plot_func.at = at_ptr;

		    num_iso_to_use = iso_samples_2;
		    num_sam_to_use = hidden3d ? iso_samples_1 : samples_1;

		    calculate_set_of_isolines(crnt_param, FALSE, &this_iso,
					      v_axis, v_min, v_isostep,
					      num_iso_to_use,
					      u_axis, u_min, u_step,
					      num_sam_to_use,
					      NEED_PALETTE(this_plot));

		    if (!hidden3d) {
			num_iso_to_use = iso_samples_1;
			num_sam_to_use = samples_2;

			calculate_set_of_isolines(crnt_param, TRUE, &this_iso,
						  u_axis, u_min, u_isostep,
						  num_iso_to_use,
						  v_axis, v_min, v_step,
						  num_sam_to_use,
						  NEED_PALETTE(this_plot));
		    }
		    /*}}} */
		}		/* end of ITS A FUNCTION TO PLOT */
		/* we saved it from first pass */
		c_token = this_plot->token;

		/* we may have seen this one data file in multiple iterations */
		i = this_plot->iteration;
		do {
		    this_plot = this_plot->next_sp;
		} while (this_plot
			&& this_plot->token == c_token
			&& this_plot->iteration == i
			);

	    }			/* !is_definition */

	    /* Iterate-over-plot mechanism */
	    if (crnt_param == 0 && next_iteration(plot_iterator)) {
		if (plot_iterator->iteration <= highest_iteration) {
		    c_token = start_token;
		    continue;
		}
	    }

	    if (crnt_param == 0)
		plot_iterator = cleanup_iteration(plot_iterator);

	    if (equals(c_token, ",")) {
		c_token++;
		if (crnt_param == 0)
		    plot_iterator = check_for_iteration();
	    } else
		break;

	}			/* while(TRUE) */


	if (parametric) {
	    /* Now actually fix the plot triplets to be single plots. */
	    parametric_3dfixup(first_3dplot, &plot_num);
	}
    }				/* some functions */

    /* if first_3dplot is NULL, we have no functions or data at all.
       * This can happen, if you type "splot x=5", since x=5 is a
       * variable assignment
     */
    if (plot_num == 0 || first_3dplot == NULL) {
	int_error(c_token, "no functions or data to plot");
    }

    axis_checked_extend_empty_range(FIRST_X_AXIS, "All points x value undefined");
    axis_revert_and_unlog_range(FIRST_X_AXIS);
    axis_checked_extend_empty_range(FIRST_Y_AXIS, "All points y value undefined");
    axis_revert_and_unlog_range(FIRST_Y_AXIS);
    if (splot_map)
	axis_checked_extend_empty_range(FIRST_Z_AXIS, NULL); /* Suppress warning message */
    else
	axis_checked_extend_empty_range(FIRST_Z_AXIS, "All points z value undefined");

    axis_revert_and_unlog_range(FIRST_Z_AXIS);

    setup_tics(FIRST_X_AXIS, 20);
    setup_tics(FIRST_Y_AXIS, 20);
    setup_tics(FIRST_Z_AXIS, 20);
    if (splot_map) {
	setup_tics(SECOND_X_AXIS, 20);
	setup_tics(SECOND_Y_AXIS, 20);
    }

    set_plot_with_palette(plot_num, MODE_SPLOT);
    if (is_plot_with_palette()) {
	set_cbminmax();
	axis_checked_extend_empty_range(COLOR_AXIS, "All points of colorbox value undefined");
	setup_tics(COLOR_AXIS, 20);
	/* axis_revert_and_unlog_range(COLOR_AXIS); */
	/* fprintf(stderr,"plot3d.c: CB_AXIS.min=%g\tCB_AXIS.max=%g\n",CB_AXIS.min,CB_AXIS.max); */
    }

    AXIS_WRITEBACK(FIRST_X_AXIS);

    if (plot_num == 0 || first_3dplot == NULL) {
	int_error(c_token, "no functions or data to plot");
    }
    /* Creates contours if contours are to be plotted as well. */

    if (draw_contour) {
	struct surface_points *this_plot;
	for (this_plot = first_3dplot, i = 0;
	     i < plot_num;
	     this_plot = this_plot->next_sp, i++) {

	    if (this_plot->contours) {
		struct gnuplot_contours *cntrs = this_plot->contours;

		while (cntrs) {
		    struct gnuplot_contours *cntr = cntrs;
		    cntrs = cntrs->next;
		    free(cntr->coords);
		    free(cntr);
		}
		this_plot->contours = NULL;
	    }

	    /* Make sure this one can be contoured. */
	    if (this_plot->plot_style == VECTOR
	    ||  this_plot->plot_style == IMAGE
	    ||  this_plot->plot_style == RGBIMAGE
	    ||  this_plot->plot_style == RGBA_IMAGE)
		continue;

	    /* Allow individual surfaces to opt out of contouring */
	    if (this_plot->opt_out_of_contours)
		continue;

	    if (!this_plot->has_grid_topology) {
		int_warn(NO_CARET,"Cannot contour non grid data. Please use \"set dgrid3d\".");
	    } else if (this_plot->plot_type == DATA3D) {
		this_plot->contours = contour(this_plot->num_iso_read,
					      this_plot->iso_crvs);
	    } else {
		this_plot->contours = contour(iso_samples_2,
					      this_plot->iso_crvs);
	    }
	}
    }				/* draw_contour */

    /* the following ~9 lines were moved from the end of the
     * function to here, as do_3dplot calles term->text, which
     * itself might process input events in mouse enhanced
     * terminals. For redrawing to work, line capturing and
     * setting the plot3d_num must already be done before
     * entering do_3dplot(). Thu Jan 27 23:54:49 2000 (joze) */

    /* if we get here, all went well, so record the line for replot */
    if (plot_token != -1) {
	/* note that m_capture also frees the old replot_line */
	m_capture(&replot_line, plot_token, c_token - 1);
	plot_token = -1;
	fill_gpval_string("GPVAL_LAST_PLOT", replot_line);
    }
/* record that all went well */
    plot3d_num=plot_num;

    /* perform the plot */
    if (table_mode)
	print_3dtable(plot_num);
    else {
	do_3dplot(first_3dplot, plot_num, 0);

	/* after do_3dplot(), axis_array[] and max_array[].min
	 * contain the plotting range actually used (rounded
	 * to tic marks, not only the min/max data values)
	 * --> save them now for writeback if requested
	 */
	save_writeback_all_axes();
	/* update GPVAL_ variables available to user */
	update_gpval_variables(1);

	/* Mark these plots as safe for quick refresh */
	SET_REFRESH_OK(E_REFRESH_OK_3D, plot_num);
    }
}



/*
 * The hardest part of this routine is collapsing the FUNC plot types in the
 * list (which are gauranteed to occur in (x,y,z) triplets while preserving
 * the non-FUNC type plots intact.  This means we have to work our way
 * through various lists.  Examples (hand checked):
 * start_plot:F1->F2->F3->NULL ==> F3->NULL
 * start_plot:F1->F2->F3->F4->F5->F6->NULL ==> F3->F6->NULL
 * start_plot:F1->F2->F3->D1->D2->F4->F5->F6->D3->NULL ==>
 * F3->D1->D2->F6->D3->NULL
 *
 * x and y ranges now fixed in eval_3dplots
 */
static void
parametric_3dfixup(struct surface_points *start_plot, int *plot_num)
{
    struct surface_points *xp, *new_list, *free_list = NULL;
    struct surface_points **last_pointer = &new_list;
    int i, surface;

    /*
     * Ok, go through all the plots and move FUNC3D types together.  Note:
     * this originally was written to look for a NULL next pointer, but
     * gnuplot wants to be sticky in grabbing memory and the right number of
     * items in the plot list is controlled by the plot_num variable.
     *
     * Since gnuplot wants to do this sticky business, a free_list of
     * surface_points is kept and then tagged onto the end of the plot list
     * as this seems more in the spirit of the original memory behavior than
     * simply freeing the memory.  I'm personally not convinced this sort of
     * concern is worth it since the time spent computing points seems to
     * dominate any garbage collecting that might be saved here...
     */
    new_list = xp = start_plot;
    for (surface = 0; surface < *plot_num; surface++) {
	if (xp->plot_type == FUNC3D) {
	    struct surface_points *yp = xp->next_sp;
	    struct surface_points *zp = yp->next_sp;

	    /* Here's a FUNC3D parametric function defined as three parts.
	     * Go through all the points and assign the x's and y's from xp and
	     * yp to zp. min/max already done
	     */
	    struct iso_curve *xicrvs = xp->iso_crvs;
	    struct iso_curve *yicrvs = yp->iso_crvs;
	    struct iso_curve *zicrvs = zp->iso_crvs;

	    (*plot_num) -= 2;

	    assert(INRANGE < OUTRANGE && OUTRANGE < UNDEFINED);

	    while (zicrvs) {
		struct coordinate GPHUGE *xpoints = xicrvs->points;
		struct coordinate GPHUGE *ypoints = yicrvs->points;
		struct coordinate GPHUGE *zpoints = zicrvs->points;

		for (i = 0; i < zicrvs->p_count; ++i) {
		    zpoints[i].x = xpoints[i].z;
		    zpoints[i].y = ypoints[i].z;
		    if (zpoints[i].type < xpoints[i].type)
			zpoints[i].type = xpoints[i].type;
		    if (zpoints[i].type < ypoints[i].type)
			zpoints[i].type = ypoints[i].type;

		}
		xicrvs = xicrvs->next;
		yicrvs = yicrvs->next;
		zicrvs = zicrvs->next;
	    }

	    /* add xp and yp to head of free list */
	    assert(xp->next_sp == yp);
	    yp->next_sp = free_list;
	    free_list = xp;

	    /* add zp to tail of new_list */
	    *last_pointer = zp;
	    last_pointer = &(zp->next_sp);
	    xp = zp->next_sp;
	} else {		/* its a data plot */
	    assert(*last_pointer == xp);	/* think this is true ! */
	    last_pointer = &(xp->next_sp);
	    xp = xp->next_sp;
	}
    }

    /* Ok, append free list and write first_plot */
    *last_pointer = free_list;
    first_3dplot = new_list;
}

static void load_contour_label_options (struct text_label *contour_label)
{
    struct lp_style_type *lp = &(contour_label->lp_properties);
    if (!contour_label->font)
	contour_label->font = gp_strdup(clabel_font);
    lp->p_interval = clabel_interval;
    lp->flags |= LP_SHOW_POINTS;
    lp_parse(lp, LP_ADHOC, TRUE);
}
