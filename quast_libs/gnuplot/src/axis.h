/*
 * $Id: axis.h,v 1.103.2.5 2016/06/14 22:08:10 sfeam Exp $
 *
 */

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

#ifndef GNUPLOT_AXIS_H
#define GNUPLOT_AXIS_H

#include <stddef.h>		/* for offsetof() */
#include "gp_types.h"		/* for TBOOLEAN */

#include "gadgets.h"
#include "parse.h"		/* for const_*() */
#include "tables.h"		/* for the axis name parse table */
#include "term_api.h"		/* for lp_style_type */
#include "util.h"		/* for int_error() */

/* typedefs / #defines */

/* give some names to some array elements used in command.c and grap*.c
 * maybe one day the relevant items in setshow will also be stored
 * in arrays.
 *
 * Always keep the following conditions alive:
 * SECOND_X_AXIS = FIRST_X_AXIS + SECOND_AXES
 * FIRST_X_AXIS & SECOND_AXES == 0
 */
typedef enum AXIS_INDEX {
    ALL_AXES = -1,
    FIRST_Z_AXIS = 0,
#define FIRST_AXES FIRST_Z_AXIS
    FIRST_Y_AXIS,
    FIRST_X_AXIS,
    COLOR_AXIS,			/* fill gap */
    SECOND_Z_AXIS,		/* not used, yet */
#define SECOND_AXES SECOND_Z_AXIS
    SECOND_Y_AXIS,
    SECOND_X_AXIS,
    POLAR_AXIS,
    T_AXIS,
    U_AXIS,
    V_AXIS,
    PARALLEL_AXES,	/* The first of up to MAX_PARALLEL_AXES */
    NO_AXIS = 99
} AXIS_INDEX;

#ifndef MAX_PARALLEL_AXES
# define MAX_PARALLEL_AXES MAX_NUM_VAR
#endif
# define AXIS_ARRAY_SIZE (11 + MAX_PARALLEL_AXES)
# define SAMPLE_AXIS SECOND_Z_AXIS
# define LAST_REAL_AXIS  POLAR_AXIS

/* What kind of ticmarking is wanted? */
typedef enum en_ticseries_type {
    TIC_COMPUTED=1, 		/* default; gnuplot figures them */
    TIC_SERIES,			/* user-defined series */
    TIC_USER,			/* user-defined points */
    TIC_MONTH,   		/* print out month names ((mo-1)%12)+1 */
    TIC_DAY      		/* print out day of week */
} t_ticseries_type;

typedef enum {
    DT_NORMAL=0,		/* default; treat values as pure numeric */
    DT_TIMEDATE,		/* old datatype */
    DT_DMS,			/* degrees minutes seconds */
    DT_UNINITIALIZED
} td_type;

/* Defines one ticmark for TIC_USER style.
 * If label==NULL, the value is printed with the usual format string.
 * else, it is used as the format string (note that it may be a constant
 * string, like "high" or "low").
 */
typedef struct ticmark {
    double position;		/* where on axis is this */
    char *label;		/* optional (format) string label */
    int level;			/* 0=major tic, 1=minor tic */
    struct ticmark *next;	/* linked list */
} ticmark;

/* Tic-mark labelling definition; see set xtics */
typedef struct ticdef {
    t_ticseries_type type;
    char *font;
    struct t_colorspec textcolor;
    struct {
	   struct ticmark *user;	/* for TIC_USER */
	   struct {			/* for TIC_SERIES */
		  double start, incr;
		  double end;		/* ymax, if VERYLARGE */
	   } series;
	   TBOOLEAN mix;		/* TRUE to use both the above */
    } def;
    struct position offset;
    TBOOLEAN rangelimited;		/* Limit tics to data range */
    TBOOLEAN enhanced;			/* Use enhanced text mode or labels */
} t_ticdef;

