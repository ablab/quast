#ifndef lint
static char *RCSid() { return RCSid("$Id: mouse.c,v 1.168.2.10 2016/06/18 05:36:05 sfeam Exp $"); }
#endif

/* GNUPLOT - mouse.c */

/* driver independent mouse part. */

/*[
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
 *   Original Software (October 1999 - January 2000):
 *     Pieter-Tjerk de Boer <ptdeboer@cs.utwente.nl>
 *     Petr Mikulik <mikulik@physics.muni.cz>
 *     Johannes Zellner <johannes@zellner.org>
 */

#include "syscfg.h"
#include "stdfn.h"
#include "gp_types.h"

#define _MOUSE_C		/* FIXME HBB 20010207: violates Codestyle */
#ifdef USE_MOUSE		/* comment out whole file, otherwise... */

#include "mouse.h"
#include "pm3d.h"
#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "datafile.h"
#include "gadgets.h"
#include "gp_time.h"
#include "graphics.h"
#include "graph3d.h"
#include "plot3d.h"
#include "readline.h"
#include "term_api.h"
#include "util3d.h"
#include "hidden3d.h"

#ifdef _Windows
# include "win/winmain.h"
#endif

#ifdef OS2
#include <os2.h>
#include "os2/pm_msgs.h"
#endif

/********************** variables ***********************************************************/
char mouse_fmt_default[] = "% #g";

mouse_setting_t default_mouse_setting = DEFAULT_MOUSE_SETTING;
mouse_setting_t         mouse_setting = DEFAULT_MOUSE_SETTING;

/* "usual well-known" keycodes, i.e. those not listed in special_keys in mouse.h
*/
static const struct gen_table usual_special_keys[] =
{
    { "BackSpace", GP_BackSpace},
    { "Tab", GP_Tab},
    { "KP_Enter", GP_KP_Enter},
    { "Return", GP_Return},
    { "Escape", GP_Escape},
    { "Delete", GP_Delete},
    { NULL, 0}
};

/* the status of the shift, ctrl and alt keys
*/
static int modifier_mask = 0;

/* Structure for the ruler: on/off, position,...
 */
static struct {
    TBOOLEAN on;
    double x, y, x2, y2;	/* ruler position in real units of the graph */
    long px, py;		/* ruler position in the viewport units */
} ruler = {
    FALSE, 0, 0, 0, 0, 0, 0
};


/* the coordinates of the mouse cursor in gnuplot's internal coordinate system
 */
static int mouse_x = -1, mouse_y = -1;


/* the "real" coordinates of the mouse cursor, i.e., in the user's coordinate
 * system(s)
 */
static double real_x, real_y, real_x2, real_y2;


/* status of buttons; button i corresponds to bit (1<<i) of this variable
 */
static int button = 0;


/* variables for setting the zoom region:
 */
/* flag, TRUE while user is outlining the zoom region */
static TBOOLEAN setting_zoom_region = FALSE;
/* coordinates of the first corner of the zoom region, in the internal
 * coordinate system */
static int setting_zoom_x, setting_zoom_y;


/* variables for changing the 3D view:
*/
/* do we allow motion to result in a replot right now? */
TBOOLEAN allowmotion = TRUE;	/* used by pm.trm, too */
/* did we already postpone a replot because allowmotion was FALSE ? */
static TBOOLEAN needreplot = FALSE;
/* mouse position when dragging started */
static int start_x, start_y;
/* ButtonPress sets this to 0, ButtonMotion to 1 */
static int motion = 0;
/* values for rot_x and rot_z corresponding to zero position of mouse */
static float zero_rot_x, zero_rot_z;


/* bind related stuff */

typedef struct bind_t {
    struct bind_t *prev;
    int key;
    char modifier;
    char *command;
    char *(*builtin) (struct gp_event_t * ge);
    TBOOLEAN allwindows;
    struct bind_t *next;
} bind_t;

static bind_t *bindings = (bind_t *) 0;
static const int NO_KEY = -1;
static TBOOLEAN trap_release = FALSE;

/* forward declarations */
static void alert __PROTO((void));
static void MousePosToGraphPosReal __PROTO((int xx, int yy, double *x, double *y, double *x2, double *y2));
static char *xy_format __PROTO((void));
static char *zoombox_format __PROTO((void));
static char *GetAnnotateString __PROTO((char *s, double x, double y, int mode, char *fmt));
static char *xDateTimeFormat __PROTO((double x, char *b, int mode));
static void GetRulerString __PROTO((char *p, double x, double y));
static void apply_zoom __PROTO((struct t_zoom * z));
static void do_zoom __PROTO((double xmin, double ymin, double x2min,
			     double y2min, double xmax, double ymax, double x2max, double y2max));
static void ZoomNext __PROTO((void));
static void ZoomPrevious __PROTO((void));
static void ZoomUnzoom __PROTO((void));
static void incr_mousemode __PROTO((const int amount));
static void UpdateStatuslineWithMouseSetting __PROTO((mouse_setting_t * ms));

static void event_keypress __PROTO((struct gp_event_t * ge, TBOOLEAN current));
static void ChangeView __PROTO((int x, int z));
static void event_buttonpress __PROTO((struct gp_event_t * ge));
static void event_buttonrelease __PROTO((struct gp_event_t * ge));
static void event_motion __PROTO((struct gp_event_t * ge));
static void event_modifier __PROTO((struct gp_event_t * ge));
static void do_save_3dplot __PROTO((struct surface_points *, int, int));
static void load_mouse_variables __PROTO((double, double, TBOOLEAN, int));

static void do_zoom_in_around_mouse __PROTO((void));
static void do_zoom_out_around_mouse __PROTO((void));
static void do_zoom_in_X __PROTO((void));
static void do_zoom_out_X __PROTO((void));
static void do_zoom_scroll_up __PROTO((void));
static void do_zoom_scroll_down __PROTO((void));
static void do_zoom_scroll_left __PROTO((void));
static void do_zoom_scroll_right __PROTO((void));

/* builtins */
static char *builtin_autoscale __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_border __PROTO((struct gp_event_t * ge));
static char *builtin_replot __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_grid __PROTO((struct gp_event_t * ge));
static char *builtin_help __PROTO((struct gp_event_t * ge));
static char *builtin_set_plots_visible __PROTO((struct gp_event_t * ge));
static char *builtin_set_plots_invisible __PROTO((struct gp_event_t * ge));
static char *builtin_invert_plot_visibilities __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_log __PROTO((struct gp_event_t * ge));
static char *builtin_nearest_log __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_mouse __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_ruler __PROTO((struct gp_event_t * ge));
static char *builtin_decrement_mousemode __PROTO((struct gp_event_t * ge));
static char *builtin_increment_mousemode __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_polardistance __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_verbose __PROTO((struct gp_event_t * ge));
static char *builtin_toggle_ratio __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_next __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_previous __PROTO((struct gp_event_t * ge));
static char *builtin_unzoom __PROTO((struct gp_event_t * ge));
static char *builtin_rotate_right __PROTO((struct gp_event_t * ge));
static char *builtin_rotate_up __PROTO((struct gp_event_t * ge));
static char *builtin_rotate_left __PROTO((struct gp_event_t * ge));
static char *builtin_rotate_down __PROTO((struct gp_event_t * ge));
static char *builtin_cancel_zoom __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_in_around_mouse __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_out_around_mouse __PROTO((struct gp_event_t * ge));
#if (0)	/* Not currently used */
static char *builtin_zoom_scroll_left __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_scroll_right __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_scroll_up __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_scroll_down __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_in_X __PROTO((struct gp_event_t * ge));
static char *builtin_zoom_out_X __PROTO((struct gp_event_t * ge));
#endif

/* prototypes for bind stuff
 * which are used only here. */
static void bind_install_default_bindings __PROTO((void));
static void bind_clear __PROTO((bind_t * b));
static int lookup_key __PROTO((char *ptr, int *len));
static int bind_scan_lhs __PROTO((bind_t * out, const char *in));
static char *bind_fmt_lhs __PROTO((const bind_t * in));
static int bind_matches __PROTO((const bind_t * a, const bind_t * b));
static void bind_display_one __PROTO((bind_t * ptr));
static void bind_display __PROTO((char *lhs));
static void bind_all __PROTO((char *lhs));
static void bind_remove __PROTO((bind_t * b));
static void bind_append __PROTO((char *lhs, char *rhs, char *(*builtin) (struct gp_event_t * ge)));
static void recalc_ruler_pos __PROTO((void));
static void turn_ruler_off __PROTO((void));
static int nearest_label_tag __PROTO((int x, int y, struct termentry * t));
static void remove_label __PROTO((int x, int y));
static void put_label __PROTO((char *label, double x, double y));

/********* functions ********************************************/

/* produce a beep */
static void
alert()
{
# ifdef OS2
    DosBeep(444, 111);
# else
#  ifdef HAVE_LIBREADLINE
#    if !defined(MISSING_RL_DING)
        rl_ding();
#    endif
    fflush(rl_outstream);
#  else
    fprintf(stderr, "\a");
#  endif
# endif
}

/* always include the prototype. The prototype might even not be
 * declared if the system supports stpcpy(). E.g. on Linux I would
 * have to define __USE_GNU before including string.h to get the
 * prototype (joze) */
/* HBB 20001109: *BUT* if a prototype is there, this one may easily
 * conflict with it... */
char *stpcpy __PROTO((char *s, const char *p));

# ifndef HAVE_STPCPY
/* handy function for composing strings; note: some platforms may
 * already provide it, how should we handle that? autoconf? -- ptdb */
char *
stpcpy(char *s, const char *p)
{
    strcpy(s, p);
    return s + strlen(p);
}
# endif


/* main job of transformation, which is not device dependent
*/
static void
MousePosToGraphPosReal(int xx, int yy, double *x, double *y, double *x2, double *y2)
{
    if (!is_3d_plot) {
	if (plot_bounds.xright == plot_bounds.xleft)
	    *x = *x2 = VERYLARGE;	/* protection */
	else {
	    *x = AXIS_MAPBACK(FIRST_X_AXIS, xx);
	    *x2 = AXIS_MAPBACK(SECOND_X_AXIS, xx);
	}
	if (plot_bounds.ytop == plot_bounds.ybot)
	    *y = *y2 = VERYLARGE;	/* protection */
	else {
	    *y = AXIS_MAPBACK(FIRST_Y_AXIS, yy);
	    *y2 = AXIS_MAPBACK(SECOND_Y_AXIS, yy);
	}
	FPRINTF((stderr, "POS: xx=%i, yy=%i  =>  x=%g  y=%g\n", xx, yy, *x, *y));

    } else {
	/* for 3D plots, we treat the mouse position as if it is
	 * in the bottom plane, i.e., the plane of the x and y axis */
	/* note: at present, this projection is only correct if
	 * surface_rot_z is a multiple of 90 degrees! */
	/* HBB 20010522: added protection against division by zero
	 * for cases like 'set view 90,0' */
	xx -= axis3d_o_x;
	yy -= axis3d_o_y;
	if (abs(axis3d_x_dx) > abs(axis3d_x_dy)) {
	    *x = axis_array[FIRST_X_AXIS].min
		+ ((double) xx) / axis3d_x_dx * (axis_array[FIRST_X_AXIS].max -
						 axis_array[FIRST_X_AXIS].min);
	} else if (axis3d_x_dy != 0) {
	    *x = axis_array[FIRST_X_AXIS].min
		+ ((double) yy) / axis3d_x_dy * (axis_array[FIRST_X_AXIS].max -
						 axis_array[FIRST_X_AXIS].min);
	} else {
	    /* both diffs are zero (x axis points into the screen */
	    *x = VERYLARGE;
	}

	if (abs(axis3d_y_dx) > abs(axis3d_y_dy)) {
	    *y = axis_array[FIRST_Y_AXIS].min
		+ ((double) xx) / axis3d_y_dx * (axis_array[FIRST_Y_AXIS].max -
						 axis_array[FIRST_Y_AXIS].min);
	} else if (axis3d_y_dy != 0) {
	    if (splot_map) 
		*y = axis_array[FIRST_Y_AXIS].max
		    + ((double) yy) / axis3d_y_dy * (axis_array[FIRST_Y_AXIS].min -
						     axis_array[FIRST_Y_AXIS].max);
	    else
		*y = axis_array[FIRST_Y_AXIS].min
		    + ((double) yy) / axis3d_y_dy * (axis_array[FIRST_Y_AXIS].max -
						     axis_array[FIRST_Y_AXIS].min);
	} else {
	    /* both diffs are zero (y axis points into the screen */
	    *y = VERYLARGE;
	}

	*x2 = *y2 = VERYLARGE;	/* protection */
    }
    /*
       Note: there is plot_bounds.xleft+0.5 in "#define map_x" in graphics.c, which
       makes no major impact here. It seems that the mistake of the real
       coordinate is at about 0.5%, which corresponds to the screen resolution.
       It would be better to round the distance to this resolution, and thus
       *x = xmin + rounded-to-screen-resolution (xdistance)
     */

    /* Now take into account possible log scales of x and y axes */
    *x = AXIS_DE_LOG_VALUE(FIRST_X_AXIS, *x);
    *y = AXIS_DE_LOG_VALUE(FIRST_Y_AXIS, *y);
    if (!is_3d_plot) {
	*x2 = AXIS_DE_LOG_VALUE(SECOND_X_AXIS, *x2);
	*y2 = AXIS_DE_LOG_VALUE(SECOND_Y_AXIS, *y2);
    }

    /* If x2 is linked to x via a mapping function, apply it now */
    /* Similarly for y/y2 */
    if (!is_3d_plot) {
	if (axis_array[SECOND_X_AXIS].linked_to_primary
	&&  axis_array[SECOND_X_AXIS].link_udf->at)
	    *x2 = eval_link_function(SECOND_X_AXIS, *x);
	if (axis_array[SECOND_Y_AXIS].linked_to_primary
	&&  axis_array[SECOND_Y_AXIS].link_udf->at)
	    *y2 = eval_link_function(SECOND_Y_AXIS, *y);
    }
}

