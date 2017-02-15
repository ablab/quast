#ifndef lint
static char *RCSid() { return RCSid("$Id: set.c,v 1.459.2.32 2016/09/15 19:21:15 sfeam Exp $"); }
#endif

/* GNUPLOT - set.c */

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


/*
 * 19 September 1992  Lawrence Crowl  (crowl@cs.orst.edu)
 * Added user-specified bases for log scaling.
 */

#include "setshow.h"

#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "contour.h"
#include "datafile.h"
#include "datablock.h"
#include "fit.h"
#include "gp_hist.h"
#include "gp_time.h"
#include "hidden3d.h"
#include "misc.h"
#include "plot.h"
#include "plot2d.h"
#include "plot3d.h"
#include "tables.h"
#include "tabulate.h"
#include "term_api.h"
#include "util.h"
#include "variable.h"
#include "pm3d.h"
#include "getcolor.h"
#include <ctype.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

static palette_color_mode pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_NONE;

static void set_angles __PROTO((void));
static void set_arrow __PROTO((void));
static int assign_arrow_tag __PROTO((void));
static void set_autoscale __PROTO((void));
static void set_bars __PROTO((void));
static void set_border __PROTO((void));
static void set_boxplot __PROTO((void));
static void set_boxwidth __PROTO((void));
static void set_clabel __PROTO((void));
static void set_clip __PROTO((void));
static void set_cntrparam __PROTO((void));
static void set_cntrlabel __PROTO((void));
static void set_contour __PROTO((void));
static void set_dashtype __PROTO((void));
static void set_dgrid3d __PROTO((void));
static void set_decimalsign __PROTO((void));
static void set_degreesign __PROTO((char *));
static void set_dummy __PROTO((void));
static void set_encoding __PROTO((void));
static void set_fit __PROTO((void));
static void set_grid __PROTO((void));
static void set_hidden3d __PROTO((void));
static void set_history __PROTO((void));
static void set_isosamples __PROTO((void));
static void set_key __PROTO((void));
static void set_label __PROTO((void));
static int assign_label_tag __PROTO((void));
static void set_loadpath __PROTO((void));
static void set_fontpath __PROTO((void));
static void set_locale __PROTO((void));
static void set_logscale __PROTO((void));
static void set_mapping __PROTO((void));
static void set_margin __PROTO((t_position *));
static void set_minus_sign __PROTO((void));
static void set_missing __PROTO((void));
static void set_separator __PROTO((void));
static void set_datafile_commentschars __PROTO((void));
static void set_monochrome __PROTO((void));
#ifdef USE_MOUSE
static void set_mouse __PROTO((void));
#endif
static void set_offsets __PROTO((void));
static void set_origin __PROTO((void));
static void set_output __PROTO((void));
static void set_parametric __PROTO((void));
static void set_pm3d __PROTO((void));
static void set_palette __PROTO((void));
static void set_colorbox __PROTO((void));
static void set_pointsize __PROTO((void));
static void set_pointintervalbox __PROTO((void));
static void set_polar __PROTO((void));
static void set_print __PROTO((void));
#ifdef EAM_OBJECTS
static void set_object __PROTO((void));
static void set_obj __PROTO((int, int));
#endif
static void set_psdir __PROTO((void));
static void set_samples __PROTO((void));
static void set_size __PROTO((void));
static void set_style __PROTO((void));
static void set_surface __PROTO((void));
static void set_table __PROTO((void));
static void set_terminal __PROTO((void));
static void set_termoptions __PROTO((void));
static void set_tics __PROTO((void));
static void set_ticscale __PROTO((void));
static void set_timefmt __PROTO((void));
static void set_timestamp __PROTO((void));
static void set_view __PROTO((void));
static void set_zero __PROTO((void));
static void set_timedata __PROTO((AXIS_INDEX));
static void set_range __PROTO((AXIS_INDEX));
static void set_paxis __PROTO((void));
static void set_raxis __PROTO((void));
static void set_xyplane __PROTO((void));
static void set_ticslevel __PROTO((void));
static void set_zeroaxis __PROTO((AXIS_INDEX));
static void set_allzeroaxis __PROTO((void));


/******** Local functions ********/

static void set_xyzlabel __PROTO((text_label * label));
static void load_tics __PROTO((AXIS_INDEX axis));
static void load_tic_user __PROTO((AXIS_INDEX axis));
static void load_tic_series __PROTO((AXIS_INDEX axis));

static void set_linestyle __PROTO((struct linestyle_def **head, lp_class destination_class));
static void set_arrowstyle __PROTO((void));
static int assign_arrowstyle_tag __PROTO((void));
static int set_tic_prop __PROTO((AXIS_INDEX));

static void check_palette_grayscale __PROTO((void));
static int set_palette_defined __PROTO((void));
static void set_palette_file __PROTO((void));
static void set_palette_function __PROTO((void));
static void parse_histogramstyle __PROTO((histogram_style *hs,
		t_histogram_type def_type, int def_gap));
static void set_style_parallel __PROTO((void));

static const char *encoding_minus __PROTO((void));

static const struct position default_position
	= {first_axes, first_axes, first_axes, 0., 0., 0.};
static const struct position default_offset
	= {character, character, character, 0., 0., 0.};

static lp_style_type default_hypertext_point_style
	= {1, LT_BLACK, 4, DASHTYPE_SOLID, 0, 1.0, PTSZ_DEFAULT, 0, {TC_RGB, 0x000000, 0.0}, DEFAULT_DASHPATTERN};

/******** The 'set' command ********/
void
set_command()
{
    c_token++;

    /* Mild form of backwards compatibility */
	/* Allow "set no{foo}" rather than "unset foo" */
    if (gp_input_line[token[c_token].start_index] == 'n' &&
	       gp_input_line[token[c_token].start_index+1] == 'o') {
	if (interactive)
	    int_warn(c_token, "deprecated syntax, use \"unset\"");
	token[c_token].start_index += 2;
	token[c_token].length -= 2;
	c_token--;
	unset_command();
    } else {

	int save_token = c_token;
	set_iterator = check_for_iteration();
	if (empty_iteration(set_iterator)) {
	    /* Skip iteration [i=start:end] where start > end */
	    while (!END_OF_COMMAND) c_token++;
	    set_iterator = cleanup_iteration(set_iterator);
	    return;
	}
	if (forever_iteration(set_iterator)) {
	    set_iterator = cleanup_iteration(set_iterator);
	    int_error(save_token, "unbounded iteration");
	}
	save_token = c_token;
	ITERATE:

	switch(lookup_table(&set_tbl[0],c_token)) {
	case S_ANGLES:
	    set_angles();
	    break;
	case S_ARROW:
	    set_arrow();
	    break;
	case S_AUTOSCALE:
	    set_autoscale();
	    break;
	case S_BARS:
	    set_bars();
	    break;
	case S_BORDER:
	    set_border();
	    break;
	case S_BOXWIDTH:
	    set_boxwidth();
	    break;
	case S_CLABEL:
	    set_clabel();
	    break;
	case S_CLIP:
	    set_clip();
	    break;
	case S_COLOR:
	    unset_monochrome();
	    c_token++;
	    break;
	case S_COLORSEQUENCE:
	    set_colorsequence(0);
	    break;
	case S_CNTRPARAM:
	    set_cntrparam();
	    break;
	case S_CNTRLABEL:
	    set_cntrlabel();
	    break;
	case S_CONTOUR:
	    set_contour();
	    break;
	case S_DASHTYPE:
	    set_dashtype();
	    break;
	case S_DGRID3D:
	    set_dgrid3d();
	    break;
	case S_DECIMALSIGN:
	    set_decimalsign();
	    break;
	case S_DUMMY:
	    set_dummy();
	    break;
	case S_ENCODING:
	    set_encoding();
	    break;
	case S_FIT:
	    set_fit();
	    break;
	case S_FONTPATH:
	    set_fontpath();
	    break;
	case S_FORMAT:
	    set_format();
	    break;
	case S_GRID:
	    set_grid();
	    break;
	case S_HIDDEN3D:
	    set_hidden3d();
	    break;
	case S_HISTORYSIZE:	/* Deprecated in favor of "set history size" */
	case S_HISTORY:
	    set_history();
	    break;
	case S_ISOSAMPLES:
	    set_isosamples();
	    break;
	case S_KEY:
	    set_key();
	    break;
	case S_LINESTYLE:
	    set_linestyle(&first_linestyle, LP_STYLE);
	    break;
	case S_LINETYPE:
	    if (equals(c_token+1,"cycle")) {
		c_token += 2;
		linetype_recycle_count = int_expression();
	    } else
		set_linestyle(&first_perm_linestyle, LP_TYPE);
	    break;
	case S_LABEL:
	    set_label();
	    break;
	case S_LINK:
	    link_command();
	    break;
	case S_LOADPATH:
	    set_loadpath();
	    break;
	case S_LOCALE:
	    set_locale();
	    break;
	case S_LOGSCALE:
	    set_logscale();
	    break;
	case S_MACROS:
	    /* Aug 2013 - macros are always enabled */
	    c_token++;
	    break;
	case S_MAPPING:
	    set_mapping();
	    break;
	case S_MARGIN:
	    /* Jan 2015: CHANGE to order <left>,<right>,<bottom>,<top> */
	    set_margin(&lmargin);
	    if (!equals(c_token,","))
		break;
	    set_margin(&rmargin);
	    if (!equals(c_token,","))
		break;
	    set_margin(&bmargin);
	    if (!equals(c_token,","))
		break;
	    set_margin(&tmargin);
	    break;
	case S_BMARGIN:
	    set_margin(&bmargin);
	    break;
	case S_LMARGIN:
	    set_margin(&lmargin);
	    break;
	case S_RMARGIN:
	    set_margin(&rmargin);
	    break;
	case S_TMARGIN:
	    set_margin(&tmargin);
	    break;
	case S_MINUS_SIGN:
	    set_minus_sign();
	    break;
	case S_DATAFILE:
	    if (almost_equals(++c_token,"miss$ing"))
		set_missing();
	    else if (almost_equals(c_token,"sep$arators"))
		set_separator();
	    else if (almost_equals(c_token,"com$mentschars"))
		set_datafile_commentschars();
	    else if (almost_equals(c_token,"bin$ary"))
		df_set_datafile_binary();
	    else if (almost_equals(c_token,"fort$ran")) {
		df_fortran_constants = TRUE;
		c_token++;
	    } else if (almost_equals(c_token,"nofort$ran")) {
		df_fortran_constants = FALSE;
		c_token++;
	    } else if (almost_equals(c_token,"fpe_trap")) {
		df_nofpe_trap = FALSE;
		c_token++;
	    } else if (almost_equals(c_token,"nofpe_trap")) {
		df_nofpe_trap = TRUE;
		c_token++;
	    } else
		int_error(c_token,"expecting datafile modifier");
	    break;
#ifdef USE_MOUSE
	case S_MOUSE:
	    set_mouse();
	    break;
#endif
	case S_MONOCHROME:
	    set_monochrome();
	    break;
	case S_MULTIPLOT:
	    term_start_multiplot();
	    break;
	case S_OFFSETS:
	    set_offsets();
	    break;
	case S_ORIGIN:
	    set_origin();
	    break;
	case SET_OUTPUT:
	    set_output();
	    break;
	case S_PARAMETRIC:
	    set_parametric();
	    break;
	case S_PM3D:
	    set_pm3d();
	    break;
	case S_PALETTE:
	    set_palette();
	    break;
	case S_COLORBOX:
	    set_colorbox();
	    break;
	case S_POINTINTERVALBOX:
	    set_pointintervalbox();
	    break;
	case S_POINTSIZE:
	    set_pointsize();
	    break;
	case S_POLAR:
	    set_polar();
	    break;
	case S_PRINT:
	    set_print();
	    break;
	case S_PSDIR:
	    set_psdir();
	    break;
#ifdef EAM_OBJECTS
	case S_OBJECT:
	    set_object();
	    break;
#endif
	case S_SAMPLES:
	    set_samples();
	    break;
	case S_SIZE:
	    set_size();
	    break;
	case S_STYLE:
	    set_style();
	    break;
	case S_SURFACE:
	    set_surface();
	    break;
	case S_TABLE:
	    set_table();
	    break;
	case S_TERMINAL:
	    set_terminal();
	    break;
	case S_TERMOPTIONS:
	    set_termoptions();
	    break;
	case S_TICS:
	    set_tics();
	    break;
	case S_TICSCALE:
	    set_ticscale();
	    break;
	case S_TIMEFMT:
	    set_timefmt();
	    break;
	case S_TIMESTAMP:
	    set_timestamp();
	    break;
	case S_TITLE:
	    set_xyzlabel(&title);
	    break;
	case S_VIEW:
	    set_view();
	    break;
	case S_ZERO:
	    set_zero();
	    break;

	case S_MXTICS:
	case S_NOMXTICS:
	case S_XTICS:
	case S_NOXTICS:
	case S_XDTICS:
	case S_NOXDTICS:
	case S_XMTICS:
	case S_NOXMTICS:
	    set_tic_prop(FIRST_X_AXIS);
	    break;
	case S_MYTICS:
	case S_NOMYTICS:
	case S_YTICS:
	case S_NOYTICS:
	case S_YDTICS:
	case S_NOYDTICS:
	case S_YMTICS:
	case S_NOYMTICS:
	    set_tic_prop(FIRST_Y_AXIS);
	    break;
	case S_MX2TICS:
	case S_NOMX2TICS:
	case S_X2TICS:
	case S_NOX2TICS:
	case S_X2DTICS:
	case S_NOX2DTICS:
	case S_X2MTICS:
	case S_NOX2MTICS:
	    set_tic_prop(SECOND_X_AXIS);
	    break;
	case S_MY2TICS:
	case S_NOMY2TICS:
	case S_Y2TICS:
	case S_NOY2TICS:
	case S_Y2DTICS:
	case S_NOY2DTICS:
	case S_Y2MTICS:
	case S_NOY2MTICS:
	    set_tic_prop(SECOND_Y_AXIS);
	    break;
	case S_MZTICS:
	case S_NOMZTICS:
	case S_ZTICS:
	case S_NOZTICS:
	case S_ZDTICS:
	case S_NOZDTICS:
	case S_ZMTICS:
	case S_NOZMTICS:
	    set_tic_prop(FIRST_Z_AXIS);
	    break;
	case S_MCBTICS:
	case S_NOMCBTICS:
	case S_CBTICS:
	case S_NOCBTICS:
	case S_CBDTICS:
	case S_NOCBDTICS:
	case S_CBMTICS:
	case S_NOCBMTICS:
	    set_tic_prop(COLOR_AXIS);
	    break;
	case S_RTICS:
	case S_NORTICS:
	case S_MRTICS:
	case S_NOMRTICS:
	    set_tic_prop(POLAR_AXIS);
	    break;
	case S_XDATA:
	    set_timedata(FIRST_X_AXIS);
	    axis_array[T_AXIS].datatype
	      = axis_array[U_AXIS].datatype
	      = axis_array[FIRST_X_AXIS].datatype;
	    break;
	case S_YDATA:
	    set_timedata(FIRST_Y_AXIS);
	    axis_array[V_AXIS].datatype
	      = axis_array[FIRST_X_AXIS].datatype;
	    break;
	case S_ZDATA:
	    set_timedata(FIRST_Z_AXIS);
	    break;
	case S_CBDATA:
	    set_timedata(COLOR_AXIS);
	    break;
	case S_X2DATA:
	    set_timedata(SECOND_X_AXIS);
	    break;
	case S_Y2DATA:
	    set_timedata(SECOND_Y_AXIS);
	    break;
	case S_XLABEL:
	    set_xyzlabel(&axis_array[FIRST_X_AXIS].label);
	    break;
	case S_YLABEL:
	    set_xyzlabel(&axis_array[FIRST_Y_AXIS].label);
	    break;
	case S_ZLABEL:
	    set_xyzlabel(&axis_array[FIRST_Z_AXIS].label);
	    break;
	case S_CBLABEL:
	    set_xyzlabel(&axis_array[COLOR_AXIS].label);
	    break;
	case S_X2LABEL:
	    set_xyzlabel(&axis_array[SECOND_X_AXIS].label);
	    break;
	case S_Y2LABEL:
	    set_xyzlabel(&axis_array[SECOND_Y_AXIS].label);
	    break;
	case S_XRANGE:
	    set_range(FIRST_X_AXIS);
	    break;
	case S_X2RANGE:
	    set_range(SECOND_X_AXIS);
	    break;
	case S_YRANGE:
	    set_range(FIRST_Y_AXIS);
	    break;
	case S_Y2RANGE:
	    set_range(SECOND_Y_AXIS);
	    break;
	case S_ZRANGE:
	    set_range(FIRST_Z_AXIS);
	    break;
	case S_CBRANGE:
	    set_range(COLOR_AXIS);
	    break;
	case S_RRANGE:
	    set_range(POLAR_AXIS);
	    if (polar)
		rrange_to_xy();
	    break;
	case S_TRANGE:
	    set_range(T_AXIS);
	    break;
	case S_URANGE:
	    set_range(U_AXIS);
	    break;
	case S_VRANGE:
	    set_range(V_AXIS);
	    break;
	case S_PAXIS:
	    set_paxis();
	    break;
	case S_RAXIS:
	    set_raxis();
	    break;
	case S_XZEROAXIS:
	    set_zeroaxis(FIRST_X_AXIS);
	    break;
	case S_YZEROAXIS:
	    set_zeroaxis(FIRST_Y_AXIS);
	    break;
	case S_ZZEROAXIS:
	    set_zeroaxis(FIRST_Z_AXIS);
	    break;
	case S_X2ZEROAXIS:
	    set_zeroaxis(SECOND_X_AXIS);
	    break;
	case S_Y2ZEROAXIS:
	    set_zeroaxis(SECOND_Y_AXIS);
	    break;
	case S_ZEROAXIS:
	    set_allzeroaxis();
	    break;
	case S_XYPLANE:
	    set_xyplane();
	    break;
	case S_TICSLEVEL:
	    set_ticslevel();
	    break;
	default:
	    int_error(c_token, "unrecognized option - see 'help set'.");
	    break;
	}

    	if (next_iteration(set_iterator)) {
	    c_token = save_token;
	    goto ITERATE;
	}

    }

    update_gpval_variables(0);

    set_iterator = cleanup_iteration(set_iterator);
}


/* process 'set angles' command */
static void
set_angles()
{
    c_token++;
    if (END_OF_COMMAND) {
	/* assuming same as defaults */
	ang2rad = 1;
    } else if (almost_equals(c_token, "r$adians")) {
	c_token++;
	ang2rad = 1;
    } else if (almost_equals(c_token, "d$egrees")) {
	c_token++;
	ang2rad = DEG2RAD;
    } else
	int_error(c_token, "expecting 'radians' or 'degrees'");

    if (polar && axis_array[T_AXIS].set_autoscale) {
	/* set trange if in polar mode and no explicit range */
	axis_array[T_AXIS].set_min = 0;
	axis_array[T_AXIS].set_max = 2 * M_PI / ang2rad;
    }
}


/* process a 'set arrow' command */
/* set arrow {tag} {from x,y} {to x,y} {{no}head} ... */
/* allow any order of options - pm 25.11.2001 */
static void
set_arrow()
{
    struct arrow_def *this_arrow = NULL;
    struct arrow_def *new_arrow = NULL;
    struct arrow_def *prev_arrow = NULL;
    TBOOLEAN duplication = FALSE;
    TBOOLEAN set_start = FALSE;
    TBOOLEAN set_end = FALSE;
    int save_token;
    int tag;

    c_token++;

    /* get tag */
    if (almost_equals(c_token, "back$head") || equals(c_token, "front")
	    || equals(c_token, "from") || equals(c_token, "at")
	    || equals(c_token, "to") || equals(c_token, "rto")
	    || equals(c_token, "size")
	    || equals(c_token, "filled") || equals(c_token, "empty")
	    || equals(c_token, "as") || equals(c_token, "arrowstyle")
	    || almost_equals(c_token, "head$s") || equals(c_token, "nohead")
	    || almost_equals(c_token, "nobo$rder")) {
	tag = assign_arrow_tag();

    } else
	tag = int_expression();

    if (tag <= 0)
	int_error(c_token, "tag must be > 0");

    /* OK! add arrow */
    if (first_arrow != NULL) {	/* skip to last arrow */
	for (this_arrow = first_arrow; this_arrow != NULL;
	     prev_arrow = this_arrow, this_arrow = this_arrow->next)
	    /* is this the arrow we want? */
	    if (tag <= this_arrow->tag)
		break;
    }
    if (this_arrow == NULL || tag != this_arrow->tag) {
	new_arrow = gp_alloc(sizeof(struct arrow_def), "arrow");
	if (prev_arrow == NULL)
	    first_arrow = new_arrow;
	else
	    prev_arrow->next = new_arrow;
	new_arrow->tag = tag;
	new_arrow->next = this_arrow;
	this_arrow = new_arrow;

	this_arrow->start = default_position;
	this_arrow->end = default_position;
	this_arrow->angle = 0.0;

	default_arrow_style(&(new_arrow->arrow_properties));
    }

    while (!END_OF_COMMAND) {

	/* get start position */
	if (equals(c_token, "from") || equals(c_token,"at")) {
	    if (set_start) { duplication = TRUE; break; }
	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "start coordinates expected");
	    /* get coordinates */
	    get_position(&this_arrow->start);
	    set_start = TRUE;
	    continue;
	}

	/* get end or relative end position */
	if (equals(c_token, "to") || equals(c_token,"rto")) {
	    if (set_end) { duplication = TRUE; break; }
	    if (equals(c_token,"rto"))
		this_arrow->type = arrow_end_relative;
	    else
		this_arrow->type = arrow_end_absolute;
	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "end coordinates expected");
	    /* get coordinates */
	    get_position(&this_arrow->end);
	    set_end = TRUE;
	    continue;
	}

	/* get end position specified as length + orientation angle */
	if (almost_equals(c_token, "len$gth")) {
	    if (set_end) { duplication = TRUE; break; }
	    this_arrow->type = arrow_end_oriented;
	    c_token++;
	    /* FIXME: we really only want one coordinate (length), not 3 */
	    get_position(&this_arrow->end);
	    set_end = TRUE;
	    continue;
	}
	if (almost_equals(c_token,"ang$le")) {
	    c_token++;
	    this_arrow->angle = real_expression();
	    continue;
	}

	/* Allow interspersed style commands */
	save_token = c_token;
	arrow_parse(&this_arrow->arrow_properties, TRUE);
	if (save_token != c_token)
	    continue;

	if (!END_OF_COMMAND)
	    int_error(c_token, "wrong argument in set arrow");

    } /* while (!END_OF_COMMAND) */

    if (duplication)
	int_error(c_token, "duplicate or contradictory arguments");

}


/* assign a new arrow tag
 * arrows are kept sorted by tag number, so this is easy
 * returns the lowest unassigned tag number
 */
static int
assign_arrow_tag()
{
    struct arrow_def *this_arrow;
    int last = 0;		/* previous tag value */

    for (this_arrow = first_arrow; this_arrow != NULL;
	 this_arrow = this_arrow->next)
	if (this_arrow->tag == last + 1)
	    last++;
	else
	    break;

    return (last + 1);
}

/* helper routine for 'set autoscale' on a single axis */
static TBOOLEAN
set_autoscale_axis(AXIS_INDEX axis)
{
    char keyword[16];
    AXIS *this = axis_array + axis;
    char *name = (char *) &(axis_name(axis)[0]);

    if (equals(c_token, name)) {
	this->set_autoscale = AUTOSCALE_BOTH;
	this->min_constraint = CONSTRAINT_NONE;
	this->max_constraint = CONSTRAINT_NONE;
	++c_token;
	return TRUE;
    }
    sprintf(keyword, "%smi$n", name);
    if (almost_equals(c_token, keyword)) {
	this->set_autoscale |= AUTOSCALE_MIN;
	this->min_constraint = CONSTRAINT_NONE;
	++c_token;
	return TRUE;
    }
    sprintf(keyword, "%sma$x", name);
    if (almost_equals(c_token, keyword)) {
	this->set_autoscale |= AUTOSCALE_MAX;
	this->max_constraint = CONSTRAINT_NONE;
	++c_token;
	return TRUE;
    }
    sprintf(keyword, "%sfix", name);
    if (equals(c_token, keyword)) {
	this->set_autoscale |= AUTOSCALE_FIXMIN | AUTOSCALE_FIXMAX;
	++c_token;
	return TRUE;
    }
    sprintf(keyword, "%sfixmi$n", name);
    if (almost_equals(c_token, keyword)) {
	this->set_autoscale |= AUTOSCALE_FIXMIN;
	++c_token;
	return TRUE;
    }
    sprintf(keyword, "%sfixma$x", name);
    if (almost_equals(c_token, keyword)) {
	this->set_autoscale |= AUTOSCALE_FIXMAX;
	++c_token;
	return TRUE;
    }

    return FALSE;
}