/* we want two auto modes for minitics - default where minitics are
 * auto for log/time and off for linear, and auto where auto for all
 * graphs I've done them in this order so that logscale-mode can
 * simply test bit 0 to see if it must do the minitics automatically.
 * similarly, conventional plot can test bit 1 to see if minitics are
 * required */
typedef enum en_minitics_status {
    MINI_OFF,
    MINI_DEFAULT,
    MINI_USER,
    MINI_AUTO
} t_minitics_status;

/* Function pointer type for callback functions doing operations for a
 * single ticmark */
typedef void (*tic_callback) __PROTO((AXIS_INDEX, double, char *, int, 
					struct lp_style_type,
					struct ticmark *));

/* Values to put in the axis_tics[] variables that decides where the
 * ticmarks should be drawn: not at all, on one or both plot borders,
 * or the zeroaxes. These look like a series of values, but TICS_MASK
 * shows that they're actually bit masks --> don't turn into an enum
 * */
#define NO_TICS        0
#define TICS_ON_BORDER 1
#define TICS_ON_AXIS   2
#define TICS_MASK      3
#define TICS_MIRROR    4

/* Tic levels 0 and 1 are maintained in the axis structure.
 * Tic levels 2 - MAX_TICLEVEL have only one property - scale.
 */
#define MAX_TICLEVEL 5
extern double ticscale[MAX_TICLEVEL];

#if 0 /* HBB 20010806 --- move GRID flags into axis struct */
/* Need to allow user to choose grid at first and/or second axes tics.
 * Also want to let user choose circles at x or y tics for polar grid.
 * Also want to allow user rectangular grid for polar plot or polar
 * grid for parametric plot. So just go for full configurability.
 * These are bitmasks
 */
#define GRID_OFF    0
#define GRID_X      (1<<0)
#define GRID_Y      (1<<1)
#define GRID_Z      (1<<2)
#define GRID_X2     (1<<3)
#define GRID_Y2     (1<<4)
#define GRID_MX     (1<<5)
#define GRID_MY     (1<<6)
#define GRID_MZ     (1<<7)
#define GRID_MX2    (1<<8)
#define GRID_MY2    (1<<9)
#define GRID_CB     (1<<10)
#define GRID_MCB    (1<<11)
#endif /* 0 */

/* HBB 20010610: new type for storing autoscale activity. Effectively
 * two booleans (bits) in a single variable, so I'm using an enum with
 * all 4 possible bit masks given readable names. */
typedef enum e_autoscale {
    AUTOSCALE_NONE = 0,
    AUTOSCALE_MIN = 1<<0,
    AUTOSCALE_MAX = 1<<1,
    AUTOSCALE_BOTH = (1<<0 | 1 << 1),
    AUTOSCALE_FIXMIN = 1<<2,
    AUTOSCALE_FIXMAX = 1<<3
} t_autoscale;

typedef enum e_constraint {
    CONSTRAINT_NONE  = 0,
    CONSTRAINT_LOWER = 1<<0,
    CONSTRAINT_UPPER = 1<<1,
    CONSTRAINT_BOTH  = (1<<0 | 1<<1)
} t_constraint;