static char *
xy_format()
{
    static char format[64];
    format[0] = NUL;
    strncat(format, mouse_setting.fmt, 30);
    strncat(format, ", ", 2);
    strncat(format, mouse_setting.fmt, 30);
    return format;
}

static char *
zoombox_format()
{
    static char format[64];
    format[0] = NUL;
    strncat(format, mouse_setting.fmt, 30);
    strncat(format, "\r", 2);
    strncat(format, mouse_setting.fmt, 30);
    return format;
}

/* formats the information for an annotation (middle mouse button clicked)
 */
static char *
GetAnnotateString(char *s, double x, double y, int mode, char *fmt)
{
    if (axis_array[FIRST_X_AXIS].datatype == DT_DMS
    ||  axis_array[FIRST_Y_AXIS].datatype == DT_DMS) {
	static char dms_format[16];
	sprintf(dms_format, "%%D%s%%.2m'", degree_sign);
	if (axis_array[FIRST_X_AXIS].datatype == DT_DMS)
	    gstrdms(s, fmt ? fmt : dms_format, x);
	else
	    sprintf(s, mouse_setting.fmt, x);
	strcat(s,", ");
	s += strlen(s);
	if (axis_array[FIRST_Y_AXIS].datatype == DT_DMS)
	    gstrdms(s, fmt ? fmt : dms_format, y);
	else
	    sprintf(s, mouse_setting.fmt, y);
	s += strlen(s);
    } else if (mode == MOUSE_COORDINATES_XDATE || mode == MOUSE_COORDINATES_XTIME || mode == MOUSE_COORDINATES_XDATETIME || mode == MOUSE_COORDINATES_TIMEFMT) {	/* time is on the x axis */
	char buf[0xff];
	char format[0xff] = "[%s, ";
	strcat(format, mouse_setting.fmt);
	strcat(format, "]");
	sprintf(s, format, xDateTimeFormat(x, buf, mode), y);
    } else if (mode == MOUSE_COORDINATES_FRACTIONAL) {
	double xrange = axis_array[FIRST_X_AXIS].max - axis_array[FIRST_X_AXIS].min;
	double yrange = axis_array[FIRST_Y_AXIS].max - axis_array[FIRST_Y_AXIS].min;
	/* calculate fractional coordinates.
	 * prevent division by zero */
	if (xrange) {
	    char format[0xff] = "/";
	    strcat(format, mouse_setting.fmt);
	    sprintf(s, format, (x - axis_array[FIRST_X_AXIS].min) / xrange);
	} else {
	    sprintf(s, "/(undefined)");
	}
	s += strlen(s);
	if (yrange) {
	    char format[0xff] = ", ";
	    strcat(format, mouse_setting.fmt);
	    strcat(format, "/");
	    sprintf(s, format, (y - axis_array[FIRST_Y_AXIS].min) / yrange);
	} else {
	    sprintf(s, ", (undefined)/");
	}
    } else if (mode == MOUSE_COORDINATES_REAL1) {
	sprintf(s, xy_format(), x, y);	/* w/o brackets */
    } else if (mode == MOUSE_COORDINATES_ALT && (fmt || polar)) {
	if (polar) {
	    double phi, r;
	    double rmin = (R_AXIS.autoscale & AUTOSCALE_MIN) ? 0.0 : R_AXIS.set_min;
	    phi = atan2(y,x);
	    if (R_AXIS.log)
		r = AXIS_UNDO_LOG(POLAR_AXIS, x/cos(phi) + AXIS_DO_LOG(POLAR_AXIS, rmin));
	    else
		r = x/cos(phi) + rmin;
	    if (fmt)
		sprintf(s, fmt, phi/ang2rad, r);
	    else {
		sprintf(s, "polar: ");
		s += strlen(s);
		sprintf(s, xy_format(), phi/ang2rad, r);
	    }
	} else {
	    sprintf(s, fmt, x, y);	/* user defined format */
	}
    } else {
	sprintf(s, xy_format(), x, y);	/* usual x,y values */
    }
    return s + strlen(s);
}


/* Format x according to the date/time mouse mode. Uses and returns b as
   a buffer
 */