/* process 'set autoscale' command */
static void
set_autoscale()
{
    c_token++;
    if (END_OF_COMMAND) {
	int axis;
	for (axis=0; axis<AXIS_ARRAY_SIZE; axis++)
	    axis_array[axis].set_autoscale = AUTOSCALE_BOTH;
	return;
    } else if (equals(c_token, "xy") || equals(c_token, "yx")) {
	axis_array[FIRST_X_AXIS].set_autoscale =
	    axis_array[FIRST_Y_AXIS].set_autoscale =  AUTOSCALE_BOTH;
	axis_array[FIRST_X_AXIS].min_constraint =
	    axis_array[FIRST_X_AXIS].max_constraint =
	    axis_array[FIRST_Y_AXIS].min_constraint =
	    axis_array[FIRST_Y_AXIS].max_constraint = CONSTRAINT_NONE;
	c_token++;
	return;
    } else if (equals(c_token, "fix") || almost_equals(c_token, "noext$end")) {
	int a = 0;
	while (a < AXIS_ARRAY_SIZE) {
	    axis_array[a].set_autoscale |= AUTOSCALE_FIXMIN | AUTOSCALE_FIXMAX;
	    a++;
	}
	c_token++;
	return;
    } else if (almost_equals(c_token, "ke$epfix")) {
	int a = 0;
	while (a < AXIS_ARRAY_SIZE)
	    axis_array[a++].set_autoscale |= AUTOSCALE_BOTH;
	c_token++;
	return;
    }

    if (set_autoscale_axis(FIRST_X_AXIS)) return;
    if (set_autoscale_axis(FIRST_Y_AXIS)) return;
    if (set_autoscale_axis(FIRST_Z_AXIS)) return;
    if (set_autoscale_axis(SECOND_X_AXIS)) return;
    if (set_autoscale_axis(SECOND_Y_AXIS)) return;
    if (set_autoscale_axis(COLOR_AXIS)) return;
    if (set_autoscale_axis(POLAR_AXIS)) return;
    /* FIXME: Do these commands make any sense? */
    if (set_autoscale_axis(T_AXIS)) return;
    if (set_autoscale_axis(U_AXIS)) return;
    if (set_autoscale_axis(V_AXIS)) return;

    /* come here only if nothing found: */
	int_error(c_token, "Invalid range");
}


/* process 'set bars' command */
static void
set_bars()
{

    int save_token = ++c_token;

    while (!END_OF_COMMAND) {
	if (almost_equals(c_token,"s$mall")) {
	    bar_size = 0.0;
	    ++c_token;
	} else if (almost_equals(c_token,"l$arge")) {
	    bar_size = 1.0;
	    ++c_token;
	} else if (almost_equals(c_token,"full$width")) {
	    bar_size = -1.0;
	    ++c_token;
	} else if (equals(c_token,"front")) {
	    bar_layer = LAYER_FRONT;
	    ++c_token;
	} else if (equals(c_token,"back")) {
	    bar_layer = LAYER_BACK;
	    ++c_token;
	} else {
	    bar_size = real_expression();
	}
    }

    if (save_token == c_token)
	bar_size = 1.0;

}


/* process 'set border' command */
static void
set_border()
{
    c_token++;
    if(END_OF_COMMAND){
	draw_border = 31;
	border_layer = LAYER_FRONT;
	border_lp = default_border_lp;
    }

    while (!END_OF_COMMAND) {
	if (equals(c_token,"front")) {
	    border_layer = LAYER_FRONT;
	    c_token++;
	} else if (equals(c_token,"back")) {
	    border_layer = LAYER_BACK;
	    c_token++;
	} else if (equals(c_token,"behind")) {
	    border_layer = LAYER_BEHIND;
	    c_token++;
	} else {
	    int save_token = c_token;
	    lp_parse(&border_lp, LP_ADHOC, FALSE);
	    if (save_token != c_token)
		continue;
	    draw_border = int_expression();
	}
    }

    /* This is the only place the user can change the border	*/
    /* so remember what he set.  If draw_border is later changed*/
    /* internally, we can still recover the user's preference.	*/
    user_border = draw_border;
}


/* process 'set style boxplot' command */
static void
set_boxplot()
{
    c_token++;
    if (END_OF_COMMAND) {
	boxplot_style defstyle = DEFAULT_BOXPLOT_STYLE;
	boxplot_opts = defstyle;
    }
    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "noout$liers")) {
	    boxplot_opts.outliers = FALSE;
	    c_token++;
	}
	else if (almost_equals(c_token, "out$liers")) {
	    boxplot_opts.outliers = TRUE;
	    c_token++;
	}
	else if (almost_equals(c_token, "point$type") || equals (c_token, "pt")) {
	    c_token++;
	    boxplot_opts.pointtype = int_expression()-1;
	}
	else if (equals(c_token,"range")) {
	    c_token++;
	    boxplot_opts.limit_type = 0;
	    boxplot_opts.limit_value = real_expression();
	}
	else if (almost_equals(c_token,"frac$tion")) {
	    c_token++;
	    boxplot_opts.limit_value = real_expression();
	    if (boxplot_opts.limit_value < 0 || boxplot_opts.limit_value > 1)
		int_error(c_token-1,"fraction must be less than 1");
	    boxplot_opts.limit_type = 1;
	}
	else if (almost_equals(c_token,"candle$sticks")) {
	    c_token++;
	    boxplot_opts.plotstyle = CANDLESTICKS;
	}
	else if (almost_equals(c_token,"finance$bars")) {
	    c_token++;
	    boxplot_opts.plotstyle = FINANCEBARS;
	}
	else if (almost_equals(c_token,"sep$aration")) {
	    c_token++;
	    boxplot_opts.separation = real_expression();
	    if (boxplot_opts.separation < 0)
		int_error(c_token-1,"separation must be > 0");
	}
	else if (almost_equals(c_token,"lab$els")) {
	    c_token++;
	    if (equals(c_token, "off")) {
		boxplot_opts.labels = BOXPLOT_FACTOR_LABELS_OFF;
	    }
	    else if (equals(c_token, "x")) {
		boxplot_opts.labels = BOXPLOT_FACTOR_LABELS_X;
	    }
	    else if (equals(c_token, "x2")) {
		boxplot_opts.labels = BOXPLOT_FACTOR_LABELS_X2;
	    }
	    else if (equals(c_token, "auto")) {
		boxplot_opts.labels = BOXPLOT_FACTOR_LABELS_AUTO;
	    }
	    else
		int_error(c_token-1,"expecting 'x', 'x2', 'auto' or 'off'");
	    c_token++;
	}
	else if (almost_equals(c_token, "so$rted")) {
	    boxplot_opts.sort_factors = TRUE;
	    c_token++;
	}
	else if (almost_equals(c_token, "un$sorted")) {
	    boxplot_opts.sort_factors = FALSE;
	    c_token++;
	}
	else
	    int_error(c_token,"unrecognized option");
    }

}


/* process 'set boxwidth' command */
static void
set_boxwidth()
{
    c_token++;
    if (END_OF_COMMAND) {
	boxwidth = -1.0;
	boxwidth_is_absolute = TRUE;
    } else {
	boxwidth = real_expression();
    }
    if (END_OF_COMMAND)
	return;
    else {
	if (almost_equals(c_token, "a$bsolute"))
	    boxwidth_is_absolute = TRUE;
	else if (almost_equals(c_token, "r$elative"))
	    boxwidth_is_absolute = FALSE;
	else
	    int_error(c_token, "expecting 'absolute' or 'relative' ");
    }
    c_token++;
}

/* process 'set clabel' command */
static void
set_clabel()
{
    char *new_format;

    c_token++;
    clabel_onecolor = FALSE;
    if ((new_format = try_to_get_string())) {
	strncpy(contour_format, new_format, sizeof(contour_format));
	free(new_format);
    }
}


/* process 'set clip' command */
static void
set_clip()
{
    c_token++;
    if (END_OF_COMMAND) {
	/* assuming same as points */
	clip_points = TRUE;
    } else if (almost_equals(c_token, "p$oints")) {
	clip_points = TRUE;
	c_token++;
    } else if (almost_equals(c_token, "o$ne")) {
	clip_lines1 = TRUE;
	c_token++;
    } else if (almost_equals(c_token, "t$wo")) {
	clip_lines2 = TRUE;
	c_token++;
    } else
	int_error(c_token, "expecting 'points', 'one', or 'two'");
}


/* process 'set cntrparam' command */
static void
set_cntrparam()
{
    c_token++;
    if (END_OF_COMMAND) {
	/* assuming same as defaults */
	contour_pts = DEFAULT_NUM_APPROX_PTS;
	contour_kind = CONTOUR_KIND_LINEAR;
	contour_order = DEFAULT_CONTOUR_ORDER;
	contour_levels = DEFAULT_CONTOUR_LEVELS;
	contour_levels_kind = LEVELS_AUTO;
    } else if (almost_equals(c_token, "p$oints")) {
	c_token++;
	contour_pts = int_expression();
    } else if (almost_equals(c_token, "li$near")) {
	c_token++;
	contour_kind = CONTOUR_KIND_LINEAR;
    } else if (almost_equals(c_token, "c$ubicspline")) {
	c_token++;
	contour_kind = CONTOUR_KIND_CUBIC_SPL;
    } else if (almost_equals(c_token, "b$spline")) {
	c_token++;
	contour_kind = CONTOUR_KIND_BSPLINE;
    } else if (almost_equals(c_token, "le$vels")) {
	c_token++;

	if (!(set_iterator && set_iterator->iteration)) {
	    free_dynarray(&dyn_contour_levels_list);
	    init_dynarray(&dyn_contour_levels_list, sizeof(double), 5, 10);
	}

	/*  RKC: I have modified the next two:
	 *   to use commas to separate list elements as in xtics
	 *   so that incremental lists start,incr[,end]as in "
	 */
	if (almost_equals(c_token, "di$screte")) {
	    contour_levels_kind = LEVELS_DISCRETE;
	    c_token++;
	    if(END_OF_COMMAND)
		int_error(c_token, "expecting discrete level");
	    else
		*(double *)nextfrom_dynarray(&dyn_contour_levels_list) =
		    real_expression();

	    while(!END_OF_COMMAND) {
		if (!equals(c_token, ","))
		    int_error(c_token,
			      "expecting comma to separate discrete levels");
		c_token++;
		*(double *)nextfrom_dynarray(&dyn_contour_levels_list) =
		    real_expression();
	    }
	    contour_levels = dyn_contour_levels_list.end;
	} else if (almost_equals(c_token, "in$cremental")) {
	    int i = 0;  /* local counter */

	    contour_levels_kind = LEVELS_INCREMENTAL;
	    c_token++;
	    contour_levels_list[i++] = real_expression();
	    if (!equals(c_token, ","))
		int_error(c_token,
			  "expecting comma to separate start,incr levels");
	    c_token++;
	    if((contour_levels_list[i++] = real_expression()) == 0)
		int_error(c_token, "increment cannot be 0");
	    if(!END_OF_COMMAND) {
		if (!equals(c_token, ","))
		    int_error(c_token,
			      "expecting comma to separate incr,stop levels");
		c_token++;
		/* need to round up, since 10,10,50 is 5 levels, not four,
		 * but 10,10,49 is four
		 */
		dyn_contour_levels_list.end = i;
		contour_levels = (int) ( (real_expression()-contour_levels_list[0])/contour_levels_list[1] + 1.0);
	    }
	} else if (almost_equals(c_token, "au$to")) {
	    contour_levels_kind = LEVELS_AUTO;
	    c_token++;
	    if(!END_OF_COMMAND)
		contour_levels = int_expression();
	} else {
	    if(contour_levels_kind == LEVELS_DISCRETE)
		int_error(c_token, "Levels type is discrete, ignoring new number of contour levels");
	    contour_levels = int_expression();
	}
    } else if (almost_equals(c_token, "o$rder")) {
	int order;
	c_token++;
	order = int_expression();
	if ( order < 2 || order > MAX_BSPLINE_ORDER )
	    int_error(c_token, "bspline order must be in [2..10] range.");
	contour_order = order;
    } else
	int_error(c_token, "expecting 'linear', 'cubicspline', 'bspline', 'points', 'levels' or 'order'");
}

/* process 'set cntrlabel' command */
static void
set_cntrlabel()
{
    c_token++;
    if (END_OF_COMMAND) {
	strcpy(contour_format, "%8.3g");
	clabel_onecolor = FALSE;
	return;
    }
    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "form$at")) {
	    char *new;
	    c_token++;
	    if ((new = try_to_get_string()))
		strncpy(contour_format,new,sizeof(contour_format));
	    free(new);
	} else if (equals(c_token, "font")) {
	    char *ctmp;
	    c_token++;
	    if ((ctmp = try_to_get_string())) {
		free(clabel_font);
		clabel_font = ctmp;
	    }
	} else if (almost_equals(c_token, "one$color")) {
	    c_token++;
	    clabel_onecolor = TRUE;
	} else if (equals(c_token, "start")) {
	    c_token++;
	    clabel_start = int_expression();
	    if (clabel_start <= 0)
		clabel_start = 5;
	} else if (almost_equals(c_token, "int$erval")) {
	    c_token++;
	    clabel_interval = int_expression();
	} else {
	    int_error(c_token, "unrecognized option");
	}
    }
}

/* process 'set contour' command */
static void
set_contour()
{
    c_token++;
    if (END_OF_COMMAND)
	/* assuming same as points */
	draw_contour = CONTOUR_BASE;
    else {
	if (almost_equals(c_token, "ba$se"))
	    draw_contour = CONTOUR_BASE;
	else if (almost_equals(c_token, "s$urface"))
	    draw_contour = CONTOUR_SRF;
	else if (almost_equals(c_token, "bo$th"))
	    draw_contour = CONTOUR_BOTH;
	else
	    int_error(c_token, "expecting 'base', 'surface', or 'both'");
	c_token++;
    }
}

/* process 'set colorsequence command */
void
set_colorsequence(int option)
{
    unsigned long default_colors[] = DEFAULT_COLOR_SEQUENCE;
    unsigned long podo_colors[] = PODO_COLOR_SEQUENCE;

    if (option == 0) {	/* Read option from command line */
	if (equals(++c_token, "default"))
	    option = 1;
	else if (equals(c_token, "podo"))
	    option = 2;
	else if (equals(c_token, "classic"))
	    option = 3;
	else
	    int_error(c_token, "unrecognized color set");
    }

    if (option == 1 || option == 2) {
	int i;
	char *command;
	char *command_template = "set linetype %2d lc rgb 0x%06x";
	unsigned long *colors = default_colors;
	if (option == 2)
	    colors = podo_colors;
	linetype_recycle_count = 8;
	for (i = 1; i <= 8; i++) {
	    command = gp_alloc(strlen(command_template)+8, "dynamic command"); 
	    sprintf(command, command_template, i, colors[i-1]);
	    do_string_and_free(command);
	}

    } else if (option == 3) {
	struct linestyle_def *this;
	for (this = first_perm_linestyle; this != NULL; this = this->next) {
	    this->lp_properties.pm3d_color.type = TC_LT;
	    this->lp_properties.pm3d_color.lt = this->tag-1;
	}
	linetype_recycle_count = 0;

    } else {
	int_error(c_token, "Expecting 'classic' or 'default'");
    }
    c_token++;
}

/* process 'set dashtype' command */
static void
set_dashtype()
{
    struct custom_dashtype_def *this_dashtype = NULL;
    struct custom_dashtype_def *new_dashtype = NULL;
    struct custom_dashtype_def *prev_dashtype = NULL;
    int tag, is_new = FALSE;

    c_token++;

    /* get tag */
    if (END_OF_COMMAND || ((tag = int_expression()) <= 0))
	int_error(c_token, "tag must be > zero");

    /* Check if dashtype is already defined */
    for (this_dashtype = first_custom_dashtype; this_dashtype != NULL;
	 prev_dashtype = this_dashtype, this_dashtype = this_dashtype->next)
	if (tag <= this_dashtype->tag)
		break;

    if (this_dashtype == NULL || tag != this_dashtype->tag) {
	struct t_dashtype loc_dt = DEFAULT_DASHPATTERN;
	new_dashtype = gp_alloc(sizeof(struct custom_dashtype_def), "dashtype");
	if (prev_dashtype != NULL)
	    prev_dashtype->next = new_dashtype;	/* add it to end of list */
	else
	    first_custom_dashtype = new_dashtype;	/* make it start of list */
	new_dashtype->tag = tag;
	new_dashtype->d_type = DASHTYPE_SOLID;
	new_dashtype->next = this_dashtype;
	new_dashtype->dashtype = loc_dt;
	this_dashtype = new_dashtype;
	is_new = TRUE;
    }

    if (almost_equals(c_token, "def$ault")) {
	delete_dashtype(prev_dashtype, this_dashtype);
	is_new = FALSE;
	c_token++;
    } else {
	/* FIXME: Maybe this should reject return values > 0 because */
	/* otherwise we have potentially recursive definitions.      */
	this_dashtype->d_type = parse_dashtype(&this_dashtype->dashtype);
    }

    if (!END_OF_COMMAND) {
	if (is_new)
	    delete_dashtype(prev_dashtype, this_dashtype);
	int_error(c_token,"Extraneous arguments to set dashtype");
    }
}

/*
 * Delete dashtype from linked list.
 */
void
delete_dashtype(struct custom_dashtype_def *prev, struct custom_dashtype_def *this)
{
    if (this != NULL) {		/* there really is something to delete */
	if (this == first_custom_dashtype)
	    first_custom_dashtype = this->next;
	else
	    prev->next = this->next;
	free(this);
    }
}

/* process 'set dgrid3d' command */
static void
set_dgrid3d()
{
    int token_cnt = 0; /* Number of comma-separated values read in */

    int gridx     = dgrid3d_row_fineness;
    int gridy     = dgrid3d_col_fineness;
    int normval   = dgrid3d_norm_value;
    double scalex = dgrid3d_x_scale;
    double scaley = dgrid3d_y_scale;

    /* dgrid3d has two different syntax alternatives: classic and new.
       If there is a "mode" keyword, the syntax is new, otherwise it is classic.*/
    dgrid3d_mode  = DGRID3D_DEFAULT;

    dgrid3d_kdensity = FALSE;

    c_token++;
    while ( !(END_OF_COMMAND) ) {
	int tmp_mode = lookup_table(&dgrid3d_mode_tbl[0],c_token);
	if (tmp_mode != DGRID3D_OTHER) {
	    dgrid3d_mode = tmp_mode;
	    c_token++;
	}

	switch (tmp_mode) {
	case DGRID3D_QNORM:
				if (!(END_OF_COMMAND)) normval = int_expression();
				break;
	case DGRID3D_SPLINES:
				break;
	case DGRID3D_GAUSS:
	case DGRID3D_CAUCHY:
	case DGRID3D_EXP:
	case DGRID3D_BOX:
	case DGRID3D_HANN:
				if (!(END_OF_COMMAND) && almost_equals( c_token, "kdens$ity2d" )) {
					dgrid3d_kdensity = TRUE;
					c_token++;
				}
				if (!(END_OF_COMMAND)) {
					scalex = real_expression();
					scaley = scalex;
					if (equals(c_token, ",")) {
						c_token++;
						scaley = real_expression();
					}
				}
				break;

	default:		/* {rows}{,cols{,norm}}} */

			if  ( equals( c_token, "," )) {
				c_token++;
				token_cnt++;
			} else if( token_cnt == 0) {
				gridx = int_expression();
				gridy = gridx; /* gridy defaults to gridx, unless overridden below */
			} else if( token_cnt == 1) {
				gridy = int_expression();
			} else if( token_cnt == 2) {
				normval = int_expression();
			} else
				int_error(c_token,"Unrecognized keyword or unexpected value");
			break;
	}

    }

    /* we could warn here about floating point values being truncated... */
    if( gridx < 2 || gridx > 1000 || gridy < 2 || gridy > 1000 )
	int_error( NO_CARET,
		   "Number of grid points must be in [2:1000] - not changed!");

    /* no mode token found: classic format */
    if( dgrid3d_mode == DGRID3D_DEFAULT )
	dgrid3d_mode = DGRID3D_QNORM;

    if( scalex < 0.0 || scaley < 0.0 )
	int_error( NO_CARET,
		   "Scale factors must be greater than zero - not changed!" );

    dgrid3d_row_fineness = gridx;
    dgrid3d_col_fineness = gridy;
    dgrid3d_norm_value = normval;
    dgrid3d_x_scale = scalex;
    dgrid3d_y_scale = scaley;
    dgrid3d = TRUE;
}


/* process 'set decimalsign' command */
static void
set_decimalsign()
{
    c_token++;

    /* Clear current setting */
	free(decimalsign);
	decimalsign=NULL;

    if (END_OF_COMMAND) {
	reset_numeric_locale();
	free(numeric_locale);
	numeric_locale = NULL;
#ifdef HAVE_LOCALE_H
    } else if (equals(c_token,"locale")) {
	char *newlocale = NULL;
	c_token++;
	newlocale = try_to_get_string();
	if (!newlocale)
	    newlocale = gp_strdup(setlocale(LC_NUMERIC,""));
	if (!newlocale)
	    newlocale = gp_strdup(getenv("LC_ALL"));
	if (!newlocale)
	    newlocale = gp_strdup(getenv("LC_NUMERIC"));
	if (!newlocale)
	    newlocale = gp_strdup(getenv("LANG"));
	if (!setlocale(LC_NUMERIC, newlocale ? newlocale : ""))
	    int_error(c_token-1, "Could not find requested locale");
	decimalsign = gp_strdup(get_decimal_locale());
	fprintf(stderr,"decimal_sign in locale is %s\n", decimalsign);
	/* Save this locale for later use, but return to "C" for now */
	free(numeric_locale);
	numeric_locale = newlocale;
	setlocale(LC_NUMERIC,"C");
#endif
    } else if (!(decimalsign = try_to_get_string()))
	int_error(c_token, "expecting string");
}

/* process 'set dummy' command */
static void
set_dummy()
{
    int i;
    c_token++;
    for (i=0; i<MAX_NUM_VAR; i++) {
	if (END_OF_COMMAND)
	    return;
	if (isalpha(gp_input_line[token[c_token].start_index]))
	    copy_str(set_dummy_var[i],c_token++, MAX_ID_LEN);
	if (equals(c_token,","))
	    c_token++;
	else
	    break;
    }
    if (!END_OF_COMMAND)
	int_error(c_token,"unrecognized syntax");
}


/* process 'set encoding' command */
static void
set_encoding()
{
    char *l = NULL;
    c_token++;

    if (END_OF_COMMAND) {
	encoding = S_ENC_DEFAULT;
#ifdef HAVE_LOCALE_H
    } else if (equals(c_token, "locale")) {
#ifndef WIN32
	l = setlocale(LC_CTYPE, "");
	if (l && (strstr(l, "utf") || strstr(l, "UTF")))
	    encoding = S_ENC_UTF8;
	if (l && (strstr(l, "sjis") || strstr(l, "SJIS") || strstr(l, "932")))
	    encoding = S_ENC_SJIS;
	/* FIXME: "set encoding locale" supports only sjis and utf8 on non-Windows systems */
#else
	char * cp_str;

	l = setlocale(LC_CTYPE, "");
	/* preserve locale string, skip language information */
	cp_str = strchr(l, '.');
	if (cp_str) {
	    unsigned cp;

	    cp_str++; /* Step past the dot in, e.g., German_Germany.1252 */
	    cp = strtoul(cp_str, NULL, 10);

	    /* The code below is the inverse to the code found in UnicodeText().
	       For a list of code page identifiers see
	       http://msdn.microsoft.com/en-us/library/dd317756%28v=vs.85%29.aspx
	    */
	    switch (cp) {
	    case 437:   encoding = S_ENC_CP437; break;
	    case 850:   encoding = S_ENC_CP850; break;
	    case 852:   encoding = S_ENC_CP852; break;
	    case 932:   encoding = S_ENC_SJIS; break;
	    case 950:   encoding = S_ENC_CP950; break;
	    case 1250:  encoding = S_ENC_CP1250; break;
	    case 1251:  encoding = S_ENC_CP1251; break;
	    case 1252:  encoding = S_ENC_CP1252; break;
	    case 1254:  encoding = S_ENC_CP1254; break;
	    case 20866: encoding = S_ENC_KOI8_R; break;
	    case 21866: encoding = S_ENC_KOI8_U; break;
	    case 28591: encoding = S_ENC_ISO8859_1; break;
	    case 28592: encoding = S_ENC_ISO8859_2; break;
	    case 28599: encoding = S_ENC_ISO8859_9; break;
	    case 28605: encoding = S_ENC_ISO8859_15; break;
	    case 65001: encoding = S_ENC_UTF8; break;
	    case 0:
		int_warn(NO_CARET, "Error converting locale \"%s\" to codepage number", l);
		encoding = S_ENC_DEFAULT;
		break;
	    default:
		int_warn(NO_CARET, "Locale not supported by gnuplot: %s", l);
		encoding = S_ENC_DEFAULT;
	    }
	}
#endif
	c_token++;
#endif
    } else {
	int temp = lookup_table(&set_encoding_tbl[0],c_token);
	char *senc;

	/* allow string variables as parameter */
	if ((temp == S_ENC_INVALID) && isstringvalue(c_token) && (senc = try_to_get_string())) {
	    int i;
	    for (i = 0; encoding_names[i] != NULL; i++)
		if (strcmp(encoding_names[i], senc) == 0)
		    temp = i;
	    free(senc);
	} else {
	    c_token++;
	}

	if (temp == S_ENC_INVALID)
	    int_error(c_token, "unrecognized encoding specification; see 'help encoding'.");
	encoding = temp;
    }

    /* Set degree sign to match encoding */
    set_degreesign(l);

    /* Set minus sign to match encoding */
    minus_sign = encoding_minus();
}