typedef struct axis {
/* range of this axis */
    t_autoscale autoscale;	/* Which end(s) are autoscaled? */
    t_autoscale set_autoscale;	/* what does 'set' think autoscale to be? */
    int range_flags;		/* flag bits about autoscale/writeback: */
    /* write auto-ed ranges back to variables for autoscale */
#define RANGE_WRITEBACK   1
#define RANGE_SAMPLED     2
#define RANGE_IS_REVERSED 4
    double min;			/* 'transient' axis extremal values */
    double max;
    double set_min;		/* set/show 'permanent' values */
    double set_max;
    double writeback_min;	/* ULIG's writeback implementation */
    double writeback_max;
    double data_min;		/* Not necessarily the same as axis min */
    double data_max;

/* range constraints */
    t_constraint min_constraint;
    t_constraint max_constraint;
    double min_lb, min_ub;     /* min lower- and upper-bound */
    double max_lb, max_ub;     /* min lower- and upper-bound */
    
/* output-related quantities */
    int term_lower;		/* low and high end of the axis on output, */
    int term_upper;		/* ... (in terminal coordinates)*/
    double term_scale;		/* scale factor: plot --> term coords */
    unsigned int term_zero;	/* position of zero axis */

/* log axis control */
    TBOOLEAN log;		/* log axis stuff: flag "islog?" */
    double base;		/* logarithm base value */
    double log_base;		/* ln(base), for easier computations */

/* linked axis information (used only by x2, y2)
 * If linked_to_primary is TRUE, the primary axis info will be cloned into the
 * secondary axis only up to this point in the structure.
 */
    TBOOLEAN linked_to_primary;
    struct udft_entry *link_udf;

/* time/date axis control */
    td_type datatype;		/* {DT_NORMAL|DT_TIMEDATE} controls _input_ */
    td_type tictype;		/* {DT_NORMAL|DT_TIMEDATE|DT_DMS} controls _output_ */
    char *formatstring;		/* the format string for output */

/* ticmark control variables */
    int ticmode;		/* tics on border/axis? mirrored? */
    struct ticdef ticdef;	/* tic series definition */
    int tic_rotate;		/* ticmarks rotated by this angle */
    TBOOLEAN gridmajor;		/* Grid lines wanted on major tics? */
    TBOOLEAN gridminor;		/* Grid lines for minor tics? */
    t_minitics_status minitics;	/* minor tic mode (none/auto/user)? */
    double mtic_freq;		/* minitic stepsize */
    double ticscale;		/* scale factor for tic marks (was (0..1])*/
    double miniticscale;	/* and for minitics */
    TBOOLEAN tic_in;		/* tics to be drawn inward?  */

/* other miscellaneous fields */
    text_label label;		/* label string and position offsets */
    TBOOLEAN manual_justify;	/* override automatic justification */
    lp_style_type *zeroaxis;	/* usually points to default_axis_zeroaxis */
} AXIS;

#define DEFAULT_AXIS_TICDEF {TIC_COMPUTED, NULL, {TC_DEFAULT, 0, 0.0}, {NULL, {0.,0.,0.}, FALSE},  { character, character, character, 0., 0., 0. }, FALSE, TRUE }
#define DEFAULT_AXIS_ZEROAXIS {0, LT_AXIS, 0, DASHTYPE_AXIS, 0, 1.0, PTSZ_DEFAULT, 0, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}

#define DEFAULT_AXIS_STRUCT {						    \
	AUTOSCALE_BOTH, AUTOSCALE_BOTH, /* auto, set_auto */		    \
	0, 			/* range_flags for autoscaling */	    \
	-10.0, 10.0,		/* 3 pairs of min/max for axis itself */    \
	-10.0, 10.0,							    \
	-10.0, 10.0,							    \
	  0.0,  0.0,		/* and another min/max for the data */	    \
	CONSTRAINT_NONE, CONSTRAINT_NONE,  /* min and max constraints */    \
	0., 0., 0., 0.,         /* lower and upper bound for min and max */ \
	0, 0,   		/* terminal lower and upper coords */	    \
	0.,        		/* terminal scale */			    \
	0,        		/* zero axis position */		    \
	FALSE, 0.0, 0.0,	/* log, base, log(base) */		    \
	FALSE, NULL,		/* linked_to_primary, link function */      \
	DT_NORMAL,		/* datatype for input */	            \
	DT_NORMAL, NULL,      	/* tictype for output, output format, */    \
	NO_TICS,		/* tic output positions (border, mirror) */ \
	DEFAULT_AXIS_TICDEF,	/* tic series definition */		    \
	0, FALSE, FALSE, 	/* tic_rotate, grid{major,minor} */	    \
	MINI_DEFAULT, 10.,	/* minitics, mtic_freq */		    \
        1.0, 0.5, TRUE,		/* ticscale, miniticscale, tic_in */	    \
	EMPTY_LABELSTRUCT,	/* axis label */			    \
	FALSE,			/* override automatic justification */	    \
	NULL			/* NULL means &default_axis_zeroaxis */	    \
}

