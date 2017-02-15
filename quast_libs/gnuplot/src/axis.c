#ifndef lint
static char *RCSid() { return RCSid("$Id: axis.c,v 1.135.2.10 2016/04/14 20:13:52 sfeam Exp $"); }
#endif

/* GNUPLOT - axis.c */

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

#include "axis.h"

#include "stdfn.h"

#include "alloc.h"
#include "command.h"
#include "gadgets.h"
#include "gp_time.h"
#include "term_api.h"
#include "variable.h"

/* HBB 20000725: gather all per-axis variables into a struct, and set
 * up a single large array of such structs. Next step might be to use
 * isolated AXIS structs, instead of an array.
 * EAM 2013: tried that.  The problem is that all the routines and macros
 * that manipulate axis data take an index, not a pointer.  We'd have to
 * rewrite all of them and it just didn't seem worth it.
 * Instead I've added additional non-standard entries on the end, used for
 * parallel axis plots if nothing else.
 * Note: This array is now initialized in reset_command().
 */
AXIS axis_array[AXIS_ARRAY_SIZE];

/* Keep defaults varying by axis in their own array, to ease initialization
 * of the main array */
const AXIS_DEFAULTS axis_defaults[AXIS_ARRAY_SIZE] = {
    { -10, 10, "z" , TICS_ON_BORDER,               },
    { -10, 10, "y" , TICS_ON_BORDER | TICS_MIRROR, },
    { -10, 10, "x" , TICS_ON_BORDER | TICS_MIRROR, },
    { -10, 10, "cb", TICS_ON_BORDER | TICS_MIRROR, },
    { -10, 10, "z2", NO_TICS,                      },
    { -10, 10, "y2", NO_TICS,                      },
    { -10, 10, "x2", NO_TICS,                      },
    { - 0, 10, "r" , TICS_ON_AXIS,                 },
    { - 5,  5, "t" , NO_TICS,                      },
    { - 5,  5, "u" , NO_TICS,                      },
    { - 5,  5, "v" , NO_TICS,                      },
    {   0,  1, "p" , NO_TICS,                      }, /* shared by all parallel axes */
};


/* either the 'set format <axis>' or an automatically invented time
 * format string */
static char ticfmt[AXIS_ARRAY_SIZE][MAX_ID_LEN+1];

/* HBB 20010831: new enum typedef, to make code using this more
 * self-explanatory */
/* The unit the tics of a given time/date axis are to interpreted in */
/* HBB 20040318: start at one, to avoid undershoot */
typedef enum e_timelevel {
    TIMELEVEL_SECONDS = 1, TIMELEVEL_MINUTES, TIMELEVEL_HOURS,
    TIMELEVEL_DAYS, TIMELEVEL_WEEKS, TIMELEVEL_MONTHS,
    TIMELEVEL_YEARS
} t_timelevel;
static t_timelevel timelevel[AXIS_ARRAY_SIZE];

/* The <increment> given in a 'set {x|y|...}tics', or an automatically
 * generated one, if automatic tic placement is active */
static double ticstep[AXIS_ARRAY_SIZE];

/* HBB 20000506 new variable: parsing table for use with the table
 * module, to help generalizing set/show/unset/save, where possible */
const struct gen_table axisname_tbl[AXIS_ARRAY_SIZE + 1] =
{
    { "z", FIRST_Z_AXIS},
    { "y", FIRST_Y_AXIS},
    { "x", FIRST_X_AXIS},
    { "cb",COLOR_AXIS},
    { "z2",SECOND_Z_AXIS},
    { "y2",SECOND_Y_AXIS},
    { "x2",SECOND_X_AXIS},
    { "r", POLAR_AXIS},
    { "t", T_AXIS},
    { "u", U_AXIS},
    { "v", V_AXIS},
    { NULL, -1}
};


/* penalty for doing tics by callback in gen_tics is need for global
 * variables to communicate with the tic routines. Dont need to be
 * arrays for this */
/* HBB 20000416: they may not need to be array[]ed, but it'd sure
 * make coding easier, in some places... */
/* HBB 20000416: for the testing, these are global... */
/* static */ int tic_start, tic_direction, tic_text,
    rotate_tics, tic_hjust, tic_vjust, tic_mirror;

/* These are declare volatile in order to fool the compiler into not */
/* optimizing out intermediate values, thus hiding loss of precision.*/
volatile double vol_this_tic;
volatile double vol_previous_tic;

const struct ticdef default_axis_ticdef = DEFAULT_AXIS_TICDEF;

/* Tic scale for tics with level > 1.  0 means 'inherit minitics scale'  */
double ticscale[MAX_TICLEVEL] = {1,0.5,1,1,1};

/* global default time format */
char *timefmt = NULL;

/* axis labels */
const text_label default_axis_label = EMPTY_LABELSTRUCT;

/* zeroaxis drawing */
const lp_style_type default_axis_zeroaxis = DEFAULT_AXIS_ZEROAXIS;

/* grid drawing */
/* int grid_selection = GRID_OFF; */
#define DEFAULT_GRID_LP {0, LT_AXIS, 0, DASHTYPE_AXIS, 0, 0.5, 0.0, 0, {TC_LT, LT_AXIS, 0.0}, DEFAULT_DASHPATTERN}
const struct lp_style_type default_grid_lp = DEFAULT_GRID_LP;
struct lp_style_type grid_lp   = DEFAULT_GRID_LP;
struct lp_style_type mgrid_lp  = DEFAULT_GRID_LP;
int grid_layer = LAYER_BEHIND;
TBOOLEAN grid_tics_in_front = FALSE;
double polar_grid_angle = 0;	/* nonzero means a polar grid */
TBOOLEAN raxis = TRUE;

/* Length of the longest tics label, set by widest_tic_callback(): */
int widest_tic_strlen;

/* flag to indicate that in-line axis ranges should be ignored */
TBOOLEAN inside_zoom;

/* axes being used by the current plot */
/* These are mainly convenience variables, replacing separate copies of
 * such variables originally found in the 2D and 3D plotting code */
AXIS_INDEX x_axis = FIRST_X_AXIS;
AXIS_INDEX y_axis = FIRST_Y_AXIS;
AXIS_INDEX z_axis = FIRST_Z_AXIS;

/* --------- internal prototypes ------------------------- */
static double make_auto_time_minitics __PROTO((t_timelevel, double));
static double make_tics __PROTO((AXIS_INDEX, int));
static double quantize_time_tics __PROTO((AXIS_INDEX, double, double, int));
static double time_tic_just __PROTO((t_timelevel, double));
static double round_outward __PROTO((AXIS_INDEX, TBOOLEAN, double));
static TBOOLEAN axis_position_zeroaxis __PROTO((AXIS_INDEX));
static void load_one_range __PROTO((AXIS_INDEX axis, double *a, t_autoscale *autoscale, t_autoscale which ));
static double quantize_duodecimal_tics __PROTO((double, int));
static void get_position_type __PROTO((enum position_type * type, int *axes));

/* ---------------------- routines ----------------------- */

/* check range and take logs of min and max if logscale
 * this also restores min and max for ranges like [10:-10]
 */
#define LOG_MSG(x) x " range must be greater than 0 for scale"

/* {{{ axis_unlog_interval() */

/* this is used in a few places all over the code: undo logscaling of
 * a given range if necessary. If checkrange is TRUE, will int_error() if
 * range is invalid */
void
axis_unlog_interval(AXIS_INDEX axis, double *min, double *max, TBOOLEAN checkrange)
{
    if (axis_array[axis].log) {
	if (checkrange && (*min<= 0.0 || *max <= 0.0))
	    int_error(NO_CARET,
		      "%s range must be greater than 0 for log scale",
		      axis_name(axis));
	*min = (*min<=0) ? -VERYLARGE : AXIS_DO_LOG(axis,*min);
	*max = (*max<=0) ? -VERYLARGE : AXIS_DO_LOG(axis,*max);
    }
}

/* }}} */

/* {{{ axis_revert_range() */

void
axis_revert_range(AXIS_INDEX axis)
{
  if (((axis_array[axis].range_flags & RANGE_IS_REVERSED))
  &&  (axis_array[axis].autoscale != 0)
  &&  (axis_array[axis].max > axis_array[axis].min) ) {
    double temp = axis_array[axis].min;
    axis_array[axis].min = axis_array[axis].max;
    axis_array[axis].max = temp;
  }
}

/* {{{ axis_revert_and_unlog_range() */

void
axis_revert_and_unlog_range(AXIS_INDEX axis)
{
  axis_revert_range( axis );
  axis_unlog_interval(axis, &axis_array[axis].min, &axis_array[axis].max, 1);
}

/* }}} */

/* {{{ axis_log_value_checked() */
double
axis_log_value_checked(AXIS_INDEX axis, double coord, const char *what)
{
    if (axis_array[axis].log) {
	if (coord <= 0.0) {
	    graph_error("%s has %s coord of %g; must be above 0 for log scale!",
			what, axis_name(axis), coord);
	} else
	    return (AXIS_DO_LOG(axis,coord));
    }
    return (coord);
}

/* }}} */

#if (defined MAX_PARALLEL_AXES) && (MAX_PARALLEL_AXES > 0)
char *
axis_name(AXIS_INDEX axis)
{
    static char name[] = "paxis 00 ";
    if (axis >= PARALLEL_AXES) {
	sprintf(name, "paxis %d ", axis-PARALLEL_AXES+1);
	return name;
    }
    return (char *)axis_defaults[axis].name;
}
#endif