static char *
xDateTimeFormat(double x, char *b, int mode)
{
    struct tm tm;

    switch (mode) {
    case MOUSE_COORDINATES_XDATE:
	ggmtime(&tm, x);
	sprintf(b, "%d. %d. %04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year);
	break;
    case MOUSE_COORDINATES_XTIME:
	ggmtime(&tm, x);
	sprintf(b, "%d:%02d", tm.tm_hour, tm.tm_min);
	break;
    case MOUSE_COORDINATES_XDATETIME:
	ggmtime(&tm, x);
	sprintf(b, "%d. %d. %04d %d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year,
		tm.tm_hour, tm.tm_min);
	break;
    case MOUSE_COORDINATES_TIMEFMT:
	/* FIXME HBB 20000507: timefmt is for *reading* timedata, not
	 * for writing them! */
	gstrftime(b, 0xff, timefmt, x);
	break;
    default:
	sprintf(b, mouse_setting.fmt, x);
    }
    return b;
}


/* HBB 20000507: fixed a construction error. Was using the 'timefmt'
 * string (which is for reading, not writing time data) to output the
 * value. Code is now closer to what setup_tics does. */
#define MKSTR(sp,x,axis)					\
do {								\
    if (x >= VERYLARGE)	break;					\
    if (axis_array[axis].datatype == DT_TIMEDATE) {		\
	char *format = copy_or_invent_formatstring(axis);	\
	while (strchr(format,'\n'))				\
	     *(strchr(format,'\n')) = ' ';			\
	gstrftime(sp, 40, format, x);				\
    } else {							\
	sprintf(sp, mouse_setting.fmt ,x);			\
    }								\
    sp += strlen(sp);						\
} while (0)


/* ratio for log, distance for linear */
# define DIST(x,rx,axis)			\
   (axis_array[axis].log)			\
    ? ( (rx==0) ? 99999 : x / rx )		\
    : (x - rx)


/* formats the ruler information (position, distance,...) into string p
	(it must be sufficiently long)
   x, y is the current mouse position in real coords (for the calculation
	of distance)
*/
static void
GetRulerString(char *p, double x, double y)
{
    double dx, dy;

    char format[0xff] = "  ruler: [";
    strcat(format, mouse_setting.fmt);
    strcat(format, ", ");
    strcat(format, mouse_setting.fmt);
    strcat(format, "]  distance: ");
    strcat(format, mouse_setting.fmt);
    strcat(format, ", ");
    strcat(format, mouse_setting.fmt);

    dx = DIST(x, ruler.x, FIRST_X_AXIS);
    dy = DIST(y, ruler.y, FIRST_Y_AXIS);
    sprintf(p, format, ruler.x, ruler.y, dx, dy);

    /* Previously, the following "if" let the polar coordinates to be shown only
       for lin-lin plots:
	    if (mouse_setting.polardistance && !axis_array[FIRST_X_AXIS].log && !axis_array[FIRST_Y_AXIS].log) ...
       Now, let us support also semilog and log-log plots.
       Values of mouse_setting.polardistance are:
	    0 (no polar coordinates), 1 (polar coordinates), 2 (tangent instead of angle).
    */
    if (mouse_setting.polardistance) {
	double rho, phi, rx, ry;
	char ptmp[69];
	x = AXIS_LOG_VALUE(FIRST_X_AXIS, x);
	y = AXIS_LOG_VALUE(FIRST_Y_AXIS, y);
	rx = AXIS_LOG_VALUE(FIRST_X_AXIS, ruler.x);
	ry = AXIS_LOG_VALUE(FIRST_Y_AXIS, ruler.y);
	format[0] = '\0';
	strcat(format, " (");
	strcat(format, mouse_setting.fmt);
	rho = sqrt((x - rx) * (x - rx) + (y - ry) * (y - ry)); /* distance */
	if (mouse_setting.polardistance == 1) { /* (distance, angle) */
	    phi = (180 / M_PI) * atan2(y - ry, x - rx);
# ifdef OS2
	    strcat(format, ";% #.4gø)");
# else
	    strcat(format, ", % #.4gdeg)");
# endif
	} else { /* mouse_setting.polardistance==2: (distance, tangent) */
	    phi = x - rx;
	    phi = (phi == 0) ? ((y-ry>0) ? VERYLARGE : -VERYLARGE) : (y - ry)/phi;
	    sprintf(format+strlen(format), ", tangent=%s)", mouse_setting.fmt);
	}
	sprintf(ptmp, format, rho, phi);
	strcat(p, ptmp);
    }
}


static struct t_zoom *zoom_head = NULL, *zoom_now = NULL;
static AXIS *axis_array_copy = NULL;

/* Applies the zoom rectangle of  z  by sending the appropriate command
   to gnuplot
*/

static void
apply_zoom(struct t_zoom *z)
{
    char s[1024];		/* HBB 20011005: made larger */
    int is_splot_map = (is_3d_plot && (splot_map == TRUE));

    if (zoom_now != NULL) {	/* remember the current zoom */
	zoom_now->xmin = axis_array[FIRST_X_AXIS].set_min;
	zoom_now->xmax = axis_array[FIRST_X_AXIS].set_max;
	zoom_now->x2min = axis_array[SECOND_X_AXIS].set_min;
	zoom_now->x2max = axis_array[SECOND_X_AXIS].set_max;
	zoom_now->ymin = axis_array[FIRST_Y_AXIS].set_min;
	zoom_now->ymax = axis_array[FIRST_Y_AXIS].set_max;
	zoom_now->y2min = axis_array[SECOND_Y_AXIS].set_min;
	zoom_now->y2max = axis_array[SECOND_Y_AXIS].set_max;
    }

    /* EAM DEBUG - The autoscale save/restore was too complicated, and
     * broke refresh. Just save the complete axis state and have done with it.
     */
    if (zoom_now == zoom_head && z != zoom_head) {
	axis_array_copy = gp_realloc( axis_array_copy, sizeof(axis_array), "axis_array copy");
	memcpy(axis_array_copy, axis_array, sizeof(axis_array));
    }

    /* If we are zooming, we don't want to autoscale the range.
     * This wasn't necessary before we introduced "refresh".  Why?
     */
    if (zoom_now == zoom_head && z != zoom_head) {
	axis_array[FIRST_X_AXIS].autoscale = AUTOSCALE_NONE;
	axis_array[FIRST_Y_AXIS].autoscale = AUTOSCALE_NONE;
	axis_array[SECOND_X_AXIS].autoscale = AUTOSCALE_NONE;
	axis_array[SECOND_Y_AXIS].autoscale = AUTOSCALE_NONE;
    }

    zoom_now = z;
    if (zoom_now == NULL) {
	alert();
	return;
    }

    /* Now we're committed. Notify the terminal the the next replot is a zoom */
    (*term->layer)(TERM_LAYER_BEFORE_ZOOM);

    sprintf(s, "set xr[%.12g:%.12g]; set yr[%.12g:%.12g]",
	       zoom_now->xmin, zoom_now->xmax, 
	       zoom_now->ymin, zoom_now->ymax);

    /* EAM Apr 2013 - The tests on VERYLARGE protect against trying to   */
    /* interpret the autoscaling initial state as an actual limit value. */
    if (!is_3d_plot
    && (zoom_now->x2min < VERYLARGE && zoom_now->x2max > -VERYLARGE)) {
	sprintf(s + strlen(s), "; set x2r[% #g:% #g]",
		zoom_now->x2min, zoom_now->x2max);
    }
    if (!is_3d_plot
    && (zoom_now->y2min < VERYLARGE && zoom_now->y2max > -VERYLARGE)) {
	sprintf(s + strlen(s), "; set y2r[% #g:% #g]",
		zoom_now->y2min, zoom_now->y2max);
    }

    /* EAM Jun 2007 - The autoscale save/restore was too complicated, and broke
     * refresh. Just save/restore the complete axis state and have done with it.
     * Well, not _quite_ the complete state.  The labels are maintained dynamically.
     */
    if (zoom_now == zoom_head) {
	int i;
	for (i=0; i<AXIS_ARRAY_SIZE; i++) {
	    axis_array_copy[i].label = axis_array[i].label;
	    axis_array_copy[i].ticdef.def.user = axis_array[i].ticdef.def.user;
	    axis_array_copy[i].ticdef.font = axis_array[i].ticdef.font;
	}
	memcpy(axis_array, axis_array_copy, sizeof(axis_array));
	s[0] = '\0';	/* FIXME:  Is this better than calling replotrequest()? */

	/* Falling through to do_string_replot() does not work! */
	if (volatile_data) {
	    if (refresh_ok == E_REFRESH_OK_2D) {
		refresh_request();
		return;
	    }
	    if (is_splot_map && (refresh_ok == E_REFRESH_OK_3D)) {
		refresh_request();
		return;
	    }
	}

    } else {
	inside_zoom = TRUE;
    }

    do_string_replot(s);
    inside_zoom = FALSE;
}


/* makes a zoom: update zoom history, call gnuplot to set ranges + replot
*/

static void
do_zoom(double xmin, double ymin, double x2min, double y2min, double xmax, double ymax, double x2max, double y2max)
{
    struct t_zoom *z;
    if (zoom_head == NULL) {	/* queue not yet created, thus make its head */
	zoom_head = gp_alloc(sizeof(struct t_zoom), "mouse zoom history head");
	zoom_head->prev = NULL;
	zoom_head->next = NULL;
    }
    if (zoom_now == NULL)
	zoom_now = zoom_head;
    if (zoom_now->next == NULL) {	/* allocate new item */
	z = gp_alloc(sizeof(struct t_zoom), "mouse zoom history element");
	z->prev = zoom_now;
	z->next = NULL;
	zoom_now->next = z;
	z->prev = zoom_now;
    } else			/* overwrite next item */
	z = zoom_now->next;

#define SET_AXIS(axis, name, minmax, condition)                         \
    z->name ## minmax = (axis_array[axis].minmax condition) ? name ## minmax : axis_array[axis].minmax

    SET_AXIS(FIRST_X_AXIS,  x,  min, < VERYLARGE);
    SET_AXIS(FIRST_Y_AXIS,  y,  min, < VERYLARGE);
    SET_AXIS(SECOND_X_AXIS, x2, min, < VERYLARGE);
    SET_AXIS(SECOND_Y_AXIS, y2, min, < VERYLARGE);

    SET_AXIS(FIRST_X_AXIS,  x,  max, > -VERYLARGE);
    SET_AXIS(FIRST_Y_AXIS,  y,  max, > -VERYLARGE);
    SET_AXIS(SECOND_X_AXIS, x2, max, > -VERYLARGE);
    SET_AXIS(SECOND_Y_AXIS, y2, max, > -VERYLARGE);

#undef SET_AXIS

    apply_zoom(z);
}


static void
ZoomNext()
{
    if (zoom_now == NULL || zoom_now->next == NULL)
	alert();
    else
	apply_zoom(zoom_now->next);
    if (display_ipc_commands()) {
	fprintf(stderr, "next zoom.\n");
    }
}

static void
ZoomPrevious()
{
    if (zoom_now == NULL || zoom_now->prev == NULL)
	alert();
    else
	apply_zoom(zoom_now->prev);
    if (display_ipc_commands()) {
	fprintf(stderr, "previous zoom.\n");
    }
}

static void
ZoomUnzoom()
{
    if (zoom_head == NULL || zoom_now == zoom_head)
	alert();
    else
	apply_zoom(zoom_head);
    if (display_ipc_commands()) {
	fprintf(stderr, "unzoom.\n");
    }
}

static void
incr_mousemode(const int amount)
{
    long int old = mouse_mode;
    mouse_mode += amount;
    if (MOUSE_COORDINATES_ALT == mouse_mode && !(mouse_alt_string || polar))
	mouse_mode += amount;	/* stepping over */
    if (mouse_mode > MOUSE_COORDINATES_ALT) {
	mouse_mode = MOUSE_COORDINATES_REAL1;
    } else if (mouse_mode <= MOUSE_COORDINATES_REAL) {
	mouse_mode = MOUSE_COORDINATES_ALT;
	if (!(mouse_alt_string || polar))
	    mouse_mode--;	/* stepping over */
    }
    UpdateStatusline();
    if (display_ipc_commands())
	fprintf(stderr, "switched mouse format from %ld to %ld\n", old, mouse_mode);
}

# define TICS_ON(ti) (((ti)&TICS_MASK)!=NO_TICS)

void
UpdateStatusline()
{
    UpdateStatuslineWithMouseSetting(&mouse_setting);
}

static void
UpdateStatuslineWithMouseSetting(mouse_setting_t * ms)
{
    char s0[256], *sp;

/* This suppresses mouse coordinate update after a ^C, but I think
 * that the relevant terminals do their own checks anyhow so we
 * we can just let the ones that care silently skip the update
 * while the ones that don't care keep on updating as usual.
 */
#if (0)
    if (!term_initialised)
	return;
#endif

    if (!ms->on) {
	s0[0] = 0;
    } else if (!ALMOST2D) {
	char format[0xff];
	format[0] = '\0';
	strcat(format, "view: ");
	strcat(format, ms->fmt);
	strcat(format, ", ");
	strcat(format, ms->fmt);
	strcat(format, "   scale: ");
	strcat(format, ms->fmt);
	strcat(format, ", ");
	strcat(format, ms->fmt);
	sprintf(s0, format, surface_rot_x, surface_rot_z, surface_scale, surface_zscale);
    } else if (!TICS_ON(axis_array[SECOND_X_AXIS].ticmode) && !TICS_ON(axis_array[SECOND_Y_AXIS].ticmode)) {
	/* only first X and Y axis are in use */
	sp = GetAnnotateString(s0, real_x, real_y, mouse_mode, mouse_alt_string);
	if (ruler.on) {
	    GetRulerString(sp, real_x, real_y);
	}
    } else {
	/* X2 and/or Y2 are in use: use more verbose format */
	sp = s0;
	if (TICS_ON(axis_array[FIRST_X_AXIS].ticmode)) {
	    sp = stpcpy(sp, "x=");
	    MKSTR(sp, real_x, FIRST_X_AXIS);
	    *sp++ = ' ';
	}
	if (TICS_ON(axis_array[FIRST_Y_AXIS].ticmode)) {
	    sp = stpcpy(sp, "y=");
	    MKSTR(sp, real_y, FIRST_Y_AXIS);
	    *sp++ = ' ';
	}
	if (TICS_ON(axis_array[SECOND_X_AXIS].ticmode)) {
	    sp = stpcpy(sp, "x2=");
	    MKSTR(sp, real_x2, SECOND_X_AXIS);
	    *sp++ = ' ';
	}
	if (TICS_ON(axis_array[SECOND_Y_AXIS].ticmode)) {
	    sp = stpcpy(sp, "y2=");
	    MKSTR(sp, real_y2, SECOND_Y_AXIS);
	    *sp++ = ' ';
	}
	if (ruler.on) {
	    /* ruler on? then also print distances to ruler */
	    if (TICS_ON(axis_array[FIRST_X_AXIS].ticmode)) {
		stpcpy(sp,"dx=");
		sprintf(sp+3, mouse_setting.fmt, DIST(real_x, ruler.x, FIRST_X_AXIS));
		sp += strlen(sp);
	    }
	    if (TICS_ON(axis_array[FIRST_Y_AXIS].ticmode)) {
		stpcpy(sp,"dy=");
		sprintf(sp+3, mouse_setting.fmt, DIST(real_y, ruler.y, FIRST_Y_AXIS));
		sp += strlen(sp);
	    }
	    if (TICS_ON(axis_array[SECOND_X_AXIS].ticmode)) {
		stpcpy(sp,"dx2=");
		sprintf(sp+4, mouse_setting.fmt, DIST(real_x2, ruler.x2, SECOND_X_AXIS));
		sp += strlen(sp);
	    }
	    if (TICS_ON(axis_array[SECOND_Y_AXIS].ticmode)) {
		stpcpy(sp,"dy2=");
		sprintf(sp+4, mouse_setting.fmt, DIST(real_y2, ruler.y2, SECOND_Y_AXIS));
		sp += strlen(sp);
	    }
	}
	*--sp = 0;		/* delete trailing space */
    }
    if (term->put_tmptext) {
	(term->put_tmptext) (0, s0);
    }
}
#undef MKSTR


void
recalc_statusline()
{
    MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);
    UpdateStatusline();
}

/****************** handlers for user's actions ******************/

static char *
builtin_autoscale(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-autoscale` (set autoscale keepfix; replot)";
    }
    do_string_replot("set autoscale keepfix");
    return (char *) 0;
}

static char *
builtin_toggle_border(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-toggle-border`";

    /* EAM July 2009  Cycle through border settings
     * - no border
     * - last border requested by the user
     * - default border
     * - (3D only) full border
     */
    if (draw_border == 0 && draw_border != user_border)
	draw_border = user_border;
    else if (draw_border == user_border && draw_border != 31)
	draw_border = 31;
    else if (is_3d_plot && draw_border == 31)
	draw_border = 4095;
    else
	draw_border = 0;

    do_string_replot("");
    return (char *) 0;
}

static char *
builtin_replot(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-replot`";
    }
    do_string_replot("");
    return (char *) 0;
}

static char *
builtin_toggle_grid(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-grid`";
    }
    if (! some_grid_selected())
	do_string_replot("set grid");
    else
	do_string_replot("unset grid");
    return (char *) 0;
}

static char *
builtin_help(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-help`";
    }
    fprintf(stderr, "\n");
    bind_display((char *) 0);	/* display all bindings */
    restore_prompt();
    return (char *) 0;
}

static char *
builtin_set_plots_visible(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-set-plots-visible`";
    }
    if (term->modify_plots)
	term->modify_plots(MODPLOTS_SET_VISIBLE, -1);
    return (char *) 0;
}

static char *
builtin_set_plots_invisible(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-set-plots-invisible`";
    }
    if (term->modify_plots)
	term->modify_plots(MODPLOTS_SET_INVISIBLE, -1);
    return (char *) 0;
}

static char *
builtin_invert_plot_visibilities(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-invert-plot-visibilities`";
    }
    if (term->modify_plots)
	term->modify_plots(MODPLOTS_INVERT_VISIBILITIES, -1);
    return (char *) 0;
}