/* This much of the axis structure is cloned by the "set x2range link" command */
#define AXIS_CLONE_SIZE offsetof(AXIS, linked_to_primary)

/* Table of default behaviours --- a subset of the struct above. Only
 * those fields are present that differ from axis to axis. */
typedef struct axis_defaults {
    double min;			/* default axis endpoints */
    double max;
    char name[4];		/* axis name, like in "x2" or "t" */
    int ticmode;		/* tics on border/axis? mirrored? */
} AXIS_DEFAULTS;

/* global variables in axis.c */

extern AXIS axis_array[AXIS_ARRAY_SIZE];
extern const AXIS_DEFAULTS axis_defaults[AXIS_ARRAY_SIZE];

/* A parsing table for mapping axis names into axis indices. For use
 * by the set/show machinery, mainly */
extern const struct gen_table axisname_tbl[AXIS_ARRAY_SIZE+1];


extern const struct ticdef default_axis_ticdef;

/* default format for tic mark labels */
#define DEF_FORMAT "% h"
#define DEF_FORMAT_LATEX "$%h$"

/* default parse timedata string */
#define TIMEFMT "%d/%m/%y,%H:%M"
extern char *timefmt;

/* axis labels */
extern const text_label default_axis_label;

/* zeroaxis linetype (flag type==-3 if none wanted) */
extern const lp_style_type default_axis_zeroaxis;

/* default grid linetype, to be used by 'unset grid' and 'reset' */
extern const struct lp_style_type default_grid_lp;

/* grid layer: LAYER_BEHIND LAYER_BACK LAYER_FRONT */
extern int grid_layer;

/* Whether to draw the axis tic labels and tic marks in front of everything else */
extern TBOOLEAN grid_tics_in_front;

/* Whether or not to draw a separate polar axis in polar mode */
extern TBOOLEAN raxis;

/* global variables for communication with the tic callback functions */
/* FIXME HBB 20010806: had better be collected into a struct that's
 * passed to the callback */
extern int tic_start, tic_direction, tic_mirror;
/* These are for passing on to write_multiline(): */
extern int tic_text, rotate_tics, tic_hjust, tic_vjust;
/* The remaining ones are for grid drawing; controlled by 'set grid': */
/* extern int grid_selection; --- comm'ed out, HBB 20010806 */
extern struct lp_style_type grid_lp; /* linestyle for major grid lines */
extern struct lp_style_type mgrid_lp; /* linestyle for minor grid lines */
extern double polar_grid_angle; /* angle step in polar grid in radians */

/* Length of the longest tics label, set by widest_tic_callback(): */
extern int widest_tic_strlen;

/* flag to indicate that in-line axis ranges should be ignored */
extern TBOOLEAN inside_zoom;

/* axes being used by the current plot */
extern AXIS_INDEX x_axis, y_axis, z_axis;
/* macros to reduce code clutter caused by the array notation, mainly
 * in graphics.c and fit.c */
#define X_AXIS axis_array[x_axis]
#define Y_AXIS axis_array[y_axis]
#define Z_AXIS axis_array[z_axis]
#define R_AXIS axis_array[POLAR_AXIS]
#define CB_AXIS axis_array[COLOR_AXIS]

/* -------- macros using these variables: */

/* Macros to map from user to terminal coordinates and back */
#define AXIS_MAP(axis, variable)		\
  (int) ((axis_array[axis].term_lower)		\
	 + ((variable) - axis_array[axis].min)	\
	 * axis_array[axis].term_scale + 0.5)