/* {{{ axis_checked_extend_empty_range() */
/*
 * === SYNOPSIS ===
 *
 * This function checks whether the data and/or plot range in a given axis
 * is too small (which would cause divide-by-zero and/or infinite-loop
 * problems later on).  If so,
 * - if autoscaling is in effect for this axis, we widen the range
 * - otherwise, we abort with a call to  int_error()  (which prints out
 *   a suitable error message, then (hopefully) aborts this command and
 *   returns to the command prompt or whatever).
 *
 *
 * === HISTORY AND DESIGN NOTES ===
 *
 * 1998 Oct 4, Jonathan Thornburg <jthorn@galileo.thp.univie.ac.at>
 *
 * This function used to be a (long) macro  FIXUP_RANGE(AXIS, WHICH)
 * which was (identically!) defined in  plot2d.c  and  plot3d.c .  As
 * well as now being a function instead of a macro, the logic is also
 * changed:  The "too small" range test no longer depends on 'set zero'
 * and is now properly scaled relative to the data magnitude.
 *
 * The key question in designing this function is the policy for just how
 * much to widen the data range by, as a function of the data magnitude.
 * This is to some extent a matter of taste.  IMHO the key criterion is
 * that (at least) all of the following should (a) not infinite-loop, and
 * (b) give correct plots, regardless of the 'set zero' setting:
 *      plot 6.02e23            # a huge number >> 1 / FP roundoff level
 *      plot 3                  # a "reasonable-sized" number
 *      plot 1.23e-12           # a small number still > FP roundoff level
 *      plot 1.23e-12 * sin(x)  # a small function still > FP roundoff level
 *      plot 1.23e-45           # a tiny number << FP roundoff level
 *      plot 1.23e-45 * sin(x)  # a tiny function << FP roundoff level
 *      plot 0          # or (more commonly) a data file of all zeros
 * That is, IMHO gnuplot should *never* infinite-loop, and it should *never*
 * producing an incorrect or misleading plot.  In contrast, the old code
 * would infinite-loop on most of these examples with 'set zero 0.0' in
 * effect, or would plot the small-amplitude sine waves as the zero function
 * with 'zero' set larger than the sine waves' amplitude.
 *
 * The current code plots all the above examples correctly and without
 * infinite looping.
 *
 * HBB 2000/05/01: added an additional up-front test, active only if
 *   the new 'mesg' parameter is non-NULL.
 *
 * === USAGE ===
 *
 * Arguments:
 * axis = (in) An integer specifying which axis (x1, x2, y1, y2, z, etc)
 *             we should do our stuff for.  We use this argument as an
 *             index into the global arrays  {min,max,auto}_array .  In
 *             practice this argument will typically be one of the constants
 *              {FIRST,SECOND}_{X,Y,Z}_AXIS  defined in plot.h.
 * mesg = (in) if non-NULL, will check if the axis range is valid (min
 *             not +VERYLARGE, max not -VERYLARGE), and int_error() out
 *             if it isn't.
 *
 * Global Variables:
 * auto_array, min_array, max_array (in out) (defined in axis.[ch]):
 *    variables describing the status of autoscaling and range ends, for
 *    each of the possible axes.
 *
 * c_token = (in) (defined in plot.h) Used in formatting an error message.
 *
 */
void
axis_checked_extend_empty_range(AXIS_INDEX axis, const char *mesg)
{
    /* These two macro definitions set the range-widening policy: */

    /* widen [0:0] by +/- this absolute amount */
#define FIXUP_RANGE__WIDEN_ZERO_ABS	1.0
    /* widen [nonzero:nonzero] by -/+ this relative amount */
#define FIXUP_RANGE__WIDEN_NONZERO_REL	0.01

    double dmin = axis_array[axis].min;
    double dmax = axis_array[axis].max;

    /* HBB 20000501: this same action was taken just before most of
     * the invocations of this function, so I moved it into here.
     * Only do this if 'mesg' is non-NULL --> pass NULL if you don't
     * want the test */
    if (mesg
	&& (axis_array[axis].min == VERYLARGE
	    || axis_array[axis].max == -VERYLARGE))
	int_error(c_token, mesg);

    if (dmax - dmin == 0.0) {
	/* empty range */
	if (axis_array[axis].autoscale) {
	    /* range came from autoscaling ==> widen it */
	    double widen = (dmax == 0.0) ?
		FIXUP_RANGE__WIDEN_ZERO_ABS
		: FIXUP_RANGE__WIDEN_NONZERO_REL * fabs(dmax);
	    if (!(axis == FIRST_Z_AXIS && !mesg)) /* set view map */
		fprintf(stderr, "Warning: empty %s range [%g:%g], ",
		    axis_name(axis), dmin, dmax);
	    /* HBB 20010525: correctly handle single-ended
	     * autoscaling, too: */
	    if (axis_array[axis].autoscale & AUTOSCALE_MIN)
		axis_array[axis].min -= widen;
	    if (axis_array[axis].autoscale & AUTOSCALE_MAX)
		axis_array[axis].max += widen;
	    if (!(axis == FIRST_Z_AXIS && !mesg)) /* set view map */
		fprintf(stderr, "adjusting to [%g:%g]\n",
		    axis_array[axis].min, axis_array[axis].max);
	} else {
	    /* user has explicitly set the range (to something empty)
               ==> we're in trouble */
	    int_error(NO_CARET, "Can't plot with an empty %s range!",
		      axis_name(axis));
	}
    }
}

/* }}} */

/* {{{ make smalltics for time-axis */
static double
make_auto_time_minitics(t_timelevel tlevel, double incr)
{
    double tinc = 0.0;

    if ((int)tlevel < TIMELEVEL_SECONDS)
	tlevel = TIMELEVEL_SECONDS;
    switch (tlevel) {
    case TIMELEVEL_SECONDS:
    case TIMELEVEL_MINUTES:
	if (incr >= 5)
	    tinc = 1;
	if (incr >= 10)
	    tinc = 5;
	if (incr >= 20)
	    tinc = 10;
	if (incr >= 60)
	    tinc = 20;
	if (incr >= 2 * 60)
	    tinc = 60;
	if (incr >= 6 * 60)
	    tinc = 2 * 60;
	if (incr >= 12 * 60)
	    tinc = 3 * 60;
	if (incr >= 24 * 60)
	    tinc = 6 * 60;
	break;
    case TIMELEVEL_HOURS:
	if (incr >= 20 * 60)
	    tinc = 10 * 60;
	if (incr >= 3600)
	    tinc = 30 * 60;
	if (incr >= 2 * 3600)
	    tinc = 3600;
	if (incr >= 6 * 3600)
	    tinc = 2 * 3600;
	if (incr >= 12 * 3600)
	    tinc = 3 * 3600;
	if (incr >= 24 * 3600)
	    tinc = 6 * 3600;
	break;
    case TIMELEVEL_DAYS:
	if (incr > 2 * 3600)
	    tinc = 3600;
	if (incr > 4 * 3600)
	    tinc = 2 * 3600;
	if (incr > 7 * 3600)
	    tinc = 3 * 3600;
	if (incr > 13 * 3600)
	    tinc = 6 * 3600;
	if (incr > DAY_SEC)
	    tinc = 12 * 3600;
	if (incr > 2 * DAY_SEC)
	    tinc = DAY_SEC;
	break;
    case TIMELEVEL_WEEKS:
	if (incr > 2 * DAY_SEC)
	    tinc = DAY_SEC;
	if (incr > 7 * DAY_SEC)
	    tinc = 7 * DAY_SEC;
	break;
    case TIMELEVEL_MONTHS:
	if (incr > 2 * DAY_SEC)
	    tinc = DAY_SEC;
	if (incr > 15 * DAY_SEC)
	    tinc = 10 * DAY_SEC;
	if (incr > 2 * MON_SEC)
	    tinc = MON_SEC;
	if (incr > 6 * MON_SEC)
	    tinc = 3 * MON_SEC;
	if (incr > 2 * YEAR_SEC)
	    tinc = YEAR_SEC;
	break;
    case TIMELEVEL_YEARS:
	if (incr > 2 * MON_SEC)
	    tinc = MON_SEC;
	if (incr > 6 * MON_SEC)
	    tinc = 3 * MON_SEC;
	if (incr > 2 * YEAR_SEC)
	    tinc = YEAR_SEC;
	if (incr > 10 * YEAR_SEC)
	    tinc = 5 * YEAR_SEC;
	if (incr > 50 * YEAR_SEC)
	    tinc = 10 * YEAR_SEC;
	if (incr > 100 * YEAR_SEC)
	    tinc = 20 * YEAR_SEC;
	if (incr > 200 * YEAR_SEC)
	    tinc = 50 * YEAR_SEC;
	if (incr > 300 * YEAR_SEC)
	    tinc = 100 * YEAR_SEC;
	break;
    }
    return (tinc);
}
/* }}} */

/* {{{ copy_or_invent_formatstring() */
/* Rarely called helper function looks_like_numeric() */
int
looks_like_numeric(char *format)
{
    if (!(format = strchr(format, '%')))
	return 0;

    while (++format && (*format == ' '
			|| *format == '-'
			|| *format == '+'
			|| *format == '#'))
	;			/* do nothing */

    while (isdigit((unsigned char) *format) || *format == '.')
	++format;

    return (*format == 'e' || *format == 'f' || *format == 'g' || *format == 'h');
}


/* Either copies the axis formatstring over to the ticfmt[] array, or
 * in case that's not applicable because the format hasn't been
 * specified correctly, invents a time/date output format by looking
 * at the range of values.  Considers time/date fields that don't
 * change across the range to be unimportant */
char *
copy_or_invent_formatstring(AXIS_INDEX axis)
{
    struct tm t_min, t_max;

    if (axis_array[axis].tictype != DT_TIMEDATE
    ||  !looks_like_numeric(axis_array[axis].formatstring)) {
	/* The simple case: formatstring is usable, so use it! */
	strncpy(ticfmt[axis], axis_array[axis].formatstring, MAX_ID_LEN);
	/* Ensure enough precision to distinguish tics */
	if (!strcmp(ticfmt[axis], DEF_FORMAT)) {
	    double axmin = AXIS_DE_LOG_VALUE(axis,axis_array[axis].min);
	    double axmax = AXIS_DE_LOG_VALUE(axis,axis_array[axis].max);
	    int precision = ceil(-log10(GPMIN(fabs(axmax-axmin),fabs(axmin))));

	    if ((axmin*axmax > 0) && precision > 4)
		sprintf(ticfmt[axis],"%%.%df", (precision>14) ? 14 : precision);
	}

	return ticfmt[axis];
    }

    /* Else, have to invent an output format string. */
    *ticfmt[axis] = 0;		/* make sure we strcat to empty string */

    ggmtime(&t_min, time_tic_just(timelevel[axis], axis_array[axis].min));
    ggmtime(&t_max, time_tic_just(timelevel[axis], axis_array[axis].max));

    if (t_max.tm_year == t_min.tm_year
	&& t_max.tm_yday == t_min.tm_yday) {
	/* same day, skip date */
	if (t_max.tm_hour != t_min.tm_hour) {
	    strcpy(ticfmt[axis], "%H");
	}
	if (timelevel[axis] < TIMELEVEL_DAYS) {
	    if (ticfmt[axis][0])
		strcat(ticfmt[axis], ":");
	    strcat(ticfmt[axis], "%M");
	}
	if (timelevel[axis] < TIMELEVEL_HOURS) {
	    strcat(ticfmt[axis], ":%S");
	}
    } else {
	if (t_max.tm_year != t_min.tm_year) {
	    /* different years, include year in ticlabel */
	    /* check convention, day/month or month/day */
	    if (strchr(timefmt, 'm')
		< strchr(timefmt, 'd')) {
		strcpy(ticfmt[axis], "%m/%d/%");
	    } else {
		strcpy(ticfmt[axis], "%d/%m/%");
	    }
	    if (((int) (t_max.tm_year / 100)) != ((int) (t_min.tm_year / 100))) {
		strcat(ticfmt[axis], "Y");
	    } else {
		strcat(ticfmt[axis], "y");
	    }

	} else {
	    /* Copy day/month order over from input format */
	    if (strchr(timefmt, 'm')
		< strchr(timefmt, 'd')) {
		strcpy(ticfmt[axis], "%m/%d");
	    } else {
		strcpy(ticfmt[axis], "%d/%m");
	    }
	}
	if (timelevel[axis] < TIMELEVEL_WEEKS) {
	    /* Note: seconds can't be useful if there's more than 1
	     * day's worth of data... */
	    strcat(ticfmt[axis], "\n%H:%M");
	}
    }
    return ticfmt[axis];
}