static void
set_degreesign(char *locale)
{
#if defined(HAVE_ICONV) && !(defined WIN32)
    char degree_utf8[3] = {'\302', '\260', '\0'};
    size_t lengthin = 3;
    size_t lengthout = 8;
    char *in = degree_utf8;
    char *out = degree_sign;
    iconv_t cd;

    if (locale) {
	/* This should work even if gnuplot doesn't understand the encoding */
#ifdef HAVE_LANGINFO_H
	char *cencoding = nl_langinfo(CODESET);
#else
	char *cencoding = strchr(locale, '.');
	if (cencoding) cencoding++; /* Step past the dot in, e.g., ja_JP.EUC-JP */
#endif
	if (cencoding) {
	    if (strcmp(cencoding,"UTF-8") == 0)
		strcpy(degree_sign,degree_utf8);
	    else if ((cd = iconv_open(cencoding, "UTF-8")) == (iconv_t)(-1))
		int_warn(NO_CARET, "iconv_open failed for %s",cencoding);
	    else {
		if (iconv(cd, &in, &lengthin, &out, &lengthout) == (size_t)(-1))
		    int_warn(NO_CARET, "iconv failed to convert degree sign");
		iconv_close(cd);
	    }
	}
	return;
    }
#elif defined(WIN32)
    if (locale) {
	char *encoding = strchr(locale, '.');
	if (encoding) {
	    unsigned cp;
	    encoding++; /* Step past the dot in, e.g., German_Germany.1252 */
	    /* iconv does not understand encodings returned by setlocale() */
	    if (sscanf(encoding, "%i", &cp)) {
		wchar_t wdegreesign = 176; /* "\u00B0" */
		int n = WideCharToMultiByte(cp, WC_COMPOSITECHECK, &wdegreesign, 1,
			degree_sign, sizeof(degree_sign) - 1, NULL, NULL);
		degree_sign[n] = NUL;
	    }
	}
	return;
    }
#endif

    /* These are the internally-known encodings */
    memset(degree_sign, 0, sizeof(degree_sign));
    switch (encoding) {
    case S_ENC_UTF8:	degree_sign[0] = '\302'; degree_sign[1] = '\260'; break;
    case S_ENC_KOI8_R:
    case S_ENC_KOI8_U:	degree_sign[0] = '\234'; break;
    case S_ENC_CP437:
    case S_ENC_CP850:
    case S_ENC_CP852:	degree_sign[0] = '\370'; break;
    case S_ENC_SJIS:	break;  /* should be 0x818B */
    case S_ENC_CP950:	break;  /* should be 0xA258 */
    default:		degree_sign[0] = '\260'; break;
    }
}

/* Encoding-specific character enabled by "set minussign" */
static const char *
encoding_minus()
{
    static const char minus_utf8[4] = {0xE2, 0x88, 0x92, 0x0};
    static const char minus_1252[2] = {0x96, 0x0};
    /* NB: This SJIS character is correct, but produces bad spacing if used	*/
    /*     static const char minus_sjis[4] = {0x81, 0x7c, 0x0, 0x0};		*/
    switch (encoding) {
	case S_ENC_UTF8:	return minus_utf8;
	case S_ENC_CP1252:	return minus_1252;
	case S_ENC_SJIS:
	default:		return NULL;
    }
}

/* process 'set fit' command */
static void
set_fit()
{
    c_token++;

    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "log$file")) {
	    char *tmp;

	    c_token++;
	    fit_suppress_log = FALSE;
	    if (END_OF_COMMAND) {
		free(fitlogfile);
		fitlogfile = NULL;
	    } else if (equals(c_token, "default")) {
		c_token++;
		free(fitlogfile);
		fitlogfile = NULL;
	    } else if ((tmp = try_to_get_string()) != NULL) {
		free(fitlogfile);
		fitlogfile = tmp;
	    } else {
		int_error(c_token, "expecting string");
	    }
	} else if (almost_equals(c_token, "nolog$file")) {
	    fit_suppress_log = TRUE;
	    c_token++;
	} else if (almost_equals(c_token, "err$orvariables")) {
	    fit_errorvariables = TRUE;
	    c_token++;
	} else if (almost_equals(c_token, "noerr$orvariables")) {
	    fit_errorvariables = FALSE;
	    c_token++;
	} else if (almost_equals(c_token, "cov$ariancevariables")) {
	    fit_covarvariables = TRUE;
	    c_token++;
	} else if (almost_equals(c_token, "nocov$ariancevariables")) {
	    fit_covarvariables = FALSE;
	    c_token++;
	} else if (almost_equals(c_token, "errors$caling")) {
	    fit_errorscaling = TRUE;
	    c_token++;
	} else if (almost_equals(c_token, "noerrors$caling")) {
	    fit_errorscaling = FALSE;
	    c_token++;
	} else if (equals(c_token, "quiet")) {
	    fit_verbosity = QUIET;
	    c_token++;
	} else if (equals(c_token, "noquiet")) {
	    fit_verbosity = BRIEF;
	    c_token++;
	} else if (equals(c_token, "results")) {
	    fit_verbosity = RESULTS;
	    c_token++;
	} else if (equals(c_token, "brief")) {
	    fit_verbosity = BRIEF;
	    c_token++;
	} else if (equals(c_token, "verbose")) {
	    fit_verbosity = VERBOSE;
	    c_token++;
	} else if (equals(c_token, "prescale")) {
	    fit_prescale = TRUE;
	    c_token++;
	} else if (equals(c_token, "noprescale")) {
	    fit_prescale = FALSE;
	    c_token++;
	} else if (equals(c_token, "limit")) {
	    /* preserve compatibility with FIT_LIMIT user variable */
	    struct udvt_entry *v;
	    double value;

	    c_token++;
	    if (equals(c_token, "default")) {
		c_token++;
		value = 0.;
	    } else
		value = real_expression();
	    if ((value > 0.) && (value < 1.)) {
		v = add_udv_by_name((char *)FITLIMIT);
		v->udv_undef = FALSE;
		Gcomplex(&v->udv_value, value, 0);
	    } else {
		del_udv_by_name((char *)FITLIMIT, FALSE);
	    }
	} else if (equals(c_token, "limit_abs")) {
	    double value;
	    c_token++;
	    value = real_expression();
	    epsilon_abs = (value > 0.) ? value : 0.;
	} else if (equals(c_token, "maxiter")) {
	    /* preserve compatibility with FIT_MAXITER user variable */
	    struct udvt_entry *v;
	    int maxiter;

	    c_token++;
	    if (equals(c_token, "default")) {
		c_token++;
		maxiter = 0;
	    } else
		maxiter = int_expression();
	    if (maxiter > 0) {
		v = add_udv_by_name((char *)FITMAXITER);
		v->udv_undef = FALSE;
		Ginteger(&v->udv_value, maxiter);
	    } else {
		del_udv_by_name((char *)FITMAXITER, FALSE);
	    }
	} else if (equals(c_token, "start_lambda")) {
	    /* preserve compatibility with FIT_START_LAMBDA user variable */
	    struct udvt_entry *v;
	    double value;

	    c_token++;
	    if (equals(c_token, "default")) {
		c_token++;
		value = 0.;
	    } else
		value = real_expression();
	    if (value > 0.) {
		v = add_udv_by_name((char *)FITSTARTLAMBDA);
		v->udv_undef = FALSE;
		Gcomplex(&v->udv_value, value, 0);
	    } else {
		del_udv_by_name((char *)FITSTARTLAMBDA, FALSE);
	    }
	} else if (equals(c_token, "lambda_factor")) {
	    /* preserve compatibility with FIT_LAMBDA_FACTOR user variable */
	    struct udvt_entry *v;
	    double value;

	    c_token++;
	    if (equals(c_token, "default")) {
		c_token++;
		value = 0.;
	    } else
		value = real_expression();
	    if (value > 0.) {
		v = add_udv_by_name((char *)FITLAMBDAFACTOR);
		v->udv_undef = FALSE;
		Gcomplex(&v->udv_value, value, 0);
	    } else {
		del_udv_by_name((char *)FITLAMBDAFACTOR, FALSE);
	    }
	} else if (equals(c_token, "script")) {
	    char *tmp;

	    c_token++;
	    if (END_OF_COMMAND) {
		free(fit_script);
		fit_script = NULL;
	    } else if (equals(c_token, "default")) {
		c_token++;
		free(fit_script);
		fit_script = NULL;
	    } else if ((tmp = try_to_get_string())) {
		free(fit_script);
		fit_script = tmp;
	    } else {
		int_error(c_token, "expecting string");
	    }
	} else if (equals(c_token, "wrap")) {
	    c_token++;
	    fit_wrap = int_expression();
	    if (fit_wrap < 0) fit_wrap = 0;
	} else if (equals(c_token, "nowrap")) {
	    c_token++;
	    fit_wrap = 0;
	} else if (equals(c_token, "v4")) {
	    c_token++;
	    fit_v4compatible = TRUE;
	} else if (equals(c_token, "v5")) {
	    c_token++;
	    fit_v4compatible = FALSE;
	} else {
	    int_error(c_token, "unrecognized option --- see `help set fit`");
	}
    } /* while (!end) */
}


/* process 'set format' command */
void
set_format()
{
    TBOOLEAN set_for_axis[AXIS_ARRAY_SIZE] = AXIS_ARRAY_INITIALIZER(FALSE);
    AXIS_INDEX axis;
    char *format;
    td_type tictype = DT_UNINITIALIZED;

    c_token++;
    if ((axis = lookup_table(axisname_tbl, c_token)) >= 0) {
	set_for_axis[axis] = TRUE;
	c_token++;
    } else if (equals(c_token,"xy") || equals(c_token,"yx")) {
	set_for_axis[FIRST_X_AXIS] = set_for_axis[FIRST_Y_AXIS] = TRUE;
	c_token++;
    } else {
	/* Set all of them */
	for (axis = 0; axis < AXIS_ARRAY_SIZE; axis++)
	    set_for_axis[axis] = TRUE;
    }

    if (END_OF_COMMAND) {
	for (axis = FIRST_AXES; axis <= POLAR_AXIS; axis++) {
	    if (set_for_axis[axis]) {
		free(axis_array[axis].formatstring);
		axis_array[axis].formatstring = gp_strdup(DEF_FORMAT);
		axis_array[axis].tictype = DT_NORMAL;
	    }
	}
	return;
    }

    if (!(format = try_to_get_string()))
	int_error(c_token, "expecting format string");

    if (almost_equals(c_token,"time$date")) {
	tictype = DT_TIMEDATE;
	c_token++;
    } else if (almost_equals(c_token,"geo$graphic")) {
	tictype = DT_DMS;
	c_token++;
    } else if (almost_equals(c_token,"num$eric")) {
	tictype = DT_NORMAL;
	c_token++;
    }

    for (axis = FIRST_AXES; axis <= POLAR_AXIS; axis++) {
	if (set_for_axis[axis]) {
	    free(axis_array[axis].formatstring);
	    axis_array[axis].formatstring = gp_strdup(format);
	    if (tictype != DT_UNINITIALIZED)
		axis_array[axis].tictype = tictype;
	}
    }
    free(format);
}


/* process 'set grid' command */

static void
set_grid()
{
    TBOOLEAN explicit_change = FALSE;
    c_token++;
#define	GRID_MATCH(axis, string)				\
	    if (almost_equals(c_token, string+2)) {		\
		if (string[2] == 'm')				\
		    axis_array[axis].gridminor = TRUE;		\
		else						\
		    axis_array[axis].gridmajor = TRUE;		\
		explicit_change = TRUE;				\
		++c_token;					\
	    } else if (almost_equals(c_token, string)) {	\
		if (string[2] == 'm')				\
		    axis_array[axis].gridminor = FALSE;		\
		else						\
		    axis_array[axis].gridmajor = FALSE;		\
		explicit_change = TRUE;				\
		++c_token;					\
	    }
    while (!END_OF_COMMAND) {
	GRID_MATCH(FIRST_X_AXIS, "nox$tics")
	else GRID_MATCH(FIRST_Y_AXIS, "noy$tics")
	else GRID_MATCH(FIRST_Z_AXIS, "noz$tics")
	else GRID_MATCH(SECOND_X_AXIS, "nox2$tics")
	else GRID_MATCH(SECOND_Y_AXIS, "noy2$tics")
	else GRID_MATCH(FIRST_X_AXIS, "nomx$tics")
	else GRID_MATCH(FIRST_Y_AXIS, "nomy$tics")
	else GRID_MATCH(FIRST_Z_AXIS, "nomz$tics")
	else GRID_MATCH(SECOND_X_AXIS, "nomx2$tics")
	else GRID_MATCH(SECOND_Y_AXIS, "nomy2$tics")
	else GRID_MATCH(COLOR_AXIS, "nocb$tics")
	else GRID_MATCH(COLOR_AXIS, "nomcb$tics")
	else GRID_MATCH(POLAR_AXIS, "nor$tics")
	else GRID_MATCH(POLAR_AXIS, "nomr$tics")
	else if (almost_equals(c_token,"po$lar")) {
	    if (!some_grid_selected())
		axis_array[POLAR_AXIS].gridmajor = TRUE;
	    polar_grid_angle = 30*DEG2RAD;
	    c_token++;
	    if (isanumber(c_token) || type_udv(c_token) == INTGR || type_udv(c_token) == CMPLX)
		polar_grid_angle = ang2rad*real_expression();
	} else if (almost_equals(c_token,"nopo$lar")) {
	    polar_grid_angle = 0; /* not polar grid */
	    c_token++;
	} else if (equals(c_token,"back")) {
	    grid_layer = LAYER_BACK;
	    c_token++;
	} else if (equals(c_token,"front")) {
	    grid_layer = LAYER_FRONT;
	    c_token++;
	} else if (almost_equals(c_token,"layerd$efault")
		|| equals(c_token, "behind")) {
	    grid_layer = LAYER_BEHIND;
	    c_token++;
	} else { /* only remaining possibility is a line type */
	    int save_token = c_token;
	    lp_parse(&grid_lp, LP_ADHOC, FALSE);
	    if (equals(c_token,",")) {
		c_token++;
		lp_parse(&mgrid_lp, LP_ADHOC, FALSE);
	    } else if (save_token != c_token)
		mgrid_lp = grid_lp;
	    if (save_token == c_token)
		break;
	}
    }

    if (!explicit_change && !some_grid_selected()) {
	/* no axis specified, thus select default grid */
	if (polar) {
	    axis_array[POLAR_AXIS].gridmajor = TRUE;
	} else {
	    axis_array[FIRST_X_AXIS].gridmajor = TRUE;
	    axis_array[FIRST_Y_AXIS].gridmajor = TRUE;
	}
    }
}


/* process 'set hidden3d' command */
static void
set_hidden3d()
{
    c_token++;
    set_hidden3doptions();
    hidden3d = TRUE;
}


static void
set_history()
{
    c_token++;

    while (!END_OF_COMMAND) {
	if (equals(c_token, "quiet")) {
	    c_token++;
	    history_quiet = TRUE;
	    continue;
	} else if (almost_equals(c_token, "num$bers")) {
	    c_token++;
	    history_quiet = FALSE; 
	    continue;
	} else if (equals(c_token, "full")) {
	    c_token++;
	    history_full = TRUE;
	    continue;
	} else if (equals(c_token, "trim")) {
	    c_token++;
	    history_full = FALSE;
	    continue;
	} else if (almost_equals(c_token, "def$ault")) {
	    c_token++;
	    history_quiet = FALSE;
	    history_full = TRUE;
	    gnuplot_history_size = HISTORY_SIZE;
	    continue;
	} else if (equals(c_token, "size")) {
	    c_token++;
	    /* fall through */
	}
	/* Catches both the deprecated "set historysize" and "set history size" */
	gnuplot_history_size = int_expression();
#ifndef GNUPLOT_HISTORY
	int_warn(NO_CARET, "This copy of gnuplot was built without support for command history.");
#endif
    }
}


/* process 'set isosamples' command */
static void
set_isosamples()
{
    int tsamp1, tsamp2;

    c_token++;
    tsamp1 = abs(int_expression());
    tsamp2 = tsamp1;
    if (!END_OF_COMMAND) {
	if (!equals(c_token,","))
	    int_error(c_token, "',' expected");
	c_token++;
	tsamp2 = abs(int_expression());
    }
    if (tsamp1 < 2 || tsamp2 < 2)
	int_error(c_token, "sampling rate must be > 1; sampling unchanged");
    else {
	struct curve_points *f_p = first_plot;
	struct surface_points *f_3dp = first_3dplot;

	first_plot = NULL;
	first_3dplot = NULL;
	cp_free(f_p);
	sp_free(f_3dp);

	iso_samples_1 = tsamp1;
	iso_samples_2 = tsamp2;
    }
}


/* When plotting an external key, the margin and l/r/t/b/c are
   used to determine one of twelve possible positions.  They must
   be defined appropriately in the case where stack direction
   determines exact position. */
static void
set_key_position_from_stack_direction(legend_key *key)
{
    if (key->stack_dir == GPKEY_VERTICAL) {
	switch(key->hpos) {
	case LEFT:
	    key->margin = GPKEY_LMARGIN;
	    break;
	case CENTRE:
	    if (key->vpos == JUST_TOP)
		key->margin = GPKEY_TMARGIN;
	    else
		key->margin = GPKEY_BMARGIN;
	    break;
	case RIGHT:
	    key->margin = GPKEY_RMARGIN;
	    break;
	}
    } else {
	switch(key->vpos) {
	case JUST_TOP:
	    key->margin = GPKEY_TMARGIN;
	    break;
	case JUST_CENTRE:
	    if (key->hpos == LEFT)
		key->margin = GPKEY_LMARGIN;
	    else
		key->margin = GPKEY_RMARGIN;
	    break;
	case JUST_BOT:
	    key->margin = GPKEY_BMARGIN;
	    break;
	}
    }
}


/* process 'set key' command */
static void
set_key()
{
    TBOOLEAN vpos_set = FALSE, hpos_set = FALSE, reg_set = FALSE, sdir_set = FALSE;
    char *vpos_warn = "Multiple vertical position settings";
    char *hpos_warn = "Multiple horizontal position settings";
    char *reg_warn = "Multiple location region settings";
    char *sdir_warn = "Multiple stack direction settings";
    legend_key *key = &keyT;

    /* Only for backward compatibility with deprecated "set keytitle foo" */
    if (almost_equals(c_token,"keyt$itle"))
	goto S_KEYTITLE;

    c_token++;
    key->visible = TRUE;

    while (!END_OF_COMMAND) {
	switch(lookup_table(&set_key_tbl[0],c_token)) {
	case S_KEY_ON:
	    key->visible = TRUE;
	    break;
	case S_KEY_OFF:
	    key->visible = FALSE;
	    break;
	case S_KEY_DEFAULT:
	    reset_key();
	    break;
	case S_KEY_TOP:
	    if (vpos_set)
		int_warn(c_token, vpos_warn);
	    key->vpos = JUST_TOP;
	    vpos_set = TRUE;
	    break;
	case S_KEY_BOTTOM:
	    if (vpos_set)
		int_warn(c_token, vpos_warn);
	    key->vpos = JUST_BOT;
	    vpos_set = TRUE;
	    break;
	case S_KEY_LEFT:
	    if (hpos_set)
		int_warn(c_token, hpos_warn);
	    key->hpos = LEFT;
	    hpos_set = TRUE;
	    break;
	case S_KEY_RIGHT:
	    if (hpos_set)
		int_warn(c_token, hpos_warn);
	    key->hpos = RIGHT;
	    hpos_set = TRUE;
	    break;
	case S_KEY_CENTER:
	    if (!vpos_set) key->vpos = JUST_CENTRE;
	    if (!hpos_set) key->hpos = CENTRE;
	    if (vpos_set || hpos_set)
		vpos_set = hpos_set = TRUE;
	    break;
	case S_KEY_VERTICAL:
	    if (sdir_set)
		int_warn(c_token, sdir_warn);
	    key->stack_dir = GPKEY_VERTICAL;
	    sdir_set = TRUE;
	    break;
	case S_KEY_HORIZONTAL:
	    if (sdir_set)
		int_warn(c_token, sdir_warn);
	    key->stack_dir = GPKEY_HORIZONTAL;
	    sdir_set = TRUE;
	    break;
	case S_KEY_OVER:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    /* Fall through */
	case S_KEY_ABOVE:
	    if (!hpos_set)
		key->hpos = CENTRE;
	    if (!sdir_set)
		key->stack_dir = GPKEY_HORIZONTAL;
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_TMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_UNDER:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    /* Fall through */
	case S_KEY_BELOW:
	    if (!hpos_set)
		key->hpos = CENTRE;
	    if (!sdir_set)
		key->stack_dir = GPKEY_HORIZONTAL;
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_BMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_INSIDE:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_INTERIOR_LRTBC;
	    reg_set = TRUE;
	    break;
	case S_KEY_OUTSIDE:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_EXTERIOR_LRTBC;
	    reg_set = TRUE;
	    break;
	case S_KEY_TMARGIN:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_TMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_BMARGIN:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_BMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_LMARGIN:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_LMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_RMARGIN:
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    key->region = GPKEY_AUTO_EXTERIOR_MARGIN;
	    key->margin = GPKEY_RMARGIN;
	    reg_set = TRUE;
	    break;
	case S_KEY_LLEFT:
	    key->just = GPKEY_LEFT;
	    break;
	case S_KEY_RRIGHT:
	    key->just = GPKEY_RIGHT;
	    break;
	case S_KEY_REVERSE:
	    key->reverse = TRUE;
	    break;
	case S_KEY_NOREVERSE:
	    key->reverse = FALSE;
	    break;
	case S_KEY_INVERT:
	    key->invert = TRUE;
	    break;
	case S_KEY_NOINVERT:
	    key->invert = FALSE;
	    break;
	case S_KEY_ENHANCED:
	    key->enhanced = TRUE;
	    break;
	case S_KEY_NOENHANCED:
	    key->enhanced = FALSE;
	    break;
	case S_KEY_BOX:
	    c_token++;
	    key->box.l_type = LT_BLACK;
	    if (!END_OF_COMMAND) {
		int old_token = c_token;
		lp_parse(&key->box, LP_ADHOC, FALSE);
		if (old_token == c_token && isanumber(c_token)) {
		    key->box.l_type = int_expression() - 1;
		    c_token++;
		}
	    }
	    c_token--;  /* is incremented after loop */
	    break;
	case S_KEY_NOBOX:
	    key->box.l_type = LT_NODRAW;
	    break;
	case S_KEY_SAMPLEN:
	    c_token++;
	    key->swidth = real_expression();
	    c_token--; /* it is incremented after loop */
	    break;
	case S_KEY_SPACING:
	    c_token++;
	    key->vert_factor = real_expression();
	    if (key->vert_factor < 0.0)
		key->vert_factor = 0.0;
	    c_token--; /* it is incremented after loop */
	    break;
	case S_KEY_WIDTH:
	    c_token++;
	    key->width_fix = real_expression();
	    c_token--; /* it is incremented after loop */
	    break;
	case S_KEY_HEIGHT:
	    c_token++;
	    key->height_fix = real_expression();
	    c_token--; /* it is incremented after loop */
	    break;
	case S_KEY_AUTOTITLES:
	    if (almost_equals(++c_token, "col$umnheader"))
		key->auto_titles = COLUMNHEAD_KEYTITLES;
	    else {
		key->auto_titles = FILENAME_KEYTITLES;
		c_token--;
	    }
	    break;
	case S_KEY_NOAUTOTITLES:
	    key->auto_titles = NOAUTO_KEYTITLES;
	    break;
	case S_KEY_TITLE:
	     S_KEYTITLE:
	    key->title.pos = CENTRE;
	    set_xyzlabel( &key->title );
	    c_token--;
	    break;
	case S_KEY_NOTITLE:
	    free(key->title.text);
	    key->title.text = NULL;
	    break;
	case S_KEY_FONT:
	    c_token++;
	    /* Make sure they've specified a font */
	    if (!isstringvalue(c_token))
		int_error(c_token,"expected font");
	    else {
		char *tmp = try_to_get_string();
		if (tmp) {
		    free(key->font);
		    key->font = tmp;
		}
		c_token--;
	    }
	    break;
	case S_KEY_TEXTCOLOR:
	    {
	    struct t_colorspec lcolor = DEFAULT_COLORSPEC;
	    parse_colorspec(&lcolor, TC_VARIABLE);
	    /* Only for backwards compatibility */
	    if (lcolor.type == TC_RGB && lcolor.value == -1.0)
		lcolor.type = TC_VARIABLE;
	    key->textcolor = lcolor;
	    }
	    c_token--;
	    break;
	case S_KEY_MAXCOLS:
	    c_token++;
	    if (END_OF_COMMAND || almost_equals(c_token, "a$utomatic"))
		key->maxcols = 0;
	    else
		key->maxcols = int_expression();
	    if (key->maxcols < 0)
		key->maxcols = 0;
	    c_token--; /* it is incremented after loop */
	    break;
	case S_KEY_MAXROWS:
	    c_token++;
	    if (END_OF_COMMAND || almost_equals(c_token, "a$utomatic"))
		key->maxrows = 0;
	    else
		key->maxrows = int_expression();
	    if (key->maxrows < 0)
		key->maxrows = 0;
	    c_token--; /* it is incremented after loop */
	    break;

	case S_KEY_FRONT:
	    key->front = TRUE;
	    break;
	case S_KEY_NOFRONT:
	    key->front = FALSE;
	    break;

	case S_KEY_MANUAL:
	    c_token++;
	    if (reg_set)
		int_warn(c_token, reg_warn);
	    get_position(&key->user_pos);
	    key->region = GPKEY_USER_PLACEMENT;
	    reg_set = TRUE;
	    c_token--;  /* will be incremented again soon */
	    break;

	case S_KEY_INVALID:
	default:
	    int_error(c_token, "unknown key option");
	    break;
	}
	c_token++;
    }

    if (key->region == GPKEY_AUTO_EXTERIOR_LRTBC)
	set_key_position_from_stack_direction(key);
    else if (key->region == GPKEY_AUTO_EXTERIOR_MARGIN) {
	if (vpos_set && (key->margin == GPKEY_TMARGIN || key->margin == GPKEY_BMARGIN))
	    int_warn(NO_CARET,
		     "ignoring top/center/bottom; incompatible with tmargin/bmargin.");
	else if (hpos_set && (key->margin == GPKEY_LMARGIN || key->margin == GPKEY_RMARGIN))
	    int_warn(NO_CARET,
		     "ignoring left/center/right; incompatible with lmargin/tmargin.");
    }
}