static char *
builtin_toggle_log(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-toggle-log` y logscale for plots, z and cb for splots";

    if (volatile_data)
	int_warn(NO_CARET, "Cannot toggle log scale for volatile data");
    else if ((color_box.bounds.xleft < mouse_x && mouse_x < color_box.bounds.xright)
	 &&  (color_box.bounds.ybot  < mouse_y && mouse_y < color_box.bounds.ytop))
	do_string_replot( CB_AXIS.log ? "unset log cb" : "set log cb");
    else if (is_3d_plot && !splot_map)
	do_string_replot( Z_AXIS.log ? "unset log z" : "set log z");
    else
	do_string_replot( axis_array[FIRST_Y_AXIS].log ? "unset log y" : "set log y");

    return (char *) 0;
}

static char *
builtin_nearest_log(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-nearest-log` toggle logscale of axis nearest cursor";

    if ((color_box.bounds.xleft < mouse_x && mouse_x < color_box.bounds.xright)
    &&  (color_box.bounds.ybot  < mouse_y && mouse_y < color_box.bounds.ytop)) {
	do_string_replot( CB_AXIS.log ? "unset log cb" : "set log cb");
    } else if (is_3d_plot && !splot_map) {
	do_string_replot( Z_AXIS.log ? "unset log z" : "set log z");
    } else {
	/* 2D-plot: figure out which axis/axes is/are
	 * close to the mouse cursor, and toggle those lin/log */
	/* note: here it is assumed that the x axis is at
	 * the bottom, x2 at top, y left and y2 right; it
	 * would be better to derive that from the ..tics settings */
	TBOOLEAN change_x1 = FALSE;
	TBOOLEAN change_y1 = FALSE;
	TBOOLEAN change_x2 = FALSE;
	TBOOLEAN change_y2 = FALSE;
	if (mouse_y < plot_bounds.ybot + (plot_bounds.ytop - plot_bounds.ybot) / 4
	&&  mouse_x > plot_bounds.xleft && mouse_x < plot_bounds.xright)
	    change_x1 = TRUE;
	if (mouse_x < plot_bounds.xleft + (plot_bounds.xright - plot_bounds.xleft) / 4
	&&  mouse_y > plot_bounds.ybot && mouse_y < plot_bounds.ytop)
	    change_y1 = TRUE;
	if (mouse_y > plot_bounds.ytop - (plot_bounds.ytop - plot_bounds.ybot) / 4
	&&  mouse_x > plot_bounds.xleft && mouse_x < plot_bounds.xright)
	    change_x2 = TRUE;
	if (mouse_x > plot_bounds.xright - (plot_bounds.xright - plot_bounds.xleft) / 4
	&&  mouse_y > plot_bounds.ybot && mouse_y < plot_bounds.ytop)
	    change_y2 = TRUE;
	
	if (change_x1)
	    do_string(axis_array[FIRST_X_AXIS].log ? "unset log x" : "set log x");
	if (change_y1)
	    do_string(axis_array[FIRST_Y_AXIS].log ? "unset log y" : "set log y");
	if (change_x2 && !splot_map)
	    do_string(axis_array[SECOND_X_AXIS].log ? "unset log x2" : "set log x2");
	if (change_y2 && !splot_map)
	    do_string(axis_array[SECOND_Y_AXIS].log ? "unset log y2" : "set log y2");
	if (!change_x1 && !change_y1 && splot_map)
	    do_string_replot( Z_AXIS.log ? "unset log z" : "set log z");
	
	if (change_x1 || change_y1 || change_x2 || change_y2)
	    do_string_replot("");
    }

    return (char *) 0;
}

static char *
builtin_toggle_mouse(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-mouse`";
    }
    if (!mouse_setting.on) {
	mouse_setting.on = 1;
	if (display_ipc_commands()) {
	    fprintf(stderr, "turning mouse on.\n");
	}
    } else {
	mouse_setting.on = 0;
	if (display_ipc_commands()) {
	    fprintf(stderr, "turning mouse off.\n");
	}
    }
# ifdef OS2
    PM_update_menu_items();
# endif
    UpdateStatusline();
    return (char *) 0;
}

static char *
builtin_toggle_ruler(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-ruler`";
    }
    if (!term->set_ruler)
	return (char *) 0;
    if (ruler.on) {
	turn_ruler_off();
	if (display_ipc_commands())
	    fprintf(stderr, "turning ruler off.\n");
    } else if (ALMOST2D) {
	/* only allow ruler, if the plot
	 * is 2d or a 3d `map' */
	struct udvt_entry *u;
	ruler.on = TRUE;
	ruler.px = ge->mx;
	ruler.py = ge->my;
	MousePosToGraphPosReal(ruler.px, ruler.py, &ruler.x, &ruler.y, &ruler.x2, &ruler.y2);
	(*term->set_ruler) (ruler.px, ruler.py);
	if ((u = add_udv_by_name("MOUSE_RULER_X"))) {
	    u->udv_undef = FALSE;
	    Gcomplex(&u->udv_value,ruler.x,0);
	}
	if ((u = add_udv_by_name("MOUSE_RULER_Y"))) {
	    u->udv_undef = FALSE;
	    Gcomplex(&u->udv_value,ruler.y,0);
	}
	if (display_ipc_commands()) {
	    fprintf(stderr, "turning ruler on.\n");
	}
    }
    UpdateStatusline();
    return (char *) 0;
}

static char *
builtin_decrement_mousemode(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-previous-mouse-format`";
    }
    incr_mousemode(-1);
    return (char *) 0;
}

static char *
builtin_increment_mousemode(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-next-mouse-format`";
    }
    incr_mousemode(1);
    return (char *) 0;
}

static char *
builtin_toggle_polardistance(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-polardistance`";
    }
    if (++mouse_setting.polardistance > 2) mouse_setting.polardistance = 0;
	/* values: 0 (no polar coordinates), 1 (polar coordinates), 2 (tangent instead of angle) */
    term->set_cursor((mouse_setting.polardistance ? -3:-4), ge->mx, ge->my); /* change cursor type */
# ifdef OS2
    PM_update_menu_items();
# endif
    UpdateStatusline();
    if (display_ipc_commands()) {
	fprintf(stderr, "distance to ruler will %s be shown in polar coordinates.\n", mouse_setting.polardistance ? "" : "not");
    }
    return (char *) 0;
}

static char *
builtin_toggle_verbose(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-verbose`";
    }
    /* this is tricky as the command itself modifies
     * the state of display_ipc_commands() */
    if (display_ipc_commands()) {
	fprintf(stderr, "echoing of communication commands is turned off.\n");
    }
    toggle_display_of_ipc_commands();
    if (display_ipc_commands()) {
	fprintf(stderr, "communication commands will be echoed.\n");
    }
    return (char *) 0;
}

static char *
builtin_toggle_ratio(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-toggle-ratio`";
    }
    if (aspect_ratio == 0)
	do_string_replot("set size ratio -1");
    else if (aspect_ratio == 1)
	do_string_replot("set size nosquare");
    else
	do_string_replot("set size square");
    return (char *) 0;
}

static char *
builtin_zoom_next(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-zoom-next` go to next zoom in the zoom stack";
    }
    ZoomNext();
    return (char *) 0;
}

static char *
builtin_zoom_previous(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-zoom-previous` go to previous zoom in the zoom stack";
    }
    ZoomPrevious();
    return (char *) 0;
}

static char *
builtin_unzoom(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-unzoom`";
    }
    ZoomUnzoom();
    return (char *) 0;
}

static char *
builtin_rotate_right(struct gp_event_t *ge)
{
    if (!ge)
	return "`scroll right in 2d, rotate right in 3d`; <Shift> faster";
    if (is_3d_plot)
	ChangeView(0, -1);
    else {
	int k = (modifier_mask & Mod_Shift) ? 3 : 1;
	while (k-- > 0)
	    do_zoom_scroll_right();
    }
    return (char *) 0;
}

static char *
builtin_rotate_left(struct gp_event_t *ge)
{
    if (!ge)
	return "`scroll left in 2d, rotate left in 3d`; <Shift> faster";
    if (is_3d_plot)
	ChangeView(0, 1);
    else {
	int k = (modifier_mask & Mod_Shift) ? 3 : 1;
	while (k-- > 0)
	    do_zoom_scroll_left();
    }
    return (char *) 0;
}

static char *
builtin_rotate_up(struct gp_event_t *ge)
{
    if (!ge)
	return "`scroll up in 2d, rotate up in 3d`; <Shift> faster";
    if (is_3d_plot)
	ChangeView(1, 0);
    else {
	int k = (modifier_mask & Mod_Shift) ? 3 : 1;
	while (k-- > 0)
	    do_zoom_scroll_up();
    }
    return (char *) 0;
}

static char *
builtin_rotate_down(struct gp_event_t *ge)
{
    if (!ge)
	return "`scroll down in 2d, rotate down in 3d`; <Shift> faster";
    if (is_3d_plot)
	ChangeView(-1, 0);
    else {
	int k = (modifier_mask & Mod_Shift) ? 3 : 1;
	while (k-- > 0)
	    do_zoom_scroll_down();
    }
    return (char *) 0;
}

static char *
builtin_cancel_zoom(struct gp_event_t *ge)
{
    if (!ge) {
	return "`builtin-cancel-zoom` cancel zoom region";
    }
    if (!setting_zoom_region)
	return (char *) 0;
    if (term->set_cursor)
	term->set_cursor(0, 0, 0);
    setting_zoom_region = FALSE;
    if (display_ipc_commands()) {
	fprintf(stderr, "zooming cancelled.\n");
    }
    return (char *) 0;
}

static void
event_keypress(struct gp_event_t *ge, TBOOLEAN current)
{
    int x, y;
    int c, par2;
    bind_t *ptr;
    bind_t keypress;

    c = ge->par1;
    par2 = ge->par2;
    x = ge->mx;
    y = ge->my;

    if (!bindings) {
	bind_install_default_bindings();
    }

    if ((modifier_mask & Mod_Shift) && ((c & 0xff) == 0)) {
	c = toupper(c);
    }

    bind_clear(&keypress);
    keypress.key = c;
    keypress.modifier = modifier_mask;

    /*
     * On 'pause mouse keypress' in active window export current keypress 
     * and mouse coords to user variables. A key with 'bind all' terminates 
     * a pause even from non-active windows.
     * Ignore NULL keypress.
     *
     * If we are paused for a keystroke, this takes precendence over normal
     * key bindings. Otherwise, for example typing 'm' would turn off mousing,
     * which is a bad thing if you are in the  middle of a mousing operation.
     */

    if ((paused_for_mouse & PAUSE_KEYSTROKE) && (c > '\0') && current) {
	load_mouse_variables(x, y, FALSE, c);
	return;
    }

    for (ptr = bindings; ptr; ptr = ptr->next) {
	if (bind_matches(&keypress, ptr)) {
	    struct udvt_entry *keywin;
	    if ((keywin = add_udv_by_name("MOUSE_KEY_WINDOW"))) {
		keywin->udv_undef = FALSE;
		Ginteger(&keywin->udv_value, ge->winid);
	    }
	    /* Always honor keys set with "bind all" */
	    if (ptr->allwindows && ptr->command) {
		if (current)
		    load_mouse_variables(x, y, FALSE, c);
		else
		    /* FIXME - Better to clear MOUSE_[XY] than to set it wrongly. */
		    /*         This may be worth a separate subroutine.           */
		    load_mouse_variables(0, 0, FALSE, c);
		do_string(ptr->command);
		/* Treat as a current event after we return to x11.trm */
		ge->type = GE_keypress;
		break;
	    /* But otherwise ignore inactive windows */
	    } else if (!current) {
		break;
	    /* Let user defined bindings overwrite the builtin bindings */
	    } else if ((par2 & 1) == 0 && ptr->command) {
		do_string(ptr->command);
		break;
	    } else if (ptr->builtin) {
		ptr->builtin(ge);
	    } else {
		fprintf(stderr, "%s:%d protocol error\n", __FILE__, __LINE__);
	    }
	}
    }

}


static void
ChangeView(int x, int z)
{
    if (modifier_mask & Mod_Shift) {
	x *= 10;
	z *= 10;
    }

    if (x) {
	surface_rot_x += x;
	if (surface_rot_x < 0)
	    surface_rot_x += 360;
	if (surface_rot_x > 360)
	    surface_rot_x -= 360;
    }
    if (z) {
	surface_rot_z += z;
	if (surface_rot_z < 0)
	    surface_rot_z += 360;
	if (surface_rot_z > 360)
	    surface_rot_z -= 360;
    }

    if (x || z) {
	fill_gpval_float("GPVAL_VIEW_ROT_X", surface_rot_x);
	fill_gpval_float("GPVAL_VIEW_ROT_Z", surface_rot_z);
    }

    if (display_ipc_commands()) {
	fprintf(stderr, "changing view to %f, %f.\n", surface_rot_x, surface_rot_z);
    }

    do_save_3dplot(first_3dplot, plot3d_num, 0 /* not quick */ );

    if (ALMOST2D) {
	/* 2D plot, or suitably aligned 3D plot: update statusline */
	if (!term->put_tmptext)
	    return;
	recalc_statusline();
    }
}