/* }}} */

/* {{{ quantize_normal_tics() */
/* the guide parameter was intended to allow the number of tics
 * to depend on the relative sizes of the plot and the font.
 * It is the approximate upper limit on number of tics allowed.
 * But it did not go down well with the users.
 * A value of 20 gives the same behaviour as 3.5, so that is
 * hardwired into the calls to here. Maybe we will restore it
 * to the automatic calculation one day
 */

/* HBB 20020220: Changed to use value itself as first argument, not
 * log10(value).  Done to allow changing the calculation method
 * to avoid numerical problems */
double
quantize_normal_tics(double arg, int guide)
{
    /* order of magnitude of argument: */
    double power = pow(10.0, floor(log10(arg)));
    double xnorm = arg / power;	/* approx number of decades */
    /* we expect 1 <= xnorm <= 10 */
    double posns = guide / xnorm; /* approx number of tic posns per decade */
    /* with guide=20, we expect 2 <= posns <= 20 */
    double tics;

    /* HBB 20020220: Looking at these, I would normally expect
     * to see posns*tics to be always about the same size. But we
     * rather suddenly drop from 2.0 to 1.0 at tic step 0.5. Why? */
    /* JRV 20021117: fixed this by changing next to last threshold
       from 1 to 2.  However, with guide=20, this doesn't matter. */
    if (posns > 40)
	tics = 0.05;		/* eg 0, .05, .10, ... */
    else if (posns > 20)
	tics = 0.1;		/* eg 0, .1, .2, ... */
    else if (posns > 10)
	tics = 0.2;		/* eg 0,0.2,0.4,... */
    else if (posns > 4)
	tics = 0.5;		/* 0,0.5,1, */
    else if (posns > 2)
	tics = 1;		/* 0,1,2,.... */
    else if (posns > 0.5)
	tics = 2;		/* 0, 2, 4, 6 */
    else
	/* getting desperate... the ceil is to make sure we
	 * go over rather than under - eg plot [-10:10] x*x
	 * gives a range of about 99.999 - tics=xnorm gives
	 * tics at 0, 99.99 and 109.98  - BAD !
	 * This way, inaccuracy the other way will round
	 * up (eg 0->100.0001 => tics at 0 and 101
	 * I think latter is better than former
	 */
	tics = ceil(xnorm);

    return (tics * power);
}

/* }}} */

/* {{{ make_tics() */
/* Implement TIC_COMPUTED case, i.e. automatically choose a usable
 * ticking interval for the given axis. For the meaning of the guide
 * parameter, see the comment on quantize_normal_tics() */
static double
make_tics(AXIS_INDEX axis, int guide)
{
    double xr, tic;

    xr = fabs(axis_array[axis].min - axis_array[axis].max);
    if (xr == 0)
	return 1;	/* Anything will do, since we'll never use it */
    if (xr >= VERYLARGE)
	int_warn(NO_CARET,"%s axis range undefined or overflow",
		axis_name(axis));
    tic = quantize_normal_tics(xr, guide);
    /* FIXME HBB 20010831: disabling this might allow short log axis
     * to receive better ticking... */
    if (axis_array[axis].log && tic < 1.0)
	  tic = 1.0;

    if (axis_array[axis].tictype == DT_TIMEDATE)
	return quantize_time_tics(axis, tic, xr, guide);
    else
	return tic;
}
/* }}} */

/* {{{ quantize_duodecimal_tics */
/* HBB 20020220: New function, to be used to properly tic axes with a
 * duodecimal reference, as used in times (60 seconds, 60 minuts, 24
 * hours, 12 months). Derived from quantize_normal_tics(). The default
 * guide is assumed to be 12, here, not 20 */
static double
quantize_duodecimal_tics(double arg, int guide)
{
    /* order of magnitude of argument: */
    double power = pow(12.0, floor(log(arg)/log(12.0)));
    double xnorm = arg / power;	/* approx number of decades */
    double posns = guide / xnorm; /* approx number of tic posns per decade */

    if (posns > 24)
	return power / 24;	/* half a smaller unit --- shouldn't happen */
    else if (posns > 12)
	return power / 12;	/* one smaller unit */
    else if (posns > 6)
	return power / 6;	/* 2 smaller units = one-6th of a unit */
    else if (posns > 4)
	return power / 4;	/* 3 smaller units = quarter unit */
    else if (posns > 2)
	return power / 2;	/* 6 smaller units = half a unit */
    else if (posns > 1)
	return power;		/* 0, 1, 2, ..., 11 */
    else if (posns > 0.5)
	return power * 2;		/* 0, 2, 4, ..., 10 */
    else if (posns > 1.0/3)
	return power * 3;		/* 0, 3, 6, 9 */
    else
	/* getting desperate... the ceil is to make sure we
	 * go over rather than under - eg plot [-10:10] x*x
	 * gives a range of about 99.999 - tics=xnorm gives
	 * tics at 0, 99.99 and 109.98  - BAD !
	 * This way, inaccuracy the other way will round
	 * up (eg 0->100.0001 => tics at 0 and 101
	 * I think latter is better than former
	 */
	return power * ceil(xnorm);
}
/* }}} */

/* {{{ quantize_time_tics */
/* HBB 20010831: newly isolated subfunction. Used to be part of
 * make_tics() */
/* Look at the tic interval given, and round it to a nice figure
 * suitable for time/data axes, i.e. a small integer number of
 * seconds, minutes, hours, days, weeks or months. As a side effec,
 * this routine also modifies the static timelevel[axis] to indicate
 * the units these tics are calculated in. */
static double
quantize_time_tics(AXIS_INDEX axis, double tic, double xr, int guide)
{
    int guide12 = guide * 3 / 5; /* --> 12 for default of 20 */

    timelevel[axis] = TIMELEVEL_SECONDS;
    if (tic > 5) {
	/* turn tic into units of minutes */
	tic = quantize_duodecimal_tics(xr / 60.0, guide12) * 60;
	if (tic >= 60)
	    timelevel[axis] = TIMELEVEL_MINUTES;
    }
    if (tic > 5 * 60) {
	/* turn tic into units of hours */
	tic = quantize_duodecimal_tics(xr / 3600.0, guide12) * 3600;
	if (tic >= 3600)
	    timelevel[axis] = TIMELEVEL_HOURS;
    }
    if (tic > 3600) {
	/* turn tic into units of days */
        tic = quantize_duodecimal_tics(xr / DAY_SEC, guide12) * DAY_SEC;
	if (tic >= DAY_SEC)
	    timelevel[axis] = TIMELEVEL_DAYS;
    }
    if (tic > 2 * DAY_SEC) {
	/* turn tic into units of weeks */
	tic = quantize_normal_tics(xr / WEEK_SEC, guide) * WEEK_SEC;
	if (tic < WEEK_SEC) {	/* force */
	    tic = WEEK_SEC;
	}
	if (tic >= WEEK_SEC)
	    timelevel[axis] = TIMELEVEL_WEEKS;
    }
    if (tic > 3 * WEEK_SEC) {
	/* turn tic into units of month */
	tic = quantize_normal_tics(xr / MON_SEC, guide) * MON_SEC;
	if (tic < MON_SEC) {	/* force */
	    tic = MON_SEC;
	}
	if (tic >= MON_SEC)
	    timelevel[axis] = TIMELEVEL_MONTHS;
    }
    if (tic > MON_SEC) {
	/* turn tic into units of years */
	tic = quantize_duodecimal_tics(xr / YEAR_SEC, guide12) * YEAR_SEC;
	if (tic >= YEAR_SEC)
	    timelevel[axis] = TIMELEVEL_YEARS;
    }
    return (tic);
}

/* }}} */


/* {{{ round_outward */
/* HBB 20011204: new function (repeated code ripped out of setup_tics)
 * that rounds an axis endpoint outward. If the axis is a time/date
 * one, take care to round towards the next whole time unit, not just
 * a multiple of the (averaged) tic size */
static double
round_outward(
    AXIS_INDEX axis,		/* Axis to work on */
    TBOOLEAN upwards,		/* extend upwards or downwards? */
    double input)		/* the current endpoint */
{
    double tic = ticstep[axis];
    double result = tic * (upwards
			   ? ceil(input / tic)
			   : floor(input / tic));

    if (axis_array[axis].tictype == DT_TIMEDATE) {
	double ontime = time_tic_just(timelevel[axis], result);

	/* FIXME: how certain is it that we don't want to *always*
	 * return 'ontime'? */
	if ((upwards && (ontime > result))
	    || (!upwards && (ontime <result)))
	    return ontime;
    }

    return result;
}

/* }}} */

/* {{{ setup_tics */
/* setup_tics allows max number of tics to be specified but users dont
 * like it to change with size and font, so we use value of 20, which
 * is 3.5 behaviour.  Note also that if format is '', yticlin = 0, so
 * this gives division by zero.  */