#define AXIS_MAPBACK(axis, pos)						   \
  (((double)(pos)-axis_array[axis].term_lower)/axis_array[axis].term_scale \
   + axis_array[axis].min)


#define AXIS_SETSCALE(axis, out_low, out_high)			\
    axis_array[axis].term_scale = ((out_high) - (out_low))	\
        / (axis_array[axis].max - axis_array[axis].min)

/* write current min/max_array contents into the set/show status
 * variables */
#define AXIS_WRITEBACK(axis)			\
do {						\
    AXIS *this = axis_array + axis;		\
						\
    if (this->range_flags & RANGE_WRITEBACK) {	\
	if (this->autoscale & AUTOSCALE_MIN)	\
	    this->set_min = this->min;		\
	if (this->autoscale & AUTOSCALE_MAX)	\
	    this->set_max = this->max;		\
    }						\
} while(0)

/* HBB 20000430: New macros, logarithmize a value into a stored
 * coordinate*/
#define AXIS_DO_LOG(axis,value) (log(value) / axis_array[axis].log_base)
#define AXIS_UNDO_LOG(axis,value) exp((value) * axis_array[axis].log_base)

/* HBB 20000430: same, but these test if the axis is log, first: */
#define AXIS_LOG_VALUE(axis,value)				\
    (axis_array[axis].log ? AXIS_DO_LOG(axis,value) : (value))
#define AXIS_DE_LOG_VALUE(axis,coordinate)				  \
    (axis_array[axis].log ? AXIS_UNDO_LOG(axis,coordinate): (coordinate))


/* copy scalar data to arrays. The difference between 3D and 2D
 * versions is: dont know we have to support ranges [10:-10] - lets
 * reverse it for now, then fix it at the end.  */
/* FIXME HBB 20000426: unknown if this distinction makes any sense... */
#define AXIS_INIT3D(axis, islog_override, infinite)			\
do {									\
    AXIS *this = axis_array + axis;					\
									\
    this->autoscale = this->set_autoscale;				\
    if ((this->autoscale & AUTOSCALE_BOTH) == AUTOSCALE_NONE		\
	&& this->set_max < this->set_min) {				\
	this->min = this->set_max;					\
	this->max = this->set_min;					\
        /* we will fix later */						\
    } else {								\
	this->min = (infinite && (this->set_autoscale & AUTOSCALE_MIN))	\
	    ? VERYLARGE : this->set_min;				\
	this->max = (infinite && (this->set_autoscale & AUTOSCALE_MAX))	\
	    ? -VERYLARGE : this->set_max;				\
    }									\
    if (islog_override) {						\
	this->log = 0;							\
	this->base = 1;							\
	this->log_base = 0;						\
    } else {								\
	this->log_base = this->log ? log(this->base) : 0;		\
    }									\
    this->data_min = VERYLARGE;						\
    this->data_max = -VERYLARGE;					\
} while(0)

#define AXIS_INIT2D(axis, infinite)					\
do {									\
    AXIS *this = axis_array + axis;					\
									\
    this->autoscale = this->set_autoscale;				\
    this->min = (infinite && (this->set_autoscale & AUTOSCALE_MIN))	\
	? VERYLARGE : this->set_min;					\
    this->max = (infinite && (this->set_autoscale & AUTOSCALE_MAX))	\
	? -VERYLARGE : this->set_max;					\
    this->data_min = VERYLARGE;						\
    this->data_max = -VERYLARGE;					\
} while(0)

/* AXIS_INIT2D_REFRESH and AXIS_UPDATE2D_REFRESH(axis) are for volatile data */
#define AXIS_INIT2D_REFRESH(axis, infinite)				\
do {									\
    AXIS *this = axis_array + axis;					\
									\
    this->autoscale = this->set_autoscale;				\
    this->min = (infinite && (this->set_autoscale & AUTOSCALE_MIN))	\
	? VERYLARGE : AXIS_LOG_VALUE(axis, this->set_min);		\
    this->max = (infinite && (this->set_autoscale & AUTOSCALE_MAX))	\
	? -VERYLARGE : AXIS_LOG_VALUE(axis, this->set_max);		\
    this->log_base = this->log ? log(this->base) : 0;			\
} while(0)