int is_mouse_outside_plot(void)
{
    // Here I look at both min/max each time because reversed ranges can make
    // min > max
#define CHECK_AXIS_OUTSIDE(real, axis)                                  \
    ( axis_array[axis].min <  VERYLARGE &&                              \
      axis_array[axis].max > -VERYLARGE &&                              \
      ( (real < AXIS_DE_LOG_VALUE(axis, axis_array[axis].min) &&        \
         real < AXIS_DE_LOG_VALUE(axis, axis_array[axis].max)) ||       \
        (real > AXIS_DE_LOG_VALUE(axis, axis_array[axis].min) &&        \
         real > AXIS_DE_LOG_VALUE(axis, axis_array[axis].max))))

    return
        CHECK_AXIS_OUTSIDE(real_x,  FIRST_X_AXIS)  ||
        CHECK_AXIS_OUTSIDE(real_y,  FIRST_Y_AXIS)  ||
        CHECK_AXIS_OUTSIDE(real_x2, SECOND_X_AXIS) ||
        CHECK_AXIS_OUTSIDE(real_y2, SECOND_Y_AXIS);

#undef CHECK_AXIS_OUTSIDE
}

/* Return a new (upper or lower) axis limit that is a linear
   combination of the current limits.  */
static double
rescale(int AXIS, double w1, double w2) 
{
  double logval, val;
  logval = w1*axis_array[AXIS].min+w2*axis_array[AXIS].max;
  val = AXIS_DE_LOG_VALUE(AXIS,logval);
  return val;
}


/* Rescale axes and do zoom. */
static void
zoom_rescale_xyx2y2(double a0,double a1,double a2,double a3,double a4,double a5,double a6,
	double a7,double a8,double a9,double a10,double a11,double a12,double a13,double a14,double a15,
	char msg[])
{
    double xmin  = rescale(FIRST_X_AXIS,   a0, a1);
    double ymin  = rescale(FIRST_Y_AXIS,   a2, a3);
    double x2min = rescale(SECOND_X_AXIS,  a4, a5);
    double y2min = rescale(SECOND_Y_AXIS,  a6, a7);

    double xmax  = rescale(FIRST_X_AXIS,   a8, a9);
    double ymax  = rescale(FIRST_Y_AXIS,  a10, a11);
    double x2max = rescale(SECOND_X_AXIS, a12, a13);
    double y2max = rescale(SECOND_Y_AXIS, a14, a15);
    do_zoom(xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max);

    if (msg[0] && display_ipc_commands()) {
	fputs(msg, stderr); fputs("\n", stderr);
    }
}


/* Scroll left. */
static void
do_zoom_scroll_left()
{
    zoom_rescale_xyx2y2(1.1, -0.1,
                        1,   0,
                        1.1, -0.1,
                        1,   0,
                        0.1, 0.9,
                        0,   1,
                        0.1, 0.9,
                        0,   1,
                        "scroll left.\n");
}

/* Scroll right. */
static void
do_zoom_scroll_right()
{
    zoom_rescale_xyx2y2(0.9,  0.1,
                        1,    0,
                        0.9,  0.1,
                        1,    0,
                        -0.1, 1.1,
                        0,    1,
                        -0.1, 1.1,
                        0,    1,
                        "scroll right");
}

/* Scroll up. */
static void
do_zoom_scroll_up()
{
    zoom_rescale_xyx2y2(1,    0,
                        0.9,  0.1,
                        1,    0,
                        0.9,  0.1,
                        0,    1,
                        -0.1, 1.1,
                        0,    1,
                        -0.1, 1.1,
                        "scroll up");
}

/* Scroll down. */
static void
do_zoom_scroll_down()
{
    zoom_rescale_xyx2y2(1,   0,
                        1.1, -0.1,
                        1,   0,
                        1.1, -0.1,
                        0,   1,
                        0.1, 0.9,
                        0,   1,
                        0.1, 0.9,
                        "scroll down");
}


/* Return new lower and upper axis limits as current limits resized
   around current mouse position. */
static void
rescale_around_mouse(double *newmin, double *newmax, int AXIS, double mouse_pos, double scale)
{
  double unlog_pos = AXIS_LOG_VALUE(AXIS, mouse_pos);

  *newmin = unlog_pos + (axis_array[AXIS].min - unlog_pos) * scale;
  *newmin = AXIS_DE_LOG_VALUE(AXIS,*newmin);

  *newmax = unlog_pos + (axis_array[AXIS].max - unlog_pos) * scale;
  *newmax = AXIS_DE_LOG_VALUE(AXIS,*newmax);
}


/* Zoom in/out within x-axis. */
static void
zoom_in_X(int zoom_key)
{
    // I don't check for "outside" here. will do that later
    if (is_mouse_outside_plot()) {
        /* zoom in (X axis only) */
        double w1 = (zoom_key=='+') ? 23./25. : 23./21.;
        double w2 = (zoom_key=='+') ?  2./25. : -2./21.;
        zoom_rescale_xyx2y2(w1,w2, 1,0, w1,w2, 1,0,  w2,w1, 0,1, w2,w1, 0,1, 
                            (zoom_key=='+' ? "zoom in X" : "zoom out X"));
    } else {
        double xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max;
	double scale = (zoom_key=='+') ? 0.75 : 1.25;
	rescale_around_mouse(&xmin,  &xmax,  FIRST_X_AXIS,  real_x,  scale);
	rescale_around_mouse(&x2min, &x2max, SECOND_X_AXIS, real_x2, scale);

        ymin  = rescale(FIRST_Y_AXIS,  1,0);
        y2min = rescale(SECOND_Y_AXIS, 1,0);
        ymax  = rescale(FIRST_Y_AXIS,  0,1);
        y2max = rescale(SECOND_Y_AXIS, 0,1);
        do_zoom(xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max);
    }
}

static void
do_zoom_in_X()
{
    zoom_in_X('+');
}

static void
do_zoom_out_X()
{
    zoom_in_X('-');
}


/* Zoom around mouse cursor unless the cursor is outside the graph boundary,
   when it scales around the graph center.
   Syntax: zoom_key == '+' ... zoom in, zoom_key == '-' ... zoom out
*/
static void
zoom_around_mouse(int zoom_key)
{
    double xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max;
    if (is_mouse_outside_plot()) {
	/* zoom in (factor of approximately 2^(.25), so four steps gives 2x larger) */
	double w1 = (zoom_key=='+') ? 23./25. : 23./21.;
	double w2 = (zoom_key=='+') ?  2./25. : -2./21.;
	xmin  = rescale(FIRST_X_AXIS,  w1, w2);
	ymin  = rescale(FIRST_Y_AXIS,  w1, w2);
	x2min = rescale(SECOND_X_AXIS, w1, w2);
	y2min = rescale(SECOND_Y_AXIS, w1, w2);

	xmax  = rescale(FIRST_X_AXIS,  w2, w1);
	ymax  = rescale(FIRST_Y_AXIS,  w2, w1);
	x2max = rescale(SECOND_X_AXIS, w2, w1);
      	y2max = rescale(SECOND_Y_AXIS, w2, w1);
    } else {
	int zsign = (zoom_key=='+') ? -1 : 1;
	double xscale = pow(1.25, zsign * mouse_setting.xmzoom_factor);
	double yscale = pow(1.25, zsign * mouse_setting.ymzoom_factor);
	/* {x,y}zoom_factor = 0: not zoom, = 1: 0.8/1.25 zoom */
	rescale_around_mouse(&xmin,  &xmax,  FIRST_X_AXIS,  real_x,  xscale);
	rescale_around_mouse(&ymin,  &ymax,  FIRST_Y_AXIS,  real_y,  yscale);
	rescale_around_mouse(&x2min, &x2max, SECOND_X_AXIS, real_x2, xscale);
	rescale_around_mouse(&y2min, &y2max, SECOND_Y_AXIS, real_y2, yscale);
    }
    do_zoom(xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max);
    if (display_ipc_commands())
	fprintf(stderr, "zoom %s.\n", (zoom_key=='+' ? "in" : "out"));
}

static void
do_zoom_in_around_mouse()
{
    zoom_around_mouse('+');
}

static void
do_zoom_out_around_mouse()
{
    zoom_around_mouse('-');
}

static char *
builtin_zoom_in_around_mouse(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-in` zoom in";
    do_zoom_in_around_mouse();
    return (char *) 0;
}

static char *
builtin_zoom_out_around_mouse(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-out` zoom out";
    do_zoom_out_around_mouse();
    return (char *) 0;
}


#if (0) /* Not currently used */
static char *
builtin_zoom_scroll_left(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-scroll-left` scroll left";
    do_zoom_scroll_left();
    return (char *) 0;
}

static char *
builtin_zoom_scroll_right(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-scroll-right` scroll right";
    do_zoom_scroll_right();
    return (char *) 0;
}

static char *
builtin_zoom_scroll_up(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-scroll-up` scroll up";
    do_zoom_scroll_up();
    return (char *) 0;
}

static char *
builtin_zoom_scroll_down(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-scroll-down` scroll down";
    do_zoom_scroll_down();
    return (char *) 0;
}