void
setup_tics(AXIS_INDEX axis, int max)
{
    double tic = 0;
    AXIS *this = axis_array + axis;
    struct ticdef *ticdef = &(this->ticdef);

    /* HBB 20010703: New: allow _not_ to autoextend the axis endpoints
     * to an integer multiple of the ticstep, for autoscaled axes with
     * automatic tics */
    TBOOLEAN autoextend_min = (this->autoscale & AUTOSCALE_MIN)
	&& ! (this->autoscale & AUTOSCALE_FIXMIN);
    TBOOLEAN autoextend_max = (this->autoscale & AUTOSCALE_MAX)
	&& ! (this->autoscale & AUTOSCALE_FIXMAX);

    /*  Apply constraints on autoscaled axis if requested:
     *  The range is _expanded_ here only.  Limiting the range is done
     *  in the macro STORE_WITH_LOG_AND_UPDATE_RANGE() of axis.h  */
    if (this->autoscale & AUTOSCALE_MIN) {
      	if (this->min_constraint & CONSTRAINT_UPPER) {
	    if (this->min > this->min_ub)
		this->min = this->min_ub;
	}
    }
    if (this->autoscale & AUTOSCALE_MAX) {
	if (this->max_constraint & CONSTRAINT_LOWER) {
	    if (this->max < this->max_lb)
		this->max = this->max_lb;
	}
    }

    /* HBB 20000506: if no tics required for this axis, do
     * nothing. This used to be done exactly before each call of
     * setup_tics, anyway... */
    if (! this->ticmode)
	return;

    if (ticdef->type == TIC_SERIES) {
	ticstep[axis] = tic = ticdef->def.series.incr;
	autoextend_min = autoextend_min
	                 && (ticdef->def.series.start == -VERYLARGE);
	autoextend_max = autoextend_max
	                 && (ticdef->def.series.end == VERYLARGE);
    } else if (ticdef->type == TIC_COMPUTED) {
	ticstep[axis] = tic = make_tics(axis, max);
    } else {
	/* user-defined, day or month */
	autoextend_min = autoextend_max = FALSE;
    }

    /* If an explicit stepsize was set, timelevel[axis] wasn't defined,
     * leading to strange misbehaviours of minor tics on time axes.
     * We used to call quantize_time_tics, but that also caused strangeness.
     */
    if (this->tictype == DT_TIMEDATE && ticdef->type == TIC_SERIES) {
	if      (tic >= 365*24*60*60.) timelevel[axis] = TIMELEVEL_YEARS;
	else if (tic >=  28*24*60*60.) timelevel[axis] = TIMELEVEL_MONTHS;
	else if (tic >=   7*24*60*60.) timelevel[axis] = TIMELEVEL_WEEKS;
	else if (tic >=     24*60*60.) timelevel[axis] = TIMELEVEL_DAYS;
	else if (tic >=        60*60.) timelevel[axis] = TIMELEVEL_HOURS;
	else if (tic >=           60.) timelevel[axis] = TIMELEVEL_MINUTES;
	else                           timelevel[axis] = TIMELEVEL_SECONDS;
    }

    /* Note: setup_tics is always called on the primary axis first, so we can
     * clone that rather than trying to reproduce it for the secondary axis.
     */
    if (this->linked_to_primary) {
	clone_linked_axes(axis, axis - SECOND_AXES);
	autoextend_min = autoextend_max = FALSE;
    }

    if (autoextend_min) {
	this->min = round_outward(axis, ! (this->min < this->max), this->min);
	if (this->min_constraint & CONSTRAINT_LOWER && this->min < this->min_lb)
	    this->min = this->min_lb;
    }

    if (autoextend_max) {
	this->max = round_outward(axis, this->min < this->max, this->max);
	if (this->max_constraint & CONSTRAINT_UPPER && this->max > this->max_ub)
	    this->max = this->max_ub;
    }

    /* Set up ticfmt[axis] correctly. If necessary (time axis, but not
     * time/date output format), make up a formatstring that suits the
     * range of data */
    copy_or_invent_formatstring(axis);
}

/* }}} */

/* {{{  gen_tics */
/* uses global arrays ticstep[], ticfmt[], axis_array[],
 * we use any of GRID_X/Y/X2/Y2 and  _MX/_MX2/etc - caller is expected
 * to clear the irrelevent fields from global grid bitmask
 * note this is also called from graph3d, so we need GRID_Z too
 */