#define AXIS_UPDATE2D_REFRESH(axis)					\
do {									\
    AXIS *this_axis = axis_array + axis;				\
    if ((this_axis->set_autoscale & AUTOSCALE_MIN) == 0)		\
	this_axis->min = AXIS_LOG_VALUE(axis, this_axis->set_min);	\
    if ((this_axis->set_autoscale & AUTOSCALE_MAX) == 0)		\
	this_axis->max = AXIS_LOG_VALUE(axis, this_axis->set_max);	\
} while (0)

/* parse a position of the form
 *    [coords] x, [coords] y {,[coords] z}
 * where coords is one of first,second.graph,screen,character
 * if first or second, we need to take axis.datatype into account
 */
#define GET_NUMBER_OR_TIME(store,axes,axis)				\
do {									\
    if (((axes) >= 0) && (axis_array[(axes)+(axis)].datatype == DT_TIMEDATE)	\
	&& isstringvalue(c_token)) {					\
	struct tm tm;							\
	double usec;							\
	char *ss = try_to_get_string();					\
	if (gstrptime(ss,timefmt,&tm,&usec))				\
	    (store) = (double) gtimegm(&tm) + usec;			\
	free(ss);							\
    } else {								\
	(store) = real_expression();					\
    }									\
} while(0)

/* This is one is very similar to GET_NUMBER_OR_TIME, but has slightly
 * different usage: it writes out '0' in case of inparsable time data,
 * and it's used when the target axis is fixed without a 'first' or
 * 'second' keyword in front of it. */
#define GET_NUM_OR_TIME(store,axis)			\
do {							\
    (store) = 0;					\
    GET_NUMBER_OR_TIME(store, FIRST_AXES, axis);	\
} while (0);

/* store VALUE or log(VALUE) in STORE, set TYPE as appropriate
 * Do OUT_ACTION or UNDEF_ACTION as appropriate
 * adjust range provided type is INRANGE (ie dont adjust y if x is outrange
 * VALUE must not be same as STORE
 * NOAUTOSCALE is per-plot property, whereas AUTOSCALE_XXX is per-axis.
 * Note: see the particular implementation for COLOR AXIS below.
 */

#define ACTUAL_STORE_WITH_LOG_AND_UPDATE_RANGE(STORE, VALUE, TYPE, AXIS,  \
					       NOAUTOSCALE, OUT_ACTION,   \
					       UNDEF_ACTION, is_cb_axis)  \