/* process 'set label' command */
/* set label {tag} {"label_text"{,<value>{,...}}} {<label options>} */
/* EAM Mar 2003 - option parsing broken out into separate routine */
static void
set_label()
{
    struct text_label *this_label = NULL;
    struct text_label *new_label = NULL;
    struct text_label *prev_label = NULL;
    struct value a;
    int save_token;
    int tag = -1;

    c_token++;
    if (END_OF_COMMAND)
	return;

    /* The first item must be either a tag or the label text */
    save_token = c_token;
    if (isletter(c_token) && type_udv(c_token) == 0) {
	tag = assign_label_tag();
    } else {
	const_express(&a);
	if (a.type == STRING) {
	    c_token = save_token;
	    tag = assign_label_tag();
	    gpfree_string(&a);
	} else {
	    tag = (int) real(&a);
	}
    }

    if (tag <= 0)
	int_error(c_token, "tag must be > zero");

    if (first_label != NULL) {	/* skip to last label */
	for (this_label = first_label; this_label != NULL;
	     prev_label = this_label, this_label = this_label->next)
	    /* is this the label we want? */
	    if (tag <= this_label->tag)
		break;
    }
    /* Insert this label into the list if it is a new one */
    if (this_label == NULL || tag != this_label->tag) {
	new_label = new_text_label(tag);
	new_label->offset = default_offset;
	if (prev_label == NULL)
	    first_label = new_label;
	else
	    prev_label->next = new_label;
	new_label->next = this_label;
	this_label = new_label;
    }

    if (!END_OF_COMMAND) {
	char* text;
	parse_label_options( this_label, FALSE );
	text = try_to_get_string();
	if (text) {
	    free(this_label->text);
	    this_label->text = text;
	}

    }

    /* Now parse the label format and style options */
    parse_label_options( this_label, FALSE );
}


/* assign a new label tag
 * labels are kept sorted by tag number, so this is easy
 * returns the lowest unassigned tag number
 */
static int
assign_label_tag()
{
    struct text_label *this_label;
    int last = 0;		/* previous tag value */

    for (this_label = first_label; this_label != NULL;
	 this_label = this_label->next)
	if (this_label->tag == last + 1)
	    last++;
	else
	    break;

    return (last + 1);
}


/* process 'set loadpath' command */
static void
set_loadpath()
{
    /* We pick up all loadpath elements here before passing
     * them on to set_var_loadpath()
     */
    char *collect = NULL;

    c_token++;
    if (END_OF_COMMAND) {
	clear_loadpath();
    } else while (!END_OF_COMMAND) {
	char *ss;
	if ((ss = try_to_get_string())) {
	    int len = (collect? strlen(collect) : 0);
	    gp_expand_tilde(&ss);
	    collect = gp_realloc(collect, len+1+strlen(ss)+1, "tmp loadpath");
	    if (len != 0) {
		strcpy(collect+len+1,ss);
		*(collect+len) = PATHSEP;
	    }
	    else
		strcpy(collect,ss);
	    free(ss);
	} else {
	    int_error(c_token, "expected string");
	}
    }
    if (collect) {
	set_var_loadpath(collect);
	free(collect);
    }
}


/* process 'set fontpath' command */
static void
set_fontpath()
{
    /* We pick up all fontpath elements here before passing
     * them on to set_var_fontpath()
     */
    char *collect = NULL;

    c_token++;
    if (END_OF_COMMAND) {
	clear_fontpath();
    } else while (!END_OF_COMMAND) {
	char *ss;
	if ((ss = try_to_get_string())) {
	    int len = (collect? strlen(collect) : 0);
	    gp_expand_tilde(&ss);
	    collect = gp_realloc(collect, len+1+strlen(ss)+1, "tmp fontpath");
	    if (len != 0) {
		strcpy(collect+len+1,ss);
		*(collect+len) = PATHSEP;
	    }
	    else
		strcpy(collect,ss);
	    free(ss);
	} else {
	    int_error(c_token, "expected string");
	}
    }
    if (collect) {
	set_var_fontpath(collect);
	free(collect);
    }
}


/* process 'set locale' command */
static void
set_locale()
{
    char *s;

    c_token++;
    if (END_OF_COMMAND) {
	init_locale();
    } else if ((s = try_to_get_string())) {
	set_var_locale(s);
	free(s);
    } else
	int_error(c_token, "expected string");
}


/* process 'set logscale' command */
static void
set_logscale()
{
    TBOOLEAN set_for_axis[AXIS_ARRAY_SIZE] = AXIS_ARRAY_INITIALIZER(FALSE);
    int axis;
    double newbase = 10;
    c_token++;

    if (END_OF_COMMAND) {
	for (axis = 0; axis < LAST_REAL_AXIS; axis++)
	    set_for_axis[axis] = TRUE;
    } else {
	/* do reverse search because of "x", "x1", "x2" sequence in axisname_tbl */
	int i = 0;
	while (i < token[c_token].length) {
	    axis = lookup_table_nth_reverse(axisname_tbl, LAST_REAL_AXIS+1,
		       gp_input_line + token[c_token].start_index + i);
	    if (axis < 0) {
		token[c_token].start_index += i;
		int_error(c_token, "invalid axis");
	    }
	    set_for_axis[axisname_tbl[axis].value] = TRUE;
	    i += strlen(axisname_tbl[axis].key);
	}
	c_token++;

	if (!END_OF_COMMAND) {
	    newbase = fabs(real_expression());
	    if (newbase <= 1.0)
		int_error(c_token,
			  "log base must be > 1.0; logscale unchanged");
	}
    }

    for (axis = 0; axis <= LAST_REAL_AXIS; axis++) {
	if (set_for_axis[axis]) {
	    axis_array[axis].log = TRUE;
	    axis_array[axis].base = newbase;
	    axis_array[axis].log_base = log(newbase);
	    if ((axis == POLAR_AXIS) && polar)
		rrange_to_xy();
	}
    }

    /* Because the log scaling is applied during data input, a quick refresh */
    /* using existing stored data will not work if the log setting changes.  */
    SET_REFRESH_OK(E_REFRESH_NOT_OK, 0);
}

/* process 'set mapping3d' command */
static void
set_mapping()
{
    c_token++;
    if (END_OF_COMMAND)
	/* assuming same as points */
	mapping3d = MAP3D_CARTESIAN;
    else if (almost_equals(c_token, "ca$rtesian"))
	mapping3d = MAP3D_CARTESIAN;
    else if (almost_equals(c_token, "s$pherical"))
	mapping3d = MAP3D_SPHERICAL;
    else if (almost_equals(c_token, "cy$lindrical"))
	mapping3d = MAP3D_CYLINDRICAL;
    else
	int_error(c_token,
		  "expecting 'cartesian', 'spherical', or 'cylindrical'");
	c_token++;
}


/* process 'set {blrt}margin' command */
static void
set_margin(t_position *margin)
{
    margin->scalex = character;
    margin->x = -1;
    c_token++;

    if (END_OF_COMMAND)
	return;

    if (equals(c_token,"at") && !almost_equals(++c_token,"sc$reen"))
	int_error(c_token,"expecting 'screen <fraction>'");
    if (almost_equals(c_token,"sc$reen")) {
	margin->scalex = screen;
	c_token++;
    }

    margin->x = real_expression();
    if (margin->x < 0)
	margin->x = -1;

    if (margin->scalex == screen) {
	if (margin->x < 0)
	    margin->x = 0;
	if (margin->x > 1)
	    margin->x = 1;
    }

}

/* process 'set minus_sign' command */
static void
set_minus_sign()
{
    c_token++;
    use_minus_sign = TRUE;
}

static void
set_separator()
{
    c_token++;
    free(df_separators);
    df_separators = NULL;

    if (END_OF_COMMAND)
	return;

    if (almost_equals(c_token, "white$space")) {
	c_token++;
    } else if (equals(c_token, "comma")) {
	df_separators = gp_strdup(",");
	c_token++;
    } else if (equals(c_token, "tab") || equals(c_token, "\'\\t\'")) {
	df_separators = gp_strdup("\t");
	c_token++;
    } else if (!(df_separators = try_to_get_string())) {
	int_error(c_token, "expected \"<separator_char>\"");
    }
}

static void
set_datafile_commentschars()
{
    char *s;

    c_token++;

    if (END_OF_COMMAND) {
	free(df_commentschars);
	df_commentschars = gp_strdup(DEFAULT_COMMENTS_CHARS);
    } else if ((s = try_to_get_string())) {
	free(df_commentschars);
	df_commentschars = s;
    } else /* Leave it the way it was */
	int_error(c_token, "expected string with comments chars");
}

/* process 'set missing' command */
static void
set_missing()
{
    c_token++;
    if (END_OF_COMMAND) {
	free(missing_val);
	missing_val = NULL;
    } else if (!(missing_val = try_to_get_string()))
	int_error(c_token, "expected missing-value string");
}

/* (version 5) 'set monochrome' command */
static void
set_monochrome()
{
    monochrome = TRUE;
    if (!END_OF_COMMAND)
	c_token++;

    if (almost_equals(c_token, "def$ault")) {
	c_token++;
	while (first_mono_linestyle)
	    delete_linestyle(&first_mono_linestyle, first_mono_linestyle, first_mono_linestyle);
    }

    init_monochrome();

    if (almost_equals(c_token, "linet$ype") || equals(c_token, "lt")) {
	/* we can pass this off to the generic "set linetype" code */
	if (equals(c_token+1,"cycle")) {
	    c_token += 2;
	    mono_recycle_count = int_expression();
	} else
	    set_linestyle(&first_mono_linestyle, LP_TYPE);
    }

    if (!END_OF_COMMAND)
	int_error(c_token, "unrecognized option");
}

#ifdef USE_MOUSE
static void
set_mouse()
{
    char *ctmp;

    c_token++;
    mouse_setting.on = 1;

    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "do$ubleclick")) {
	    ++c_token;
	    mouse_setting.doubleclick = real_expression();
	    if (mouse_setting.doubleclick < 0)
		mouse_setting.doubleclick = 0;
	} else if (almost_equals(c_token, "nodo$ubleclick")) {
	    mouse_setting.doubleclick = 0; /* double click off */
	    ++c_token;
	} else if (almost_equals(c_token, "zoomco$ordinates")) {
	    mouse_setting.annotate_zoom_box = 1;
	    ++c_token;
	} else if (almost_equals(c_token, "nozoomco$ordinates")) {
	    mouse_setting.annotate_zoom_box = 0;
	    ++c_token;
	} else if (almost_equals(c_token, "po$lardistancedeg")) {
	    mouse_setting.polardistance = 1;
	    UpdateStatusline();
	    ++c_token;
	} else if (almost_equals(c_token, "polardistancet$an")) {
	    mouse_setting.polardistance = 2;
	    UpdateStatusline();
	    ++c_token;
	} else if (almost_equals(c_token, "nopo$lardistance")) {
	    mouse_setting.polardistance = 0;
	    UpdateStatusline();
	    ++c_token;
	} else if (almost_equals(c_token, "label$s")) {
	    mouse_setting.label = 1;
	    ++c_token;
	    /* check if the optional argument "<label options>" is present */
	    if (isstringvalue(c_token) && (ctmp = try_to_get_string())) {
		free(mouse_setting.labelopts);
		mouse_setting.labelopts = ctmp;
	    }
	} else if (almost_equals(c_token, "nola$bels")) {
	    mouse_setting.label = 0;
	    ++c_token;
	} else if (almost_equals(c_token, "ve$rbose")) {
	    mouse_setting.verbose = 1;
	    ++c_token;
	} else if (almost_equals(c_token, "nove$rbose")) {
	    mouse_setting.verbose = 0;
	    ++c_token;
	} else if (almost_equals(c_token, "zoomju$mp")) {
	    mouse_setting.warp_pointer = 1;
	    ++c_token;
	} else if (almost_equals(c_token, "nozoomju$mp")) {
	    mouse_setting.warp_pointer = 0;
	    ++c_token;
	} else if (almost_equals(c_token, "fo$rmat")) {
	    ++c_token;
	    if (isstringvalue(c_token) && (ctmp = try_to_get_string())) {
		if (mouse_setting.fmt != mouse_fmt_default)
		    free(mouse_setting.fmt);
		mouse_setting.fmt = ctmp;
	    } else
		mouse_setting.fmt = mouse_fmt_default;
	} else if (almost_equals(c_token, "mo$useformat")) {
	    ++c_token;
	    if (isstringvalue(c_token) && (ctmp = try_to_get_string())) {
		free(mouse_alt_string);
		mouse_alt_string = ctmp;
		if (!strlen(mouse_alt_string)) {
		    free(mouse_alt_string);
		    mouse_alt_string = NULL;
		    if (MOUSE_COORDINATES_ALT == mouse_mode)
			mouse_mode = MOUSE_COORDINATES_REAL;
		} else {
		    mouse_mode = MOUSE_COORDINATES_ALT;
		}
		c_token++;
	    } else {
		int itmp = int_expression();
		if (itmp >= MOUSE_COORDINATES_REAL
		    && itmp <= MOUSE_COORDINATES_ALT) {
		    if (MOUSE_COORDINATES_ALT == itmp && !mouse_alt_string) {
			fprintf(stderr,
			    "please 'set mouse mouseformat <fmt>' first.\n");
		    } else {
			mouse_mode = itmp;
		    }
		} else {
		    fprintf(stderr, "should be: %d <= mouseformat <= %d\n",
			MOUSE_COORDINATES_REAL, MOUSE_COORDINATES_ALT);
		}
	    }
	} else if (almost_equals(c_token, "noru$ler")) {
	    c_token++;
	    set_ruler(FALSE, -1, -1);
	} else if (almost_equals(c_token, "ru$ler")) {
	    c_token++;
    	    if (END_OF_COMMAND || !equals(c_token, "at")) {
		set_ruler(TRUE, -1, -1);
	    } else { /* set mouse ruler at ... */
		struct position where;
		int x, y;
		c_token++;
		if (END_OF_COMMAND)
		    int_error(c_token, "expecting ruler coordinates");
		get_position(&where);
		map_position(&where, &x, &y, "ruler at");
		set_ruler(TRUE, (int)x, (int)y);
	    }
	} else if (almost_equals(c_token, "zoomfac$tors")) {
	    double x = 1.0, y = 1.0;
	    c_token++;
	    if (!END_OF_COMMAND) {
		x = real_expression();
		if (equals(c_token,",")) {
		    c_token++;
		    y = real_expression();
		}
	    }
	    mouse_setting.xmzoom_factor = x;
	    mouse_setting.ymzoom_factor = y;
	} else {
	    if (!END_OF_COMMAND)
    		int_error(c_token, "wrong option");
	    break;
	}
    }
#ifdef OS2
    PM_update_menu_items();
#endif
}
#endif

/* process 'set offsets' command */
static void
set_offsets()
{
    c_token++;
    if (END_OF_COMMAND) {
	loff.x = roff.x = toff.y = boff.y = 0.0;
	return;
    }

    loff.scalex = first_axes;
    if (almost_equals(c_token,"gr$aph")) {
	loff.scalex = graph;
	c_token++;
    }
    loff.x = real_expression();
    if (!equals(c_token, ","))
	return;

    roff.scalex = first_axes;
    if (almost_equals(++c_token,"gr$aph")) {
	roff.scalex = graph;
	c_token++;
    }
    roff.x = real_expression();
    if (!equals(c_token, ","))
	return;

    toff.scaley = first_axes;
    if (almost_equals(++c_token,"gr$aph")) {
	toff.scaley = graph;
	c_token++;
    }
    toff.y = real_expression();
    if (!equals(c_token, ","))
	return;

    boff.scaley = first_axes;
    if (almost_equals(++c_token,"gr$aph")) {
	boff.scaley = graph;
	c_token++;
    }
    boff.y = real_expression();
}


/* process 'set origin' command */
static void
set_origin()
{
    c_token++;
    if (END_OF_COMMAND) {
	xoffset = 0.0;
	yoffset = 0.0;
    } else {
	xoffset = real_expression();
	if (!equals(c_token,","))
	    int_error(c_token, "',' expected");
	c_token++;
	yoffset = real_expression();
    }
}


/* process 'set output' command */
static void
set_output()
{
    char *testfile;

    c_token++;
    if (multiplot)
	int_error(c_token, "you can't change the output in multiplot mode");

    if (END_OF_COMMAND) {	/* no file specified */
	term_set_output(NULL);
	if (outstr) {
	    free(outstr);
	    outstr = NULL; /* means STDOUT */
	}
    } else if ((testfile = try_to_get_string())) {
	gp_expand_tilde(&testfile);
	term_set_output(testfile);
	if (testfile != outstr) {
	    if (testfile)
		free(testfile);
	    testfile = outstr;
	}
	/* if we get here then it worked, and outstr now = testfile */
    } else
	int_error(c_token, "expecting filename");

    /* Invalidate previous palette */
    invalidate_palette();

}


/* process 'set print' command */
static void
set_print()
{
    TBOOLEAN append_p = FALSE;
    char *testfile = NULL;

    c_token++;
    if (END_OF_COMMAND) {	/* no file specified */
	print_set_output(NULL, FALSE, append_p);
    } else if (equals(c_token, "$") && isletter(c_token + 1)) { /* datablock */
	/* NB: has to come first because try_to_get_string will choke on the datablock name */
	char * datablock_name = strdup(parse_datablock_name());
	if (!END_OF_COMMAND) {
	    if (equals(c_token, "append")) {
		append_p = TRUE;
		c_token++;
	    } else {
		int_error(c_token, "expecting keyword \'append\'");
	    }
	}
	print_set_output(datablock_name, TRUE, append_p);
    } else if ((testfile = try_to_get_string())) {  /* file name */
	gp_expand_tilde(&testfile);
	if (!END_OF_COMMAND) {
	    if (equals(c_token, "append")) {
		append_p = TRUE;
		c_token++;
	    } else {
		int_error(c_token, "expecting keyword \'append\'");
	    }
	}
	print_set_output(testfile, FALSE, append_p);
    } else
	int_error(c_token, "expecting filename or datablock");
}

/* process 'set psdir' command */
static void
set_psdir()
{
    c_token++;
    if (END_OF_COMMAND) {	/* no file specified */
	free(PS_psdir);
	PS_psdir = NULL;
    } else if ((PS_psdir = try_to_get_string())) {
	gp_expand_tilde(&PS_psdir);
    } else
	int_error(c_token, "expecting filename");
}

/* process 'set parametric' command */
static void
set_parametric()
{
    c_token++;

    if (!parametric) {
	parametric = TRUE;
	if (!polar) { /* already done for polar */
	    strcpy (set_dummy_var[0], "t");
	    strcpy (set_dummy_var[1], "y");
	    if (interactive)
		(void) fprintf(stderr,"\n\tdummy variable is t for curves, u/v for surfaces\n");
	}
    }
}


/* is resetting palette enabled?
 * note: reset_palette() is disabled within 'test palette'
 */
int enable_reset_palette = 1;

/* default settings for palette */
void
reset_palette()
{
    if (!enable_reset_palette) return;
    free(sm_palette.gradient);
    free(sm_palette.color);
    free_at(sm_palette.Afunc.at);
    free_at(sm_palette.Bfunc.at);
    free_at(sm_palette.Cfunc.at);
    init_color();
    pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_NONE;
}



/* Process 'set palette defined' gradient specification */
/* Syntax
 *   set palette defined   -->  use default palette
 *   set palette defined ( <pos1> <colorspec1>, ... , <posN> <colorspecN> )
 *     <posX>  gray value, automatically rescaled to [0, 1]
 *     <colorspecX>   :=  { "<color_name>" | "<X-style-color>" |  <r> <g> <b> }
 *        <color_name>     predefined colors (see below)
 *        <X-style-color>  "#rrggbb" with 2char hex values for red, green, blue
 *        <r> <g> <b>      three values in [0, 1] for red, green and blue
 *   return 1 if named colors where used, 0 otherwise
 */
static int
set_palette_defined()
{
    double p=0, r=0, g=0, b=0;
    int num, named_colors=0;
    int actual_size=8;

    /* Invalidate previous gradient */
    invalidate_palette();

    free( sm_palette.gradient );
    sm_palette.gradient = gp_alloc( actual_size*sizeof(gradient_struct), "pm3d gradient" );
    sm_palette.smallest_gradient_interval = 1;

    if (END_OF_COMMAND) {
	/* lets use some default gradient */
	double pal[][4] = { {0.0, 0.05, 0.05, 0.2}, {0.1, 0, 0, 1},
			    {0.25, 0.7, 0.85, 0.9}, {0.4, 0, 0.75, 0},
			    {0.5, 1, 1, 0}, {0.7, 1, 0, 0},
			    {0.9, 0.6, 0.6, 0.6}, {1.0, 0.95, 0.95, 0.95} };
	int i;
	for( i=0; i<8; ++i ) {
	    sm_palette.gradient[i].pos = pal[i][0];
	    sm_palette.gradient[i].col.r = pal[i][1];
	    sm_palette.gradient[i].col.g = pal[i][2];
	    sm_palette.gradient[i].col.b = pal[i][3];
	}
	sm_palette.gradient_num = 8;
	sm_palette.cmodel = C_MODEL_RGB;
	sm_palette.smallest_gradient_interval = 0.1;  /* From pal[][] */
	c_token--; /* Caller will increment! */
	return 0;
    }

    if ( !equals(c_token,"(") )
	int_error( c_token, "expected ( to start gradient definition" );

    ++c_token;
    num = -1;

    while (!END_OF_COMMAND) {
	char *col_str;
	p = real_expression();
	col_str = try_to_get_string();
	if (col_str) {
	    /* either color name or X-style rgb value "#rrggbb" */
	    if (col_str[0] == '#' || col_str[0] == '0') {
		/* X-style specifier */
		int rr,gg,bb;
		if ((sscanf( col_str, "#%2x%2x%2x", &rr, &gg, &bb ) != 3 )
		&&  (sscanf( col_str, "0x%2x%2x%2x", &rr, &gg, &bb ) != 3 ))
		    int_error( c_token-1,
			       "Unknown color specifier. Use '#RRGGBB' of '0xRRGGBB'." );
		r = (double)(rr)/255.;
		g = (double)(gg)/255.;
		b = (double)(bb)/255.;
	    }
	    else { /* some predefined names */
		/* Maybe we could scan the X11 rgb.txt file to look up color
		 * names?  Or at least move these definitions to some file
		 * which is included somehow during compilation instead
		 * hardcoding them. */
		/* Can't use lookupt_table() as it works for tokens only,
		   so we'll do it manually */
		const struct gen_table *tbl = pm3d_color_names_tbl;
		while (tbl->key) {
		    if (!strcmp(col_str, tbl->key)) {
			r = (double)((tbl->value >> 16 ) & 255) / 255.;
			g = (double)((tbl->value >> 8 ) & 255) / 255.;
			b = (double)(tbl->value & 255) / 255.;
			break;
		    }
		    tbl++;
		}
		if (!tbl->key)
		    int_error( c_token-1, "Unknown color name." );
		named_colors = 1;
	    }
	    free(col_str);
	} else {
	    /* numerical rgb, hsv, xyz, ... values  [0,1] */
	    r = real_expression();
	    if (r<0 || r>1 )  int_error(c_token-1,"Value out of range [0,1].");
	    g = real_expression();
	    if (g<0 || g>1 )  int_error(c_token-1,"Value out of range [0,1].");
	    b = real_expression();
	    if (b<0 || b>1 )  int_error(c_token-1,"Value out of range [0,1].");
	}
	++num;

	if ( num >= actual_size ) {
	    /* get more space for the gradient */
	    actual_size += 10;
	    sm_palette.gradient = gp_realloc( sm_palette.gradient,
			  actual_size*sizeof(gradient_struct),
			  "pm3d gradient" );
	}
	sm_palette.gradient[num].pos = p;
	sm_palette.gradient[num].col.r = r;
	sm_palette.gradient[num].col.g = g;
	sm_palette.gradient[num].col.b = b;
	if (equals(c_token,")") ) break;
	if ( !equals(c_token,",") )
	    int_error( c_token, "expected comma" );
	++c_token;

    }

    sm_palette.gradient_num = num + 1;
    check_palette_grayscale();

    return named_colors;
}