void
gen_tics(AXIS_INDEX axis, tic_callback callback)
{
    AXIS *this = axis_array + axis;
    struct ticdef *def = & this->ticdef;
    t_minitics_status minitics = this->minitics; /* off/default/auto/explicit */

    struct lp_style_type lgrd = grid_lp;
    struct lp_style_type mgrd = mgrid_lp;

    if (! this->gridmajor)
	lgrd.l_type = LT_NODRAW;
    if (! this->gridminor)
	mgrd.l_type = LT_NODRAW;

    /* EAM FIXME - This really shouldn't happen, but it triggers for instance */
    /* if x2tics or y2tics are autoscaled but there is no corresponding data. */
    if (this->min >= VERYLARGE || this->max <= -VERYLARGE)
	return;

    if (def->def.user) {	/* user-defined tic entries */
	struct ticmark *mark = def->def.user;
	double uncertain = (this->max - this->min) / 10;
	double internal_min = this->min - SIGNIF * uncertain;
	double internal_max = this->max + SIGNIF * uncertain;
#if 0
	double log10_base = this->log ? log10(this->base) : 1.0;
#else	/* This allows gprintf formats %L %l to work even when log scaling is off */
	double log10_base = this->base > 0.0 ? log10(this->base) : 1.0;
#endif
	double polar_shift = 0;

	/* polar labels always +ve, and if rmin has been set, they are
	 * relative to rmin.
	 */
	if (polar && axis == POLAR_AXIS) {
	    if (!(R_AXIS.autoscale & AUTOSCALE_MIN))
		polar_shift = R_AXIS.min;
	    internal_min = X_AXIS.min - SIGNIF * uncertain;
	    internal_max = X_AXIS.max + SIGNIF * uncertain;
	}

	for (mark = def->def.user; mark; mark = mark->next) {
	    char label[MAX_ID_LEN];	/* Scratch space to construct a label */
	    char *ticlabel;		/* Points either to ^^ or to some existing text */
	    double internal;

	    /* This condition is only possible if we are in polar mode */
	    if (axis == POLAR_AXIS)
		internal = AXIS_LOG_VALUE(POLAR_AXIS,mark->position)
			 - AXIS_LOG_VALUE(POLAR_AXIS,polar_shift);
	    else
		internal = AXIS_LOG_VALUE(axis,mark->position) - polar_shift;

	    if (!inrange(internal, internal_min, internal_max))
		continue;

	    if (mark->level < 0) {
		/* label read from data file */
		ticlabel = mark->label;
	    } else if (mark->label && !strchr(mark->label, '%')) {
		/* string constant that contains no format keys */
		ticlabel = mark->label;
	    } else if (axis >= PARALLEL_AXES) {
		/* FIXME: needed because ticfmt array is not maintained for parallel axes */
		gprintf(label, sizeof(label),
			mark->label ? mark->label : this->formatstring,
			log10_base, mark->position);
		ticlabel = label;
	    } else if (this->tictype == DT_TIMEDATE) {
		gstrftime(label, MAX_ID_LEN-1, mark->label ? mark->label : ticfmt[axis], mark->position);
		ticlabel = label;
	    } else if (this->tictype == DT_DMS) {
		gstrdms(label, mark->label ? mark->label : ticfmt[axis], mark->position);
		ticlabel = label;
	    } else {
		gprintf(label, sizeof(label), mark->label ? mark->label : ticfmt[axis], log10_base, mark->position);
		ticlabel = label;
	    }

	    /* use NULL instead of label for minor tics with level 1,
	     * however, allow labels for minor tics with levels > 1 */
	    (*callback) (axis, internal,
	    		(mark->level==1)?NULL:ticlabel,
	    		mark->level,
	    		(mark->level>0)?mgrd:lgrd, NULL);

	    /* Polar axis tics are mirrored across the origin */
	    if (axis == POLAR_AXIS && (R_AXIS.ticmode & TICS_MIRROR)) {
		int save_gridline = lgrd.l_type;
		lgrd.l_type = LT_NODRAW;
		(*callback) (axis, -internal,
			(mark->level==1)?NULL:ticlabel,
			mark->level,
	    		(mark->level>0)?mgrd:lgrd, NULL);
		lgrd.l_type = save_gridline;
	    }

	}
	if (def->type == TIC_USER)
	    return;
    }

    /* series-tics
     * need to distinguish user co-ords from internal co-ords.
     * - for logscale, internal = log(user), else internal = user
     *
     * The minitics are a bit of a drag - we need to distinuish
     * the cases step>1 from step == 1.
     * If step = 1, we are looking at 1,10,100,1000 for example, so
     * minitics are 2,5,8, ...  - done in user co-ordinates
     * If step>1, we are looking at 1,1e6,1e12 for example, so
     * minitics are 10,100,1000,... - done in internal co-ords
     */

    {
	double tic;		/* loop counter */
	double internal;	/* in internal co-ords */
	double user;		/* in user co-ords */
	double start, step, end;
	int    nsteps;
	double lmin = this->min, lmax = this->max;
	double internal_min, internal_max;	/* to allow for rounding errors */
	double ministart = 0, ministep = 1, miniend = 1;	/* internal or user - depends on step */

	/* gprintf uses log10() of base - log_base_array is log() */
#if 0
	double log10_base = this->log ? log10(this->base) : 1.0;
#else	/* This allows gprintf formats %L %l to work even when log scaling is off */
	double log10_base = this->base > 0.0 ? log10(this->base) : 1.0;
#endif
	if (lmax < lmin) {
	    /* hmm - they have set reversed range for some reason */
	    double temp = lmin;
	    lmin = lmax;
	    lmax = temp;
	}
	/* {{{  choose start, step and end */
	switch (def->type) {
	case TIC_SERIES:
	    if (this->log && axis != POLAR_AXIS) {
		/* we can tolerate start <= 0 if step and end > 0 */
		if (def->def.series.end <= 0 || def->def.series.incr <= 0)
		    return;	/* just quietly ignore */
		step = AXIS_DO_LOG(axis, def->def.series.incr);
		if (def->def.series.start <= 0)	/* includes case 'undefined, i.e. -VERYLARGE */
		    start = step * floor(lmin / step);
		else
		    start = AXIS_DO_LOG(axis, def->def.series.start);
		if (def->def.series.end == VERYLARGE)
		    end = step * ceil(lmax / step);
		else
		    end = AXIS_DO_LOG(axis, def->def.series.end);
	    } else {
		start = def->def.series.start;
		step = def->def.series.incr;
		end = def->def.series.end;
		if (start == -VERYLARGE)
		    start = step * floor(lmin / step);
		if (end == VERYLARGE)
		    end = step * ceil(lmax / step);
	    }
	    break;
	case TIC_COMPUTED:
	    /* round to multiple of step */
	    start = ticstep[axis] * floor(lmin / ticstep[axis]);
	    step = ticstep[axis];
	    end = ticstep[axis] * ceil(lmax / ticstep[axis]);
	    break;
	case TIC_MONTH:
	    start = floor(lmin);
	    end = ceil(lmax);
	    step = floor((end - start) / 12);
	    if (step < 1)
		step = 1;
	    break;
	case TIC_DAY:
	    start = floor(lmin);
	    end = ceil(lmax);
	    step = floor((end - start) / 14);
	    if (step < 1)
		step = 1;
	    break;
	default:
	    graph_error("Internal error : unknown tic type");
	    return;		/* avoid gcc -Wall warning about start */
	}
	/* }}} */

	/* {{{  ensure ascending order */
	if (end < start) {
	    double temp;
	    temp = end;
	    end = start;
	    start = temp;
	}
	step = fabs(step);
	/* }}} */

	if ((minitics != MINI_OFF) && (this->miniticscale != 0)) {
	    FPRINTF((stderr,"axis.c: %d  start = %g end = %g step = %g\n", 
			__LINE__, start, end, step));
	    /* {{{  figure out ministart, ministep, miniend */
	    if (minitics == MINI_USER) {
		/* they have said what they want */
		if (this->mtic_freq <= 0)
		    minitics = MINI_OFF;
 		else if (this->log) {
 		    ministart = ministep = step / this->mtic_freq * this->base;
 		    miniend = step * this->base;
		    /* Suppress minitics that would lie on top of major tic */
		    while (ministart <= 1)
			ministart += ministep;
 		} else {
		    ministart = ministep = step / this->mtic_freq;
		    miniend = step;
 		}
	    } else if (this->log) {
		if (step > 1.5) {	/* beware rounding errors */
		    /* {{{  10,100,1000 case */
		    /* no more than five minitics */
		    if (step < 65535) /* could be INT_MAX but 54K is already ridiculous */
			ministart = ministep = (int)(0.2 * step);
		    else
			ministart = ministep = 0.2 * step;
		    if (ministep < 1)
			ministart = ministep = 1;
		    miniend = step;
		    /* }}} */
		} else {
		    /* {{{  2,5,8 case */
		    miniend = this->base;
		    if (end - start >= 10)
			minitics = MINI_OFF;
		    else if (end - start >= 5) {
			ministart = 2;
			ministep = 3;
		    } else {
			ministart = 2;
			ministep = 1;
		    }
		    /* }}} */
		}
	    } else if (this->tictype == DT_TIMEDATE) {
		ministart = ministep =
		    make_auto_time_minitics(timelevel[axis], step);
		miniend = step * 0.9;
	    } else if (minitics == MINI_AUTO) {
		int k = fabs(step)/pow(10.,floor(log10(fabs(step))));

		/* so that step == k times some power of 10 */
		ministart = ministep = (k==2 ? 0.5 : 0.2) * step;
		miniend = step;
	    } else
		minitics = MINI_OFF;

	    if (ministep <= 0)
		minitics = MINI_OFF;	/* dont get stuck in infinite loop */
	    /* }}} */
	}

	/* {{{  a few tweaks and checks */
	/* watch rounding errors */
	end += SIGNIF * step;
	/* HBB 20011002: adjusting the endpoints doesn't make sense if
	 * some oversmart user used a ticstep (much) larger than the
	 * yrange itself */
	if (step < (fabs(lmax) + fabs(lmin))) {
	    internal_max = lmax + step * SIGNIF;
	    internal_min = lmin - step * SIGNIF;
	} else {
	    internal_max = lmax;
	    internal_min = lmin;
	}

	if (step == 0)
	    return;		/* just quietly ignore them ! */
	/* }}} */

	/* This protects against user error, not precision errors */
	if ( (internal_max-internal_min)/step > term->xmax) {
	    int_warn(NO_CARET,"Too many axis ticks requested (>%.0g)",
		(internal_max-internal_min)/step);
	    return;
	}

	/* This protects against infinite loops if the separation between 	*/
	/* two ticks is less than the precision of the control variables. 	*/
	/* The for(...) loop here must exactly describe the true loop below. 	*/
	/* Furthermore, compiler optimization can muck up this test, so we	*/
	/* tell the compiler that the control variables are volatile.     	*/
	nsteps = 0;
	vol_previous_tic = start-step;
	for (vol_this_tic = start; vol_this_tic <= end; vol_this_tic += step) {
	    if (fabs(vol_this_tic - vol_previous_tic) < (step/4.)) {
		step = end - start;
		nsteps = 2;
		int_warn(NO_CARET, "tick interval too small for machine precision");
		break;
	    }
	    vol_previous_tic = vol_this_tic;
	    nsteps++;
	}

	for (tic = start; nsteps > 0; tic += step, nsteps--) {

	    /* {{{  calc internal and user co-ords */
	    if (axis == POLAR_AXIS) {
		/* Defer translation until after limit check */
		internal = tic;
	    } else if (!this->log) {
		internal = (this->tictype == DT_TIMEDATE)
		    ? time_tic_just(timelevel[axis], tic)
		    : tic;
		user = CheckZero(internal, step);
	    } else {
		/* log scale => dont need to worry about zero ? */
		internal = tic;
		user = AXIS_UNDO_LOG(axis, internal);
	    }
	    /* }}} */
	    if (internal > internal_max)
		break;		/* gone too far - end of series = VERYLARGE perhaps */
	    if (internal >= internal_min) {
		/* {{{  draw tick via callback */
		switch (def->type) {
		case TIC_DAY:{
			int d = (long) floor(user + 0.5) % 7;
			if (d < 0)
			    d += 7;
			(*callback) (axis, internal, abbrev_day_names[d], 0, lgrd,
					def->def.user);
			break;
		    }
		case TIC_MONTH:{
			int m = (long) floor(user - 1) % 12;
			if (m < 0)
			    m += 12;
			(*callback) (axis, internal, abbrev_month_names[m], 0, lgrd,
					def->def.user);
			break;
		    }
		default:{	/* comp or series */
			char label[MAX_ID_LEN]; /* Leave room for enhanced text markup */
			if (this->tictype == DT_TIMEDATE) {
			    /* If they are doing polar time plot, good luck to them */
			    gstrftime(label, MAX_ID_LEN-1, ticfmt[axis], (double) user);
			} else if (this->tictype == DT_DMS) {
			    gstrdms(label, ticfmt[axis], (double)user);
			} else if (polar && axis == POLAR_AXIS) {
			    double min = (R_AXIS.autoscale & AUTOSCALE_MIN) ? 0 : R_AXIS.min;
			    double r = fabs(user) + min;
			    /* POLAR_AXIS is the only sane axis, where the actual value */
			    /* is stored and we shift its position just before plotting.*/
			    internal = AXIS_LOG_VALUE(axis, tic)
				     - AXIS_LOG_VALUE(axis, R_AXIS.min);
			    r = tic;
			    gprintf(label, sizeof(label), ticfmt[axis], log10_base, r);
			} else if (axis >= PARALLEL_AXES) {
			    /* FIXME: needed because ticfmt array is not maintained for parallel axes */
			    gprintf(label, sizeof(label), this->formatstring,
				    log10_base, user);
			} else {
			    gprintf(label, sizeof(label), ticfmt[axis], log10_base, user);
			}

			/* Range-limited tic placement */
			if (def->rangelimited
			&&  !inrange(internal, this->data_min, this->data_max))
			    continue;

			(*callback) (axis, internal, label, 0, lgrd, def->def.user);

	 		/* Polar axis tics are mirrored across the origin */
			if (axis == POLAR_AXIS && (R_AXIS.ticmode & TICS_MIRROR)) {
			    int save_gridline = lgrd.l_type;
			    lgrd.l_type = LT_NODRAW;
			    (*callback) (axis, -internal, label, 0, lgrd, def->def.user);
			    lgrd.l_type = save_gridline;
			}
		    }
		}
		/* }}} */

	    }
	    if ((minitics != MINI_OFF) && (this->miniticscale != 0)) {
		/* {{{  process minitics */
		double mplace, mtic, temptic;
		for (mplace = ministart; mplace < miniend; mplace += ministep) {
		    if (this->tictype == DT_TIMEDATE)
			mtic = time_tic_just(timelevel[axis] - 1,
					     internal + mplace);
		    else
			mtic = internal
			    + (this->log && step <= 1.5
			       ? AXIS_DO_LOG(axis,mplace)
			       : mplace);
		    temptic = mtic;
		    if (polar && axis == POLAR_AXIS)
			temptic += R_AXIS.min;

		    /* Range-limited tic placement */
		    if (def->rangelimited
		    &&  !inrange(temptic, this->data_min, this->data_max))
			continue;

		    if (inrange(temptic, internal_min, internal_max)
			&& inrange(temptic, start - step * SIGNIF, end + step * SIGNIF))
			(*callback) (axis, mtic, NULL, 1, mgrd, NULL);
		}
		/* }}} */
	    }
	}
    }
}

/* }}} */

/* {{{ time_tic_just() */
/* justify ticplace to a proper date-time value */
static double
time_tic_just(t_timelevel level, double ticplace)
{
    struct tm tm;

    if (level <= TIMELEVEL_SECONDS) {
	return (ticplace);
    }
    ggmtime(&tm, ticplace);
    if (level >= TIMELEVEL_MINUTES) { /* units of minutes */
	if (tm.tm_sec > 55)
	    tm.tm_min++;
	tm.tm_sec = 0;
    }
    if (level >= TIMELEVEL_HOURS) { /* units of hours */
	if (tm.tm_min > 55)
	    tm.tm_hour++;
	tm.tm_min = 0;
    }
    if (level >= TIMELEVEL_DAYS) { /* units of days */
	if (tm.tm_hour > 22) {
	    tm.tm_hour = 0;
	    tm.tm_mday = 0;
	    tm.tm_yday++;
	    ggmtime(&tm, gtimegm(&tm));
	}
    }
    /* skip it, I have not bothered with weekday so far */
    if (level >= TIMELEVEL_MONTHS) {/* units of month */
	if (tm.tm_mday > 25) {
	    tm.tm_mon++;
	    if (tm.tm_mon > 11) {
		tm.tm_year++;
		tm.tm_mon = 0;
	    }
	}
	tm.tm_mday = 1;
    }

    ticplace = gtimegm(&tm);
    return (ticplace);
}
/* }}} */