do {									  \
    struct axis *axis = &axis_array[AXIS];				  \
    double curval = VALUE;						  \
    if (AXIS == NO_AXIS)						  \
	break;								  \
    /* Version 5: OK to store infinities or NaN */			  \
    STORE = curval;							  \
    if (! (curval > -VERYLARGE && curval < VERYLARGE)) {		  \
	TYPE = UNDEFINED;						  \
	UNDEF_ACTION;							  \
	break;								  \
    }									  \
    if (axis->log) {							  \
	if (curval < 0.0) {						  \
	    STORE = not_a_number();					  \
	    TYPE = UNDEFINED;						  \
	    UNDEF_ACTION;						  \
	    break;							  \
	} else if (curval == 0.0) {					  \
	    STORE = -VERYLARGE;						  \
	    TYPE = OUTRANGE;						  \
	    OUT_ACTION;							  \
	    break;							  \
	} else {							  \
	    STORE = log(curval) / axis->log_base; /* AXIS_DO_LOG() */	  \
	}								  \
    }									  \
    if (NOAUTOSCALE)							  \
	break;  /* this plot is not being used for autoscaling */	  \
    if (TYPE != INRANGE)						  \
	break;  /* don't set y range if x is outrange, for example */	  \
    if ((! is_cb_axis) && axis->linked_to_primary) {	  		  \
	axis -= SECOND_AXES;						  \
	if (axis->link_udf->at) 					  \
	    curval = eval_link_function(AXIS - SECOND_AXES, curval);	  \
    } 									  \
    if ( curval < axis->min						  \
    &&  (curval <= axis->max || axis->max == -VERYLARGE)) {		  \
	if (axis->autoscale & AUTOSCALE_MIN)	{			  \
	    if (axis->min_constraint & CONSTRAINT_LOWER) {		  \
		if (axis->min_lb <= curval) {				  \
		    axis->min = curval;					  \
		} else {						  \
		    axis->min = axis->min_lb;				  \
		    TYPE = OUTRANGE;					  \
		    OUT_ACTION;						  \
		    break;						  \
		}							  \
	    } else {							  \
		axis->min = curval;					  \
	    }								  \
	} else if (curval != axis->max) {				  \
	    TYPE = OUTRANGE;						  \
	    OUT_ACTION;							  \
	    break;							  \
	}								  \
    }									  \
    if ( curval > axis->max						  \
    &&  (curval >= axis->min || axis->min == VERYLARGE)) {		  \
	if (axis->autoscale & AUTOSCALE_MAX)	{			  \
	    if (axis->max_constraint & CONSTRAINT_UPPER) {		  \
		if (axis->max_ub >= curval) {				  \
		    axis->max = curval;					  \
		} else {						  \
		    axis->max = axis->max_ub;				  \
		    TYPE =OUTRANGE;					  \
		    OUT_ACTION;						  \
		    break;						  \
		}							  \
	    } else {							  \
		axis->max = curval;					  \
	    }								  \
	} else if (curval != axis->min) {				  \
	    TYPE = OUTRANGE;						  \
	    OUT_ACTION;							  \
	}								  \
    }									  \
    /* Only update data min/max if the point is INRANGE Jun 2016 */	  \
    if (TYPE == INRANGE) {						  \
	if (axis->data_min > curval)					  \
	    axis->data_min = curval;					  \
	if (axis->data_max < curval)					  \
	    axis->data_max = curval;					  \
	}								  \
} while(0)

/* normal calls go though this macro, marked as not being a color axis */
#define STORE_WITH_LOG_AND_UPDATE_RANGE(STORE, VALUE, TYPE, AXIS, NOAUTOSCALE, OUT_ACTION, UNDEF_ACTION)	 \
 ACTUAL_STORE_WITH_LOG_AND_UPDATE_RANGE(STORE, VALUE, TYPE, AXIS, NOAUTOSCALE, OUT_ACTION, UNDEF_ACTION, 0)

/* Implementation of the above for the color axis. It should not change
 * the type of the point (out-of-range color is plotted with the color
 * of the min or max color value).
 */
#define COLOR_STORE_WITH_LOG_AND_UPDATE_RANGE(STORE, VALUE, TYPE, AXIS,	  \
			       NOAUTOSCALE, OUT_ACTION, UNDEF_ACTION)	  \
{									  \
    coord_type c_type_tmp = TYPE;					  \
    ACTUAL_STORE_WITH_LOG_AND_UPDATE_RANGE(STORE, VALUE, c_type_tmp, AXIS,	  \
					   NOAUTOSCALE, OUT_ACTION, UNDEF_ACTION, 1); \
}

/* Empty macro arguments triggered NeXT cpp bug       */
/* #define NOOP (0) caused many warnings from gcc 3.2 */
/* Now trying ((void)0) */
#define NOOP ((void)0)

/* HBB 20000506: new macro to automatically build initializer lists
 * for arrays of AXIS_ARRAY_SIZE=11 equal elements */