/*  process 'set palette file' command
 *  load a palette from file, honor datafile modifiers
 */
static void
set_palette_file()
{
    int specs;
    double v[4];
    int i, j, actual_size;
    char *file_name;

    ++c_token;

    /* get filename */
    if (!(file_name = try_to_get_string()))
	int_error(c_token, "missing filename");

    df_set_plot_mode(MODE_QUERY);	/* Needed only for binary datafiles */
    specs = df_open(file_name, 4, NULL);
    free(file_name);

    if (specs > 0 && specs < 3)
	int_error( c_token, "Less than 3 using specs for palette");

    if (sm_palette.gradient) {
	free( sm_palette.gradient );
	sm_palette.gradient = 0;
    }
    actual_size = 10;
    sm_palette.gradient =
      gp_alloc( actual_size*sizeof(gradient_struct), "gradient" );

    i = 0;

#define VCONSTRAIN(x) ( (x)<0 ? 0 : ( (x)>1 ? 1: (x) ) )
    /* values are simply clipped to [0,1] without notice */
    while ((j = df_readline(v, 4)) != DF_EOF) {
	if (i >= actual_size) {
	  actual_size += 10;
	  sm_palette.gradient = (gradient_struct*)
	    gp_realloc( sm_palette.gradient,
			actual_size*sizeof(gradient_struct),
			"pm3d gradient" );
	}
	switch (j) {
	    case 3:
		sm_palette.gradient[i].col.r = VCONSTRAIN(v[0]);
		sm_palette.gradient[i].col.g = VCONSTRAIN(v[1]);
		sm_palette.gradient[i].col.b = VCONSTRAIN(v[2]);
		sm_palette.gradient[i].pos = i ;
		break;
	    case 4:
		sm_palette.gradient[i].col.r = VCONSTRAIN(v[1]);
		sm_palette.gradient[i].col.g = VCONSTRAIN(v[2]);
		sm_palette.gradient[i].col.b = VCONSTRAIN(v[3]);
		sm_palette.gradient[i].pos = v[0];
		break;
	    default:
		df_close();
		int_error(c_token, "Bad data on line %d", df_line_number);
		break;
	}
	++i;
    }
#undef VCONSTRAIN
    df_close();
    if (i==0)
	int_error( c_token, "No valid palette found" );

    sm_palette.gradient_num = i;
    check_palette_grayscale();

}


/* Process a 'set palette function' command.
 *  Three functions with fixed dummy variable gray are registered which
 *  map gray to the different color components.
 *  If ALLOW_DUMMY_VAR_FOR_GRAY is set:
 *    A different dummy variable may proceed the formulae in quotes.
 *    This syntax is different from the usual '[u=<start>:<end>]', but
 *    as <start> and <end> are fixed to 0 and 1 you would have to type
 *    always '[u=]' which looks strange, especially as just '[u]'
 *    wouldn't work.
 *  If unset:  dummy variable is fixed to 'gray'.
 */
static void
set_palette_function()
{
    int start_token;
    char saved_dummy_var[MAX_ID_LEN+1];

    ++c_token;
    strncpy( saved_dummy_var, c_dummy_var[0], MAX_ID_LEN );

    /* set dummy variable */
#ifdef ALLOW_DUMMY_VAR_FOR_GRAY
    if (isstring(c_token)) {
	quote_str( c_dummy_var[0], c_token, MAX_ID_LEN );
	++c_token;
    }
    else
#endif /* ALLOW_DUMMY_VAR_FOR_GRAY */
    strncpy( c_dummy_var[0], "gray", MAX_ID_LEN );


    /* Afunc */
    start_token = c_token;
    if (sm_palette.Afunc.at) {
	free_at( sm_palette.Afunc.at );
	sm_palette.Afunc.at = NULL;
    }
    dummy_func = &sm_palette.Afunc;
    sm_palette.Afunc.at = perm_at();
    if (! sm_palette.Afunc.at)
	int_error(start_token, "not enough memory for function");
    m_capture(&(sm_palette.Afunc.definition), start_token, c_token-1);
    dummy_func = NULL;
    if (!equals(c_token,","))
	int_error(c_token,"expected comma" );
    ++c_token;

    /* Bfunc */
    start_token = c_token;
    if (sm_palette.Bfunc.at) {
	free_at( sm_palette.Bfunc.at );
	sm_palette.Bfunc.at = NULL;
    }
    dummy_func = &sm_palette.Bfunc;
    sm_palette.Bfunc.at = perm_at();
    if (! sm_palette.Bfunc.at)
	int_error(start_token, "not enough memory for function");
    m_capture(&(sm_palette.Bfunc.definition), start_token, c_token-1);
    dummy_func = NULL;
    if (!equals(c_token,","))
	int_error(c_token,"expected comma" );
    ++c_token;

    /* Cfunc */
    start_token = c_token;
    if (sm_palette.Cfunc.at) {
	free_at( sm_palette.Cfunc.at );
	sm_palette.Cfunc.at = NULL;
    }
    dummy_func = &sm_palette.Cfunc;
    sm_palette.Cfunc.at = perm_at();
    if (! sm_palette.Cfunc.at)
	int_error(start_token, "not enough memory for function");
    m_capture(&(sm_palette.Cfunc.definition), start_token, c_token-1);
    dummy_func = NULL;

    strncpy( c_dummy_var[0], saved_dummy_var, MAX_ID_LEN );
}


/*
 *  Normalize gray scale of gradient to fill [0,1] and
 *  complain if gray values are not strictly increasing.
 *  Maybe automatic sorting of the gray values could be a
 *  feature.
 */
static void
check_palette_grayscale()
{
    int i;
    double off, f;
    gradient_struct *gradient = sm_palette.gradient;

    /* check if gray values are sorted */
    for (i=0; i<sm_palette.gradient_num-1; ++i ) {
	if (gradient[i].pos > gradient[i+1].pos) {
	    int_error( c_token, "Gray scale not sorted in gradient." );
	}
    }

    /* fit gray axis into [0:1]:  subtract offset and rescale */
    off = gradient[0].pos;
    f = 1.0 / ( gradient[sm_palette.gradient_num-1].pos-off );
    for (i=1; i<sm_palette.gradient_num-1; ++i ) {
	gradient[i].pos = f*(gradient[i].pos-off);
    }

    /* paranoia on the first and last entries */
    gradient[0].pos = 0.0;
    gradient[sm_palette.gradient_num-1].pos = 1.0;

    /* save smallest interval */
    sm_palette.smallest_gradient_interval = 1.0;
    for (i=1; i<sm_palette.gradient_num-1; ++i ) {
	if (((gradient[i].pos - gradient[i-1].pos) > 0)
	&&  (sm_palette.smallest_gradient_interval > (gradient[i].pos - gradient[i-1].pos)))
	     sm_palette.smallest_gradient_interval = (gradient[i].pos - gradient[i-1].pos);
    }
}

#define CHECK_TRANSFORM  do {				  \
    if (transform_defined)				  \
	int_error(c_token, "inconsistent palette options" ); \
    transform_defined = 1;				  \
}  while(0)

/* Process 'set palette' command */
static void
set_palette()
{
    int transform_defined = 0;
    int named_color = 0;

    c_token++;

    if (END_OF_COMMAND) /* reset to default settings */
	reset_palette();
    else { /* go through all options of 'set palette' */
	for ( ; !END_OF_COMMAND; c_token++ ) {
	    switch (lookup_table(&set_palette_tbl[0],c_token)) {
	    /* positive and negative picture */
	    case S_PALETTE_POSITIVE: /* "pos$itive" */
		sm_palette.positive = SMPAL_POSITIVE;
		continue;
	    case S_PALETTE_NEGATIVE: /* "neg$ative" */
		sm_palette.positive = SMPAL_NEGATIVE;
		continue;
	    /* Now the options that determine the palette of smooth colours */
	    /* gray or rgb-coloured */
	    case S_PALETTE_GRAY: /* "gray" */
		sm_palette.colorMode = SMPAL_COLOR_MODE_GRAY;
		continue;
	    case S_PALETTE_GAMMA: /* "gamma" */
		++c_token;
		sm_palette.gamma = real_expression();
		--c_token;
		continue;
	    case S_PALETTE_COLOR: /* "col$or" */
		if (pm3d_last_set_palette_mode != SMPAL_COLOR_MODE_NONE) {
		    sm_palette.colorMode = pm3d_last_set_palette_mode;
		} else {
		    sm_palette.colorMode = SMPAL_COLOR_MODE_RGB;
		}
		continue;
	    /* rgb color mapping formulae: rgb$formulae r,g,b (3 integers) */
	    case S_PALETTE_RGBFORMULAE: { /* "rgb$formulae" */
		int i;
		char * formerr = "color formula out of range (use `show palette rgbformulae' to display the range)";

		CHECK_TRANSFORM;
		c_token++;
		i = int_expression();
		if (abs(i) >= sm_palette.colorFormulae)
		    int_error(c_token, formerr);
		sm_palette.formulaR = i;
		if (!equals(c_token--,","))
		    continue;
		c_token += 2;
		i = int_expression();
		if (abs(i) >= sm_palette.colorFormulae)
		    int_error(c_token, formerr);
		sm_palette.formulaG = i;
		if (!equals(c_token--,","))
		    continue;
		c_token += 2;
		i = int_expression();
		if (abs(i) >= sm_palette.colorFormulae)
		    int_error(c_token, formerr);
		sm_palette.formulaB = i;
		c_token--;
		sm_palette.colorMode = SMPAL_COLOR_MODE_RGB;
		pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_RGB;
		continue;
	    } /* rgbformulae */
	    /* rgb color mapping based on the "cubehelix" scheme proposed by */
	    /* D A Green (2011)  http://arxiv.org/abs/1108.5083		     */
	    case S_PALETTE_CUBEHELIX: { /* cubehelix */
		TBOOLEAN done = FALSE;
		CHECK_TRANSFORM;
		sm_palette.colorMode = SMPAL_COLOR_MODE_CUBEHELIX;
		sm_palette.cubehelix_start = 0.5;
		sm_palette.cubehelix_cycles = -1.5;
		sm_palette.cubehelix_saturation = 1.0;
		c_token++;
		do {
		    if (equals(c_token,"start")) {
			c_token++;
			sm_palette.cubehelix_start = real_expression();
		    }
		    else if (almost_equals(c_token,"cyc$les")) {
			c_token++;
			sm_palette.cubehelix_cycles = real_expression();
		    }
		    else if (almost_equals(c_token, "sat$uration")) {
			c_token++;
			sm_palette.cubehelix_saturation = real_expression();
		    }
		    else
			done = TRUE;
		} while (!done);
		--c_token;
		continue;
	    } /* cubehelix */
	    case S_PALETTE_DEFINED: { /* "def$ine" */
		CHECK_TRANSFORM;
		++c_token;
		named_color = set_palette_defined();
		sm_palette.colorMode = SMPAL_COLOR_MODE_GRADIENT;
		pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_GRADIENT;
		continue;
	    }
	    case S_PALETTE_FILE: { /* "file" */
		CHECK_TRANSFORM;
		set_palette_file();
		sm_palette.colorMode = SMPAL_COLOR_MODE_GRADIENT;
		pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_GRADIENT;
		--c_token;
		continue;
	    }
	    case S_PALETTE_FUNCTIONS: { /* "func$tions" */
		CHECK_TRANSFORM;
		set_palette_function();
		sm_palette.colorMode = SMPAL_COLOR_MODE_FUNCTIONS;
		pm3d_last_set_palette_mode = SMPAL_COLOR_MODE_FUNCTIONS;
		--c_token;
		continue;
	    }
	    case S_PALETTE_MODEL: { /* "mo$del" */
		int model;

		++c_token;
		if (END_OF_COMMAND)
		    int_error( c_token, "expected color model" );
		model = lookup_table(&color_model_tbl[0],c_token);
		if (model == -1)
		    int_error(c_token,"unknown color model");
		sm_palette.cmodel = model;
		continue;
	    }
	    /* ps_allcF: write all rgb formulae into PS file? */
	    case S_PALETTE_NOPS_ALLCF: /* "nops_allcF" */
		sm_palette.ps_allcF = FALSE;
		continue;
	    case S_PALETTE_PS_ALLCF: /* "ps_allcF" */
		sm_palette.ps_allcF = TRUE;
		continue;
	    /* max colors used */
	    case S_PALETTE_MAXCOLORS: { /* "maxc$olors" */
		int i;

		c_token++;
		i = int_expression();
		if (i<0 || i==1)
		    int_warn(c_token,"maxcolors must be > 1");
		else
		    sm_palette.use_maxcolors = i;
		--c_token;
		continue;
	    }
	    } /* switch over palette lookup table */
	    int_error(c_token,"invalid palette option");
	} /* end of while !end of command over palette options */
    } /* else(arguments found) */

    if (named_color && sm_palette.cmodel != C_MODEL_RGB && interactive)
	int_warn(NO_CARET,
		 "Named colors will produce strange results if not in color mode RGB." );

    /* Invalidate previous palette */
    invalidate_palette();
}

#undef CHECK_TRANSFORM

/* process 'set colorbox' command */
static void
set_colorbox()
{
    c_token++;

    if (END_OF_COMMAND) /* reset to default position */
	color_box.where = SMCOLOR_BOX_DEFAULT;
    else { /* go through all options of 'set colorbox' */
	for ( ; !END_OF_COMMAND; c_token++ ) {
	    switch (lookup_table(&set_colorbox_tbl[0],c_token)) {
	    /* vertical or horizontal color gradient */
	    case S_COLORBOX_VERTICAL: /* "v$ertical" */
		color_box.rotation = 'v';
		continue;
	    case S_COLORBOX_HORIZONTAL: /* "h$orizontal" */
		color_box.rotation = 'h';
		continue;
	    /* color box where: default position */
	    case S_COLORBOX_DEFAULT: /* "def$ault" */
		color_box.where = SMCOLOR_BOX_DEFAULT;
		continue;
	    /* color box where: position by user */
	    case S_COLORBOX_USER: /* "u$ser" */
		color_box.where = SMCOLOR_BOX_USER;
		continue;
	    /* color box layer: front or back */
	    case S_COLORBOX_FRONT: /* "fr$ont" */
		color_box.layer = LAYER_FRONT;
		continue;
	    case S_COLORBOX_BACK: /* "ba$ck" */
		color_box.layer = LAYER_BACK;
		continue;
	    /* border of the color box */
	    case S_COLORBOX_BORDER: /* "bo$rder" */

		color_box.border = 1;
		c_token++;

		if (!END_OF_COMMAND) {
		    /* expecting a border line type */
		    color_box.border_lt_tag = int_expression();
		    if (color_box.border_lt_tag <= 0) {
			color_box.border_lt_tag = 0;
			int_error(c_token, "tag must be strictly positive (see `help set style line')");
		    }
		    --c_token;
		}
		continue;
	    case S_COLORBOX_BDEFAULT: /* "bd$efault" */
		color_box.border_lt_tag = -1; /* use default border */
		continue;
	    case S_COLORBOX_NOBORDER: /* "nobo$rder" */
		color_box.border = 0;
		continue;
	    /* colorbox origin */
	    case S_COLORBOX_ORIGIN: /* "o$rigin" */
		c_token++;
		if (END_OF_COMMAND) {
		    int_error(c_token, "expecting screen value [0 - 1]");
		} else {
		    get_position_default(&color_box.origin, screen);
		}
		c_token--;
		continue;
	    /* colorbox size */
	    case S_COLORBOX_SIZE: /* "s$ize" */
		c_token++;
		if (END_OF_COMMAND) {
		    int_error(c_token, "expecting screen value [0 - 1]");
		} else {
		    get_position_default(&color_box.size, screen);
		}
		c_token--;
		continue;
	    case S_COLORBOX_INVERT: /* Flip direction of color gradient + cbaxis */
		c_token++;
		color_box.invert = TRUE;
		continue;
	    case S_COLORBOX_NOINVERT: /* Flip direction of color gradient + cbaxis */
		c_token++;
		color_box.invert = FALSE;
		continue;
	    } /* switch over colorbox lookup table */
	    int_error(c_token,"invalid colorbox option");
	} /* end of while !end of command over colorbox options */
    if (color_box.where == SMCOLOR_BOX_NO) /* default: draw at default position */
	color_box.where = SMCOLOR_BOX_DEFAULT;
    }
}