/* {{{ axis_output_tics() */
/* HBB 20000416: new routine. Code like this appeared 4 times, once
 * per 2D axis, in graphics.c. Always slightly different, of course,
 * but generally, it's always the same. I distinguish two coordinate
 * directions, here. One is the direction of the axis itself (the one
 * it's "running" along). I refer to the one orthogonal to it as
 * "non-running", below. */
void
axis_output_tics(
     AXIS_INDEX axis,		/* axis number we're dealing with */
     int *ticlabel_position,	/* 'non-running' coordinate */
     AXIS_INDEX zeroaxis_basis,	/* axis to base 'non-running' position of
				 * zeroaxis on */
     tic_callback callback)	/* tic-drawing callback function */
{
    struct termentry *t = term;
    TBOOLEAN axis_is_vertical = ((axis % SECOND_AXES) == FIRST_Y_AXIS);
    TBOOLEAN axis_is_second = ((axis / SECOND_AXES) == 1);
    int axis_position;		/* 'non-running' coordinate */
    int mirror_position;	/* 'non-running' coordinate, 'other' side */
    double axis_coord = 0.0;	/* coordinate of this axis along non-running axis */

    if (zeroaxis_basis / SECOND_AXES) {
	axis_position = axis_array[zeroaxis_basis].term_upper;
	mirror_position = axis_array[zeroaxis_basis].term_lower;
    } else {
	axis_position = axis_array[zeroaxis_basis].term_lower;
	mirror_position = axis_array[zeroaxis_basis].term_upper;
    }

    if (axis >= PARALLEL_AXES)
	axis_coord = axis - PARALLEL_AXES + 1;

    if (axis_array[axis].ticmode) {
	/* set the globals needed by the _callback() function */

	if (axis_array[axis].tic_rotate == TEXT_VERTICAL
	    && (*t->text_angle)(TEXT_VERTICAL)) {
	    tic_hjust = axis_is_vertical
		? CENTRE
		: (axis_is_second ? LEFT : RIGHT);
	    tic_vjust = axis_is_vertical
		? (axis_is_second ? JUST_TOP : JUST_BOT)
		: JUST_CENTRE;
	    rotate_tics = TEXT_VERTICAL;
	    /* FIXME HBB 20000501: why would we want this? */
	    if (axis == FIRST_Y_AXIS)
		(*ticlabel_position) += t->v_char / 2;
	/* EAM - allow rotation by arbitrary angle in degrees      */
	/*       Justification of ytic labels is a problem since   */
	/*	 the position is already [mis]corrected for length */
	} else if (axis_array[axis].tic_rotate
		   && (*t->text_angle)(axis_array[axis].tic_rotate)) {
	    switch (axis) {
	    case FIRST_Y_AXIS:		/* EAM Purely empirical shift - is there a better? */
	    				*ticlabel_position += t->h_char * 2.5;
	    				tic_hjust = RIGHT; break;
	    case SECOND_Y_AXIS:		tic_hjust = LEFT;  break;
	    case FIRST_X_AXIS:		tic_hjust = LEFT;  break;
	    case SECOND_X_AXIS:		tic_hjust = LEFT;  break;
	    default:			tic_hjust = LEFT;  break;
	    }
	    tic_vjust = JUST_CENTRE;
	    rotate_tics = axis_array[axis].tic_rotate;
	} else {
	    tic_hjust = axis_is_vertical
		? (axis_is_second ? LEFT : RIGHT)
		: CENTRE;
	    tic_vjust = axis_is_vertical
		? JUST_CENTRE
		: (axis_is_second ? JUST_BOT : JUST_TOP);
	    rotate_tics = 0;
	}

	if (axis_array[axis].manual_justify)
	    tic_hjust = axis_array[axis].label.pos;
	else
	    axis_array[axis].label.pos = tic_hjust;

	if (axis_array[axis].ticmode & TICS_MIRROR)
	    tic_mirror = mirror_position;
	else
	    tic_mirror = -1;	/* no thank you */

	if ((axis_array[axis].ticmode & TICS_ON_AXIS)
	    && !axis_array[zeroaxis_basis].log
	    && inrange(axis_coord, axis_array[zeroaxis_basis].min,
		       axis_array[zeroaxis_basis].max)
	    ) {
	    tic_start = AXIS_MAP(zeroaxis_basis, axis_coord);
	    tic_direction = axis_is_second ? 1 : -1;
	    if (axis_array[axis].ticmode & TICS_MIRROR)
		tic_mirror = tic_start;
	    /* put text at boundary if axis is close to boundary and the
	     * corresponding boundary is switched on */
	    if (axis_is_vertical) {
		if (((axis_is_second ? -1 : 1) * (tic_start - axis_position)
		     > (3 * t->h_char))
		    || (!axis_is_second && (!(draw_border & 2)))
		    || (axis_is_second && (!(draw_border & 8))))
		    tic_text = tic_start;
		else
		    tic_text = axis_position;
		tic_text += (axis_is_second ? 1 : -1) * t->h_char;
	    } else {
		if (((axis_is_second ? -1 : 1) * (tic_start - axis_position)
		     > (2 * t->v_char))
		    || (!axis_is_second && (!(draw_border & 1)))
		    || (axis_is_second && (!(draw_border & 4))))
		    tic_text = tic_start +
			(axis_is_second ? 0
			 : - axis_array[axis].ticscale * t->v_tic);
		else
		    tic_text = axis_position;
		tic_text -= t->v_char;
	    }
	} else {
	    /* tics not on axis --> on border */
	    tic_start = axis_position;
	    tic_direction = (axis_array[axis].tic_in ? 1 : -1) * (axis_is_second ? -1 : 1);
	    tic_text = (*ticlabel_position);
	}
	/* go for it */
	gen_tics(axis, callback);
	(*t->text_angle) (0);	/* reset rotation angle */
    }
}

/* }}} */

/* {{{ axis_set_graphical_range() */

void
axis_set_graphical_range(AXIS_INDEX axis, unsigned int lower, unsigned int upper)
{
    axis_array[axis].term_lower = lower;
    axis_array[axis].term_upper = upper;
}
/* }}} */


/* {{{ axis_position_zeroaxis */
static TBOOLEAN
axis_position_zeroaxis(AXIS_INDEX axis)
{
    TBOOLEAN is_inside = FALSE;
    AXIS *this = axis_array + axis;

    /* NB: This is the only place that axis->term_zero is set. */
    /*     So it is important to reach here before plotting.   */
    if ((this->min > 0.0 && this->max > 0.0)
	|| this->log) {
	this->term_zero = (this->max < this->min)
	    ? this->term_upper : this->term_lower;
    } else if (this->min < 0.0 && this->max < 0.0) {
	this->term_zero = (this->max < this->min)
	    ? this->term_lower : this->term_upper;
    } else {
	this->term_zero = AXIS_MAP(axis, 0.0);
	is_inside = TRUE;
    }

    return is_inside;
}
/* }}} */


/* {{{ axis_draw_2d_zeroaxis() */
void
axis_draw_2d_zeroaxis(AXIS_INDEX axis, AXIS_INDEX crossaxis)
{
    AXIS *this = axis_array + axis;

    if (axis_position_zeroaxis(crossaxis) && this->zeroaxis) {
	term_apply_lp_properties(this->zeroaxis);
	if ((axis % SECOND_AXES) == FIRST_X_AXIS) {
	    (*term->move) (this->term_lower, axis_array[crossaxis].term_zero);
	    (*term->vector) (this->term_upper, axis_array[crossaxis].term_zero);
	} else {
	    (*term->move) (axis_array[crossaxis].term_zero, this->term_lower);
	    (*term->vector) (axis_array[crossaxis].term_zero, this->term_upper);
	}
    }
}
/* }}} */