static char *
builtin_zoom_in_X(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-in-X` zoom in X axis";
    do_zoom_in_X();
    return (char *) 0;
}

static char *
builtin_zoom_out_X(struct gp_event_t *ge)
{
    if (!ge)
	return "`builtin-zoom-out-X` zoom out X axis";
    do_zoom_out_X();
    return (char *) 0;
}
#endif /* Not currently used */

static void
event_buttonpress(struct gp_event_t *ge)
{
    int b;

    motion = 0;

    b = ge->par1;
    mouse_x = ge->mx;
    mouse_y = ge->my;

    button |= (1 << b);

    FPRINTF((stderr, "(event_buttonpress) mouse_x = %d\tmouse_y = %d\n", mouse_x, mouse_y));

    MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);


    if ((b == 4 || b == 6) && /* 4 - wheel up, 6 - wheel left */
	(!replot_disabled || (E_REFRESH_NOT_OK != refresh_ok))	/* Use refresh if available */
	&& !(paused_for_mouse & PAUSE_BUTTON3)) {

	/* Ctrl+Shift+wheel up or Squeeze (not implemented) */
	if ((modifier_mask & Mod_Ctrl) && (modifier_mask & Mod_Shift))
	    do_zoom_in_X();
	
	/* Ctrl+wheel up or Ctrl+stroke */
	else if ((modifier_mask & Mod_Ctrl))
	    do_zoom_in_around_mouse();

	/* Horizontal stroke (button 6) or Shift+wheel up */
	else if (b == 6 || (modifier_mask & Mod_Shift)) 
	    do_zoom_scroll_left();
	
	/* Wheel up (no modifier keys) */
	else
	    do_zoom_scroll_up();
	
    } else if (((b == 5) || (b == 7)) && /* 5 - wheel down, 7 - wheel right */
	       (!replot_disabled || (E_REFRESH_NOT_OK != refresh_ok))	/* Use refresh if available */
	       && !(paused_for_mouse & PAUSE_BUTTON3)) {

	/* Ctrl+Shift+wheel down or Unsqueeze (not implemented) */
	if ((modifier_mask & Mod_Ctrl) && (modifier_mask & Mod_Shift))
	    do_zoom_out_X();
	
	/* Ctrl+wheel down or Ctrl+stroke */
	else if ((modifier_mask & Mod_Ctrl))
	    do_zoom_out_around_mouse();

	/* Horizontal stroke (button 7) or Shift+wheel down */
	else if (b == 7 || (modifier_mask & Mod_Shift)) 
	    do_zoom_scroll_right();
	
	/* Wheel down (no modifier keys) */
	else
	    do_zoom_scroll_down();
	
    } else if (ALMOST2D) {
        if (!setting_zoom_region) {
	    if (1 == b) {
		/* "pause button1" or "pause any" takes precedence over key bindings */
		if (paused_for_mouse & PAUSE_BUTTON1) {
		    load_mouse_variables(mouse_x, mouse_y, TRUE, b);
		    trap_release = TRUE;	/* Don't trigger on release also */
		    return;
		}
	    } else if (2 == b) {
		/* not bound in 2d graphs */
	    } else if (3 == b && 
	    	(!replot_disabled || (E_REFRESH_NOT_OK != refresh_ok))	/* Use refresh if available */
		&& !(paused_for_mouse & PAUSE_BUTTON3)) {
		/* start zoom; but ignore it when
		 *   - replot is disabled, e.g. with inline data, or
		 *   - during 'pause mouse'
		 * allow zooming during 'pause mouse key' */
		setting_zoom_x = mouse_x;
		setting_zoom_y = mouse_y;
		setting_zoom_region = TRUE;
		if (term->set_cursor) {
		    int mv_mouse_x, mv_mouse_y;
		    if (mouse_setting.annotate_zoom_box && term->put_tmptext) {
			double real_x, real_y, real_x2, real_y2;
			char s[64];
			/* tell driver annotations */
			MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);
			sprintf(s, zoombox_format(), real_x, real_y);
			term->put_tmptext(1, s);
			term->put_tmptext(2, s);
		    }
		    /* displace mouse in order not to start with an empty zoom box */
		    mv_mouse_x = term->xmax / 20;
		    mv_mouse_y = (term->xmax == term->ymax) ? mv_mouse_x : (int) ((mv_mouse_x * (double) term->ymax) / term->xmax);
		    mv_mouse_x += mouse_x;
		    mv_mouse_y += mouse_y;

		    /* change cursor type */
		    term->set_cursor(3, 0, 0);

		    /* warp pointer */
		    if (mouse_setting.warp_pointer)
			term->set_cursor(-2, mv_mouse_x, mv_mouse_y);

		    /* turn on the zoom box */
		    term->set_cursor(-1, setting_zoom_x, setting_zoom_y);
		}
		if (display_ipc_commands()) {
		    fprintf(stderr, "starting zoom region.\n");
		}
	    }
	} else {

	    /* complete zoom (any button
	     * finishes zooming.) */

	    /* the following variables are used to check,
	     * if the box is big enough to be considered
	     * as zoom box. */
	    int dist_x = setting_zoom_x - mouse_x;
	    int dist_y = setting_zoom_y - mouse_y;
	    int dist = sqrt((double)(dist_x * dist_x + dist_y * dist_y));

	    if (1 == b || 2 == b) {
		/* zoom region is finished by the `wrong' button.
		 * `trap' the next button-release event so that
		 * it won't trigger the actions which are bound
		 * to these events.
		 */
		trap_release = TRUE;
	    }

	    if (term->set_cursor) {
		term->set_cursor(0, 0, 0);
		if (mouse_setting.annotate_zoom_box && term->put_tmptext) {
		    term->put_tmptext(1, "");
		    term->put_tmptext(2, "");
		}
	    }

	    if (dist > 10 /* more ore less arbitrary */ ) {

		double xmin, ymin, x2min, y2min;
		double xmax, ymax, x2max, y2max;

		MousePosToGraphPosReal(setting_zoom_x, setting_zoom_y, &xmin, &ymin, &x2min, &y2min);
		xmax = real_x;
		x2max = real_x2;
		ymax = real_y;
		y2max = real_y2;
		/* keep the axes (no)reversed as they are now */
#define rev(a1,a2,A) if (sgn(a2-a1) != sgn(axis_array[A].max-axis_array[A].min)) \
			    { double tmp = a1; a1 = a2; a2 = tmp; }
		rev(xmin,  xmax,  FIRST_X_AXIS);
		rev(ymin,  ymax,  FIRST_Y_AXIS);
		rev(x2min, x2max, SECOND_X_AXIS);
		rev(y2min, y2max, SECOND_Y_AXIS);
#undef rev
		do_zoom(xmin, ymin, x2min, y2min, xmax, ymax, x2max, y2max);
		if (display_ipc_commands()) {
		    fprintf(stderr, "zoom region finished.\n");
		}
	    } else {
		/* silently ignore a tiny zoom box. This might
		 * happen, if the user starts and finishes the
		 * zoom box at the same position. */
	    }
	    setting_zoom_region = FALSE;
	}
    } else {
	if (term->set_cursor) {
	    if (button & (1 << 1))
		term->set_cursor(1, 0, 0);
	    else if (button & (1 << 2))
		term->set_cursor(2, 0, 0);
	}
    }
    start_x = mouse_x;
    start_y = mouse_y;
    zero_rot_z = surface_rot_z + 360.0 * mouse_x / term->xmax;
    /* zero_rot_x = surface_rot_x - 180.0 * mouse_y / term->ymax; */
    zero_rot_x = surface_rot_x - 360.0 * mouse_y / term->ymax;
}


static void
event_buttonrelease(struct gp_event_t *ge)
{
    int b, doubleclick;

    b = ge->par1;
    mouse_x = ge->mx;
    mouse_y = ge->my;
    doubleclick = ge->par2;

    button &= ~(1 << b);	/* remove button */

    if (setting_zoom_region) {
	return;
    }
    if (TRUE == trap_release) {
	trap_release = FALSE;
	return;
    }

    MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);

    FPRINTF((stderr, "MOUSE.C: doublclick=%i, set=%i, motion=%i, ALMOST2D=%i\n", (int) doubleclick, (int) mouse_setting.doubleclick,
	     (int) motion, (int) ALMOST2D));

    if (ALMOST2D) {
	char s0[256];
	if (b == 1 && term->set_clipboard && ((doubleclick <= mouse_setting.doubleclick)
					      || !mouse_setting.doubleclick)) {

	    /* put coordinates to clipboard. For 3d plots this takes
	     * only place, if the user didn't drag (rotate) the plot */

	    if (!is_3d_plot || !motion) {
		GetAnnotateString(s0, real_x, real_y, mouse_mode, mouse_alt_string);
		term->set_clipboard(s0);
		if (display_ipc_commands()) {
		    fprintf(stderr, "put `%s' to clipboard.\n", s0);
		}
	    }
	}
	if (b == 2) {

	    /* draw temporary annotation or label. For 3d plots this is
	     * only done if the user didn't drag (scale) the plot */

	    if (!is_3d_plot || !motion) {

		GetAnnotateString(s0, real_x, real_y, mouse_mode, mouse_alt_string);
		if (mouse_setting.label) {
		    if (modifier_mask & Mod_Ctrl) {
			remove_label(mouse_x, mouse_y);
		    } else {
			put_label(s0, real_x, real_y);
		    }
		} else {
		    int dx, dy;
		    int x = mouse_x;
		    int y = mouse_y;
		    dx = term->h_tic;
		    dy = term->v_tic;
		    (term->linewidth) (border_lp.l_width);
		    (term->linetype) (border_lp.l_type);
		    (term->move) (x - dx, y);
		    (term->vector) (x + dx, y);
		    (term->move) (x, y - dy);
		    (term->vector) (x, y + dy);
		    (term->justify_text) (LEFT);
		    (term->put_text) (x + dx / 2, y + dy / 2 + term->v_char / 3, s0);
		    (term->text) ();
		}
	    }
	}
    }
    if (is_3d_plot && (b == 1 || b == 2)) {
	if (!!(modifier_mask & Mod_Ctrl) && !needreplot) {
	    /* redraw the 3d plot if its last redraw was 'quick'
	     * (only axes) because modifier key was pressed */
	    do_save_3dplot(first_3dplot, plot3d_num, 0);
	}
	if (term->set_cursor)
	    term->set_cursor(0, 0, 0);
    }

    /* Export current mouse coords to user-accessible variables also */
    load_mouse_variables(mouse_x, mouse_y, TRUE, b);
    UpdateStatusline();

    /* In 2D mouse button 1 is available for "bind" commands */
    if (!is_3d_plot && (b == 1)) {
	int save = ge->par1;
	ge->par1 = GP_Button1;
	ge->par2 = 0;
	event_keypress(ge, TRUE);
	ge->par1 = save;	/* needed for "pause mouse" */
    }
}


static void
event_motion(struct gp_event_t *ge)
{
    motion = 1;

    mouse_x = ge->mx;
    mouse_y = ge->my;

    if (is_3d_plot
	&& (splot_map == FALSE) /* Rotate the surface if it is 3D graph but not "set view map". */
	) {

	TBOOLEAN redraw = FALSE;

	if (button & (1 << 1)) {
	    /* dragging with button 1 -> rotate */
	  /*surface_rot_x = floor(0.5 + zero_rot_x + 180.0 * mouse_y / term->ymax);*/
	    surface_rot_x = floor(0.5 + fmod(zero_rot_x + 360.0 * mouse_y / term->ymax, 360));
	    if (surface_rot_x < 0)
		surface_rot_x += 360;
	    if (surface_rot_x > 360)
		surface_rot_x -= 360;
	    surface_rot_z = floor(0.5 + fmod(zero_rot_z - 360.0 * mouse_x / term->xmax, 360));
	    if (surface_rot_z < 0)
		surface_rot_z += 360;
	    redraw = TRUE;
	} else if (button & (1 << 2)) {
	    /* dragging with button 2 -> scale or changing ticslevel.
	     * we compare the movement in x and y direction, and
	     * change either scale or zscale */
	    double relx, rely;
	    relx = (double)abs(mouse_x - start_x) / (double)term->h_tic;
	    rely = (double)abs(mouse_y - start_y) / (double)term->v_tic;

	    if (modifier_mask & Mod_Shift) {
		xyplane.z += (1 + fabs(xyplane.z))
		    * (mouse_y - start_y) * 2.0 / term->ymax;
	    } else {

		if (relx > rely) {
		    surface_lscale += (mouse_x - start_x) * 2.0 / term->xmax;
		    surface_scale = exp(surface_lscale);
		    if (surface_scale < 0)
			surface_scale = 0;
		} else {
		    if (disable_mouse_z && (mouse_y-start_y > 0))
			;
		    else {
			surface_zscale += (mouse_y - start_y) * 2.0 / term->ymax;
			disable_mouse_z = FALSE;
		    }
		    if (surface_zscale < 0)
			surface_zscale = 0;
		}
	    }
	    /* reset the start values */
	    start_x = mouse_x;
	    start_y = mouse_y;
	    redraw = TRUE;
	} /* if (mousebutton 2 is down) */

	if (!ALMOST2D) {
	    turn_ruler_off();
	}

	if (redraw) {
	    if (allowmotion) {
		/* is processing of motions allowed right now?
		 * then replot while
		 * disabling further replots until it completes */
		allowmotion = FALSE;
		do_save_3dplot(first_3dplot, plot3d_num, !!(modifier_mask & Mod_Ctrl));
		fill_gpval_float("GPVAL_VIEW_ROT_X", surface_rot_x);
		fill_gpval_float("GPVAL_VIEW_ROT_Z", surface_rot_z);
		fill_gpval_float("GPVAL_VIEW_SCALE", surface_scale);
		fill_gpval_float("GPVAL_VIEW_ZSCALE", surface_zscale);
	    } else {
		/* postpone the replotting */
		needreplot = TRUE;
	    }
	}
    } /* if (3D plot) */


    if (ALMOST2D) {
	/* 2D plot, or suitably aligned 3D plot: update
	 * statusline and possibly the zoombox annotation */
	if (!term->put_tmptext)
	    return;
	MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);
	UpdateStatusline();

	if (setting_zoom_region && mouse_setting.annotate_zoom_box) {
	    double real_x, real_y, real_x2, real_y2;
	    char s[64];
	    MousePosToGraphPosReal(mouse_x, mouse_y, &real_x, &real_y, &real_x2, &real_y2);
	    sprintf(s, zoombox_format(), real_x, real_y);
	    term->put_tmptext(2, s);
	}
    }
}


static void
event_modifier(struct gp_event_t *ge)
{
    modifier_mask = ge->par1;

    if (modifier_mask == 0 && is_3d_plot && (button & ((1 << 1) | (1 << 2))) && !needreplot) {
	/* redraw the 3d plot if modifier key released */
	do_save_3dplot(first_3dplot, plot3d_num, 0);
    }
}


void
event_plotdone()
{
    if (needreplot) {
	needreplot = FALSE;
	do_save_3dplot(first_3dplot, plot3d_num, !!(modifier_mask & Mod_Ctrl));
    } else {
	allowmotion = TRUE;
    }
}


void
event_reset(struct gp_event_t *ge)
{
    modifier_mask = 0;
    button = 0;
    builtin_cancel_zoom(ge);
    if (term && term_initialised && term->set_cursor) {
	term->set_cursor(0, 0, 0);
	if (mouse_setting.annotate_zoom_box && term->put_tmptext) {
	    term->put_tmptext(1, "");
	    term->put_tmptext(2, "");
	}
    }

    if (paused_for_mouse) {
	paused_for_mouse = 0;
#ifdef WIN32
	/* close pause message box */
	kill_pending_Pause_dialog();
#endif
	/* This hack is necessary on some systems in order to prevent one  */
	/* character of input from being swallowed when the plot window is */
	/* closed. But which systems, exactly?                             */
	if (term && (!strncmp("x11",term->name,3) 
		 || !strncmp("wxt",term->name,3)
		 || !strncmp("qt",term->name,2)))
	    ungetc('\n',stdin);
    }

    /* Dummy up a keystroke event so that we can conveniently check for a  */
    /* binding to "Close". We only get these for the current window. */
    if (ge != (void *)1) {
	ge->par1 = GP_Cancel;	/* Dummy keystroke */
	ge->par2 = 0;		/* Not used; could pass window id here? */
	event_keypress(ge, TRUE);
    }
}