/* process 'set pm3d' command */
static void
set_pm3d()
{
    int c_token0 = ++c_token;

    if (END_OF_COMMAND) { /* assume default settings */
	pm3d_reset(); /* sets pm3d.implicit to PM3D_IMPLICIT and pm3d.where to "s" */
	pm3d.implicit = PM3D_IMPLICIT; /* for historical reasons */
    }
    else { /* go through all options of 'set pm3d' */
	for ( ; !END_OF_COMMAND; c_token++ ) {
	    switch (lookup_table(&set_pm3d_tbl[0],c_token)) {
	    /* where to plot */
	    case S_PM3D_AT: /* "at" */
		c_token++;
		if (get_pm3d_at_option(&pm3d.where[0]))
		    return; /* error */
		c_token--;
#if 1
		if (c_token == c_token0+1)
		    /* for historical reasons: if "at" is the first option of pm3d,
		     * like "set pm3d at X other_opts;", then implicit is switched on */
		    pm3d.implicit = PM3D_IMPLICIT;
#endif
		continue;
	    case S_PM3D_INTERPOLATE: /* "interpolate" */
		c_token++;
		if (END_OF_COMMAND) {
		    int_error(c_token, "expecting step values i,j");
		} else {
		    pm3d.interp_i = int_expression();
		    if (!equals(c_token,","))
			int_error(c_token, "',' expected");
		    c_token++;
		    pm3d.interp_j = int_expression();
		    c_token--;
		}
		continue;
	    /* forward and backward drawing direction */
	    case S_PM3D_SCANSFORWARD: /* "scansfor$ward" */
		pm3d.direction = PM3D_SCANS_FORWARD;
		continue;
	    case S_PM3D_SCANSBACKWARD: /* "scansback$ward" */
		pm3d.direction = PM3D_SCANS_BACKWARD;
		continue;
	    case S_PM3D_SCANS_AUTOMATIC: /* "scansauto$matic" */
		pm3d.direction = PM3D_SCANS_AUTOMATIC;
		continue;
	    case S_PM3D_DEPTH: /* "dep$thorder" */
		pm3d.direction = PM3D_DEPTH;
		continue;
	    /* flush scans: left, right or center */
	    case S_PM3D_FLUSH:  /* "fl$ush" */
		c_token++;
		if (almost_equals(c_token, "b$egin"))
		    pm3d.flush = PM3D_FLUSH_BEGIN;
		else if (almost_equals(c_token, "c$enter"))
		    pm3d.flush = PM3D_FLUSH_CENTER;
		else if (almost_equals(c_token, "e$nd"))
		    pm3d.flush = PM3D_FLUSH_END;
		else
		    int_error(c_token,"expecting flush 'begin', 'center' or 'end'");
		continue;
	    /* clipping method */
	    case S_PM3D_CLIP_1IN: /* "clip1$in" */
		pm3d.clip = PM3D_CLIP_1IN;
		continue;
	    case S_PM3D_CLIP_4IN: /* "clip4$in" */
		pm3d.clip = PM3D_CLIP_4IN;
		continue;
	    /* setup everything for plotting a map */
	    case S_PM3D_MAP: /* "map" */
		pm3d.where[0] = 'b'; pm3d.where[1] = 0; /* set pm3d at b */
		data_style = PM3DSURFACE;
		func_style = PM3DSURFACE;
		splot_map = TRUE;
		continue;
	    /* flushing triangles */
	    case S_PM3D_FTRIANGLES: /* "ftr$iangles" */
		pm3d.ftriangles = 1;
		continue;
	    case S_PM3D_NOFTRIANGLES: /* "noftr$iangles" */
		pm3d.ftriangles = 0;
		continue;
	    /* deprecated pm3d "hidden3d" option, now used for borders */
	    case S_PM3D_HIDDEN:
		if (isanumber(c_token+1)) {
		    c_token++;
		    load_linetype(&pm3d.border, int_expression());
		    c_token--;
		    continue;
		}
		/* fall through */
	    case S_PM3D_BORDER: /* border {linespec} */
		c_token++;
		pm3d.border = default_pm3d_border;
		lp_parse(&pm3d.border, LP_ADHOC, FALSE);
		c_token--;
		continue;
	    case S_PM3D_NOHIDDEN:
	    case S_PM3D_NOBORDER:
		pm3d.border.l_type = LT_NODRAW;
		continue;
	    case S_PM3D_SOLID: /* "so$lid" */
	    case S_PM3D_NOTRANSPARENT: /* "notr$ansparent" */
	    case S_PM3D_NOSOLID: /* "noso$lid" */
	    case S_PM3D_TRANSPARENT: /* "tr$ansparent" */
		if (interactive)
		    int_warn(c_token, "Deprecated syntax --- ignored");
	    case S_PM3D_IMPLICIT: /* "i$mplicit" */
	    case S_PM3D_NOEXPLICIT: /* "noe$xplicit" */
		pm3d.implicit = PM3D_IMPLICIT;
		continue;
	    case S_PM3D_NOIMPLICIT: /* "noi$mplicit" */
	    case S_PM3D_EXPLICIT: /* "e$xplicit" */
		pm3d.implicit = PM3D_EXPLICIT;
		continue;
	    case S_PM3D_WHICH_CORNER: /* "corners2color" */
		c_token++;
		if (equals(c_token, "mean"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_MEAN;
		else if (equals(c_token, "geomean"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_GEOMEAN;
		else if (equals(c_token, "harmean"))
			pm3d.which_corner_color = PM3D_WHICHCORNER_HARMEAN;
		else if (equals(c_token, "median"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_MEDIAN;
		else if (equals(c_token, "min"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_MIN;
		else if (equals(c_token, "max"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_MAX;
		else if (equals(c_token, "rms"))
			pm3d.which_corner_color = PM3D_WHICHCORNER_RMS;
		else if (equals(c_token, "c1"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_C1;
		else if (equals(c_token, "c2"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_C2;
		else if (equals(c_token, "c3"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_C3;
		else if (equals(c_token, "c4"))
		    pm3d.which_corner_color = PM3D_WHICHCORNER_C4;
		else
		    int_error(c_token,"expecting 'mean', 'geomean', 'harmean', 'median', 'min', 'max', 'c1', 'c2', 'c3' or 'c4'");
		continue;
	    } /* switch over pm3d lookup table */
	    int_error(c_token,"invalid pm3d option");
	} /* end of while !end of command over pm3d options */
	if (PM3D_SCANS_AUTOMATIC == pm3d.direction
	    && PM3D_FLUSH_BEGIN != pm3d.flush) {
	    pm3d.direction = PM3D_SCANS_FORWARD;
	    /* FIXME: Why isn't this combination supported? */
	    FPRINTF((stderr, "pm3d: `scansautomatic' and `flush %s' are incompatible\n",
		PM3D_FLUSH_END == pm3d.flush ? "end": "center"));
	}
    }
}


/* process 'set pointintervalbox' command */
static void
set_pointintervalbox()
{
    c_token++;
    if (END_OF_COMMAND)
	pointintervalbox = 1.0;
    else
	pointintervalbox = real_expression();
    if (pointintervalbox <= 0)
	pointintervalbox = 1.0;
}

/* process 'set pointsize' command */
static void
set_pointsize()
{
    c_token++;
    if (END_OF_COMMAND)
	pointsize = 1.0;
    else
	pointsize = real_expression();
    if (pointsize <= 0)
	pointsize = 1.0;
}


/* process 'set polar' command */
static void
set_polar()
{
    c_token++;

    if (polar)
	return;
    else
	polar = TRUE;

    if (!parametric) {
	if (interactive)
	    (void) fprintf(stderr,"\n\tdummy variable is t for curves\n");
	strcpy (set_dummy_var[0], "t");
    }
    if (axis_array[T_AXIS].set_autoscale) {
	/* only if user has not set a range manually */
	axis_array[T_AXIS].set_min = 0.0;
	/* 360 if degrees, 2pi if radians */
	axis_array[T_AXIS].set_max = 2 * M_PI / ang2rad;
    }
    if (axis_array[POLAR_AXIS].set_autoscale != AUTOSCALE_BOTH)
	rrange_to_xy();
}

#ifdef EAM_OBJECTS
/*
 * Process command     'set object <tag> {rectangle|ellipse|circle|polygon}'
 * set object {tag} rectangle {from <bottom_left> {to|rto} <top_right>}
 *                     {{at|center} <xcen>,<ycen> size <w>,<h>}
 *                     {fc|fillcolor <colorspec>} {lw|linewidth <lw>}
 *                     {fs <fillstyle>} {front|back|behind}
 *                     {default}
 * EAM Jan 2005
 */

static void
set_object()
{
    int tag;

    /* The next token must either be a tag or the object type */
    c_token++;
    if (almost_equals(c_token, "rect$angle") || almost_equals(c_token, "ell$ipse")
    ||  almost_equals(c_token, "circ$le") || almost_equals(c_token, "poly$gon"))
	tag = -1; /* We'll figure out what it really is later */
    else {
	tag = int_expression();
	if (tag <= 0)
	    int_error(c_token, "tag must be > zero");
    }

    if (almost_equals(c_token, "rect$angle")) {
	set_obj(tag, OBJ_RECTANGLE);

    } else if (almost_equals(c_token, "ell$ipse")) {
	set_obj(tag, OBJ_ELLIPSE);

    } else if (almost_equals(c_token, "circ$le")) {
	set_obj(tag, OBJ_CIRCLE);

    } else if (almost_equals(c_token, "poly$gon")) {
	set_obj(tag, OBJ_POLYGON);

    } else if (tag > 0) {
	/* Look for existing object with this tag */
	t_object *this_object = first_object;
	for (; this_object != NULL; this_object = this_object->next)
	     if (tag == this_object->tag)
		break;
	if (this_object && tag == this_object->tag) {
	    c_token--;
	    set_obj(tag, this_object->object_type);
	} else
	    int_error(c_token, "unknown object");

    } else
	int_error(c_token, "unrecognized object type");

}

static t_object *
new_object(int tag, int object_type, t_object *new)
{
    t_object def_rect = DEFAULT_RECTANGLE_STYLE;
    t_object def_ellipse = DEFAULT_ELLIPSE_STYLE;
    t_object def_circle = DEFAULT_CIRCLE_STYLE;
    t_object def_polygon = DEFAULT_POLYGON_STYLE;

    if (!new)
	new = gp_alloc(sizeof(struct object), "object");
    else if (new->object_type == OBJ_POLYGON)
	free(new->o.polygon.vertex);

    if (object_type == OBJ_RECTANGLE) {
	*new = def_rect;
	new->lp_properties.l_type = LT_DEFAULT; /* Use default rectangle color */
	new->fillstyle.fillstyle = FS_DEFAULT;  /* and default fill style */
    } else if (object_type == OBJ_ELLIPSE)
	*new = def_ellipse;
    else if (object_type == OBJ_CIRCLE)
	*new = def_circle;
    else if (object_type == OBJ_POLYGON)
	*new = def_polygon;
    else
	int_error(NO_CARET,"object initialization failure");

    new->tag = tag;
    new->object_type = object_type;

    return new;
}

static void
set_obj(int tag, int obj_type)
{
    t_rectangle *this_rect = NULL;
    t_ellipse *this_ellipse = NULL;
    t_circle *this_circle = NULL;
    t_polygon *this_polygon = NULL;
    t_object *this_object = NULL;
    t_object *new_obj = NULL;
    t_object *prev_object = NULL;
    TBOOLEAN got_fill = FALSE;
    TBOOLEAN got_lt = FALSE;
    TBOOLEAN got_fc = FALSE;
    TBOOLEAN got_corners = FALSE;
    TBOOLEAN got_center = FALSE;
    TBOOLEAN got_origin = FALSE;

    c_token++;

    /* We are setting the default, not any particular rectangle */
    if (tag < -1) {
	c_token--;
	if (obj_type == OBJ_RECTANGLE) {
	    this_object = &default_rectangle;
	    this_rect = &this_object->o.rectangle;
	} else
	    int_error(c_token, "Unknown object type");

    } else {
	/* Look for existing object with this tag */
	for (this_object = first_object; this_object != NULL;
	     prev_object = this_object, this_object = this_object->next)
	     /* is this the one we want? */
	     if (0 < tag  &&  tag <= this_object->tag)
		break;

	/* Insert this rect into the list if it is a new one */
	if (this_object == NULL || tag != this_object->tag) {
	    if (tag == -1)
		tag = (prev_object) ? prev_object->tag+1 : 1;
	    new_obj = new_object(tag, obj_type, NULL);
	    if (prev_object == NULL)
		first_object = new_obj;
	    else
		prev_object->next = new_obj;
	    new_obj->next = this_object;
	    this_object = new_obj;
	    /* V5 CHANGE: Apply default rectangle style now rather than later */
	    if (obj_type == OBJ_RECTANGLE) {
		this_object->fillstyle = default_rectangle.fillstyle;
		this_object->lp_properties = default_rectangle.lp_properties;
	    }
	}

	/* Over-write old object if the type has changed */
	else if (this_object->object_type != obj_type) {
	    t_object *save_link = this_object->next;
	    new_obj = new_object(tag, obj_type, this_object);
	    this_object->next = save_link;
	}

	this_rect = &this_object->o.rectangle;
	this_ellipse = &this_object->o.ellipse;
	this_circle = &this_object->o.circle;
	this_polygon = &this_object->o.polygon;

    }

    while (!END_OF_COMMAND) {
	int save_token = c_token;

	switch (obj_type) {
	case OBJ_RECTANGLE:
		if (equals(c_token,"from")) {
		    /* Read in the bottom left and upper right corners */
		    c_token++;
		    get_position(&this_rect->bl);
		    if (equals(c_token,"to")) {
			c_token++;
			get_position(&this_rect->tr);
		    } else if (equals(c_token,"rto")) {
			c_token++;
			get_position_default(&this_rect->tr,this_rect->bl.scalex);
			if (this_rect->bl.scalex != this_rect->tr.scalex
			||  this_rect->bl.scaley != this_rect->tr.scaley)
			    int_error(c_token,"relative coordinates must match in type");
			this_rect->tr.x += this_rect->bl.x;
			this_rect->tr.y += this_rect->bl.y;
		    } else
			int_error(c_token,"Expecting to or rto");
		    got_corners = TRUE;
		    this_rect->type = 0;
		    continue;

		} else if (equals(c_token,"at") || almost_equals(c_token,"cen$ter")) {
		    /* Read in the center position */
		    c_token++;
		    get_position(&this_rect->center);
		    got_center = TRUE;
		    this_rect->type = 1;
		    continue;

		} else if (equals(c_token,"size")) {
		    /* Read in the width and height */
		    c_token++;
		    get_position(&this_rect->extent);
		    got_center = TRUE;
		    this_rect->type = 1;
		    continue;
		}
		break;

	case OBJ_CIRCLE:
		if (equals(c_token,"at") || almost_equals(c_token,"cen$ter")) {
		    /* Read in the center position */
		    c_token++;
		    get_position(&this_circle->center);
		    continue;

		} else if (equals(c_token,"size") || equals(c_token,"radius")) {
		    /* Read in the radius */
		    c_token++;
		    get_position(&this_circle->extent);
		    continue;

		} else if (equals(c_token, "arc")) {
		    /* Start and end angle for arc */
		    if (equals(++c_token,"[")) {
			double arc;
			c_token++;
			arc = real_expression();
			if (fabs(arc) > 1000.)
			    int_error(c_token-1,"Angle out of range");
			else
			    this_circle->arc_begin = arc;
			if (equals(c_token++, ":")) {
			    arc = real_expression();
			    if (fabs(arc) > 1000.)
				int_error(c_token-1,"Angle out of range");
			    else
				this_circle->arc_end = arc;
			    if (equals(c_token++,"]"))
				continue;
			}
		    }
		    int_error(--c_token, "Expecting arc [<begin>:<end>]");
		} else if (equals(c_token, "wedge")) {
		    c_token++;
		    this_circle->wedge = TRUE;
		    continue;
		} else if (equals(c_token, "nowedge")) {
		    c_token++;
		    this_circle->wedge = FALSE;
		    continue;
		}
		break;

	case OBJ_ELLIPSE:
		if (equals(c_token,"at") || almost_equals(c_token,"cen$ter")) {
		    /* Read in the center position */
		    c_token++;
		    get_position(&this_ellipse->center);
		    continue;

		} else if (equals(c_token,"size")) {
		    /* Read in the width and height */
		    c_token++;
		    get_position(&this_ellipse->extent);
		    continue;

		} else if (almost_equals(c_token,"ang$le")) {
		    c_token++;
		    this_ellipse->orientation = real_expression();
		    continue;

		} else if (almost_equals(c_token,"unit$s")) {
		    c_token++;
		    if (equals(c_token,"xy") || END_OF_COMMAND) {
			this_ellipse->type = ELLIPSEAXES_XY;
		    } else if (equals(c_token,"xx")) {
			this_ellipse->type = ELLIPSEAXES_XX;
		    } else if (equals(c_token,"yy")) {
			this_ellipse->type = ELLIPSEAXES_YY;
		    } else {
			int_error(c_token, "expecting 'xy', 'xx' or 'yy'" );
		    }
		    c_token++;
		    continue;

		}
		break;

	case OBJ_POLYGON:
		if (equals(c_token,"from")) {
		    c_token++;
		    this_polygon->vertex = gp_realloc(this_polygon->vertex,
					sizeof(struct position),
					"polygon vertex");
		    get_position(&this_polygon->vertex[0]);
		    this_polygon->type = 1;
		    got_origin = TRUE;
		    continue;
		}
		if (!got_corners && (equals(c_token,"to") || equals(c_token,"rto"))) {
		    while (equals(c_token,"to") || equals(c_token,"rto")) {
			if (!got_origin)
			    goto polygon_error;
			this_polygon->vertex = gp_realloc(this_polygon->vertex,
					    (this_polygon->type+1) * sizeof(struct position),
					    "polygon vertex");
			if (equals(c_token++,"to")) {
			    get_position(&this_polygon->vertex[this_polygon->type]);
			} else /* "rto" */ {
			    int v = this_polygon->type;
			    get_position_default(&this_polygon->vertex[v],
						  this_polygon->vertex->scalex);
			    if (this_polygon->vertex[v].scalex != this_polygon->vertex[v-1].scalex
			    ||  this_polygon->vertex[v].scaley != this_polygon->vertex[v-1].scaley)
				int_error(c_token,"relative coordinates must match in type");
			    this_polygon->vertex[v].x += this_polygon->vertex[v-1].x;
			    this_polygon->vertex[v].y += this_polygon->vertex[v-1].y;
			}
			this_polygon->type++;
			got_corners = TRUE;
		    }
		    if (got_corners && memcmp(&this_polygon->vertex[this_polygon->type-1],
					      &this_polygon->vertex[0],sizeof(struct position))) {
			fprintf(stderr,"Polygon is not closed - adding extra vertex\n");
			this_polygon->vertex = gp_realloc(this_polygon->vertex,
					    (this_polygon->type+1) * sizeof(struct position),
					    "polygon vertex");
			this_polygon->vertex[this_polygon->type] = this_polygon->vertex[0];
			this_polygon->type++;
		    }
		    continue;
		}
		break;
		polygon_error:
			free(this_polygon->vertex);
			this_polygon->vertex = NULL;
			this_polygon->type = 0;
			int_error(c_token, "Unrecognized polygon syntax");
		/* End of polygon options */

	default:
		int_error(c_token, "unrecognized object type");
	} /* End of object-specific options */

	/* The rest of the options apply to any type of object */

	if (equals(c_token,"front")) {
	    this_object->layer = LAYER_FRONT;
	    c_token++;
	    continue;
	} else if (equals(c_token,"back")) {
	    this_object->layer = LAYER_BACK;
	    c_token++;
	    continue;
	} else if (equals(c_token,"behind")) {
	    this_object->layer = LAYER_BEHIND;
	    c_token++;
	    continue;
	} else if (almost_equals(c_token,"def$ault")) {
	    if (tag < 0) {
		int_error(c_token,
		    "Invalid command - did you mean 'unset style rectangle'?");
	    } else {
		this_object->lp_properties.l_type = LT_DEFAULT;
		this_object->fillstyle.fillstyle = FS_DEFAULT;
	    }
	    got_fill = got_lt = TRUE;
	    c_token++;
	    continue;
	} else if (equals(c_token, "clip")) {
	    this_object->clip = OBJ_CLIP;
	    c_token++;
	    continue;
	} else if (equals(c_token, "noclip")) {
	    this_object->clip = OBJ_NOCLIP;
	    c_token++;
	    continue;
	}

	/* Now parse the style options; default to whatever the global style is  */
	if (!got_fill) {
	    fill_style_type *default_style;
	    if (this_object->object_type == OBJ_RECTANGLE)
		default_style = &default_rectangle.fillstyle;
	    else
		default_style = &default_fillstyle;

	    if (new_obj)
		parse_fillstyle(&this_object->fillstyle, default_style->fillstyle,
			default_style->filldensity, default_style->fillpattern,
			default_style->border_color);
	    else
		parse_fillstyle(&this_object->fillstyle, this_object->fillstyle.fillstyle,
			this_object->fillstyle.filldensity, this_object->fillstyle.fillpattern,
			this_object->fillstyle.border_color);
	    if (c_token != save_token) {
		got_fill = TRUE;
		continue;
	    }
	}

	/* Parse the colorspec */
	if (!got_fc) {
	    if (equals(c_token,"fc") || almost_equals(c_token,"fillc$olor")) {
		this_object->lp_properties.l_type = LT_BLACK; /* Anything but LT_DEFAULT */
		parse_colorspec(&this_object->lp_properties.pm3d_color, TC_FRAC);
		if (this_object->lp_properties.pm3d_color.type == TC_DEFAULT)
		    this_object->lp_properties.l_type = LT_DEFAULT;
	    }

	    if (c_token != save_token) {
		got_fc = TRUE;
		continue;
	    }
	}

	/* Line properties (will be used for the object border if the fillstyle has one. */
	/* LP_NOFILL means don't eat fillcolor here since at is set separately with "fc". */
	if (!got_lt) {
	    lp_style_type lptmp = this_object->lp_properties;
	    lp_parse(&lptmp, LP_NOFILL, FALSE);
	    if (c_token != save_token) {
		this_object->lp_properties.l_width = lptmp.l_width;
		this_object->lp_properties.d_type = lptmp.d_type;
		this_object->lp_properties.custom_dash_pattern = lptmp.custom_dash_pattern;
		got_lt = TRUE;
		continue;
	    }
	}

	int_error(c_token, "Unrecognized or duplicate option");
    }

    if (got_center && got_corners)
	int_error(NO_CARET,"Inconsistent options");

}
#endif

/* process 'set samples' command */
static void
set_samples()
{
    int tsamp1, tsamp2;

    c_token++;
    tsamp1 = abs(int_expression());
    tsamp2 = tsamp1;
    if (!END_OF_COMMAND) {
	if (!equals(c_token,","))
	    int_error(c_token, "',' expected");
	c_token++;
	tsamp2 = abs(int_expression());
    }
    if (tsamp1 < 2 || tsamp2 < 2)
	int_error(c_token, "sampling rate must be > 1; sampling unchanged");
    else {
	struct surface_points *f_3dp = first_3dplot;

	first_3dplot = NULL;
	sp_free(f_3dp);

	samples_1 = tsamp1;
	samples_2 = tsamp2;
    }
}


/* process 'set size' command */
static void
set_size()
{
    c_token++;
    if (END_OF_COMMAND) {
	xsize = 1.0;
	ysize = 1.0;
    } else {
	if (almost_equals(c_token, "sq$uare")) {
	    aspect_ratio = 1.0;
	    ++c_token;
	} else if (almost_equals(c_token,"ra$tio")) {
	    ++c_token;
	    aspect_ratio = real_expression();
	} else if (almost_equals(c_token, "nora$tio") || almost_equals(c_token, "nosq$uare")) {
	    aspect_ratio = 0.0;
	    ++c_token;
	}

	if (!END_OF_COMMAND) {
	    xsize = real_expression();
	    if (equals(c_token,",")) {
		c_token++;
		ysize = real_expression();
	    } else {
		ysize = xsize;
	    }
	}
    }
    if (xsize <= 0 || ysize <=0) {
	xsize = ysize = 1.0;
	int_error(NO_CARET,"Illegal value for size");
    }
}


/* process 'set style' command */
static void
set_style()
{
    c_token++;

    switch(lookup_table(&show_style_tbl[0],c_token)){
    case SHOW_STYLE_DATA:
	data_style = get_style();
	if (data_style == FILLEDCURVES) {
	    get_filledcurves_style_options(&filledcurves_opts_data);
	    if (!filledcurves_opts_data.opt_given) /* default value */
		filledcurves_opts_data.closeto = FILLEDCURVES_CLOSED;
	}
	break;
    case SHOW_STYLE_FUNCTION:
	{
	    enum PLOT_STYLE temp_style = get_style();

	    if ((temp_style & PLOT_STYLE_HAS_ERRORBAR)
	    ||  (temp_style == LABELPOINTS) || (temp_style == HISTOGRAMS)
	    ||  (temp_style == IMAGE) || (temp_style == RGBIMAGE) || (temp_style == RGBA_IMAGE)
	    ||  (temp_style == PARALLELPLOT))
		int_error(c_token, "style not usable for function plots, left unchanged");
	    else
		func_style = temp_style;
	    if (func_style == FILLEDCURVES) {
		get_filledcurves_style_options(&filledcurves_opts_func);
		if (!filledcurves_opts_func.opt_given) /* default value */
		    filledcurves_opts_func.closeto = FILLEDCURVES_CLOSED;
	    }
	    break;
	}
    case SHOW_STYLE_LINE:
	set_linestyle(&first_linestyle, LP_STYLE);
	break;
    case SHOW_STYLE_FILLING:
	parse_fillstyle( &default_fillstyle,
			default_fillstyle.fillstyle,
			default_fillstyle.filldensity,
			default_fillstyle.fillpattern,
			default_fillstyle.border_color);
	break;
    case SHOW_STYLE_ARROW:
	set_arrowstyle();
	break;
#ifdef EAM_OBJECTS
    case SHOW_STYLE_RECTANGLE:
	c_token++;
	set_obj(-2, OBJ_RECTANGLE);
	break;
    case SHOW_STYLE_CIRCLE:
	c_token++;
	while (!END_OF_COMMAND) {
	    if (almost_equals(c_token,"r$adius")) {
		c_token++;
		get_position(&default_circle.o.circle.extent);
	    } else if (almost_equals(c_token, "wedge$s")) {
		c_token++;
		default_circle.o.circle.wedge = TRUE;
	    } else if (almost_equals(c_token, "nowedge$s")) {
		c_token++;
		default_circle.o.circle.wedge = FALSE;
	    } else if (equals(c_token, "clip")) {
		c_token++;
		default_circle.clip = OBJ_CLIP;
	    } else if (equals(c_token, "noclip")) {
		c_token++;
		default_circle.clip = OBJ_NOCLIP;
	    } else
		int_error(c_token, "unrecognized style option" );
	}
	break;
    case SHOW_STYLE_ELLIPSE:
	c_token++;
	while (!END_OF_COMMAND) {
	    if (equals(c_token,"size")) {
		c_token++;
		get_position(&default_ellipse.o.ellipse.extent);
		c_token--;
	    } else if (almost_equals(c_token,"ang$le")) {
		c_token++;
		if (isanumber(c_token) || type_udv(c_token) == INTGR || type_udv(c_token) == CMPLX) {
		    default_ellipse.o.ellipse.orientation = real_expression();
		    c_token--;
		}
	    } else if (almost_equals(c_token,"unit$s")) {
		c_token++;
		if (equals(c_token,"xy") || END_OF_COMMAND) {
		    default_ellipse.o.ellipse.type = ELLIPSEAXES_XY;
		} else if (equals(c_token,"xx")) {
		    default_ellipse.o.ellipse.type = ELLIPSEAXES_XX;
		} else if (equals(c_token,"yy")) {
		    default_ellipse.o.ellipse.type = ELLIPSEAXES_YY;
		} else {
		    int_error(c_token, "expecting 'xy', 'xx' or 'yy'" );
		}
	    } else if (equals(c_token, "clip")) {
		c_token++;
		default_ellipse.clip = OBJ_CLIP;
	    } else if (equals(c_token, "noclip")) {
		c_token++;
		default_ellipse.clip = OBJ_NOCLIP;
	    } else
		int_error(c_token, "expecting 'units {xy|xx|yy}', 'angle <number>' or 'size <position>'" );

	    c_token++;
	}
	break;
#endif
    case SHOW_STYLE_HISTOGRAM:
	parse_histogramstyle(&histogram_opts,HT_CLUSTERED,histogram_opts.gap);
	break;
#ifdef EAM_BOXED_TEXT
    case SHOW_STYLE_TEXTBOX:
	c_token++;
	while (!END_OF_COMMAND) {
	    if (almost_equals(c_token,"op$aque")) {
		textbox_opts.opaque = TRUE;
		c_token++;
	    } else if (almost_equals(c_token,"trans$parent")) {
		textbox_opts.opaque = FALSE;
		c_token++;
	    } else if (almost_equals(c_token,"mar$gins")) {
		struct value a;
		c_token++;
		if (END_OF_COMMAND) {
		    textbox_opts.xmargin = 1.;
		    textbox_opts.ymargin = 1.;
		    break;
		}
		textbox_opts.xmargin = real(const_express(&a));
		if (!equals(c_token++,",") || END_OF_COMMAND)
		    break;
		textbox_opts.ymargin = real(const_express(&a));
	    } else if (almost_equals(c_token,"nobo$rder")) {
		c_token++;
		textbox_opts.noborder = TRUE;
	    } else if (almost_equals(c_token,"bo$rder")) {
		c_token++;
		textbox_opts.noborder = FALSE;
	    } else
		int_error(c_token,"unrecognized option");
	}
	break;
#endif
    case SHOW_STYLE_INCREMENT:
#if TRUE || defined(BACKWARDS_COMPATIBLE)
	c_token++;
	if (END_OF_COMMAND || almost_equals(c_token,"def$ault"))
	    prefer_line_styles = FALSE;
	else if (almost_equals(c_token,"u$serstyles"))
	    prefer_line_styles = TRUE;
	else
	    int_error(c_token,"unrecognized option");
	c_token++;
#endif
	break;
    case SHOW_STYLE_BOXPLOT:
	set_boxplot();
	break;
    case SHOW_STYLE_PARALLEL:
	set_style_parallel();
	break;
    default:
	int_error(c_token, "unrecognized option - see 'help set style'");
    }
}


/* process 'set surface' command */
static void
set_surface()
{
    c_token++;
    draw_surface = TRUE;
    implicit_surface = TRUE;
    if (!END_OF_COMMAND) {
	if (equals(c_token, "implicit"))
	    ;
	else if (equals(c_token, "explicit"))
	    implicit_surface = FALSE;
	c_token++;
    }
}


/* process 'set table' command */
static void
set_table()
{
    char *tablefile;

    c_token++;

    if (table_outfile) {
	fclose(table_outfile);
	table_outfile = NULL;
    }
    table_var = NULL;

    if (equals(c_token, "$") && isletter(c_token + 1)) { /* datablock */
	/* NB: has to come first because try_to_get_string will choke on the datablock name */
	table_var = add_udv_by_name(parse_datablock_name());
	if (!table_var->udv_undef) {
	    gpfree_string(&table_var->udv_value);
	    gpfree_datablock(&table_var->udv_value);
	}
	table_var->udv_value.type = DATABLOCK;
	table_var->udv_value.v.data_array = NULL;
	table_var->udv_undef = FALSE;

    } else if ((tablefile = try_to_get_string())) {  /* file name */
	/* 'set table "foo"' creates a new output file */
	if (!(table_outfile = fopen(tablefile, "w")))
	   os_error(c_token, "cannot open table output file");
	free(tablefile);
    }

    table_mode = TRUE;
}


/* process 'set terminal' comamnd */
static void
set_terminal()
{
    c_token++;

    if (multiplot)
	int_error(c_token, "You can't change the terminal in multiplot mode");

    if (END_OF_COMMAND) {
	list_terms();
	screen_ok = FALSE;
	return;
    }

    /* `set term push' */
    if (equals(c_token,"push")) {
	push_terminal(interactive);
	c_token++;
	return;
    } /* set term push */

#ifdef USE_MOUSE
    event_reset((void *)1);   /* cancel zoombox etc. */
#endif
    term_reset();

    /* `set term pop' */
    if (equals(c_token,"pop")) {
	pop_terminal();
	c_token++;
	return;
    } /* set term pop */

    /* `set term <normal terminal>' */
    /* NB: if set_term() exits via int_error() then term will not be changed */
    term = set_term();

    /* get optional mode parameters
     * not all drivers reset the option string before
     * strcat-ing to it, so we reset it for them
     */
    *term_options = 0;
    term->options();
    if (interactive && *term_options)
	fprintf(stderr,"Options are '%s'\n",term_options);
    if ((term->flags & TERM_MONOCHROME))
	init_monochrome();
}


/*
 * Accept a single terminal option to apply to the current terminal if possible.
 * If the current terminal cannot support this option, we silently ignore it.
 * Only reasonably common terminal options are supported.
 *
 * If necessary, the code in term->options() can detect that it was called
 * from here because in this case almost_equals(c_token-1, "termopt$ion");
 */

static void
set_termoptions()
{
    TBOOLEAN ok_to_call_terminal = FALSE;
    int save_end_of_line = num_tokens;
    c_token++;

    if (END_OF_COMMAND || !term)
	return;

    if (almost_equals(c_token,"enh$anced")
	   ||  almost_equals(c_token,"noenh$anced")) {
	num_tokens = GPMIN(num_tokens,c_token+1);
	if (term->enhanced_open)
	    ok_to_call_terminal = TRUE;
	else
	    c_token++;
    } else if (equals(c_token,"font") ||  equals(c_token,"fname")) {
	num_tokens = GPMIN(num_tokens,c_token+2);
	ok_to_call_terminal = TRUE;
    } else if (equals(c_token,"fontscale")) {
	num_tokens = GPMIN(num_tokens,c_token+2);
	if (term->flags & TERM_FONTSCALE)
	    ok_to_call_terminal = TRUE;
	else {
	    c_token++;
	    real_expression();   /* Silently ignore the request */
	}
    } else if (equals(c_token,"lw") || almost_equals(c_token,"linew$idth")) {
	num_tokens = GPMIN(num_tokens,c_token+2);
	if (term->flags & TERM_LINEWIDTH)
	    ok_to_call_terminal = TRUE;
	else {
	    c_token++;
	    real_expression();   /* Silently ignore the request */
	}
    } else if (almost_equals(c_token,"dash$ed") || equals(c_token,"solid")) {
	/* Silently ignore the request */
	num_tokens = GPMIN(num_tokens,++c_token);
    } else if (almost_equals(c_token,"dashl$ength") || equals(c_token,"dl")) {
	num_tokens = GPMIN(num_tokens,c_token+2);
	if (term->flags & TERM_CAN_DASH)
	    ok_to_call_terminal = TRUE;
	else
	    c_token+=2;
    } else if (!strcmp(term->name,"gif") && equals(c_token,"delay") && num_tokens==4) {
	ok_to_call_terminal = TRUE;
    } else {
	int_error(c_token,"This option cannot be changed using 'set termoption'");
    }
    if (ok_to_call_terminal) {
	*term_options = 0;
	(term->options)();
    }
    num_tokens = save_end_of_line;
}


/* process 'set tics' command */
static void
set_tics()
{
    unsigned int i = 0;
    TBOOLEAN axisset = FALSE;
    TBOOLEAN mirror_opt = FALSE; /* set to true if (no)mirror option specified) */

    ++c_token;

    if (END_OF_COMMAND) {
	for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
	    axis_array[i].tic_in = TRUE;
    }

    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "ax$is")) {
	    axisset = TRUE;
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].ticmode &= ~TICS_ON_BORDER;
		axis_array[i].ticmode |= TICS_ON_AXIS;
	    }
	    ++c_token;
	} else if (almost_equals(c_token, "bo$rder")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].ticmode &= ~TICS_ON_AXIS;
		axis_array[i].ticmode |= TICS_ON_BORDER;
	    }
	    ++c_token;
	} else if (almost_equals(c_token, "mi$rror")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticmode |= TICS_MIRROR;
    	    mirror_opt = TRUE;
	    ++c_token;
	} else if (almost_equals(c_token, "nomi$rror")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticmode &= ~TICS_MIRROR;
	    mirror_opt = TRUE;
	    ++c_token;
	} else if (almost_equals(c_token,"in$wards")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].tic_in = TRUE;
	    ++c_token;
	} else if (almost_equals(c_token,"out$wards")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].tic_in = FALSE;
	    ++c_token;
	} else if (almost_equals(c_token, "sc$ale")) {
	    set_ticscale();
	} else if (almost_equals(c_token, "ro$tate")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].tic_rotate = TEXT_VERTICAL;
	    }
	    ++c_token;
	    if (equals(c_token, "by")) {
		int langle;
		++c_token;
		langle = int_expression();
		for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		    axis_array[i].tic_rotate = langle;
	    }
	} else if (almost_equals(c_token, "noro$tate")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].tic_rotate = 0;
	    ++c_token;
	} else if (almost_equals(c_token, "l$eft")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].label.pos = LEFT;
		axis_array[i].manual_justify = TRUE;
	    }
	    c_token++;
	} else if (almost_equals(c_token, "c$entre")
		|| almost_equals(c_token, "c$enter")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].label.pos = CENTRE;
		axis_array[i].manual_justify = TRUE;
	    }
	    c_token++;
	} else if (almost_equals(c_token, "ri$ght")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		axis_array[i].label.pos = RIGHT;
		axis_array[i].manual_justify = TRUE;
	    }
	    c_token++;
	} else if (almost_equals(c_token, "autoj$ustify")) {
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].manual_justify = FALSE;
	    c_token++;
	} else if (almost_equals(c_token, "off$set")) {
	    struct position lpos;
	    ++c_token;
	    get_position_default(&lpos, character);
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticdef.offset = lpos;
	} else if (almost_equals(c_token, "nooff$set")) {
	    ++c_token;
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticdef.offset = default_offset;
	} else if (almost_equals(c_token, "format")) {
	    set_format();
	} else if (almost_equals(c_token, "enh$anced")) {
	    ++c_token;
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticdef.enhanced = TRUE;
	} else if (almost_equals(c_token, "noenh$anced")) {
	    ++c_token;
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticdef.enhanced = FALSE;
	} else if (almost_equals(c_token, "f$ont")) {
	    ++c_token;
	    /* Make sure they've specified a font */
	    if (!isstringvalue(c_token))
		int_error(c_token,"expected font");
	    else {
		char *lfont = try_to_get_string();
		for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
		    free(axis_array[i].ticdef.font);
		    axis_array[i].ticdef.font = gp_strdup(lfont);
		}
		free(lfont);
	    }
	} else if (equals(c_token,"tc") ||
		   almost_equals(c_token,"text$color")) {
	    struct t_colorspec lcolor;
	    parse_colorspec(&lcolor, TC_FRAC);
	    for (i = 0; i < AXIS_ARRAY_SIZE; ++i)
		axis_array[i].ticdef.textcolor = lcolor;
	} else if (equals(c_token,"front")) {
	    grid_tics_in_front = TRUE;
	    ++c_token;
	} else if (equals(c_token,"back")) {
	    grid_tics_in_front = FALSE;
	    ++c_token;
	} else if (!END_OF_COMMAND) {
	    int_error(c_token, "extraneous arguments in set tics");
	}
    }

    /* if tics are off and not set by axis, reset to default (border) */
    for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
	if (((axis_array[i].ticmode & TICS_MASK) == NO_TICS) && (!axisset)) {
	    if ((i == SECOND_X_AXIS) || (i == SECOND_Y_AXIS))
		continue; /* don't switch on secondary axes by default */
	    axis_array[i].ticmode = TICS_ON_BORDER;
	    if ((mirror_opt == FALSE) && ((i == FIRST_X_AXIS) || (i == FIRST_Y_AXIS) || (i == COLOR_AXIS))) {
		axis_array[i].ticmode |= TICS_MIRROR;
	    }
	}
    }
}