static void
load_one_range(AXIS_INDEX axis, double *a, t_autoscale *autoscale, t_autoscale which )
{
    double number;

    assert( which==AUTOSCALE_MIN || which==AUTOSCALE_MAX );

    if (equals(c_token, "*")) {
	/*  easy:  do autoscaling!  */
	*autoscale |= which;
	if (which==AUTOSCALE_MIN) {
	    axis_array[axis].min_constraint &= ~CONSTRAINT_LOWER;
	    axis_array[axis].min_lb = 0;  /*  dummy entry  */
	} else {
	    axis_array[axis].max_constraint &= ~CONSTRAINT_LOWER;
	    axis_array[axis].max_lb = 0;  /*  dummy entry  */
	}
	c_token++;
    } else {
	/*  this _might_ be autoscaling with constraint or fixed value */
	/*  The syntax of '0 < *...' confuses the parser as he will try to
            include the '<' as a comparison operator in the expression.
            Setting scanning_range_in_progress will stop the parser from
            trying to build an action table if he finds '<' followed by '*'
            (which would normaly trigger a 'invalid expression'),  */
	scanning_range_in_progress = TRUE;
	GET_NUM_OR_TIME(number, axis);
	scanning_range_in_progress = FALSE;

	if (END_OF_COMMAND)
	    int_error(c_token, "unfinished range");

	if (equals(c_token, "<")) {
	    /*  this _seems_ to be autoscaling with lower bound  */
	    c_token++;
	    if (END_OF_COMMAND) {
		int_error(c_token, "unfinished range with constraint");
	    } else if (equals(c_token, "*")) {
		/*  okay:  this _is_ autoscaling with lower bound!  */
		*autoscale |= which;
		if (which==AUTOSCALE_MIN) {
		    axis_array[axis].min_constraint |= CONSTRAINT_LOWER;
		    axis_array[axis].min_lb = number;
		} else {
		    axis_array[axis].max_constraint |= CONSTRAINT_LOWER;
		    axis_array[axis].max_lb = number;
		}
		c_token++;
	    } else {
		int_error(c_token, "malformed range with constraint");
            }
        } else if (equals(c_token, ">")) {
	    int_error(c_token, "malformed range with constraint (use '<' only)");
	} else {
	    /*  no autoscaling-with-lower-bound but simple fixed value only  */
	    *autoscale &= ~which;
	    if (which==AUTOSCALE_MIN) {
		axis_array[axis].min_constraint = CONSTRAINT_NONE;
		axis_array[axis].min_ub = 0;  /*  dummy entry  */
	    } else {
		axis_array[axis].max_constraint = CONSTRAINT_NONE;
		axis_array[axis].max_ub = 0;  /*  dummy entry  */
	    }
	    *a = number;
        }
    }

    if (*autoscale & which) {
	/*  check for upper bound only if autoscaling is on  */
	if (END_OF_COMMAND)  int_error(c_token, "unfinished range");
	if (equals(c_token, "<")) {
	    /*  looks like upper bound up to now...  */

	    c_token++;
	    if (END_OF_COMMAND) int_error(c_token, "unfinished range with constraint");

	    GET_NUM_OR_TIME(number, axis);
	    /*  this autoscaling has an upper bound:  */

	    if (which==AUTOSCALE_MIN) {
		axis_array[axis].min_constraint |= CONSTRAINT_UPPER;
		axis_array[axis].min_ub = number;
	    } else {
		axis_array[axis].max_constraint |= CONSTRAINT_UPPER;
		axis_array[axis].max_ub = number;
	    }
	} else if (equals(c_token, ">")) {
	    int_error(c_token, "malformed range with constraint (use '<' only)");
	} else {
	    /*  there is _no_ upper bound on this autoscaling  */
	    if (which==AUTOSCALE_MIN) {
		axis_array[axis].min_constraint &= ~CONSTRAINT_UPPER;
		axis_array[axis].min_ub = 0;  /*  dummy entry  */
	    } else {
		axis_array[axis].max_constraint &= ~CONSTRAINT_UPPER;
		axis_array[axis].max_ub = 0;  /*  dummy entry  */
	    }
	}
    } else if (!END_OF_COMMAND){
	/*  no autoscaling = fixed value --> complain about constraints  */
	if (equals(c_token, "<") || equals(c_token, ">") ) {
	    int_error(c_token, "no upper bound constraint allowed if not autoscaling");
	}
    }

    /*  Consitency check  */
    if (*autoscale & which) {
	if (which==AUTOSCALE_MIN && axis_array[axis].min_constraint==CONSTRAINT_BOTH) {
	    if (axis_array[axis].min_ub < axis_array[axis].min_lb ) {
		int_warn(c_token,"Upper bound of constraint < lower bound:  Turning of constraints.");
		axis_array[axis].min_constraint = CONSTRAINT_NONE;
	    }
	}
	if (which==AUTOSCALE_MAX && axis_array[axis].max_constraint==CONSTRAINT_BOTH) {
	    if (axis_array[axis].max_ub < axis_array[axis].max_lb ) {
		int_warn(c_token,"Upper bound of constraint < lower bound:  Turning of constraints.");
		axis_array[axis].max_constraint = CONSTRAINT_NONE;
	    }
	}
    }
}


/* {{{ load_range() */
/* loads a range specification from the input line into variables 'a'
 * and 'b' */
t_autoscale
load_range(AXIS_INDEX axis, double *a, double *b, t_autoscale autoscale)
{
    if (equals(c_token, "]")) {
	axis_array[axis].min_constraint = CONSTRAINT_NONE;
	axis_array[axis].max_constraint = CONSTRAINT_NONE;
	return (autoscale);
    }

    if (END_OF_COMMAND) {
	int_error(c_token, "starting range value or ':' or 'to' expected");
    } else if (!equals(c_token, "to") && !equals(c_token, ":")) {
	load_one_range(axis, a, &autoscale, AUTOSCALE_MIN );
    }

    if (!equals(c_token, "to") && !equals(c_token, ":"))
	int_error(c_token, "':' or keyword 'to' expected");
    c_token++;

    if (!equals(c_token, "]")) {
	load_one_range(axis, b, &autoscale, AUTOSCALE_MAX );
    }

    /* Not all the code can deal nicely with +/- infinity */
    if (*a < -VERYLARGE)
	*a = -VERYLARGE;
    if (*b > VERYLARGE)
	*b = VERYLARGE;

    return (autoscale);
}

/* }}} */


/* we determine length of the widest tick label by getting gen_ticks to
 * call this routine with every label
 */

void
widest_tic_callback(AXIS_INDEX axis, double place, char *text,
    int ticlevel,
    struct lp_style_type grid,
    struct ticmark *userlabels)
{
    (void) axis;		/* avoid "unused parameter" warnings */
    (void) place;
    (void) grid;
    (void) userlabels;

    /* historically, minitics used to have no text,
     * but now they can, except at ticlevel 1
     * (and this restriction is there only for compatibility reasons) */
    if (ticlevel != 1) {
	int len = label_width(text, NULL);
	if (len > widest_tic_strlen)
	    widest_tic_strlen = len;
    }
}


/*
 * get and set routines for range writeback
 * ULIG *
 */

double
get_writeback_min(AXIS_INDEX axis)
{
    /* printf("get min(%d)=%g\n",axis,axis_array[axis].writeback_min); */
    return axis_array[axis].writeback_min;
}

double
get_writeback_max(AXIS_INDEX axis)
{
    /* printf("get max(%d)=%g\n",axis,axis_array[axis].writeback_min); */
    return axis_array[axis].writeback_max;
}

void
set_writeback_min(AXIS_INDEX axis)
{
    double val = AXIS_DE_LOG_VALUE(axis,axis_array[axis].min);
    /* printf("set min(%d)=%g\n",axis,val); */
    axis_array[axis].writeback_min = val;
}

void
set_writeback_max(AXIS_INDEX axis)
{
    double val = AXIS_DE_LOG_VALUE(axis,axis_array[axis].max);
    /* printf("set max(%d)=%g\n",axis,val); */
    axis_array[axis].writeback_max = val;
}

void
save_writeback_all_axes()
{
    AXIS_INDEX axis;

    for (axis = 0; axis < AXIS_ARRAY_SIZE; axis++)
	if (axis_array[axis].range_flags & RANGE_WRITEBACK) {
	    set_writeback_min(axis);
	    set_writeback_max(axis);
	}
}

void
check_axis_reversed( AXIS_INDEX axis )
{
    AXIS *this = axis_array + axis;
    if (((this->autoscale & AUTOSCALE_BOTH) == AUTOSCALE_NONE)
    &&  (this->set_max < this->set_min)) {
	this->min = this->set_min;
	this->max = this->set_max;
    }
}

TBOOLEAN
some_grid_selected()
{
    AXIS_INDEX i;
    for (i = 0; i <= LAST_REAL_AXIS; i++)
	if (axis_array[i].gridmajor || axis_array[i].gridminor)
	    return TRUE;
    return FALSE;
}

/*
 * Range checks for the color axis.
 */
void
set_cbminmax()
{
    if (CB_AXIS.set_autoscale & AUTOSCALE_MIN) {
	if (CB_AXIS.min >= VERYLARGE)
	    CB_AXIS.min = AXIS_DE_LOG_VALUE(FIRST_Z_AXIS,Z_AXIS.min);
    }
    CB_AXIS.min = axis_log_value_checked(COLOR_AXIS, CB_AXIS.min, "color axis");

    if (CB_AXIS.set_autoscale & AUTOSCALE_MAX) {
	if (CB_AXIS.max <= -VERYLARGE)
	    CB_AXIS.max = AXIS_DE_LOG_VALUE(FIRST_Z_AXIS,Z_AXIS.max);
    }
    CB_AXIS.max = axis_log_value_checked(COLOR_AXIS, CB_AXIS.max, "color axis");

    if (CB_AXIS.min > CB_AXIS.max) {
	double tmp = CB_AXIS.max;
	CB_AXIS.max = CB_AXIS.min;
	CB_AXIS.min = tmp;
    }
}

static void
get_position_type(enum position_type *type, int *axes)
{
    if (almost_equals(c_token, "fir$st")) {
	++c_token;
	*type = first_axes;
    } else if (almost_equals(c_token, "sec$ond")) {
	++c_token;
	*type = second_axes;
    } else if (almost_equals(c_token, "gr$aph")) {
	++c_token;
	*type = graph;
    } else if (almost_equals(c_token, "sc$reen")) {
	++c_token;
	*type = screen;
    } else if (almost_equals(c_token, "char$acter")) {
	++c_token;
	*type = character;
    }
    switch (*type) {
    case first_axes:
	*axes = FIRST_AXES;
	return;
    case second_axes:
	*axes = SECOND_AXES;
	return;
    default:
	*axes = (-1);
	return;
    }
}

/* get_position() - reads a position for label,arrow,key,... */

void
get_position(struct position *pos)
{
    get_position_default(pos,first_axes);
}

/* get_position() - reads a position for label,arrow,key,...
 * with given default coordinate system
 */
void
get_position_default(struct position *pos, enum position_type default_type)
{
    int axes;
    enum position_type type = default_type;

    memset(pos, 0, sizeof(struct position));

    get_position_type(&type, &axes);
    pos->scalex = type;
    GET_NUMBER_OR_TIME(pos->x, axes, FIRST_X_AXIS);

    if (equals(c_token, ",")) {
	++c_token;
	get_position_type(&type, &axes);
	pos->scaley = type;
	GET_NUMBER_OR_TIME(pos->y, axes, FIRST_Y_AXIS);
    } else {
	pos->y = 0;
	pos->scaley = type;
    }

    /* z is not really allowed for a screen co-ordinate, but keep it simple ! */
    if (equals(c_token, ",")
       /* Partial fix for ambiguous syntax when trailing comma ends a plot command */
	&& !(isstringvalue(c_token+1)) && !(almost_equals(c_token+1,"newhist$ogram"))
	&& !(almost_equals(c_token+1,"for"))
       ) {
	++c_token;
	get_position_type(&type, &axes);
	pos->scalez = type;
	GET_NUMBER_OR_TIME(pos->z, axes, FIRST_Z_AXIS);
    } else {
	pos->z = 0;
	pos->scalez = type;	/* same as y */
    }
}

/*
 * Add a single tic mark, with label, to the list for this axis.
 * To avoid duplications and overprints, sort the list and allow
 * only one label per position.
 * EAM - called from set.c during `set xtics` (level = 0 or 1)
 *       called from datafile.c during `plot using ::xtic()` (level = -1)
 */