void
do_event(struct gp_event_t *ge)
{
    if (!term)
	return;

    /* disable `replot` when some data were sent through stdin */
    replot_disabled = plotted_data_from_stdin;

    if (ge->type) {
	FPRINTF((stderr, "(do_event) type       = %d\n", ge->type));
	FPRINTF((stderr, "           mx, my     = %d, %d\n", ge->mx, ge->my));
	FPRINTF((stderr, "           par1, par2 = %d, %d\n", ge->par1, ge->par2));
    }

    switch (ge->type) {
    case GE_plotdone:
	event_plotdone();
	if (ge->winid) {
	    current_x11_windowid = ge->winid;
	    update_gpval_variables(6); /* fill GPVAL_TERM_WINDOWID */
	}
	break;
    case GE_keypress:
	event_keypress(ge, TRUE);
	break;
    case GE_keypress_old:
	event_keypress(ge, FALSE);
	break;
    case GE_modifier:
	event_modifier(ge);
	break;
    case GE_motion:
	if (!mouse_setting.on)
	    break;
	event_motion(ge);
	break;
    case GE_buttonpress:
	if (!mouse_setting.on)
	    break;
	event_buttonpress(ge);
	break;
    case GE_buttonrelease:
	if (!mouse_setting.on)
	    break;
	event_buttonrelease(ge);
	break;
    case GE_replot:
	/* auto-generated replot (e.g. from replot-on-resize) */
	/* FIXME: more terminals should use this! */
	if (replot_line == NULL || replot_line[0] == '\0')
	    break;
	if (!strncmp(replot_line,"test",4))
	    break;
	if (multiplot)
	    break;
	do_string_replot("");
	break;
    case GE_reset:
	event_reset(ge);
	break;
    case GE_fontprops:
#ifdef X11
	/* EAM FIXME:  Despite the name, only X11 uses this to pass font info.	*/
	/* Everyone else passes just the plot height and width.			*/
	if (!strcmp(term->name,"x11")) {
	    /* These are declared in ../term/x11.trm */
	    extern int          X11_hchar_saved, X11_vchar_saved;
	    extern double       X11_ymax_saved;

	    /* Cached sizing values for the x11 terminal. Each time an X11 window is
	       resized, these are updated with the new sizes. When a replot happens some
	       time later, these saved values are used. The normal mechanism for doing this
	       is sending a QG from inboard to outboard driver, then the outboard driver
	       responds with the sizing info in a GE_fontprops event. The problem is that
	       other than during plot initialization the communication is asynchronous.
	    */
	    X11_hchar_saved = ge->par1;
	    X11_vchar_saved = ge->par2;
	    X11_ymax_saved = (double)term->xmax * (double)ge->my / fabs((double)ge->mx);

	    /* If mx < 0, we simply save the values for future use, and move on */
	    if (ge->mx < 0) {
		break;
	    } else {
	    /* Otherwise we apply the changes right now */
		term->h_char = X11_hchar_saved;
		term->v_char = X11_vchar_saved;
		/* factor of 2.5 must match the use in x11.trm */
		term->h_tic = term->v_tic = X11_vchar_saved / 2.5;
		term->ymax  = X11_ymax_saved;
	    }
	} else
	    /* Fall through to cover non-x11 case */
#endif
	/* Other terminals update aspect ratio based on current window size */
	    term->v_tic = term->h_tic * (double)ge->mx / (double)ge->my;

	FPRINTF((stderr, "mouse do_event: window size %d X %d, font hchar %d vchar %d\n",
		ge->mx, ge->my, ge->par1,ge->par2));
	break;
    case GE_buttonpress_old:
    case GE_buttonrelease_old:
	/* ignore */
	break;
    default:
	fprintf(stderr, "%s:%d protocol error\n", __FILE__, __LINE__);
	break;
    }

    replot_disabled = FALSE;	/* enable replot again */
}

static void
do_save_3dplot(struct surface_points *plots, int pcount, int quick)
{
#define M_TEST_AXIS(A) \
     (A.log && ((!(A.set_autoscale & AUTOSCALE_MIN) && A.set_min <= 0) || \
		(!(A.set_autoscale & AUTOSCALE_MAX) && A.set_max <= 0)))

    if (!plots || (E_REFRESH_NOT_OK == refresh_ok)) {
	/* !plots might happen after the `reset' command for example
	 * (reported by Franz Bakan).
	 * !refresh_ok can happen for example if log scaling is reset (EAM).
	 * replotrequest() should set up everything again in either case.
	 */
	replotrequest();
    } else {
	if (M_TEST_AXIS(X_AXIS) || M_TEST_AXIS(Y_AXIS) || M_TEST_AXIS(Z_AXIS)
	    || M_TEST_AXIS(CB_AXIS)
	    ) {
		graph_error("axis ranges must be above 0 for log scale!");
		return;
	}
	do_3dplot(plots, pcount, quick);
    }

#undef M_TEST_AXIS
}


/*
 * bind related functions
 */

static void
bind_install_default_bindings()
{
    bind_remove_all();
    bind_append("a", (char *) 0, builtin_autoscale);
    bind_append("b", (char *) 0, builtin_toggle_border);
    bind_append("e", (char *) 0, builtin_replot);
    bind_append("g", (char *) 0, builtin_toggle_grid);
    bind_append("h", (char *) 0, builtin_help);
    bind_append("i", (char *) 0, builtin_invert_plot_visibilities);
    bind_append("l", (char *) 0, builtin_toggle_log);
    bind_append("L", (char *) 0, builtin_nearest_log);
    bind_append("m", (char *) 0, builtin_toggle_mouse);
    bind_append("r", (char *) 0, builtin_toggle_ruler);
    bind_append("V", (char *) 0, builtin_set_plots_invisible);
    bind_append("v", (char *) 0, builtin_set_plots_visible);
    bind_append("1", (char *) 0, builtin_decrement_mousemode);
    bind_append("2", (char *) 0, builtin_increment_mousemode);
    bind_append("5", (char *) 0, builtin_toggle_polardistance);
    bind_append("6", (char *) 0, builtin_toggle_verbose);
    bind_append("7", (char *) 0, builtin_toggle_ratio);
    bind_append("n", (char *) 0, builtin_zoom_next);
    bind_append("p", (char *) 0, builtin_zoom_previous);
    bind_append("u", (char *) 0, builtin_unzoom);
    bind_append("+", (char *) 0, builtin_zoom_in_around_mouse);
    bind_append("=", (char *) 0, builtin_zoom_in_around_mouse); /* same key as + but no need for Shift */
    bind_append("-", (char *) 0, builtin_zoom_out_around_mouse);
    bind_append("Right", (char *) 0, builtin_rotate_right);
    bind_append("Up", (char *) 0, builtin_rotate_up);
    bind_append("Left", (char *) 0, builtin_rotate_left);
    bind_append("Down", (char *) 0, builtin_rotate_down);
    bind_append("Escape", (char *) 0, builtin_cancel_zoom);
}

static void
bind_clear(bind_t * b)
{
    b->key = NO_KEY;
    b->modifier = 0;
    b->command = (char *) 0;
    b->builtin = 0;
    b->prev = (struct bind_t *) 0;
    b->next = (struct bind_t *) 0;
}

/* returns the enum which corresponds to the
 * string (ptr) or NO_KEY if ptr matches not
 * any of special_keys. */
static int
lookup_key(char *ptr, int *len)
{
    char **keyptr;
    /* first, search in the table of "usual well-known" keys */
    int what = lookup_table_nth(usual_special_keys, ptr);
    if (what >= 0) {
	*len = strlen(usual_special_keys[what].key);
	return usual_special_keys[what].value;
    }
    /* second, search in the table of other keys */
    for (keyptr = special_keys; *keyptr; ++keyptr) {
	if (!strcmp(ptr, *keyptr)) {
	    *len = strlen(ptr);
	    return keyptr - special_keys + GP_FIRST_KEY;
	}
    }
    return NO_KEY;
}

/* returns 1 on success, else 0. */
static int
bind_scan_lhs(bind_t * out, const char *in)
{
    static const char DELIM = '-';
    int itmp = NO_KEY;
    char *ptr;
    int len;
    bind_clear(out);
    if (!in) {
	return 0;
    }
    for (ptr = (char *) in; ptr && *ptr; /* EMPTY */ ) {
	if (!strncasecmp(ptr, "alt-", 4)) {
	    out->modifier |= Mod_Alt;
	    ptr += 4;
	} else if (!strncasecmp(ptr, "ctrl-", 5)) {
	    out->modifier |= Mod_Ctrl;
	    ptr += 5;
	} else if (NO_KEY != (itmp = lookup_key(ptr, &len))) {
	    out->key = itmp;
	    ptr += len;
	} else if ((out->key = *ptr++) && *ptr && *ptr != DELIM) {
	    fprintf(stderr, "bind: cannot parse %s\n", in);
	    return 0;
	}
    }
    if (NO_KEY == out->key)
	return 0;		/* failed */
    else
	return 1;		/* success */
}

/* note, that this returns a pointer
 * to the static char* `out' which is
 * modified on subsequent calls.
 */
static char *
bind_fmt_lhs(const bind_t * in)
{
    static char out[0x40];
    out[0] = '\0';		/* empty string */
    if (!in)
	return out;
    if (in->modifier & Mod_Ctrl) {
	sprintf(out, "Ctrl-");
    }
    if (in->modifier & Mod_Alt) {
	strcat(out, "Alt-");
    }
    if (in->key > GP_FIRST_KEY && in->key < GP_LAST_KEY) {
	strcat(out,special_keys[in->key - GP_FIRST_KEY]);
    } else {
	int k = 0;
	for ( ; usual_special_keys[k].value > 0; k++) {
	    if (usual_special_keys[k].value == in->key) {
		strcat(out, usual_special_keys[k].key);
		k = -1;
		break;
	    }
	}
	if (k >= 0) {
	    char foo[2] = {'\0','\0'};
	    foo[0] = in->key;
	    strcat(out,foo);
	}
    }
    return out;
}

static int
bind_matches(const bind_t * a, const bind_t * b)
{
    /* discard Shift modifier */
    int a_mod = a->modifier & (Mod_Ctrl | Mod_Alt);
    int b_mod = b->modifier & (Mod_Ctrl | Mod_Alt);

    if (a->key == b->key && a_mod == b_mod)
	return 1;
    else
	return 0;
}

static void
bind_display_one(bind_t * ptr)
{
    fprintf(stderr, " %-12s ", bind_fmt_lhs(ptr));
    fprintf(stderr, "%c ", ptr->allwindows ? '*' : ' ');
    if (ptr->command) {
	fprintf(stderr, "`%s`\n", ptr->command);
    } else if (ptr->builtin) {
	fprintf(stderr, "%s\n", ptr->builtin(0));
    } else {
	fprintf(stderr, "`%s:%d oops.'\n", __FILE__, __LINE__);
    }
}

static void
bind_display(char *lhs)
{
    bind_t *ptr;
    bind_t lhs_scanned;

    if (!bindings) {
	bind_install_default_bindings();
    }

    if (!lhs) {
	/* display all bindings */
	char fmt[] = " %-17s  %s\n";
	fprintf(stderr, "\n");
	/* mouse buttons */
	fprintf(stderr, fmt, "<B1> doubleclick", "send mouse coordinates to clipboard (pm win wxt x11)");
	fprintf(stderr, fmt, "<B2>", "annotate the graph using `mouseformat` (see keys '1', '2')");
	fprintf(stderr, fmt, "", "or draw labels if `set mouse labels is on`");
	fprintf(stderr, fmt, "<Ctrl-B2>", "remove label close to pointer if `set mouse labels` is on");
	fprintf(stderr, fmt, "<B3>", "mark zoom region (only for 2d-plots and maps)");
	fprintf(stderr, fmt, "<B1-Motion>", "change view (rotation); use <Ctrl> to rotate the axes only");
	fprintf(stderr, fmt, "<B2-Motion>", "change view (scaling); use <Ctrl> to scale the axes only");
	fprintf(stderr, fmt, "<Shift-B2-Motion>", "vertical motion -- change xyplane");

	/* mouse wheel */
	fprintf(stderr, fmt, "<wheel-up>", "  scroll up (in +Y direction)");
	fprintf(stderr, fmt, "<wheel-down>", "  scroll down");
	fprintf(stderr, fmt, "<shift-wheel-up>", "  scroll left (in -X direction)");
	fprintf(stderr, fmt, "<shift-wheel-down>", " scroll right");
	fprintf(stderr, fmt, "<Control-WheelUp>", "  zoom in on mouse position");
	fprintf(stderr, fmt, "<Control-WheelDown>", "zoom out on mouse position");
	fprintf(stderr, fmt, "<Shift-Control-WheelUp>", "  pinch on x");
	fprintf(stderr, fmt, "<Shift-Control-WheelDown>", "expand on x");

	fprintf(stderr, "\n");
	/* keystrokes */
#ifndef DISABLE_SPACE_RAISES_CONSOLE
	fprintf(stderr, " %-12s   %s\n", "Space", "raise gnuplot console window");
#endif
	fprintf(stderr, " %-12s * %s\n", "q", "close this plot window");
	fprintf(stderr, "\n");
	for (ptr = bindings; ptr; ptr = ptr->next) {
	    bind_display_one(ptr);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "              * indicates this key is active from all plot windows\n");
	fprintf(stderr, "\n");
	return;
    }

    if (!bind_scan_lhs(&lhs_scanned, lhs)) {
	return;
    }
    for (ptr = bindings; ptr; ptr = ptr->next) {
	if (bind_matches(&lhs_scanned, ptr)) {
	    bind_display_one(ptr);
	    break;		/* only one match */
	}
    }
}