/* process 'set ticscale' command */
static void
set_ticscale()
{
    int i, ticlevel;

    ++c_token;
    if (almost_equals(c_token, "def$ault")) {
	++c_token;
	for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
	    axis_array[i].ticscale = 1.0;
	    axis_array[i].miniticscale = 0.5;
	}
	ticscale[0] = 1.0;
	ticscale[1] = 0.5;
	for (ticlevel = 2; ticlevel < MAX_TICLEVEL; ticlevel++)
	    ticscale[ticlevel] = 1.0;
    } else {
	double lticscale, lminiticscale;
	lticscale = real_expression();
	if (equals(c_token, ",")) {
	    ++c_token;
	    lminiticscale = real_expression();
	} else {
	    lminiticscale = 0.5 * lticscale;
	}
	for (i = 0; i < AXIS_ARRAY_SIZE; ++i) {
	    axis_array[i].ticscale = lticscale;
	    axis_array[i].miniticscale = lminiticscale;
	}
	ticlevel = 2;
	while (equals(c_token, ",")) {
	    ++c_token;
	    ticscale[ticlevel++] = real_expression();
	    if (ticlevel >= MAX_TICLEVEL)
		break;
	}
    }
}


/* process 'set ticslevel' command */
/* is datatype 'time' relevant here ? */
static void
set_ticslevel()
{
    c_token++;
    xyplane.z = real_expression();
    xyplane.absolute = FALSE;
}


/* process 'set xyplane' command */
/* is datatype 'time' relevant here ? */
static void
set_xyplane()
{
    if (equals(++c_token, "at")) {
	c_token++;
	xyplane.z = real_expression();
	xyplane.absolute = TRUE;
	return;
    } else if (!almost_equals(c_token,"rel$ative")) {
	c_token--;
	/* int_warn(NO_CARET, "deprecated syntax"); */
    }
    set_ticslevel();
}


/* Process 'set timefmt' command */
/* HBB 20000507: changed this to a per-axis setting. I.e. you can now
 * have separate timefmt parse strings, different axes */
/* V5 Oct 2014: But that was never documented, and makes little sense since
 * the input format is a property of the data file, not the graph axis.
 * Revert to a single global default timefmt as documented.
 * If the default is not sufficient, use timecolumn(N,"format") on input.
 * Use "set {axis}tics format" to control the output format.
 */
static void
set_timefmt()
{
    char *ctmp;
    c_token++;

    if ((ctmp = try_to_get_string())) {
	free(timefmt);
	timefmt = ctmp;
    } else {
	free(timefmt);
	timefmt = gp_strdup(TIMEFMT);
    }
}


/* process 'set timestamp' command */
static void
set_timestamp()
{
    TBOOLEAN got_format = FALSE;
    char *new;

    c_token++;

    while (!END_OF_COMMAND) {

	if (almost_equals(c_token,"t$op")) {
	    timelabel_bottom = FALSE;
	    c_token++;
	    continue;
	} else if (almost_equals(c_token, "b$ottom")) {
	    timelabel_bottom = TRUE;
	    c_token++;
	    continue;
	}

	if (almost_equals(c_token,"r$otate")) {
	    timelabel_rotate = TRUE;
	    c_token++;
	    continue;
	} else if (almost_equals(c_token, "n$orotate")) {
	    timelabel_rotate = FALSE;
	    c_token++;
	    continue;
	}

	if (almost_equals(c_token,"off$set")) {
	    c_token++;
	    get_position_default(&(timelabel.offset),character);
	    continue;
	}

	if (equals(c_token,"font")) {
	    c_token++;
	    new = try_to_get_string();
	    free(timelabel.font);
	    timelabel.font = new;
	    continue;
	}

	if (equals(c_token,"tc") || almost_equals(c_token,"text$color")) {
	    parse_colorspec(&(timelabel.textcolor), TC_VARIABLE);
	    continue;
	}

	if (!got_format && ((new = try_to_get_string()))) {
	    /* we have a format string */
	    free(timelabel.text);
	    timelabel.text = new;
	    got_format = TRUE;
	    continue;
	}

	int_error(c_token,"unrecognized option");

    }

    if (!(timelabel.text))
	timelabel.text = gp_strdup(DEFAULT_TIMESTAMP_FORMAT);

}


/* process 'set view' command */
static void
set_view()
{
    int i;
    TBOOLEAN was_comma = TRUE;
    static const char errmsg1[] = "rot_%c must be in [0:%d] degrees range; view unchanged";
    static const char errmsg2[] = "%sscale must be > 0; view unchanged";
    double local_vals[4];

    c_token++;
    if (equals(c_token,"map")) {
	splot_map = TRUE;
	mapview_scale = 1.0;
	c_token++;
	if (equals(c_token,"scale")) {
	    c_token++;
	    mapview_scale = real_expression();
	} 
	return;
    };

    if (splot_map == TRUE)
	splot_map = FALSE; /* default is no map */

    if (almost_equals(c_token,"equal$_axes")) {
	c_token++;
	if (END_OF_COMMAND || equals(c_token,"xy")) {
	    aspect_ratio_3D = 2;
	    c_token++;
	} else if (equals(c_token,"xyz")) {
	    aspect_ratio_3D = 3;
	    c_token++;
	}
	return;
    } else if (almost_equals(c_token,"noequal$_axes")) {
	aspect_ratio_3D = 0;
	c_token++;
	return;
    }

    local_vals[0] = surface_rot_x;
    local_vals[1] = surface_rot_z;
    local_vals[2] = surface_scale;
    local_vals[3] = surface_zscale;
    for (i = 0; i < 4 && !(END_OF_COMMAND);) {
	if (equals(c_token,",")) {
	    if (was_comma) i++;
	    was_comma = TRUE;
	    c_token++;
	} else {
	    if (!was_comma)
		int_error(c_token, "',' expected");
	    local_vals[i] = real_expression();
	    i++;
	    was_comma = FALSE;
	}
    }

    if (local_vals[0] < 0 || local_vals[0] > 360)
	int_error(c_token, errmsg1, 'x', 360);
    if (local_vals[1] < 0 || local_vals[1] > 360)
	int_error(c_token, errmsg1, 'z', 360);
    if (local_vals[2] < 1e-6)
	int_error(c_token, errmsg2, "");
    if (local_vals[3] < 1e-6)
	int_error(c_token, errmsg2, "z");

    surface_rot_x = local_vals[0];
    surface_rot_z = local_vals[1];
    surface_scale = local_vals[2];
    surface_zscale = local_vals[3];
    surface_lscale = log(surface_scale);
}


/* process 'set zero' command */
static void
set_zero()
{
    struct value a;
    c_token++;
    zero = magnitude(const_express(&a));
}


/* process 'set {x|y|z|x2|y2}data' command */
static void
set_timedata(AXIS_INDEX axis)
{
    c_token++;
    axis_array[axis].datatype = DT_NORMAL;
    if (almost_equals(c_token,"t$ime")) {
	axis_array[axis].datatype = DT_TIMEDATE;
	c_token++;
    } else if (almost_equals(c_token,"geo$graphic")) {
	axis_array[axis].datatype = DT_DMS;
	c_token++;
    }
    /* FIXME: this provides approximate backwards compatibility */
    /*        but may be more trouble to explain than it's worth */
    axis_array[axis].tictype = axis_array[axis].datatype;
}


static void
set_range(AXIS_INDEX axis)
{
    c_token++;

    /* If this is a secondary axis linked to the primary, ignore the command */
    if (axis_array[axis].linked_to_primary) {
	while (!END_OF_COMMAND)
	    c_token++;
	return;
    }

    if (almost_equals(c_token,"re$store")) {
	c_token++;
	axis_array[axis].set_min = get_writeback_min(axis);
	axis_array[axis].set_max = get_writeback_max(axis);
	axis_array[axis].set_autoscale = AUTOSCALE_NONE;
    } else {
	if (!equals(c_token,"["))
	    int_error(c_token, "expecting '[' or 'restore'");
	c_token++;
	axis_array[axis].set_autoscale =
	    load_range(axis,
		       &axis_array[axis].set_min,&axis_array[axis].set_max,
		       axis_array[axis].set_autoscale);
	if (!equals(c_token,"]"))
	    int_error(c_token, "expecting ']'");
	c_token++;
	while (!END_OF_COMMAND) {
	    if (almost_equals(c_token, "rev$erse")) {
		++c_token;
		axis_array[axis].range_flags |= RANGE_IS_REVERSED;
	    } else if (almost_equals(c_token, "norev$erse")) {
		++c_token;
		axis_array[axis].range_flags &= ~RANGE_IS_REVERSED;
	    } else if (almost_equals(c_token, "wr$iteback")) {
		++c_token;
		axis_array[axis].range_flags |= RANGE_WRITEBACK;
	    } else if (almost_equals(c_token, "nowri$teback")) {
		++c_token;
		axis_array[axis].range_flags &= ~RANGE_WRITEBACK;
	    } else if (almost_equals(c_token, "ext$end")) {
		++c_token;
		axis_array[axis].set_autoscale &= ~(AUTOSCALE_FIXMIN | AUTOSCALE_FIXMAX);
	    } else if (almost_equals(c_token, "noext$end")) {
		++c_token;
		axis_array[axis].set_autoscale |= AUTOSCALE_FIXMIN | AUTOSCALE_FIXMAX;
	    } else
		int_error(c_token,"unrecognized option");
	}
    }

    /* If there is a secondary axis linked to this one, */
    /* replicate the new range information to it.       */
    if ((axis == FIRST_X_AXIS || axis == FIRST_Y_AXIS)
    &&  (axis_array[axis + SECOND_AXES].linked_to_primary))
	    clone_linked_axes(axis + SECOND_AXES, axis);
}

/*
 * set paxis <axis> {range <range-options> | tics <tic-options> }
 */
static void
set_paxis()
{
    int p;
    c_token++;
    p = int_expression();

    if (p <= 0 || p > MAX_PARALLEL_AXES)
	int_error(c_token-1, "expecting parallel axis number 1 - %d",MAX_PARALLEL_AXES);

    if (equals(c_token, "range"))
	set_range(PARALLEL_AXES+p-1);
    else if (almost_equals(c_token, "tic$s"))
	set_tic_prop(PARALLEL_AXES+p-1);
    else
	int_error(c_token, "expecting 'range' or 'tics'");
}

static void
set_raxis()
{
    raxis = TRUE;
    c_token++;
}

/* process 'set {xyz}zeroaxis' command */
static void
set_zeroaxis(AXIS_INDEX axis)
{
    c_token++;
    if (axis_array[axis].zeroaxis != (void *)(&default_axis_zeroaxis))
	free(axis_array[axis].zeroaxis);
    if (END_OF_COMMAND)
	axis_array[axis].zeroaxis = (void *)(&default_axis_zeroaxis);
    else {
	/* Some non-default style for the zeroaxis */
	axis_array[axis].zeroaxis = gp_alloc(sizeof(lp_style_type), "zeroaxis");
	*(axis_array[axis].zeroaxis) = default_axis_zeroaxis;
	lp_parse(axis_array[axis].zeroaxis, LP_ADHOC, FALSE);
    }
}

/* process 'set zeroaxis' command */
static void
set_allzeroaxis()
{
    int save_token = c_token;
    set_zeroaxis(FIRST_X_AXIS);
    c_token = save_token;
    set_zeroaxis(FIRST_Y_AXIS);
    c_token = save_token;
    set_zeroaxis(FIRST_Z_AXIS);
}

/* Implements 'set tics' 'set xtics' 'set ytics' etc */
static int
set_tic_prop(AXIS_INDEX axis)
{
    int match = 0;		/* flag, set by matching a tic command */
    char nocmd[12];		/* fill w/ "no"+axis_name+suffix */
    char *cmdptr = NULL, *sfxptr = NULL;

    if (axis < PARALLEL_AXES) {
	(void) strcpy(nocmd, "no");
	cmdptr = &nocmd[2];
	(void) strcpy(cmdptr, axis_name(axis));
	sfxptr = &nocmd[strlen(nocmd)];
	(void) strcpy(sfxptr, "t$ics");	/* STRING */
    }

    if (almost_equals(c_token, cmdptr) || axis >= PARALLEL_AXES) {
	TBOOLEAN axisset = FALSE;
	TBOOLEAN mirror_opt = FALSE; /* set to true if (no)mirror option specified) */
	axis_array[axis].ticdef.def.mix = FALSE;
	match = 1;
	++c_token;
	do {
	    if (almost_equals(c_token, "ax$is")) {
		axisset = TRUE;
		axis_array[axis].ticmode &= ~TICS_ON_BORDER;
		axis_array[axis].ticmode |= TICS_ON_AXIS;
		++c_token;
	    } else if (almost_equals(c_token, "bo$rder")) {
		axis_array[axis].ticmode &= ~TICS_ON_AXIS;
		axis_array[axis].ticmode |= TICS_ON_BORDER;
		++c_token;
	    } else if (almost_equals(c_token, "mi$rror")) {
		axis_array[axis].ticmode |= TICS_MIRROR;
		mirror_opt = TRUE;
		++c_token;
	    } else if (almost_equals(c_token, "nomi$rror")) {
		axis_array[axis].ticmode &= ~TICS_MIRROR;
		mirror_opt = TRUE;
		++c_token;
	    } else if (almost_equals(c_token, "in$wards")) {
		axis_array[axis].tic_in = TRUE;
		++c_token;
	    } else if (almost_equals(c_token, "out$wards")) {
		axis_array[axis].tic_in = FALSE;
		++c_token;
	    } else if (almost_equals(c_token, "sc$ale")) {
		++c_token;
		if (almost_equals(c_token, "def$ault")) {
		    axis_array[axis].ticscale = 1.0;
		    axis_array[axis].miniticscale = 0.5;
		    ++c_token;
		} else {
		    axis_array[axis].ticscale = real_expression();
		    if (equals(c_token, ",")) {
			++c_token;
			axis_array[axis].miniticscale = real_expression();
		    } else
			axis_array[axis].miniticscale =
			    0.5 * axis_array[axis].ticscale;
		}
	    } else if (almost_equals(c_token, "ro$tate")) {
		axis_array[axis].tic_rotate = TEXT_VERTICAL;
		++c_token;
		if (equals(c_token, "by")) {
		    c_token++;
		    axis_array[axis].tic_rotate = int_expression();
		}
	    } else if (almost_equals(c_token, "noro$tate")) {
		axis_array[axis].tic_rotate = 0;
		++c_token;
	    } else if (almost_equals(c_token, "off$set")) {
		++c_token;
		get_position_default(&axis_array[axis].ticdef.offset,
				     character);
	    } else if (almost_equals(c_token, "nooff$set")) {
		++c_token;
		axis_array[axis].ticdef.offset = default_offset;
	    } else if (almost_equals(c_token, "l$eft")) {
		axis_array[axis].label.pos = LEFT;
		axis_array[axis].manual_justify = TRUE;
		c_token++;
	    } else if (almost_equals(c_token, "c$entre")
		       || almost_equals(c_token, "c$enter")) {
		axis_array[axis].label.pos = CENTRE;
		axis_array[axis].manual_justify = TRUE;
		c_token++;
	    } else if (almost_equals(c_token, "ri$ght")) {
		axis_array[axis].label.pos = RIGHT;
		axis_array[axis].manual_justify = TRUE;
		c_token++;
	    } else if (almost_equals(c_token, "autoj$ustify")) {
		axis_array[axis].manual_justify = FALSE;
		c_token++;
	    } else if (almost_equals(c_token,"range$limited")) {
		axis_array[axis].ticdef.rangelimited = TRUE;
		++c_token;
	    } else if (almost_equals(c_token,"norange$limited")) {
		axis_array[axis].ticdef.rangelimited = FALSE;
		++c_token;
	    } else if (almost_equals(c_token, "f$ont")) {
		++c_token;
		/* Make sure they've specified a font */
		if (!isstringvalue(c_token))
		    int_error(c_token,"expected font");
		else {
		    free(axis_array[axis].ticdef.font);
		    axis_array[axis].ticdef.font = NULL;
		    axis_array[axis].ticdef.font = try_to_get_string();
		}

	    /* The geographic/timedate/numeric options are new in version 5 */
	    } else if (almost_equals(c_token,"geo$graphic")) {
		++c_token;
		axis_array[axis].tictype = DT_DMS;
	    } else if (almost_equals(c_token,"time$date")) {
		++c_token;
		axis_array[axis].tictype = DT_TIMEDATE;
	    } else if (almost_equals(c_token,"numeric")) {
		++c_token;
		axis_array[axis].tictype = DT_NORMAL;

	    } else if (equals(c_token,"format")) {
		char *format;
		++c_token;
		if (!((format = try_to_get_string())))
		    int_error(c_token,"expected format");
		free(axis_array[axis].formatstring);
		axis_array[axis].formatstring  = format;
	    } else if (almost_equals(c_token, "enh$anced")) {
		++c_token;
		axis_array[axis].ticdef.enhanced = TRUE;
	    } else if (almost_equals(c_token, "noenh$anced")) {
		++c_token;
		axis_array[axis].ticdef.enhanced = FALSE;
	    } else if (equals(c_token,"tc") ||
		       almost_equals(c_token,"text$color")) {
		parse_colorspec(&axis_array[axis].ticdef.textcolor,
				axis == FIRST_Z_AXIS ? TC_Z : TC_FRAC);
	    } else if (almost_equals(c_token, "au$tofreq")) {
		/* auto tic interval */
		++c_token;
		if (!axis_array[axis].ticdef.def.mix) {
		    free_marklist(axis_array[axis].ticdef.def.user);
		    axis_array[axis].ticdef.def.user = NULL;
		}
		axis_array[axis].ticdef.type = TIC_COMPUTED;
	    } else if (equals(c_token,"add")) {
		++c_token;
		axis_array[axis].ticdef.def.mix = TRUE;
	    } else if (!END_OF_COMMAND) {
		load_tics(axis);
	    }
	} while (!END_OF_COMMAND);

	/* if tics are off and not set by axis, reset to default (border) */
	if (((axis_array[axis].ticmode & TICS_MASK) == NO_TICS) && (!axisset)) {
	    if (axis >= PARALLEL_AXES)
		axis_array[axis].ticmode |= TICS_ON_AXIS;
	    else
		axis_array[axis].ticmode |= TICS_ON_BORDER;
	    if ((mirror_opt == FALSE) && ((axis == FIRST_X_AXIS) || (axis == FIRST_Y_AXIS) || (axis == COLOR_AXIS))) {
		axis_array[axis].ticmode |= TICS_MIRROR;
	    }
	}

    }

    /* The remaining command options cannot work for parallel axes */
    if (axis >= PARALLEL_AXES)
	return match;

    if (almost_equals(c_token, nocmd)) {	/* NOSTRING */
	axis_array[axis].ticmode &= ~TICS_MASK;
	c_token++;
	match = 1;
    }

/* other options */

    (void) strcpy(sfxptr, "m$tics");	/* MONTH */
    if (almost_equals(c_token, cmdptr)) {
	if (!axis_array[axis].ticdef.def.mix) {
	    free_marklist(axis_array[axis].ticdef.def.user);
	    axis_array[axis].ticdef.def.user = NULL;
	}
	axis_array[axis].ticdef.type = TIC_MONTH;
	++c_token;
	match = 1;
    }
    if (almost_equals(c_token, nocmd)) {	/* NOMONTH */
	axis_array[axis].ticdef.type = TIC_COMPUTED;
	++c_token;
	match = 1;
    }
    (void) strcpy(sfxptr, "d$tics");	/* DAYS */
    if (almost_equals(c_token, cmdptr)) {
	match = 1;
	if (!axis_array[axis].ticdef.def.mix) {
	    free_marklist(axis_array[axis].ticdef.def.user);
	    axis_array[axis].ticdef.def.user = NULL;
	}
	axis_array[axis].ticdef.type = TIC_DAY;
	++c_token;
    }
    if (almost_equals(c_token, nocmd)) {	/* NODAYS */
	axis_array[axis].ticdef.type = TIC_COMPUTED;
	++c_token;
	match = 1;
    }
    *cmdptr = 'm';
    (void) strcpy(cmdptr + 1, axis_name(axis));
    (void) strcat(cmdptr, "t$ics");	/* MINISTRING */

    if (almost_equals(c_token, cmdptr)) {
	c_token++;
	match = 1;
	if (END_OF_COMMAND) {
	    axis_array[axis].minitics = MINI_AUTO;
	} else if (almost_equals(c_token, "def$ault")) {
	    axis_array[axis].minitics = MINI_DEFAULT;
	    ++c_token;
	} else {
	    int freq = int_expression();
	    if (freq > 0 && freq < 101) {
		axis_array[axis].mtic_freq = freq;
		axis_array[axis].minitics = MINI_USER;
	    } else {
		axis_array[axis].minitics = MINI_DEFAULT;
		int_warn(c_token-1,"Expecting number of intervals");
	    }
	}
    }
    if (almost_equals(c_token, nocmd)) {	/* NOMINI */
	axis_array[axis].minitics = MINI_OFF;
	c_token++;
	match = 1;
    }
    return (match);
}