void
add_tic_user(AXIS_INDEX axis, char *label, double position, int level)
{
    struct ticmark *tic, *newtic;
    struct ticmark listhead;

    if (!label && level < 0)
	return;

    /* Mark this axis as user-generated ticmarks only, unless the */
    /* mix flag indicates that both user- and auto- tics are OK.  */
    if (!axis_array[axis].ticdef.def.mix)
	axis_array[axis].ticdef.type = TIC_USER;

    /* Walk along list to sorted positional order */
    listhead.next = axis_array[axis].ticdef.def.user;
    listhead.position = -DBL_MAX;
    for (tic = &listhead;
	 tic->next && (position > tic->next->position);
	 tic = tic->next) {
    }

    if ((tic->next == NULL) || (position < tic->next->position)) {
	/* Make a new ticmark */
	newtic = (struct ticmark *) gp_alloc(sizeof(struct ticmark), (char *) NULL);
	newtic->position = position;
	/* Insert it in the list */
	newtic->next = tic->next;
	tic->next = newtic;
    } else {
	/* The new tic must duplicate position of tic->next */
	if (position != tic->next->position)
	    int_warn(NO_CARET,"add_tic_user: list sort error");
	newtic = tic->next;
	/* Don't over-write a major tic with a minor tic */
	if (level == 1)
	    return;
	/* User-specified tics are preferred to autogenerated tics. */
	if (level == 0 && newtic->level > 1)
	    return;
	/* FIXME: But are they preferred to data-generated tics?    */
	if (newtic->level < level)
	    return;
	if (newtic->label) {
	    free(newtic->label);
	    newtic->label = NULL;
	}
    }
    newtic->level = level;

    if (label)
	newtic->label = gp_strdup(label);
    else
	newtic->label = NULL;

    /* Make sure the listhead is kept */
    axis_array[axis].ticdef.def.user = listhead.next;
}

/*
 * Degrees/minutes/seconds geographic coordinate format
 * ------------------------------------------------------------
 *  %D 			= integer degrees, truncate toward zero
 *  %<width.precision>d	= floating point degrees
 *  %M 			= integer minutes, truncate toward zero
 *  %<width.precision>m	= floating point minutes
 *  %S 			= integer seconds, truncate toward zero
 *  %<width.precision>s	= floating point seconds
 *  %E                  = E/W instead of +/-
 *  %N                  = N/S instead of +/-
 */
void
gstrdms (char *label, char *format, double value)
{
double Degrees, Minutes, Seconds;
double degrees, minutes, seconds;
int dtype = 0, mtype = 0, stype = 0;
TBOOLEAN EWflag = FALSE;
TBOOLEAN NSflag = FALSE;
char compass = ' ';
char *c, *cfmt;

    /* Limit the range to +/- 180 degrees */
    if (value > 180.)
	value -= 360.;
    if (value < -180.)
	value += 360.;

    degrees = fabs(value);
    Degrees = floor(degrees);
    minutes = (degrees - (double)Degrees) * 60.;
    Minutes = floor(minutes);
    seconds = (degrees - (double)Degrees) * 3600. -  (double)Minutes*60.;
    Seconds = floor(seconds);

    for (c = cfmt = gp_strdup(format); *c; ) {
	if (*c++ == '%') {
	    while (*c && !strchr("DdMmSsEN%",*c))
		c++;
	    switch (*c) {
	    case 'D':	*c = 'g'; dtype = 1; degrees = Degrees; break;
	    case 'd':	*c = 'f'; dtype = 2; break;
	    case 'M':	*c = 'g'; mtype = 1; minutes = Minutes; break;
	    case 'm':	*c = 'f'; mtype = 2; break;
	    case 'S':	*c = 'g'; stype = 1; seconds = Seconds; break;
	    case 's':	*c = 'f'; stype = 2; break;
	    case 'E':	*c = 'c'; EWflag = TRUE; break;
	    case 'N':	*c = 'c'; NSflag = TRUE; break;
	    case '%':	int_error(NO_CARET,"unrecognized format: \"%s\"",format);
	    }
	}
    }

    /* By convention the minus sign goes only in front of the degrees */
    /* Watch out for round-off errors! */
    if (value < 0 && !EWflag && !NSflag) {
	if (dtype > 0)  degrees = -fabs(degrees);
	else if (mtype > 0)  minutes = -fabs(minutes);
	else if (stype > 0)  seconds = -fabs(seconds);
    }
    if (EWflag)
	compass = (value == 0) ? ' ' : (value < 0) ? 'W' : 'E';
    if (NSflag)
	compass = (value == 0) ? ' ' : (value < 0) ? 'S' : 'N';

    /* This is tricky because we have to deal with the possibility that
     * the user may not have specified all the possible format components
     */
    if (dtype == 0) {	/* No degrees */
	if (mtype == 0) {
	    if (stype == 0) /* Must be some non-DMS format */
		snprintf(label, MAX_ID_LEN, cfmt, value);
	    else
		snprintf(label, MAX_ID_LEN, cfmt,
		    seconds, compass);
	} else {
	    if (stype == 0)
		snprintf(label, MAX_ID_LEN, cfmt,
		    minutes, compass);
	    else
		snprintf(label, MAX_ID_LEN, cfmt,
		    minutes, seconds, compass);
	}
    } else {	/* Some form of degrees in first field */
	if (mtype == 0) {
	    if (stype == 0)
		snprintf(label, MAX_ID_LEN, cfmt,
		    degrees, compass);
	    else
		snprintf(label, MAX_ID_LEN, cfmt,
		    degrees, seconds, compass);
	} else {
	    if (stype == 0)
		snprintf(label, MAX_ID_LEN, cfmt,
		    degrees, minutes, compass);
	    else
		snprintf(label, MAX_ID_LEN, cfmt,
		    degrees, minutes, seconds, compass);
	}
    }

    free(cfmt);
}

/*
 * EAM Nov 2012
 * This routine used to be macros PARSE_RANGE, PARSE_NAMED_RANGE
 */

/* Accepts a range of the form [MIN:MAX] or [var=MIN:MAX]
 * Returns
 *	 0 = no range spec present
 *	-1 = range spec with no attached variable name
 *	>0 = token indexing the attached variable name
 */
int
parse_range(AXIS_INDEX axis)
{
    if (equals(c_token, "[")) {
	int dummy_token = -1;
	c_token++;
	/* If the range starts with "[var=" return the token of the named variable. */
	if (isletter(c_token) && equals(c_token + 1, "=")) {
		dummy_token = c_token;
		c_token += 2;
	}
	axis_array[axis].autoscale =
		load_range(axis, &axis_array[axis].min, &axis_array[axis].max,
			   axis_array[axis].autoscale);
	if (!equals(c_token, "]"))
	    int_error(c_token, "']' expected");
	c_token++;
	return dummy_token;
    } else
	return 0;
}

/* Called if an in-line range is encountered while inside a zoom command */
void
parse_skip_range()
{
    while (!equals(c_token++,"]"))
	if (END_OF_COMMAND)
	    break;
    return;
}

/*
 * When a secondary axis is linked to the corresponding primary axis,
 * this routine copies the relevant range/scale data
 */
void
clone_linked_axes(AXIS_INDEX axis2, AXIS_INDEX axis1)
{
    double testmin, testmax;
    TBOOLEAN suspect = FALSE;

    memcpy(&axis_array[axis2], &axis_array[axis1], AXIS_CLONE_SIZE);
    if (axis_array[axis2].link_udf == NULL || axis_array[axis2].link_udf->at == NULL)
	return;

    /* Transform the min/max limits of linked secondary axis */
	axis_array[axis2].set_min = eval_link_function(axis2, axis_array[axis1].set_min);
	axis_array[axis2].set_max = eval_link_function(axis2, axis_array[axis1].set_max);
	axis_array[axis2].min = eval_link_function(axis2, axis_array[axis1].min);
	axis_array[axis2].max = eval_link_function(axis2, axis_array[axis1].max);

	if (isnan(axis_array[axis2].min) || isnan(axis_array[axis2].set_min)
	||  isnan(axis_array[axis2].max) || isnan(axis_array[axis2].set_max))
	    int_warn(NO_CARET, "axis mapping function must return a real value");

    /* Confirm that the inverse mapping actually works, at least at the endpoints */
    /* FIXME:  Should we test values in between the endpoints also? */
	testmin = eval_link_function(axis1, axis_array[axis2].set_min);
	testmax = eval_link_function(axis1, axis_array[axis2].set_max);
	if (fabs(testmin - axis_array[axis1].set_min) != 0
	&&  fabs((testmin - axis_array[axis1].set_min) / testmin) > 1.e-6)
	    suspect = TRUE;
	if (fabs(testmax - axis_array[axis1].set_max) != 0
	&&  fabs((testmax - axis_array[axis1].set_max) / testmax) > 1.e-6)
	    suspect = TRUE;

	if (suspect) {
	    int_warn(NO_CARET, "could not confirm linked axis inverse mapping function");
	    fprintf(stderr,"\tmin: %g inv(via(min)): %g", axis_array[axis1].set_min, testmin);
	    fprintf(stderr,"  max: %g inv(via(max)): %g\n", axis_array[axis1].set_max, testmax);
	}
}

/*
 * Check for linked-axis coordinate transformation given by command
 *     set {x|y}2r link via <expr1> inverse <expr2>
 * If we are plotting on the secondary axis in this case, apply the inverse
 * transform to get back to the primary coordinate system before mapping.
 */

int
map_x(double value)
{
    if ((x_axis == SECOND_X_AXIS) 
    &&  axis_array[SECOND_X_AXIS].linked_to_primary
    &&  axis_array[SECOND_X_AXIS].link_udf->at != NULL) {
	if (axis_array[FIRST_X_AXIS].link_udf->at == NULL)
	    int_error(NO_CARET, "No inverse mapping function available for x2 data");
	value = eval_link_function(FIRST_X_AXIS, value);
	return AXIS_MAP(FIRST_X_AXIS, value);
    }
    return AXIS_MAP(x_axis, value);
}

int
map_y(double value)
{
    if ((y_axis == SECOND_Y_AXIS)
    &&  axis_array[SECOND_Y_AXIS].linked_to_primary
    &&  axis_array[SECOND_Y_AXIS].link_udf->at != NULL) {
	if (axis_array[FIRST_Y_AXIS].link_udf->at == NULL)
	    int_error(NO_CARET, "No inverse mapping function available for y2 data");
	value = eval_link_function(FIRST_Y_AXIS, value);
	return AXIS_MAP(FIRST_Y_AXIS, value);
    }
    return AXIS_MAP(y_axis, value);
}