static void
bind_remove(bind_t * b)
{
    if (!b) {
	return;
    } else if (b->builtin) {
	/* don't remove builtins, just remove the overriding command */
	if (b->command) {
	    free(b->command);
	    b->command = (char *) 0;
	}
	return;
    }
    if (b->prev)
	b->prev->next = b->next;
    if (b->next)
	b->next->prev = b->prev;
    else
	bindings->prev = b->prev;
    if (b->command) {
	free(b->command);
	b->command = (char *) 0;
    }
    if (b == bindings) {
	bindings = b->next;
	if (bindings && bindings->prev) {
	    bindings->prev->next = (bind_t *) 0;
	}
    }
    free(b);
}

static void
bind_append(char *lhs, char *rhs, char *(*builtin) (struct gp_event_t * ge))
{
    bind_t *new = (bind_t *) gp_alloc(sizeof(bind_t), "bind_append->new");

    if (!bind_scan_lhs(new, lhs)) {
	free(new);
	return;
    }

    if (!bindings) {
	/* first binding */
	bindings = new;
    } else {

	bind_t *ptr;
	for (ptr = bindings; ptr; ptr = ptr->next) {
	    if (bind_matches(new, ptr)) {
		/* overwriting existing binding */
		if (!rhs) {
		    ptr->builtin = builtin;
		} else if (*rhs) {
		    if (ptr->command) {
			free(ptr->command);
			ptr->command = (char *) 0;
		    }
		    ptr->command = rhs;
		} else {	/* rhs is an empty string, so remove the binding */
		    bind_remove(ptr);
		}
		free(new);	/* don't need it any more */
		return;
	    }
	}
	/* if we're here, the binding does not exist yet */
	/* append binding ... */
	bindings->prev->next = new;
	new->prev = bindings->prev;
    }

    bindings->prev = new;
    new->next = (struct bind_t *) 0;
    new->allwindows = FALSE;	/* Can be explicitly set later */
    if (!rhs) {
	new->builtin = builtin;
    } else if (*rhs) {
	new->command = rhs;	/* was allocated in command.c */
    } else {
	bind_remove(new);
    }
}

void
bind_process(char *lhs, char *rhs, TBOOLEAN allwindows)
{
    if (!bindings) {
	bind_install_default_bindings();
    }
    if (!rhs) {
	bind_display(lhs);
    } else {
	bind_append(lhs, rhs, 0);
	if (allwindows)
	    bind_all(lhs);
    }
    if (lhs)
	free(lhs);
}

void
bind_all(char *lhs)
{
    bind_t *ptr;
    bind_t keypress;

    if (!bind_scan_lhs(&keypress, lhs))
	return;

    for (ptr = bindings; ptr; ptr = ptr->next) {
	if (bind_matches(&keypress, ptr))
	    ptr->allwindows = TRUE;
    }
}

void
bind_remove_all()
{
    bind_t *ptr;
    bind_t *safe;
    for (ptr = bindings; ptr; safe = ptr, ptr = ptr->next, free(safe)) {
	if (ptr->command) {
	    free(ptr->command);
	    ptr->command = (char *) 0;
	}
    }
    bindings = (bind_t *) 0;
}

/* Ruler is on, thus recalc its (px,py) from (x,y) for the current zoom and
   log axes.
*/
static void
recalc_ruler_pos()
{
    double P, dummy;
    if (is_3d_plot) {
	/* To be exact, it is 'set view map' splot. */
	int ppx, ppy;
	dummy = 1.0; /* dummy value, but not 0.0 for the fear of log z-axis */
	map3d_xy(ruler.x, ruler.y, dummy, &ppx, &ppy);
	ruler.px = ppx;
	ruler.py = ppy;
	return;
    }
    /* It is 2D plot. */
    if (axis_array[FIRST_X_AXIS].log && ruler.x < 0)
	ruler.px = -1;
    else {
	P = AXIS_LOG_VALUE(FIRST_X_AXIS, ruler.x);
	ruler.px = AXIS_MAP(FIRST_X_AXIS, P);
    }
    if (axis_array[FIRST_Y_AXIS].log && ruler.y < 0)
	ruler.py = -1;
    else {
	P = AXIS_LOG_VALUE(FIRST_Y_AXIS, ruler.y);
	ruler.py = AXIS_MAP(FIRST_Y_AXIS, P);
    }
    MousePosToGraphPosReal(ruler.px, ruler.py, &dummy, &dummy, &ruler.x2, &ruler.y2);
}


/* Recalculate and replot the ruler after a '(re)plot'. Called from term.c.
*/
void
update_ruler()
{
    if (!term->set_ruler || !ruler.on)
	return;
    (*term->set_ruler) (-1, -1);
    recalc_ruler_pos();
    (*term->set_ruler) (ruler.px, ruler.py);
}

/* Set ruler on/off, and set its position.
   Called from set.c for 'set mouse ruler ...' command.
*/
void
set_ruler(TBOOLEAN on, int mx, int my)
{
    struct gp_event_t ge;
    if (ruler.on == FALSE && on == FALSE)
	return;
    if (ruler.on == TRUE && on == TRUE && (mx < 0 || my < 0))
	return;
    if (ruler.on == TRUE) /* ruler is on => switch it off */
	builtin_toggle_ruler(&ge);
    /* now the ruler is off */
    if (on == FALSE) /* want ruler off */
	return;
    if (mx>=0 && my>=0) { /* change ruler position */
	ge.mx = mx;
	ge.my = my;
    } else { /* don't change ruler position */
	ge.mx = ruler.px;
	ge.my = ruler.py;
    }
    builtin_toggle_ruler(&ge);
}

/* for checking if we change from plot to splot (or vice versa) */
int
plot_mode(int set)
{
    static int mode = MODE_PLOT;
    if (MODE_PLOT == set || MODE_SPLOT == set) {
	if (mode != set) {
	    turn_ruler_off();
	}
	mode = set;
    }
    return mode;
}

static void
turn_ruler_off()
{
    if (ruler.on) {
	struct udvt_entry *u;
	ruler.on = FALSE;
	if (term && term->set_ruler) {
	    (*term->set_ruler) (-1, -1);
	}
	if ((u = add_udv_by_name("MOUSE_RULER_X")))
	    u->udv_undef = TRUE;
	if ((u = add_udv_by_name("MOUSE_RULER_Y")))
	    u->udv_undef = TRUE;
	if (display_ipc_commands()) {
	    fprintf(stderr, "turning ruler off.\n");
	}
    }
}

static int
nearest_label_tag(int xref, int yref, struct termentry *t)
{
    double min = -1;
    int min_tag = -1;
    double diff_squared;
    int x, y;
    struct text_label *this_label;
    int xd;
    int yd;

    for (this_label = first_label; this_label != NULL; this_label = this_label->next) {
	if (is_3d_plot) {
	    map3d_position(&this_label->place, &xd, &yd, "label");
	    xd -= xref;
	    yd -= yref;
	} else {
	    map_position(&this_label->place, &x, &y, "label");
	    xd = x - xref;
	    yd = y - yref;
	}
	diff_squared = xd * xd + yd * yd;
	if (-1 == min || min > diff_squared) {
	    /* now we check if we're within a certain
	     * threshold around the label */
	    double tic_diff_squared;
	    int htic, vtic;
	    get_offsets(this_label, t, &htic, &vtic);
	    tic_diff_squared = htic * htic + vtic * vtic;
	    if (diff_squared < tic_diff_squared) {
		min = diff_squared;
		min_tag = this_label->tag;
	    }
	}
    }

    return min_tag;
}

static void
remove_label(int x, int y)
{
    int tag = nearest_label_tag(x, y, term);
    if (-1 != tag) {
	char cmd[0x40];
	sprintf(cmd, "unset label %d", tag);
	do_string_replot(cmd);
    }
}

static void
put_label(char *label, double x, double y)
{
    char cmd[0xff];
    sprintf(cmd, "set label \"%s\" at %g,%g %s", label, x, y, 
	mouse_setting.labelopts ? mouse_setting.labelopts : "point pt 1");
    do_string_replot(cmd);
}

#ifdef OS2
/* routine required by pm.trm: fill in information needed for (un)checking
   menu items in the Presentation Manager terminal
*/
void 
PM_set_gpPMmenu __PROTO((struct t_gpPMmenu * gpPMmenu))
{
    gpPMmenu->use_mouse = mouse_setting.on;
    if (zoom_now == NULL)
	gpPMmenu->where_zoom_queue = 0;
    else {
	gpPMmenu->where_zoom_queue = (zoom_now == zoom_head) ? 0 : 1;
	if (zoom_now->prev != NULL)
	    gpPMmenu->where_zoom_queue |= 2;
	if (zoom_now->next != NULL)
	    gpPMmenu->where_zoom_queue |= 4;
    }
    gpPMmenu->polar_distance = mouse_setting.polardistance;
}
#endif

/* Save current mouse position to user-accessible variables.
 * Save the keypress or mouse button that triggered this in MOUSE_KEY,
 * and define MOUSE_BUTTON if it was a button click.
 */
static void
load_mouse_variables(double x, double y, TBOOLEAN button, int c)
{
    struct udvt_entry *current;

    MousePosToGraphPosReal(x, y, &real_x, &real_y, &real_x2, &real_y2);

    if ((current = add_udv_by_name("MOUSE_BUTTON"))) {
	current->udv_undef = !button;
	Ginteger(&current->udv_value, button?c:-1);
    }
    if ((current = add_udv_by_name("MOUSE_KEY"))) {
	current->udv_undef = FALSE;
	Ginteger(&current->udv_value,c);
    }
    if ((current = add_udv_by_name("MOUSE_CHAR"))) {
	char *keychar = gp_alloc(2,"key_char");
	keychar[0] = c;
	keychar[1] = '\0';
	if (!current->udv_undef)
	    gpfree_string(&current->udv_value);
	current->udv_undef = FALSE;
	Gstring(&current->udv_value,keychar);
    }
    if ((current = add_udv_by_name("MOUSE_X"))) {
	current->udv_undef = FALSE;
	Gcomplex(&current->udv_value,real_x,0);
    }
    if ((current = add_udv_by_name("MOUSE_Y"))) {
	current->udv_undef = FALSE;
	Gcomplex(&current->udv_value,real_y,0);
    }
    if ((current = add_udv_by_name("MOUSE_X2"))) {
	current->udv_undef = FALSE;
	Gcomplex(&current->udv_value,real_x2,0);
    }
    if ((current = add_udv_by_name("MOUSE_Y2"))) {
	current->udv_undef = FALSE;
	Gcomplex(&current->udv_value,real_y2,0);
    }
    if ((current = add_udv_by_name("MOUSE_SHIFT"))) {
	current->udv_undef = FALSE;
	Ginteger(&current->udv_value, modifier_mask & Mod_Shift);
    }
    if ((current = add_udv_by_name("MOUSE_ALT"))) {
	current->udv_undef = FALSE;
	Ginteger(&current->udv_value, modifier_mask & Mod_Alt);
    }
    if ((current = add_udv_by_name("MOUSE_CTRL"))) {
	current->udv_undef = FALSE;
	Ginteger(&current->udv_value, modifier_mask & Mod_Ctrl);
    }
    return;
}

#endif /* USE_MOUSE */