/* process a 'set {x/y/z}label command */
/* set {x/y/z}label {label_text} {offset {x}{,y}} {<fontspec>} {<textcolor>} */
static void
set_xyzlabel(text_label *label)
{
    char *text = NULL;

    c_token++;
    if (END_OF_COMMAND) {	/* no label specified */
	free(label->text);
	label->text = NULL;
	return;
    }

    parse_label_options(label, FALSE);

    if (!END_OF_COMMAND) {
	text = try_to_get_string();
	if (text) {
	    free(label->text);
	    label->text = text;
	}
    }

    parse_label_options(label, FALSE);

}


/*
 * Change or insert a new linestyle in a list of line styles.
 * Supports the old 'set linestyle' command (backwards-compatible)
 * and the new "set style line" and "set linetype" commands.
 * destination_class is either LP_STYLE or LP_TYPE.
 */
static void
set_linestyle(struct linestyle_def **head, lp_class destination_class)
{
    struct linestyle_def *this_linestyle = NULL;
    struct linestyle_def *new_linestyle = NULL;
    struct linestyle_def *prev_linestyle = NULL;
    int tag;

    c_token++;

    /* get tag */
    if (END_OF_COMMAND || ((tag = int_expression()) <= 0))
	int_error(c_token, "tag must be > zero");

    /* Check if linestyle is already defined */
    for (this_linestyle = *head; this_linestyle != NULL;
	 prev_linestyle = this_linestyle, this_linestyle = this_linestyle->next)
	if (tag <= this_linestyle->tag)
		break;

    if (this_linestyle == NULL || tag != this_linestyle->tag) {
	/* Default style is based on linetype with the same tag id */
	struct lp_style_type loc_lp = DEFAULT_LP_STYLE_TYPE;
	loc_lp.l_type = tag - 1;
	loc_lp.p_type = tag - 1;
	loc_lp.d_type = DASHTYPE_SOLID;
	loc_lp.pm3d_color.type = TC_LT;
	loc_lp.pm3d_color.lt = tag - 1;

	new_linestyle = gp_alloc(sizeof(struct linestyle_def), "linestyle");
	if (prev_linestyle != NULL)
	    prev_linestyle->next = new_linestyle;	/* add it to end of list */
	else
	    *head = new_linestyle;	/* make it start of list */
	new_linestyle->tag = tag;
	new_linestyle->next = this_linestyle;
	new_linestyle->lp_properties = loc_lp;
	this_linestyle = new_linestyle;
    }

    if (almost_equals(c_token, "def$ault")) {
	delete_linestyle(head, prev_linestyle, this_linestyle);
	c_token++;
    } else
	/* pick up a line spec; dont allow ls, do allow point type */
	lp_parse(&this_linestyle->lp_properties, destination_class, TRUE);

    if (!END_OF_COMMAND)
	int_error(c_token,"Extraneous arguments to set %s",
		head == &first_perm_linestyle ? "linetype" : "style line");
}

/*
 * Delete linestyle from linked list.
 * Called with pointers to the head of the list,
 * to the previous linestyle (not strictly necessary),
 * and to the linestyle to delete.
 */
void
delete_linestyle(struct linestyle_def **head, struct linestyle_def *prev, struct linestyle_def *this)
{
    if (this != NULL) {		/* there really is something to delete */
	if (this == *head)
	    *head = this->next;
	else
	    prev->next = this->next;
	free(this);
    }
}


/* ======================================================== */
/* process a 'set arrowstyle' command */
/* set style arrow {tag} {nohead|head|backhead|heads} {size l,a{,b}} {{no}filled} {linestyle...} {layer n}*/
static void
set_arrowstyle()
{
    struct arrowstyle_def *this_arrowstyle = NULL;
    struct arrowstyle_def *new_arrowstyle = NULL;
    struct arrowstyle_def *prev_arrowstyle = NULL;
    struct arrow_style_type loc_arrow;
    int tag;

    default_arrow_style(&loc_arrow);

    c_token++;

    /* get tag */
    if (!END_OF_COMMAND) {
	/* must be a tag expression! */
	tag = int_expression();
	if (tag <= 0)
	    int_error(c_token, "tag must be > zero");
    } else
	tag = assign_arrowstyle_tag();	/* default next tag */

    /* search for arrowstyle */
    if (first_arrowstyle != NULL) {	/* skip to last arrowstyle */
	for (this_arrowstyle = first_arrowstyle; this_arrowstyle != NULL;
	     prev_arrowstyle = this_arrowstyle,
	     this_arrowstyle = this_arrowstyle->next)
	    /* is this the arrowstyle we want? */
	    if (tag <= this_arrowstyle->tag)
		break;
    }

    if (this_arrowstyle == NULL || tag != this_arrowstyle->tag) {
	/* adding the arrowstyle */
	new_arrowstyle = (struct arrowstyle_def *)
	    gp_alloc(sizeof(struct arrowstyle_def), "arrowstyle");
	default_arrow_style(&(new_arrowstyle->arrow_properties));
	if (prev_arrowstyle != NULL)
	    prev_arrowstyle->next = new_arrowstyle;	/* add it to end of list */
	else
	    first_arrowstyle = new_arrowstyle;	/* make it start of list */
	new_arrowstyle->arrow_properties.tag = tag;
	new_arrowstyle->tag = tag;
	new_arrowstyle->next = this_arrowstyle;
	this_arrowstyle = new_arrowstyle;
    }

    if (END_OF_COMMAND)
	this_arrowstyle->arrow_properties = loc_arrow;
    else if (almost_equals(c_token, "def$ault")) {
	this_arrowstyle->arrow_properties = loc_arrow;
	c_token++;
    } else
	/* pick up a arrow spec : dont allow arrowstyle */
	arrow_parse(&this_arrowstyle->arrow_properties, FALSE);

    if (!END_OF_COMMAND)
	int_error(c_token, "extraneous or out-of-order arguments in set arrowstyle");

}

/* assign a new arrowstyle tag
 * arrowstyles are kept sorted by tag number, so this is easy
 * returns the lowest unassigned tag number
 */
static int
assign_arrowstyle_tag()
{
    struct arrowstyle_def *this;
    int last = 0;		/* previous tag value */

    for (this = first_arrowstyle; this != NULL; this = this->next)
	if (this->tag == last + 1)
	    last++;
	else
	    break;

    return (last + 1);
}

/* For set [xy]tics... command */
static void
load_tics(AXIS_INDEX axis)
{
    if (equals(c_token, "(")) {	/* set : TIC_USER */
	c_token++;
	load_tic_user(axis);
    } else {			/* series : TIC_SERIES */
	load_tic_series(axis);
    }
}

/* load TIC_USER definition */
/* (tic[,tic]...)
 * where tic is ["string"] value [level]
 * Left paren is already scanned off before entry.
 */
static void
load_tic_user(AXIS_INDEX axis)
{
    char *ticlabel;
    double ticposition;

    /* Free any old tic labels */
    if (!axis_array[axis].ticdef.def.mix && !(set_iterator && set_iterator->iteration)) {
	free_marklist(axis_array[axis].ticdef.def.user);
	axis_array[axis].ticdef.def.user = NULL;
    }

    /* Mark this axis as user-generated ticmarks only, unless the */
    /* mix flag indicates that both user- and auto- tics are OK.  */
    if (!axis_array[axis].ticdef.def.mix)
	axis_array[axis].ticdef.type = TIC_USER;

    while (!END_OF_COMMAND && !equals(c_token,")")) {
	int ticlevel=0;
	int save_token;
	/* syntax is  (  {'format'} value {level} {, ...} )
	 * but for timedata, the value itself is a string, which
	 * complicates things somewhat
	 */

	/* has a string with it? */
	save_token = c_token;
	ticlabel = try_to_get_string();
	if (ticlabel && axis_array[axis].datatype == DT_TIMEDATE
	    && (equals(c_token,",") || equals(c_token,")"))) {
	    c_token = save_token;
	    free(ticlabel);
	    ticlabel = NULL;
	}

	/* in any case get the value */
	GET_NUM_OR_TIME(ticposition, axis);

	if (!END_OF_COMMAND &&
	    !equals(c_token, ",") &&
	    !equals(c_token, ")")) {
	  ticlevel = int_expression(); /* tic level */
	}

	/* add to list */
	add_tic_user(axis, ticlabel, ticposition, ticlevel);
	free(ticlabel);

	/* expect "," or ")" here */
	if (!END_OF_COMMAND && equals(c_token, ","))
	    c_token++;		/* loop again */
	else
	    break;		/* hopefully ")" */
    }

    if (END_OF_COMMAND || !equals(c_token, ")")) {
	free_marklist(axis_array[axis].ticdef.def.user);
	axis_array[axis].ticdef.def.user = NULL;
	int_error(c_token, "expecting right parenthesis )");
    }
    c_token++;
}

void
free_marklist(struct ticmark *list)
{
    while (list != NULL) {
	struct ticmark *freeable = list;
	list = list->next;
	if (freeable->label != NULL)
	    free(freeable->label);
	free(freeable);
    }
}

/* Remove tic labels that were read from a datafile during a previous plot
 * via the 'using xtics(n)' mechanism.  These have tick level < 0.
 */
struct ticmark *
prune_dataticks(struct ticmark *list)
{
    struct ticmark a = {0.0,NULL,0,NULL};
    struct ticmark *b = &a;
    struct ticmark *tmp;

    while (list) {
	if (list->level < 0) {
	    free(list->label);
	    tmp = list->next;
	    free(list);
	    list = tmp;
	} else {
	    b->next = list;
	    b = list;
	    list = list->next;
	}
    }
    b->next = NULL;
    return a.next;
}

/* load TIC_SERIES definition */
/* [start,]incr[,end] */
static void
load_tic_series(AXIS_INDEX axis)
{
    double start, incr, end;
    int incr_token;

    struct ticdef *tdef = &axis_array[axis].ticdef;

    GET_NUM_OR_TIME(start, axis);

    if (!equals(c_token, ",")) {
	/* only step specified */
	incr = start;
	start = -VERYLARGE;
	end = VERYLARGE;
    } else {
	c_token++;
	incr_token = c_token;
	GET_NUM_OR_TIME(incr, axis);

	if (!equals(c_token, ",")) {
	    /* only step and increment specified */
	    end = VERYLARGE;
	} else {
	    c_token++;
	    GET_NUM_OR_TIME(end, axis);
	}
    }

    if (start < end && incr <= 0)
	int_error(incr_token, "increment must be positive");
    if (start > end && incr >= 0)
	int_error(incr_token, "increment must be negative");
    if (start > end) {
	/* put in order */
	double numtics = floor((end * (1 + SIGNIF) - start) / incr);

	end = start;
	start = end + numtics * incr;
	incr = -incr;
    }

    if (!tdef->def.mix) { /* remove old list */
	free_marklist(tdef->def.user);
	tdef->def.user = NULL;
    }
    tdef->type = TIC_SERIES;
    tdef->def.series.start = start;
    tdef->def.series.incr = incr;
    tdef->def.series.end = end;
}

/*
 * new_text_label() allocates and initializes a text_label structure.
 * This routine is also used by the plot and splot with labels commands.
 */
struct text_label *
new_text_label(int tag)
{
    struct text_label *new;

    new = gp_alloc( sizeof(struct text_label), "text_label");
    memset(new, 0, sizeof(struct text_label));
    new->tag = tag;
    new->place = default_position;
    new->pos = LEFT;
    new->textcolor.type = TC_DEFAULT;
    new->lp_properties.p_type = 1;
    new->offset = default_offset;

    return(new);
}

/*
 * Parse the sub-options for label style and placement.
 * This is called from set_label, and from plot2d and plot3d
 * to handle options for 'plot with labels'
 */
void
parse_label_options( struct text_label *this_label, TBOOLEAN in_plot )
{
    struct position pos;
    char *font = NULL;
    enum JUSTIFY just = LEFT;
    int rotate = 0;
    TBOOLEAN set_position = FALSE, set_just = FALSE, set_point = FALSE,
	set_rot = FALSE, set_font = FALSE, set_offset = FALSE,
	set_layer = FALSE, set_textcolor = FALSE, set_hypertext = FALSE;
    int layer = LAYER_BACK;
    TBOOLEAN axis_label = (this_label->tag == -2);
    TBOOLEAN hypertext = FALSE;
    struct position offset = default_offset;
    t_colorspec textcolor = {TC_DEFAULT,0,0.0};
    struct lp_style_type loc_lp = DEFAULT_LP_STYLE_TYPE;
    loc_lp.flags = LP_NOT_INITIALIZED;

   /* Now parse the label format and style options */
    while (!END_OF_COMMAND) {
	/* get position */
	if (!in_plot && !set_position && equals(c_token, "at") && !axis_label) {
	    c_token++;
	    get_position(&pos);
	    set_position = TRUE;
	    continue;
	}

	/* get justification */
	if (! set_just) {
	    if (almost_equals(c_token, "l$eft")) {
		just = LEFT;
		c_token++;
		set_just = TRUE;
		continue;
	    } else if (almost_equals(c_token, "c$entre")
		       || almost_equals(c_token, "c$enter")) {
		just = CENTRE;
		c_token++;
		set_just = TRUE;
		continue;
	    } else if (almost_equals(c_token, "r$ight")) {
		just = RIGHT;
		c_token++;
		set_just = TRUE;
		continue;
	    }
	}

	/* get rotation (added by RCC) */
	if (almost_equals(c_token, "rot$ate")) {
	    c_token++;
	    set_rot = TRUE;
	    rotate = this_label->rotate;
	    if (equals(c_token, "by")) {
		c_token++;
		rotate = int_expression();
	    } else if (almost_equals(c_token,"para$llel")) {
		if (this_label->tag >= 0)
		    int_error(c_token,"invalid option");
		c_token++;
		this_label->tag = ROTATE_IN_3D_LABEL_TAG;
	    } else if (almost_equals(c_token,"var$iable")) {
		if (in_plot)	/* only in 2D plot with labels */
		    this_label->tag = VARIABLE_ROTATE_LABEL_TAG;
		else
		    set_rot = FALSE;
		c_token++;
	    } else
		rotate = TEXT_VERTICAL;
	    continue;
	} else if (almost_equals(c_token, "norot$ate")) {
	    rotate = 0;
	    c_token++;
	    set_rot = TRUE;
	    if (this_label->tag == ROTATE_IN_3D_LABEL_TAG)
		this_label->tag = NONROTATABLE_LABEL_TAG;
	    continue;
	}

	/* get font (added by DJL) */
	if (! set_font && equals(c_token, "font")) {
	    c_token++;
	    if ((font = try_to_get_string())) {
		set_font = TRUE;
		continue;
	    } else
		int_error(c_token, "'fontname,fontsize' expected");
	}

	/* Flag this as hypertext rather than a normal label */
	if (!set_hypertext && almost_equals(c_token,"hyper$text")) {
	    c_token++;
	    hypertext = TRUE;
	    set_hypertext = TRUE;
	    if (!set_point)
		loc_lp = default_hypertext_point_style;
	    continue;
	} else if (!set_hypertext && almost_equals(c_token,"nohyper$text")) {
	    c_token++;
	    hypertext = FALSE;
	    set_hypertext = TRUE;
	    continue;
	}

	/* get front/back (added by JDP) */
	if (!in_plot && !set_layer && !axis_label) {
	    if (equals(c_token, "back")) {
		layer = LAYER_BACK;
		c_token++;
		set_layer = TRUE;
		continue;
	    } else if (equals(c_token, "front")) {
		layer = LAYER_FRONT;
		c_token++;
		set_layer = TRUE;
		continue;
	    }
	}

#ifdef EAM_BOXED_TEXT
	if (equals(c_token, "boxed")) {
	    this_label->boxed = 1;
	    c_token++;
	    continue;
	} else if (equals(c_token, "noboxed")) {
	    this_label->boxed = 0;
	    c_token++;
	    continue;
	}
#endif

	if (!axis_label && (loc_lp.flags == LP_NOT_INITIALIZED || set_hypertext)) {
	    if (almost_equals(c_token, "po$int")) {
		int stored_token = ++c_token;
		struct lp_style_type tmp_lp;
		loc_lp.flags = LP_SHOW_POINTS;
		tmp_lp = loc_lp;
		lp_parse(&tmp_lp, LP_ADHOC, TRUE);
		if (stored_token != c_token)
		    loc_lp = tmp_lp;
		set_point = TRUE;
		continue;
	    } else if (almost_equals(c_token, "nopo$int")) {
		loc_lp.flags = 0;
		c_token++;
		continue;
	    }
	}

	if (! set_offset && almost_equals(c_token, "of$fset")) {
	    c_token++;
	    get_position_default(&offset,character);
	    set_offset = TRUE;
	    continue;
	}

	if ((equals(c_token,"tc") || almost_equals(c_token,"text$color"))
	    && ! set_textcolor ) {
	    parse_colorspec( &textcolor, TC_VARIABLE );
	    set_textcolor = TRUE;
	    continue;
	}

	if (almost_equals(c_token,"noenh$anced")) {
	    this_label->noenhanced = TRUE;
	    c_token++;
	    continue;
	} else if (almost_equals(c_token,"enh$anced")) {
	    this_label->noenhanced = FALSE;
	    c_token++;
	    continue;
	}

	/* Coming here means that none of the previous 'if's struck
	 * its "continue" statement, i.e.  whatever is in the command
	 * line is forbidden by the 'set label' command syntax.
	 * On the other hand, 'plot with labels' may have additional stuff coming up.
	 */
	break;

    } /* while(!END_OF_COMMAND) */

    /* HBB 20011120: this chunk moved here, behind the while()
     * loop. Only after all options have been parsed it's safe to
     * overwrite the position if none has been specified. */
    if (!set_position)
	pos = default_position;

    /* OK! copy the requested options into the label */
	if (set_position)
	    this_label->place = pos;
	if (set_just)
	    this_label->pos = just;
	if (set_rot)
	    this_label->rotate = rotate;
	if (set_layer)
	    this_label->layer = layer;
	if (set_font)
	    this_label->font = font;
	if (set_textcolor)
	    this_label->textcolor = textcolor;
	if ((loc_lp.flags & LP_NOT_INITIALIZED) == 0)
	    this_label->lp_properties = loc_lp;
	if (set_offset)
	    this_label->offset = offset;
	if (set_hypertext)
	    this_label->hypertext = hypertext;

    /* Make sure the z coord and the z-coloring agree */
    if (this_label->textcolor.type == TC_Z)
	this_label->textcolor.value = this_label->place.z;
}


/* <histogramstyle> = {clustered {gap <n>} | rowstacked | columnstacked */
/*                     errorbars {gap <n>} {linewidth <lw>}}            */
/*                    {title <title_options>}                           */
static void
parse_histogramstyle( histogram_style *hs,
		t_histogram_type def_type,
		int def_gap)
{
    text_label title_specs = EMPTY_LABELSTRUCT;

    /* Set defaults */
    hs->type  = def_type;
    hs->gap   = def_gap;

    if (END_OF_COMMAND)
	return;
    if (!equals(c_token,"hs") && !almost_equals(c_token,"hist$ogram"))
	return;
    c_token++;

    while (!END_OF_COMMAND) {
	if (almost_equals(c_token, "clust$ered")) {
	    hs->type = HT_CLUSTERED;
	    c_token++;
	} else if (almost_equals(c_token, "error$bars")) {
	    hs->type = HT_ERRORBARS;
	    c_token++;
	} else if (almost_equals(c_token, "rows$tacked")) {
	    hs->type = HT_STACKED_IN_LAYERS;
	    c_token++;
	} else if (almost_equals(c_token, "columns$tacked")) {
	    hs->type = HT_STACKED_IN_TOWERS;
	    c_token++;
	} else if (equals(c_token, "gap")) {
	    if (isanumber(++c_token))
		hs->gap = int_expression();
	    else
		int_error(c_token,"expected gap value");
	} else if (almost_equals(c_token, "ti$tle")) {
	    title_specs.offset = hs->title.offset;
	    set_xyzlabel(&title_specs);
	    free(title_specs.text);
	    title_specs.text = NULL;
	    free(hs->title.font);
	    hs->title.font = NULL;
	    hs->title = title_specs;
	} else if ((equals(c_token,"lw") || almost_equals(c_token,"linew$idth"))
		  && (hs->type == HT_ERRORBARS)) {
	    c_token++;
	    hs->bar_lw = real_expression();
	    if (hs->bar_lw <= 0)
		hs->bar_lw = 1;
	} else
	    /* We hit something unexpected */
	    break;
    }
}

/* process 'set style parallelaxis' command */
static void
set_style_parallel()
{
    c_token++;
    while (!END_OF_COMMAND) {
	int save_token = c_token;
	lp_parse( &parallel_axis_style.lp_properties,  LP_ADHOC, FALSE );
	if (save_token != c_token)
	    continue;
	if (equals(c_token, "front"))
	    parallel_axis_style.layer = LAYER_FRONT;
	else if (equals(c_token, "back"))
	    parallel_axis_style.layer = LAYER_BACK;
	else
	    int_error(c_token, "unrecognized option");
	c_token++;
    }
}

/* Utility routine to propagate rrange into corresponding x and y ranges */
void
rrange_to_xy()
{
    double min;
    if (R_AXIS.set_autoscale & AUTOSCALE_MIN)
	min = 0;
    else
	min = R_AXIS.set_min;
    if (R_AXIS.set_autoscale & AUTOSCALE_MAX) {
	X_AXIS.set_autoscale = AUTOSCALE_BOTH;
	Y_AXIS.set_autoscale = AUTOSCALE_BOTH;
    } else {
	X_AXIS.set_autoscale = AUTOSCALE_NONE;
	Y_AXIS.set_autoscale = AUTOSCALE_NONE;
	if (R_AXIS.log)
	    X_AXIS.set_max =  AXIS_DO_LOG(POLAR_AXIS, R_AXIS.set_max)
			    - AXIS_DO_LOG(POLAR_AXIS, min);
	else
	    X_AXIS.set_max = R_AXIS.set_max - min;
	Y_AXIS.set_max = X_AXIS.set_max;
	Y_AXIS.set_min = X_AXIS.set_min = -X_AXIS.set_max;
    }
}