#define AXIS_ARRAY_INITIALIZER(value) {			\
    value, value, value, value, value,			\
	value, value, value, value, value, value }

/* FIXME: replace by a subroutine? */
#define clear_sample_range(axis) do {				\
	axis_array[SAMPLE_AXIS].range_flags = 0;		\
	axis_array[SAMPLE_AXIS].min = axis_array[axis].min;	\
	axis_array[SAMPLE_AXIS].max = axis_array[axis].max;	\
	} while (0)

/* 'roundoff' check tolerance: less than one hundredth of a tic mark */
#define SIGNIF (0.01)
/* (DFK) Watch for cancellation error near zero on axes labels */
/* FIXME HBB 20000521: these seem not to be used much, anywhere... */
#define CheckZero(x,tic) (fabs(x) < ((tic) * SIGNIF) ? 0.0 : (x))

/* ------------ functions exported by axis.c */
t_autoscale load_range __PROTO((AXIS_INDEX, double *, double *, t_autoscale));
void axis_unlog_interval __PROTO((AXIS_INDEX, double *, double *, TBOOLEAN));
void axis_revert_range __PROTO((AXIS_INDEX));
void axis_revert_and_unlog_range __PROTO((AXIS_INDEX));
double axis_log_value_checked __PROTO((AXIS_INDEX, double, const char *));
void axis_checked_extend_empty_range __PROTO((AXIS_INDEX, const char *mesg));
char * copy_or_invent_formatstring __PROTO((AXIS_INDEX));
double quantize_normal_tics __PROTO((double, int));
void setup_tics __PROTO((AXIS_INDEX, int));
void gen_tics __PROTO((AXIS_INDEX, /* int, */ tic_callback));
void axis_output_tics __PROTO((AXIS_INDEX, int *, AXIS_INDEX, tic_callback));
void axis_set_graphical_range __PROTO((AXIS_INDEX, unsigned int lower, unsigned int upper));
void axis_draw_2d_zeroaxis __PROTO((AXIS_INDEX, AXIS_INDEX));
TBOOLEAN some_grid_selected __PROTO((void));
void add_tic_user __PROTO((AXIS_INDEX, char *, double, int));

double get_writeback_min __PROTO((AXIS_INDEX));
double get_writeback_max __PROTO((AXIS_INDEX));
void set_writeback_min __PROTO((AXIS_INDEX));
void set_writeback_max __PROTO((AXIS_INDEX));

void save_writeback_all_axes __PROTO((void));
int  parse_range __PROTO((AXIS_INDEX axis));
void parse_skip_range __PROTO((void));
void check_axis_reversed __PROTO((AXIS_INDEX axis));

/* set widest_tic_label: length of the longest tics label */
void widest_tic_callback __PROTO((AXIS_INDEX, double place, char *text, int ticlevel,
			struct lp_style_type grid, struct ticmark *));

void get_position __PROTO((struct position *pos));
void get_position_default __PROTO((struct position *pos, enum position_type default_type));

void gstrdms __PROTO((char *label, char *format, double value));

void clone_linked_axes __PROTO((AXIS_INDEX axis2, AXIS_INDEX axis1));

int map_x __PROTO((double value));
int map_y __PROTO((double value));

void set_cbminmax __PROTO((void));

#if (defined MAX_PARALLEL_AXES) && (MAX_PARALLEL_AXES > 0)
char * axis_name __PROTO((AXIS_INDEX));
#else
#define axis_name(axis) axis_defaults[axis].name
#endif

/* macro for tic scale, used in all tic_callback functions */
#define TIC_SCALE(ticlevel, axis) \
    (ticlevel <= 0 ? axis_array[axis].ticscale : \
     ticlevel == 1 ? axis_array[axis].miniticscale : \
     ticlevel < MAX_TICLEVEL ? ticscale[ticlevel] : \
     0)

#endif /* GNUPLOT_AXIS_H */
