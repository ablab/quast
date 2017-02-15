#ifndef lint
static char *RCSid() { return RCSid("$Id: save.c,v 1.256.2.20 2016/08/27 20:50:12 sfeam Exp $"); }
#endif

/* GNUPLOT - save.c */

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

#include "save.h"

#include "command.h"
#include "contour.h"
#include "datafile.h"
#include "eval.h"
#include "fit.h"
#include "gp_time.h"
#include "graphics.h"
#include "hidden3d.h"
#include "misc.h"
#include "plot2d.h"
#include "plot3d.h"
#include "setshow.h"
#include "term_api.h"
#include "util.h"
#include "variable.h"
#include "pm3d.h"
#include "getcolor.h"

static void save_functions__sub __PROTO((FILE *));
static void save_variables__sub __PROTO((FILE *));
static void save_tics __PROTO((FILE *, AXIS_INDEX));
static void save_zeroaxis __PROTO((FILE *,AXIS_INDEX));
static void save_set_all __PROTO((FILE *));

static const char *coord_msg[] = {"first ", "second ", "graph ", "screen ", "character "};
/*
 *  functions corresponding to the arguments of the GNUPLOT `save` command
 */
void
save_functions(FILE *fp)
{
    /* I _love_ information written at the top and the end
     * of a human readable ASCII file. */
    show_version(fp);
    save_functions__sub(fp);
    fputs("#    EOF\n", fp);
}


void
save_variables(FILE *fp)
{
	show_version(fp);
	save_variables__sub(fp);
	fputs("#    EOF\n", fp);
}


void
save_set(FILE *fp)
{
	show_version(fp);
	save_set_all(fp);
	fputs("#    EOF\n", fp);
}


void
save_all(FILE *fp)
{
	show_version(fp);
	save_set_all(fp);
	save_functions__sub(fp);
	save_variables__sub(fp);
	if (df_filename)
	    fprintf(fp, "## Last datafile plotted: \"%s\"\n", df_filename);
	fprintf(fp, "%s\n", replot_line);
	if (wri_to_fil_last_fit_cmd(NULL)) {
	    fputs("## ", fp);
	    wri_to_fil_last_fit_cmd(fp);
	    putc('\n', fp);
	}
	fputs("#    EOF\n", fp);
}

/*
 *  auxiliary functions
 */

static void
save_functions__sub(FILE *fp)
{
    struct udft_entry *udf = first_udf;

    while (udf) {
	if (udf->definition) {
	    fprintf(fp, "%s\n", udf->definition);
	}
	udf = udf->next_udf;
    }
}

static void
save_variables__sub(FILE *fp)
{
    /* always skip pi */
    struct udvt_entry *udv = first_udv->next_udv;

    while (udv) {
	if (!udv->udv_undef) {
	    if (strncmp(udv->udv_name,"GPVAL_",6)
	     && strncmp(udv->udv_name,"MOUSE_",6)
	     && strncmp(udv->udv_name,"$",1)
	     && (strncmp(udv->udv_name,"ARG",3) || (strlen(udv->udv_name) != 4))
	     && strncmp(udv->udv_name,"NaN",4)) {
		fprintf(fp, "%s = ", udv->udv_name);
		disp_value(fp, &(udv->udv_value), TRUE);
		(void) putc('\n', fp);
	    }
	}
	udv = udv->next_udv;
    }
}

/* HBB 19990823: new function 'save term'. This will be mainly useful
 * for the typical 'set term post ... plot ... set term <normal term>
 * sequence. It's the only 'save' function that will write the
 * current term setting to a file uncommentedly. */
void
save_term(FILE *fp)
{
	show_version(fp);

	/* A possible gotcha: the default initialization often doesn't set
	 * term_options, but a 'set term <type>' without options doesn't
	 * reset the options to startup defaults. This may have to be
	 * changed on a per-terminal driver basis... */
	if (term)
	    fprintf(fp, "set terminal %s %s\n", term->name, term_options);
	else
	    fputs("set terminal unknown\n", fp);

	/* output will still be written in commented form.  Otherwise, the
	 * risk of overwriting files is just too high */
	if (outstr)
	    fprintf(fp, "# set output '%s'\n", outstr);
	else
	    fputs("# set output\n", fp);
	fputs("#    EOF\n", fp);
}

static void
save_justification(int just, FILE *fp)
{
    switch (just) {
	case RIGHT:
	    fputs(" right", fp);
	    break;
	case LEFT:
	    fputs(" left", fp);
	    break;
	case CENTRE:
	    fputs(" center", fp);
	    break;
    }
}

static void
save_set_all(FILE *fp)
{
    struct text_label *this_label;
    struct arrow_def *this_arrow;
    struct linestyle_def *this_linestyle;
    struct arrowstyle_def *this_arrowstyle;
    legend_key *key = &keyT;
    int axis;

    /* opinions are split as to whether we save term and outfile
     * as a compromise, we output them as comments !
     */
    if (term)
	fprintf(fp, "# set terminal %s %s\n", term->name, term_options);
    else
	fputs("# set terminal unknown\n", fp);

    if (outstr)
	fprintf(fp, "# set output '%s'\n", outstr);
    else
	fputs("# set output\n", fp);

    fprintf(fp, "\
%sset clip points\n\
%sset clip one\n\
%sset clip two\n\
set bar %f %s\n",
	    (clip_points) ? "" : "un",
	    (clip_lines1) ? "" : "un",
	    (clip_lines2) ? "" : "un",
	    bar_size, (bar_layer == LAYER_BACK) ? "back" : "front");

    if (draw_border) {
	fprintf(fp, "set border %d %s", draw_border,
	    border_layer == LAYER_BEHIND ? "behind" : border_layer == LAYER_BACK ? "back" : "front");
	save_linetype(fp, &border_lp, FALSE);
	fprintf(fp, "\n");
    } else
	fputs("unset border\n", fp);

    for (axis = FIRST_AXES; axis < LAST_REAL_AXIS; axis++) {
	if (axis == SECOND_Z_AXIS) continue;
	if (axis == COLOR_AXIS) continue;
	fprintf(fp, "set %sdata %s\n", axis_name(axis),
		axis_array[axis].datatype == DT_TIMEDATE ? "time" :
		axis_array[axis].datatype == DT_DMS ? "geographic" :
		"");
    }

    if (boxwidth < 0.0)
	fputs("set boxwidth\n", fp);
    else
	fprintf(fp, "set boxwidth %g %s\n", boxwidth,
		(boxwidth_is_absolute) ? "absolute" : "relative");

    fprintf(fp, "set style fill ");
    save_fillstyle(fp, &default_fillstyle);

#ifdef EAM_OBJECTS
    /* Default rectangle style */
    fprintf(fp, "set style rectangle %s fc ",
	    default_rectangle.layer > 0 ? "front" :
	    default_rectangle.layer < 0 ? "behind" : "back");
    /* FIXME: broke with removal of use_palette? */
    save_pm3dcolor(fp, &default_rectangle.lp_properties.pm3d_color);
    fprintf(fp, " fillstyle ");
    save_fillstyle(fp, &default_rectangle.fillstyle);

    /* Default circle properties */
    fprintf(fp, "set style circle radius ");
    save_position(fp, &default_circle.o.circle.extent, FALSE);
    fputs(" \n", fp);

    /* Default ellipse properties */
    fprintf(fp, "set style ellipse size ");
    save_position(fp, &default_ellipse.o.ellipse.extent, FALSE);
    fprintf(fp, " angle %g ", default_ellipse.o.ellipse.orientation);
    fputs("units ", fp);
    switch (default_ellipse.o.ellipse.type) {
	case ELLIPSEAXES_XY:
	    fputs("xy\n", fp);
	    break;
	case ELLIPSEAXES_XX:
	    fputs("xx\n", fp);
	    break;
	case ELLIPSEAXES_YY:
	    fputs("yy\n", fp);
	    break;
    }
#endif

    if (dgrid3d) {
      if( dgrid3d_mode == DGRID3D_QNORM ) {
	fprintf(fp, "set dgrid3d %d,%d, %d\n",
	  	dgrid3d_row_fineness,
	  	dgrid3d_col_fineness,
	  	dgrid3d_norm_value);
      } else if( dgrid3d_mode == DGRID3D_SPLINES ) {
	fprintf(fp, "set dgrid3d %d,%d splines\n",
	  	dgrid3d_row_fineness, dgrid3d_col_fineness );
      } else {
	fprintf(fp, "set dgrid3d %d,%d %s%s %f,%f\n",
	  	dgrid3d_row_fineness,
	  	dgrid3d_col_fineness,
		reverse_table_lookup(dgrid3d_mode_tbl, dgrid3d_mode),
		dgrid3d_kdensity ? " kdensity2d" : "",
	  	dgrid3d_x_scale,
	  	dgrid3d_y_scale );
      }
    }

    /* Dummy variable names */ 
    fprintf(fp, "set dummy %s", set_dummy_var[0]);
    for (axis=1; axis<MAX_NUM_VAR; axis++) {
	if (*set_dummy_var[axis] == '\0')
	    break;
	fprintf(fp, ", %s", set_dummy_var[axis]);
    }
    fprintf(fp, "\n");

#define SAVE_FORMAT(axis)					\
    fprintf(fp, "set format %s \"%s\" %s\n", axis_name(axis),	\
	    conv_text(axis_array[axis].formatstring),		\
	    axis_array[axis].tictype == DT_DMS ? "geographic" :	\
	    axis_array[axis].tictype == DT_TIMEDATE ? "timedate" : \
	    "");
    SAVE_FORMAT(FIRST_X_AXIS );
    SAVE_FORMAT(FIRST_Y_AXIS );
    SAVE_FORMAT(SECOND_X_AXIS);
    SAVE_FORMAT(SECOND_Y_AXIS);
    SAVE_FORMAT(FIRST_Z_AXIS );
    SAVE_FORMAT(COLOR_AXIS );
    SAVE_FORMAT(POLAR_AXIS );
#undef SAVE_FORMAT

    fprintf(fp, "set timefmt \"%s\"\n", timefmt);

    fprintf(fp, "set angles %s\n",
	    (ang2rad == 1.0) ? "radians" : "degrees");

    fprintf(fp,"set tics %s\n", grid_tics_in_front ? "front" : "back");

    if (! some_grid_selected())
	fputs("unset grid\n", fp);
    else {
	if (polar_grid_angle) 	/* set angle already output */
	    fprintf(fp, "set grid polar %f\n", polar_grid_angle / ang2rad);
	else
	    fputs("set grid nopolar\n", fp);

#define SAVE_GRID(axis)					\
	fprintf(fp, " %s%stics %sm%stics",		\
		axis_array[axis].gridmajor ? "" : "no",	\
		axis_name(axis),		\
		axis_array[axis].gridminor ? "" : "no",	\
		axis_name(axis));
	fputs("set grid", fp);
	SAVE_GRID(FIRST_X_AXIS);
	SAVE_GRID(FIRST_Y_AXIS);
	SAVE_GRID(FIRST_Z_AXIS);
	fputs(" \\\n", fp);
	SAVE_GRID(SECOND_X_AXIS);
	SAVE_GRID(SECOND_Y_AXIS);
	SAVE_GRID(COLOR_AXIS);
	fputs("\n", fp);
#undef SAVE_GRID

	fprintf(fp, "set grid %s  ", (grid_layer==-1) ? "layerdefault" : ((grid_layer==0) ? "back" : "front"));
	save_linetype(fp, &grid_lp, FALSE);
	fprintf(fp, ", ");
	save_linetype(fp, &mgrid_lp, FALSE);
	fputc('\n', fp);
    }
    fprintf(fp, "%sset raxis\n", raxis ? "" : "un");

    /* Save parallel axis state */
    save_style_parallel(fp);

    fprintf(fp, "set key title \"%s\"", conv_text(key->title.text));
    if (key->title.font)
	fprintf(fp, " font \"%s\" ", key->title.font);
    save_justification(key->title.pos, fp);
    fputs("\n", fp);

    fputs("set key ", fp);
    switch (key->region) {
	case GPKEY_AUTO_INTERIOR_LRTBC:
	    fputs("inside", fp);
	    break;
	case GPKEY_AUTO_EXTERIOR_LRTBC:
	    fputs("outside", fp);
	    break;
	case GPKEY_AUTO_EXTERIOR_MARGIN:
	    switch (key->margin) {
	    case GPKEY_TMARGIN:
		fputs("tmargin", fp);
		break;
	    case GPKEY_BMARGIN:
		fputs("bmargin", fp);
		break;
	    case GPKEY_LMARGIN:
		fputs("lmargin", fp);
		break;
	    case GPKEY_RMARGIN:
		fputs("rmargin", fp);
		break;
	    }
	    break;
	case GPKEY_USER_PLACEMENT:
	    fputs("at ", fp);
	    save_position(fp, &key->user_pos, FALSE);
	    break;
    }
    if (!(key->region == GPKEY_AUTO_EXTERIOR_MARGIN
	      && (key->margin == GPKEY_LMARGIN || key->margin == GPKEY_RMARGIN))) {
	save_justification(key->hpos, fp);
    }
    if (!(key->region == GPKEY_AUTO_EXTERIOR_MARGIN
	      && (key->margin == GPKEY_TMARGIN || key->margin == GPKEY_BMARGIN))) {
	switch (key->vpos) {
	    case JUST_TOP:
		fputs(" top", fp);
		break;
	    case JUST_BOT:
		fputs(" bottom", fp);
		break;
	    case JUST_CENTRE:
		fputs(" center", fp);
		break;
	}
    }
    fprintf(fp, " %s %s %sreverse %senhanced %s ",
		key->stack_dir == GPKEY_VERTICAL ? "vertical" : "horizontal",
		key->just == GPKEY_LEFT ? "Left" : "Right",
		key->reverse ? "" : "no",
		key->enhanced ? "" : "no",
		key->auto_titles == COLUMNHEAD_KEYTITLES ? "autotitle columnhead"
		: key->auto_titles == FILENAME_KEYTITLES ? "autotitle"
		: "noautotitle" );
    if (key->box.l_type > LT_NODRAW) {
	fputs("box", fp);
	save_linetype(fp, &(key->box), FALSE);
    } else
	fputs("nobox", fp);

    /* These are for the key entries, not the key title */
    if (key->font)
	fprintf(fp, " font \"%s\"", key->font);
    if (key->textcolor.type != TC_LT || key->textcolor.lt != LT_BLACK)
	save_textcolor(fp, &key->textcolor);

    /* Put less common options on separate lines */
    fprintf(fp, "\nset key %sinvert samplen %g spacing %g width %g height %g ",
		key->invert ? "" : "no",
		key->swidth, key->vert_factor, key->width_fix, key->height_fix);
    fprintf(fp, "\nset key maxcolumns %d maxrows %d",key->maxcols,key->maxrows);
    fputc('\n', fp);
    fprintf(fp, "set key %sopaque\n", key->front ? "" : "no");

    if (!(key->visible))
	fputs("unset key\n", fp);

    fputs("unset label\n", fp);
    for (this_label = first_label; this_label != NULL;
	 this_label = this_label->next) {
	fprintf(fp, "set label %d \"%s\" at ",
		this_label->tag,
		conv_text(this_label->text));
	save_position(fp, &this_label->place, FALSE);
	if (this_label->hypertext)
	    fprintf(fp, " hypertext");

	save_justification(this_label->pos, fp);
	if (this_label->rotate)
	    fprintf(fp, " rotate by %d", this_label->rotate);
	else
	    fprintf(fp, " norotate");
	if (this_label->font != NULL)
	    fprintf(fp, " font \"%s\"", this_label->font);
	fprintf(fp, " %s", (this_label->layer==0) ? "back" : "front");
	if (this_label->noenhanced)
	    fprintf(fp, " noenhanced");
	save_textcolor(fp, &(this_label->textcolor));
	if ((this_label->lp_properties.flags & LP_SHOW_POINTS) == 0)
	    fprintf(fp, " nopoint");
	else {
	    fprintf(fp, " point");
	    save_linetype(fp, &(this_label->lp_properties), TRUE);
	}
	save_position(fp, &this_label->offset, TRUE);
#ifdef EAM_BOXED_TEXT
	if (this_label->boxed)
	    fprintf(fp," boxed ");
#endif
	fputc('\n', fp);
    }
    fputs("unset arrow\n", fp);
    for (this_arrow = first_arrow; this_arrow != NULL;
	 this_arrow = this_arrow->next) {
	fprintf(fp, "set arrow %d from ", this_arrow->tag);
	save_position(fp, &this_arrow->start, FALSE);
	if (this_arrow->type == arrow_end_absolute) {
	    fputs(" to ", fp);
	    save_position(fp, &this_arrow->end, FALSE);
	} else if (this_arrow->type == arrow_end_absolute) {
	    fputs(" rto ", fp);
	    save_position(fp, &this_arrow->end, FALSE);
	} else { /* type arrow_end_oriented */
	    struct position *e = &this_arrow->end;
	    fputs(" length ", fp);
	    fprintf(fp, "%s%g", e->scalex == first_axes ? "" : coord_msg[e->scalex], e->x);
	    fprintf(fp, " angle %g", this_arrow->angle);
	}
	fprintf(fp, " %s %s %s",
		arrow_head_names[this_arrow->arrow_properties.head],
		(this_arrow->arrow_properties.layer==0) ? "back" : "front",
		(this_arrow->arrow_properties.headfill==AS_FILLED) ? "filled" :
		(this_arrow->arrow_properties.headfill==AS_EMPTY) ? "empty" :
		(this_arrow->arrow_properties.headfill==AS_NOBORDER) ? "noborder" :
		    "nofilled");
	save_linetype(fp, &(this_arrow->arrow_properties.lp_properties), FALSE);
	if (this_arrow->arrow_properties.head_length > 0) {
	    fprintf(fp, " size %s %.3f,%.3f,%.3f",
		    coord_msg[this_arrow->arrow_properties.head_lengthunit],
		    this_arrow->arrow_properties.head_length,
		    this_arrow->arrow_properties.head_angle,
		    this_arrow->arrow_properties.head_backangle);
	}
	fprintf(fp, "\n");
    }
#if TRUE || defined(BACKWARDS_COMPATIBLE)
    fprintf(fp, "set style increment %s\n", prefer_line_styles ? "userstyles" : "default");
#endif
    fputs("unset style line\n", fp);
    for (this_linestyle = first_linestyle; this_linestyle != NULL;
	 this_linestyle = this_linestyle->next) {
	fprintf(fp, "set style line %d ", this_linestyle->tag);
	save_linetype(fp, &(this_linestyle->lp_properties), TRUE);
	fprintf(fp, "\n");
    }
	/* TODO save "set linetype" as well, or instead */ 
    fputs("unset style arrow\n", fp);
    for (this_arrowstyle = first_arrowstyle; this_arrowstyle != NULL;
	 this_arrowstyle = this_arrowstyle->next) {
	fprintf(fp, "set style arrow %d", this_arrowstyle->tag);
	fprintf(fp, " %s %s %s",
		arrow_head_names[this_arrowstyle->arrow_properties.head],
		(this_arrowstyle->arrow_properties.layer==0)?"back":"front",
		(this_arrowstyle->arrow_properties.headfill==AS_FILLED)?"filled":
		(this_arrowstyle->arrow_properties.headfill==AS_EMPTY)?"empty":
		(this_arrowstyle->arrow_properties.headfill==AS_NOBORDER)?"noborder":
		   "nofilled");
	save_linetype(fp, &(this_arrowstyle->arrow_properties.lp_properties), FALSE);
	if (this_arrowstyle->arrow_properties.head_length > 0) {
	    fprintf(fp, " size %s %.3f,%.3f,%.3f",
		    coord_msg[this_arrowstyle->arrow_properties.head_lengthunit],
		    this_arrowstyle->arrow_properties.head_length,
		    this_arrowstyle->arrow_properties.head_angle,
		    this_arrowstyle->arrow_properties.head_backangle);
	    if (this_arrowstyle->arrow_properties.head_fixedsize) {
		fputs(" ", fp);
		fputs(" fixed", fp);
	    }
	}
	fprintf(fp, "\n");
    }

    fprintf(fp, "set style histogram ");
    save_histogram_opts(fp);

#ifdef EAM_OBJECTS
    fprintf(fp, "unset object\n");
    save_object(fp, 0);
#endif

#ifdef EAM_BOXED_TEXT
    fprintf(fp, "set style textbox %s margins %4.1f, %4.1f %s\n",
	    textbox_opts.opaque ? "opaque": "transparent",
	    textbox_opts.xmargin, textbox_opts.ymargin,
	    textbox_opts.noborder ? "noborder" : "border");
#endif

    fputs("unset logscale\n", fp);
#define SAVE_LOG(axis)							\
    if (axis_array[axis].log)						\
	fprintf(fp, "set logscale %s %g\n", axis_name(axis),	\
		axis_array[axis].base);
    SAVE_LOG(FIRST_X_AXIS );
    SAVE_LOG(FIRST_Y_AXIS );
    SAVE_LOG(SECOND_X_AXIS);
    SAVE_LOG(SECOND_Y_AXIS);
    SAVE_LOG(FIRST_Z_AXIS );
    SAVE_LOG(COLOR_AXIS );
    SAVE_LOG(POLAR_AXIS );
#undef SAVE_LOG

    save_offsets(fp, "set offsets");

    /* FIXME */
    fprintf(fp, "\
set pointsize %g\n\
set pointintervalbox %g\n\
set encoding %s\n\
%sset polar\n\
%sset parametric\n",
	    pointsize, pointintervalbox,
	    encoding_names[encoding],
	    (polar) ? "" : "un",
	    (parametric) ? "" : "un");

    if (numeric_locale)
	fprintf(fp, "set decimalsign locale \"%s\"\n", numeric_locale);
    if (decimalsign != NULL)
	fprintf(fp, "set decimalsign '%s'\n", decimalsign);
    if (!numeric_locale && !decimalsign)
	fprintf(fp, "unset decimalsign\n");

    if (use_minus_sign)
	fprintf(fp, "set minussign\n");
    else
	fprintf(fp, "unset minussign\n");

    fputs("set view ", fp);
    if (splot_map == TRUE)
	fprintf(fp, "map scale %g", mapview_scale);
    else {
	fprintf(fp, "%g, %g, %g, %g",
	    surface_rot_x, surface_rot_z, surface_scale, surface_zscale);
    }
    if (aspect_ratio_3D)
	fprintf(fp, "\nset view  %s", aspect_ratio_3D == 2 ? "equal xy" :
			aspect_ratio_3D == 3 ? "equal xyz": "");

    fprintf(fp, "\n\
set samples %d, %d\n\
set isosamples %d, %d\n\
%sset surface %s",
	    samples_1, samples_2,
	    iso_samples_1, iso_samples_2,
	    (draw_surface) ? "" : "un",
	    (implicit_surface) ? "" : "explicit");

    fprintf(fp, "\n\
%sset contour", (draw_contour) ? "" : "un");
    switch (draw_contour) {
    case CONTOUR_NONE:
	fputc('\n', fp);
	break;
    case CONTOUR_BASE:
	fputs(" base\n", fp);
	break;
    case CONTOUR_SRF:
	fputs(" surface\n", fp);
	break;
    case CONTOUR_BOTH:
	fputs(" both\n", fp);
	break;
    }

    /* Contour label options */
    fprintf(fp, "set cntrlabel %s format '%s' font '%s' start %d interval %d\n", 
	clabel_onecolor ? "onecolor" : "", contour_format,
	clabel_font ? clabel_font : "",
	clabel_start, clabel_interval);

    fputs("set mapping ", fp);
    switch (mapping3d) {
    case MAP3D_SPHERICAL:
	fputs("spherical\n", fp);
	break;
    case MAP3D_CYLINDRICAL:
	fputs("cylindrical\n", fp);
	break;
    case MAP3D_CARTESIAN:
    default:
	fputs("cartesian\n", fp);
	break;
    }

    if (missing_val != NULL)
	fprintf(fp, "set datafile missing '%s'\n", missing_val);
    if (df_separators)
	fprintf(fp, "set datafile separator \"%s\"\n",df_separators);
    else
	fprintf(fp, "set datafile separator whitespace\n");
    if (strcmp(df_commentschars, DEFAULT_COMMENTS_CHARS))
	fprintf(fp, "set datafile commentschars '%s'\n", df_commentschars);
    if (df_fortran_constants)
	fprintf(fp, "set datafile fortran\n");
    if (df_nofpe_trap)
	fprintf(fp, "set datafile nofpe_trap\n");

    save_hidden3doptions(fp);
    fprintf(fp, "set cntrparam order %d\n", contour_order);
    fputs("set cntrparam ", fp);
    switch (contour_kind) {
    case CONTOUR_KIND_LINEAR:
	fputs("linear\n", fp);
	break;
    case CONTOUR_KIND_CUBIC_SPL:
	fputs("cubicspline\n", fp);
	break;
    case CONTOUR_KIND_BSPLINE:
	fputs("bspline\n", fp);
	break;
    }
    fputs("set cntrparam levels ", fp);
    switch (contour_levels_kind) {
    case LEVELS_AUTO:
	fprintf(fp, "auto %d\n", contour_levels);
	break;
    case LEVELS_INCREMENTAL:
	fprintf(fp, "incremental %g,%g,%g\n",
		contour_levels_list[0], contour_levels_list[1],
		contour_levels_list[0] + contour_levels_list[1] * contour_levels);
	break;
    case LEVELS_DISCRETE:
	{
	    int i;
	    fprintf(fp, "discrete %g", contour_levels_list[0]);
	    for (i = 1; i < contour_levels; i++)
		fprintf(fp, ",%g ", contour_levels_list[i]);
	    fputc('\n', fp);
	}
    }
    fprintf(fp, "\
set cntrparam points %d\n\
set size ratio %g %g,%g\n\
set origin %g,%g\n",
	    contour_pts,
	    aspect_ratio, xsize, ysize,
	    xoffset, yoffset);

    fprintf(fp, "set style data ");
    save_data_func_style(fp,"data",data_style);
    fprintf(fp, "set style function ");
    save_data_func_style(fp,"function",func_style);

    save_zeroaxis(fp, FIRST_X_AXIS);
    save_zeroaxis(fp, FIRST_Y_AXIS);
    save_zeroaxis(fp, FIRST_Z_AXIS);
    save_zeroaxis(fp, SECOND_X_AXIS);
    save_zeroaxis(fp, SECOND_Y_AXIS);

    if (xyplane.absolute)
	fprintf(fp, "set xyplane at %g\n", xyplane.z);
    else
	fprintf(fp, "set xyplane relative %g\n", xyplane.z);

    {
    int i;
    fprintf(fp, "set tics scale ");
    for (i=0; i<MAX_TICLEVEL; i++)
	fprintf(fp, " %g%c", ticscale[i], i<MAX_TICLEVEL-1 ? ',' : '\n');
    }

#define SAVE_MINI(axis)							\
    switch(axis_array[axis].minitics & TICS_MASK) {			\
    case 0:								\
	fprintf(fp, "set nom%stics\n", axis_name(axis));	\
	break;								\
    case MINI_AUTO:							\
	fprintf(fp, "set m%stics\n", axis_name(axis));		\
	break;								\
    case MINI_DEFAULT:							\
	fprintf(fp, "set m%stics default\n", axis_name(axis));	\
	break;								\
    case MINI_USER:							\
	fprintf(fp, "set m%stics %f\n", axis_name(axis),	\
		axis_array[axis].mtic_freq);				\
	break;								\
    }

    SAVE_MINI(FIRST_X_AXIS);
    SAVE_MINI(FIRST_Y_AXIS);
    SAVE_MINI(FIRST_Z_AXIS);	/* HBB 20000506: noticed mztics were not saved! */
    SAVE_MINI(SECOND_X_AXIS);
    SAVE_MINI(SECOND_Y_AXIS);
    SAVE_MINI(COLOR_AXIS);
    SAVE_MINI(POLAR_AXIS);
#undef SAVE_MINI

    save_tics(fp, FIRST_X_AXIS);
    save_tics(fp, FIRST_Y_AXIS);
    save_tics(fp, FIRST_Z_AXIS);
    save_tics(fp, SECOND_X_AXIS);
    save_tics(fp, SECOND_Y_AXIS);
    save_tics(fp, COLOR_AXIS);
    save_tics(fp, POLAR_AXIS);
    for (axis=0; axis<MAX_PARALLEL_AXES; axis++)
	save_tics(fp, PARALLEL_AXES+axis);

#define SAVE_AXISLABEL_OR_TITLE(name,suffix,lab)			 \
    {									 \
	fprintf(fp, "set %s%s \"%s\" ",					 \
		name, suffix, lab.text ? conv_text(lab.text) : "");	 \
	fprintf(fp, "\nset %s%s ", name, suffix);			 \
	save_position(fp, &(lab.offset), TRUE);				 \
	fprintf(fp, " font \"%s\"", lab.font ? conv_text(lab.font) : "");\
	save_textcolor(fp, &(lab.textcolor));				 \
	if (lab.tag == ROTATE_IN_3D_LABEL_TAG)				 \
	    fprintf(fp, " rotate parallel");				 \
	else if (lab.rotate)						 \
	    fprintf(fp, " rotate by %d", lab.rotate);			 \
	else								 \
	    fprintf(fp, " norotate");					 \
	fprintf(fp, "%s\n", (lab.noenhanced) ? " noenhanced" : "");	 \
    }

    SAVE_AXISLABEL_OR_TITLE("", "title", title);

    /* FIXME */
    fprintf(fp, "set timestamp %s \n", timelabel_bottom ? "bottom" : "top");
    SAVE_AXISLABEL_OR_TITLE("", "timestamp", timelabel);

    save_range(fp, POLAR_AXIS);
    save_range(fp, T_AXIS);
    save_range(fp, U_AXIS);
    save_range(fp, V_AXIS);

#define SAVE_AXISLABEL(axis)					\
    SAVE_AXISLABEL_OR_TITLE(axis_name(axis),"label",	\
			    axis_array[axis].label)

    SAVE_AXISLABEL(FIRST_X_AXIS);
    SAVE_AXISLABEL(SECOND_X_AXIS);
    save_range(fp, FIRST_X_AXIS);
    save_range(fp, SECOND_X_AXIS);

    SAVE_AXISLABEL(FIRST_Y_AXIS);
    SAVE_AXISLABEL(SECOND_Y_AXIS);
    save_range(fp, FIRST_Y_AXIS);
    save_range(fp, SECOND_Y_AXIS);

    SAVE_AXISLABEL(FIRST_Z_AXIS);
    save_range(fp, FIRST_Z_AXIS);

    SAVE_AXISLABEL(COLOR_AXIS);
    save_range(fp, COLOR_AXIS);

    for (axis=0; axis<MAX_PARALLEL_AXES; axis++)
	save_range(fp, PARALLEL_AXES+axis);

#undef SAVE_AXISLABEL
#undef SAVE_AXISLABEL_OR_TITLE

    fprintf(fp, "set zero %g\n", zero);

    fprintf(fp, "set lmargin %s %g\n",
	    lmargin.scalex == screen ? "at screen" : "", lmargin.x);
    fprintf(fp, "set bmargin %s %g\n",
	    bmargin.scalex == screen ? "at screen" : "", bmargin.x);
    fprintf(fp, "set rmargin %s %g\n",
	    rmargin.scalex == screen ? "at screen" : "", rmargin.x);
    fprintf(fp, "set tmargin %s %g\n",
	    tmargin.scalex == screen ? "at screen" : "", tmargin.x);

    fprintf(fp, "set locale \"%s\"\n", get_time_locale());

    fputs("set pm3d ", fp);
    fputs((PM3D_IMPLICIT == pm3d.implicit ? "implicit" : "explicit"), fp);
    fprintf(fp, " at %s\n", pm3d.where);
    fputs("set pm3d ", fp);
    switch (pm3d.direction) {
    case PM3D_SCANS_AUTOMATIC: fputs("scansautomatic\n", fp); break;
    case PM3D_SCANS_FORWARD: fputs("scansforward\n", fp); break;
    case PM3D_SCANS_BACKWARD: fputs("scansbackward\n", fp); break;
    case PM3D_DEPTH: fputs("depthorder\n", fp); break;
    }
    fprintf(fp, "set pm3d interpolate %d,%d", pm3d.interp_i, pm3d.interp_j);
    fputs(" flush ", fp);
    switch (pm3d.flush) {
    case PM3D_FLUSH_CENTER: fputs("center", fp); break;
    case PM3D_FLUSH_BEGIN: fputs("begin", fp); break;
    case PM3D_FLUSH_END: fputs("end", fp); break;
    }
    fputs((pm3d.ftriangles ? " " : " no"), fp);
    fputs("ftriangles", fp);
    if (pm3d.border.l_type == LT_NODRAW) {
	fprintf(fp," noborder");
    } else {
	fprintf(fp," border");
	save_linetype(fp, &(pm3d.border), FALSE);
    }
    fputs(" corners2color ", fp);
    switch (pm3d.which_corner_color) {
	case PM3D_WHICHCORNER_MEAN:    fputs("mean", fp); break;
	case PM3D_WHICHCORNER_GEOMEAN: fputs("geomean", fp); break;
	case PM3D_WHICHCORNER_HARMEAN: fputs("harmean", fp); break;
	case PM3D_WHICHCORNER_MEDIAN:  fputs("median", fp); break;
	case PM3D_WHICHCORNER_MIN:     fputs("min", fp); break;
	case PM3D_WHICHCORNER_MAX:     fputs("max", fp); break;
	case PM3D_WHICHCORNER_RMS:     fputs("rms", fp); break;

	default: /* PM3D_WHICHCORNER_C1 ... _C4 */
	     fprintf(fp, "c%i", pm3d.which_corner_color - PM3D_WHICHCORNER_C1 + 1);
    }
    fputs("\n", fp);

    /*
     *  Save palette information
     */

    fprintf( fp, "set palette %s %s maxcolors %d ",
	     sm_palette.positive==SMPAL_POSITIVE ? "positive" : "negative",
	     sm_palette.ps_allcF ? "ps_allcF" : "nops_allcF",
	sm_palette.use_maxcolors);
    fprintf( fp, "gamma %g ", sm_palette.gamma );
    if (sm_palette.colorMode == SMPAL_COLOR_MODE_GRAY) {
      fputs( "gray\n", fp );
    }
    else {
      fputs( "color model ", fp );
      switch( sm_palette.cmodel ) {
	case C_MODEL_RGB: fputs( "RGB ", fp ); break;
	case C_MODEL_HSV: fputs( "HSV ", fp ); break;
	case C_MODEL_CMY: fputs( "CMY ", fp ); break;
	case C_MODEL_YIQ: fputs( "YIQ ", fp ); break;
	case C_MODEL_XYZ: fputs( "XYZ ", fp ); break;
	default:
	  fprintf( stderr, "%s:%d ooops: Unknown color model '%c'.\n",
		   __FILE__, __LINE__, (char)(sm_palette.cmodel) );
      }
      fputs( "\nset palette ", fp );
      switch( sm_palette.colorMode ) {
      case SMPAL_COLOR_MODE_RGB:
	fprintf( fp, "rgbformulae %d, %d, %d\n", sm_palette.formulaR,
		 sm_palette.formulaG, sm_palette.formulaB );
	break;
      case SMPAL_COLOR_MODE_GRADIENT: {
	int i=0;
	fprintf( fp, "defined (" );
	for( i=0; i<sm_palette.gradient_num; ++i ) {
	  fprintf( fp, " %.4g %.4g %.4g %.4g", sm_palette.gradient[i].pos,
		   sm_palette.gradient[i].col.r, sm_palette.gradient[i].col.g,
		   sm_palette.gradient[i].col.b );
	  if (i<sm_palette.gradient_num-1)  {
	      fputs( ",", fp);
	      if (i==2 || i%4==2)  fputs( "\\\n    ", fp );
	  }
	}
	fputs( " )\n", fp );
	break;
      }
      case SMPAL_COLOR_MODE_FUNCTIONS:
	fprintf( fp, "functions %s, %s, %s\n", sm_palette.Afunc.definition,
		 sm_palette.Bfunc.definition, sm_palette.Cfunc.definition );
	break;
      case SMPAL_COLOR_MODE_CUBEHELIX:
	fprintf( fp, "cubehelix start %.2g cycles %.2g saturation %.2g\n",
		sm_palette.cubehelix_start, sm_palette.cubehelix_cycles,
		sm_palette.cubehelix_saturation);
	break;
      default:
	fprintf( stderr, "%s:%d ooops: Unknown color mode '%c'.\n",
		 __FILE__, __LINE__, (char)(sm_palette.colorMode) );
      }
    }

    /*
     *  Save colorbox info
     */
    if (color_box.where != SMCOLOR_BOX_NO)
	fprintf(fp,"set colorbox %s\n", color_box.where==SMCOLOR_BOX_DEFAULT ? "default" : "user");
    fprintf(fp, "set colorbox %sal origin ", color_box.rotation ==  'v' ? "vertic" : "horizont");
    save_position(fp, &color_box.origin, FALSE);
    fputs(" size ", fp);
    save_position(fp, &color_box.size, FALSE);
    fprintf(fp, " %s ", color_box.layer ==  LAYER_FRONT ? "front" : "back");
    fprintf(fp, " %sinvert ", color_box.invert ? "" : "no");
    if (color_box.border == 0) fputs("noborder", fp);
	else if (color_box.border_lt_tag < 0) fputs("bdefault", fp);
		 else fprintf(fp, "border %d", color_box.border_lt_tag);
    if (color_box.where == SMCOLOR_BOX_NO) fputs("\nunset colorbox\n", fp);
	else fputs("\n", fp);

    fprintf(fp, "set style boxplot %s %s %5.2f %soutliers pt %d separation %g labels %s %ssorted\n",
		boxplot_opts.plotstyle == FINANCEBARS ? "financebars" : "candles",
		boxplot_opts.limit_type == 1 ? "fraction" : "range",
		boxplot_opts.limit_value,
		boxplot_opts.outliers ? "" : "no",
		boxplot_opts.pointtype+1,
		boxplot_opts.separation,
		(boxplot_opts.labels == BOXPLOT_FACTOR_LABELS_X)  ? "x"  :
		(boxplot_opts.labels == BOXPLOT_FACTOR_LABELS_X2) ? "x2" :
		(boxplot_opts.labels == BOXPLOT_FACTOR_LABELS_AUTO) ? "auto" :"off",
		boxplot_opts.sort_factors ? "" : "un");

    fputs("set loadpath ", fp);
    {
	char *s;
	while ((s = save_loadpath()) != NULL)
	    fprintf(fp, "\"%s\" ", s);
	fputc('\n', fp);
    }

    fputs("set fontpath ", fp);
    {
	char *s;
	while ((s = save_fontpath()) != NULL)
	    fprintf(fp, "\"%s\" ", s);
	fputc('\n', fp);
    }

    if (PS_psdir)
	fprintf(fp, "set psdir \"%s\"\n", PS_psdir);
    else
	fprintf(fp, "set psdir\n");

    fprintf(fp, "set fit");
    if (fit_suppress_log)
	fprintf(fp, " nologfile");
    else if (fitlogfile)
	fprintf(fp, " logfile \'%s\'", fitlogfile);
    switch (fit_verbosity) {
	case QUIET:
	    fprintf(fp, " quiet");
	    break;
	case RESULTS:
	    fprintf(fp, " results");
	    break;
	case BRIEF:
	    fprintf(fp, " brief");
	    break;
	case VERBOSE:
	    fprintf(fp, " verbose");
	    break;
    }
    fprintf(fp, " %serrorvariables",
	fit_errorvariables ? "" : "no");
    fprintf(fp, " %scovariancevariables",
	fit_covarvariables ? "" : "no");
    fprintf(fp, " %serrorscaling",
	fit_errorscaling ? "" : "no");
    fprintf(fp, " %sprescale", fit_prescale ? "" : "no");
    {
	struct udvt_entry *v;
	double d;
	int i;

	v = get_udv_by_name((char *)FITLIMIT);
	d = ((v != NULL) && (!v->udv_undef)) ? real(&(v->udv_value)) : -1.0;
	if ((d > 0.) && (d < 1.))
	    fprintf(fp, " limit %g", d);

	if (epsilon_abs > 0.)
	    fprintf(fp, " limit_abs %g", epsilon_abs);

	v = get_udv_by_name((char *)FITMAXITER);
	i = ((v != NULL) && (!v->udv_undef)) ? real_int(&(v->udv_value)) : -1;
	if (i > 0)
	    fprintf(fp, " maxiter %i", i);

	v = get_udv_by_name((char *)FITSTARTLAMBDA);
	d = ((v != NULL) && (!v->udv_undef)) ? real(&(v->udv_value)) : -1.0;
	if (d > 0.)
	    fprintf(fp, " start_lambda %g", d);

	v = get_udv_by_name((char *)FITLAMBDAFACTOR);
	d = ((v != NULL) && (!v->udv_undef)) ? real(&(v->udv_value)) : -1.0;
	if (d > 0.)
	    fprintf(fp, " lambda_factor %g", d);
    }
    if (fit_script != NULL)
	fprintf(fp, " script \'%s\'", fit_script);
    if (fit_wrap != 0)
	fprintf(fp, " wrap %i", fit_wrap);
    else
	fprintf(fp, " nowrap");
    fprintf(fp, " v%i", fit_v4compatible ? 4 : 5);
    fputc('\n', fp);
}


static void
save_tics(FILE *fp, AXIS_INDEX axis)
{
    if ((axis_array[axis].ticmode & TICS_MASK) == NO_TICS) {
	fprintf(fp, "unset %stics\n", axis_name(axis));
	return;
    }
    fprintf(fp, "set %stics %s %s scale %g,%g %smirror %s ",
	    axis_name(axis),
	    ((axis_array[axis].ticmode & TICS_MASK) == TICS_ON_AXIS)
	    ? "axis" : "border",
	    (axis_array[axis].tic_in) ? "in" : "out",
	    axis_array[axis].ticscale, axis_array[axis].miniticscale,
	    (axis_array[axis].ticmode & TICS_MIRROR) ? "" : "no",
	    axis_array[axis].tic_rotate ? "rotate" : "norotate");
    if (axis_array[axis].tic_rotate)
    	fprintf(fp,"by %d ",axis_array[axis].tic_rotate);
    save_position(fp, &axis_array[axis].ticdef.offset, TRUE);
    if (axis_array[axis].manual_justify)
	save_justification(axis_array[axis].label.pos, fp);
    else
	fputs(" autojustify", fp);
    fprintf(fp, "\nset %stics ", axis_name(axis));

    fprintf(fp, (axis_array[axis].ticdef.rangelimited)?" rangelimit ":" norangelimit ");

    switch (axis_array[axis].ticdef.type) {
    case TIC_COMPUTED:{
	    fputs("autofreq ", fp);
	    break;
	}
    case TIC_MONTH:{
	    fprintf(fp, "\nset %smtics", axis_name(axis));
	    break;
	}
    case TIC_DAY:{
	    fprintf(fp, "\nset %sdtics", axis_name(axis));
	    break;
	}
    case TIC_SERIES:
	if (axis_array[axis].ticdef.def.series.start != -VERYLARGE) {
	    save_num_or_time_input(fp,
			     (double) axis_array[axis].ticdef.def.series.start,
			     axis);
	    putc(',', fp);
	}
	fprintf(fp, "%g", axis_array[axis].ticdef.def.series.incr);
	if (axis_array[axis].ticdef.def.series.end != VERYLARGE) {
	    putc(',', fp);
	    save_num_or_time_input(fp,
			     (double) axis_array[axis].ticdef.def.series.end,
			     axis);
	}
	break;
    case TIC_USER:
	break;
    }

    if (axis_array[axis].ticdef.font && *axis_array[axis].ticdef.font)
	fprintf(fp, " font \"%s\"", axis_array[axis].ticdef.font);

    if (axis_array[axis].ticdef.enhanced == FALSE)
	fprintf(fp, " noenhanced");

    if (axis_array[axis].ticdef.textcolor.type != TC_DEFAULT)
	save_textcolor(fp, &axis_array[axis].ticdef.textcolor);

    putc('\n', fp);

    if (axis_array[axis].ticdef.def.user) {
	struct ticmark *t;
	fprintf(fp, "set %stics %s ", axis_name(axis),
		(axis_array[axis].ticdef.type == TIC_USER) ? "" : "add");
	fputs(" (", fp);
	for (t = axis_array[axis].ticdef.def.user; t != NULL; t = t->next) {
	    if (t->level < 0)	/* Don't save ticlabels read from data file */
		continue;
	    if (t->label)
		fprintf(fp, "\"%s\" ", conv_text(t->label));
	    save_num_or_time_input(fp, (double) t->position, axis);
	    if (t->level)
		fprintf(fp, " %d", t->level);
	    if (t->next) {
		fputs(", ", fp);
	    }
	}
	fputs(")\n", fp);
    }

}

void
save_num_or_time_input(FILE *fp, double x, AXIS_INDEX axis)
{
    if (axis_array[axis].datatype == DT_TIMEDATE) {
	char s[80];

	putc('"', fp);
	gstrftime(s,80,timefmt,(double)(x));
	fputs(conv_text(s), fp);
	putc('"', fp);
    } else {
	fprintf(fp,"%#g",x);
    }
}

void
save_style_parallel(FILE *fp)
{
    fprintf(fp, "set style parallel %s ",
	    parallel_axis_style.layer == LAYER_BACK ? "back" : "front");
    save_linetype(fp, &(parallel_axis_style.lp_properties), FALSE);
    fprintf(fp, "\n");
}

void
save_position(FILE *fp, struct position *pos, TBOOLEAN offset)
{
    assert(first_axes == 0 && second_axes == 1 && graph == 2 && screen == 3 &&
	   character == 4);

    if (offset) {
	if (pos->x == 0 && pos->y == 0 && pos->z == 0)
	    return;
	fprintf(fp, " offset ");
    }

    /* Save in time coordinates if appropriate */
    if (pos->scalex == first_axes) {
	save_num_or_time_input(fp, pos->x, FIRST_X_AXIS);
	fprintf(fp, ", ");
    } else {
	fprintf(fp, "%s%g, ", coord_msg[pos->scalex], pos->x);
    }

    if (pos->scaley == first_axes) {
	if (pos->scaley != pos->scalex) fprintf(fp, "first ");
	save_num_or_time_input(fp, pos->y, FIRST_Y_AXIS);
	fprintf(fp, ", ");
    } else {
	fprintf(fp, "%s%g, ", 
	    pos->scaley == pos->scalex ? "" : coord_msg[pos->scaley], pos->y);
    }

    if (pos->scalez == first_axes) {
	if (pos->scalez != pos->scaley) fprintf(fp, "first ");
	save_num_or_time_input(fp, pos->z, FIRST_Z_AXIS);
    } else {
	fprintf(fp, "%s%g", 
	    pos->scalez == pos->scaley ? "" : coord_msg[pos->scalez], pos->z);
    }
#if (0) /* v4 code */
    fprintf(fp, "%s%g, %s%g, %s%g",
	    pos->scalex == first_axes ? "" : coord_msg[pos->scalex], pos->x,
	    pos->scaley == pos->scalex ? "" : coord_msg[pos->scaley], pos->y,
	    pos->scalez == pos->scaley ? "" : coord_msg[pos->scalez], pos->z);
#endif
}


void
save_range(FILE *fp, AXIS_INDEX axis)
{
    if (axis_array[axis].linked_to_primary) {
	fprintf(fp, "set link %c2 ", axis_name(axis)[0]);
	if (axis_array[axis].link_udf->at)
	    fprintf(fp, "via %s ", axis_array[axis].link_udf->definition);
	if (axis_array[axis-SECOND_AXES].link_udf->at)
	    fprintf(fp, "inverse %s ", axis_array[axis-SECOND_AXES].link_udf->definition);
	fputs("\n\t", fp);
    }

    fprintf(fp, "set %srange [ ", axis_name(axis));
    if (axis_array[axis].set_autoscale & AUTOSCALE_MIN) {
	if (axis_array[axis].min_constraint & CONSTRAINT_LOWER ) {
	    save_num_or_time_input(fp, axis_array[axis].min_lb, axis);
	    fputs(" < ", fp);
	}
	putc('*', fp);
	if (axis_array[axis].min_constraint & CONSTRAINT_UPPER ) {
	    fputs(" < ", fp);
	    save_num_or_time_input(fp, axis_array[axis].min_ub, axis);
	}
    } else {
	save_num_or_time_input(fp, axis_array[axis].set_min, axis);
    }
    fputs(" : ", fp);
    if (axis_array[axis].set_autoscale & AUTOSCALE_MAX) {
	if (axis_array[axis].max_constraint & CONSTRAINT_LOWER ) {
	    save_num_or_time_input(fp, axis_array[axis].max_lb, axis);
	    fputs(" < ", fp);
	}
	putc('*', fp);
	if (axis_array[axis].max_constraint & CONSTRAINT_UPPER ) {
	    fputs(" < ", fp);
	    save_num_or_time_input(fp, axis_array[axis].max_ub, axis);
	}
    } else {
	save_num_or_time_input(fp, axis_array[axis].set_max, axis);
    }

    fprintf(fp, " ] %sreverse %swriteback",
	    ((axis_array[axis].range_flags & RANGE_IS_REVERSED)) ? "" : "no",
	    axis_array[axis].range_flags & RANGE_WRITEBACK ? "" : "no");

    if (axis >= PARALLEL_AXES) {
	fprintf(fp, "\n");
	return;
    }

    if (axis_array[axis].set_autoscale && fp == stderr) {
	/* add current (hidden) range as comments */
	fputs("  # (currently [", fp);
	if (axis_array[axis].set_autoscale & AUTOSCALE_MIN) {
	    save_num_or_time_input(fp, axis_array[axis].min, axis);
	}
	putc(':', fp);
	if (axis_array[axis].set_autoscale & AUTOSCALE_MAX) {
	    save_num_or_time_input(fp, axis_array[axis].max, axis);
	}
	fputs("] )\n", fp);
    } else
	putc('\n', fp);

    if (fp != stderr) {
	if (axis_array[axis].set_autoscale & (AUTOSCALE_FIXMIN))
	    fprintf(fp, "set autoscale %sfixmin\n", axis_name(axis));
	if (axis_array[axis].set_autoscale & AUTOSCALE_FIXMAX)
	    fprintf(fp, "set autoscale %sfixmax\n", axis_name(axis));
    }
}

static void
save_zeroaxis(FILE *fp, AXIS_INDEX axis)
{
    if (axis_array[axis].zeroaxis == NULL) {
	fprintf(fp, "unset %szeroaxis", axis_name(axis));
    } else {
	fprintf(fp, "set %szeroaxis", axis_name(axis));
	if (axis_array[axis].zeroaxis != &default_axis_zeroaxis)
	    save_linetype(fp, axis_array[axis].zeroaxis, FALSE);
    }
    putc('\n', fp);
}

void
save_fillstyle(FILE *fp, const struct fill_style_type *fs)
{
    switch(fs->fillstyle) {
    case FS_SOLID:
    case FS_TRANSPARENT_SOLID:
	fprintf(fp, " %s solid %.2f ",
		fs->fillstyle == FS_SOLID ? "" : "transparent",
		fs->filldensity / 100.0);
	break;
    case FS_PATTERN:
    case FS_TRANSPARENT_PATTERN:
	fprintf(fp, " %s pattern %d ",
		fs->fillstyle == FS_PATTERN ? "" : "transparent",
		fs->fillpattern);
	break;
    case FS_DEFAULT:
	fprintf(fp, " default\n");
	return;
    default:
	fprintf(fp, " empty ");
	break;
    }
    if (fs->border_color.type == TC_LT && fs->border_color.lt == LT_NODRAW) {
	fprintf(fp, "noborder\n");
    } else {
	fprintf(fp, "border");
	save_pm3dcolor(fp, &fs->border_color);
	fprintf(fp, "\n");
    }
}

void
save_textcolor(FILE *fp, const struct t_colorspec *tc)
{
    if (tc->type) {
	fprintf(fp, " textcolor");
	if (tc->type == TC_VARIABLE)
	   fprintf(fp, " variable");
	else
	   save_pm3dcolor(fp, tc);
    }
}


void
save_pm3dcolor(FILE *fp, const struct t_colorspec *tc)
{
    if (tc->type) {
	switch(tc->type) {
	case TC_LT:   if (tc->lt == LT_NODRAW)
			fprintf(fp," nodraw");
		      else if (tc->lt == LT_BACKGROUND)
			fprintf(fp," bgnd");
		      else
			fprintf(fp," lt %d", tc->lt+1);
		      break;
	case TC_LINESTYLE:   fprintf(fp," linestyle %d", tc->lt);
		      break;
	case TC_Z:    fprintf(fp," palette z");
		      break;
	case TC_CB:   fprintf(fp," palette cb %g", tc->value);
		      break;
	case TC_FRAC: fprintf(fp," palette fraction %4.2f", tc->value);
		      break;
	case TC_RGB:  {
		      const char *color = reverse_table_lookup(pm3d_color_names_tbl, tc->lt);
		      if (tc->value < 0)
		  	fprintf(fp," rgb variable ");
		      else if (color)
	    		fprintf(fp," rgb \"%s\" ", color);
    		      else
	    		fprintf(fp," rgb \"#%6.6x\" ", tc->lt);
    		      break;
	    	      }
	default:      break;
	}
    }
}

void
save_data_func_style(FILE *fp, const char *which, enum PLOT_STYLE style)
{
    switch (style) {
    case LINES:
	fputs("lines\n", fp);
	break;
    case POINTSTYLE:
	fputs("points\n", fp);
	break;
    case IMPULSES:
	fputs("impulses\n", fp);
	break;
    case LINESPOINTS:
	fputs("linespoints\n", fp);
	break;
    case DOTS:
	fputs("dots\n", fp);
	break;
    case YERRORLINES:
	fputs("yerrorlines\n", fp);
	break;
    case XERRORLINES:
	fputs("xerrorlines\n", fp);
	break;
    case XYERRORLINES:
	fputs("xyerrorlines\n", fp);
	break;
    case YERRORBARS:
	fputs("yerrorbars\n", fp);
	break;
    case XERRORBARS:
	fputs("xerrorbars\n", fp);
	break;
    case XYERRORBARS:
	fputs("xyerrorbars\n", fp);
	break;
    case BOXES:
	fputs("boxes\n", fp);
	break;
    case HISTOGRAMS:
	fputs("histograms\n", fp);
	break;
    case FILLEDCURVES:
	fputs("filledcurves ", fp);
	if (!strcmp(which, "data") || !strcmp(which, "Data"))
	    filledcurves_options_tofile(&filledcurves_opts_data, fp);
	else
	    filledcurves_options_tofile(&filledcurves_opts_func, fp);
	fputc('\n', fp);
	break;
    case BOXERROR:
	fputs("boxerrorbars\n", fp);
	break;
    case BOXXYERROR:
	fputs("boxxyerrorbars\n", fp);
	break;
    case STEPS:
	fputs("steps\n", fp);
	break;			/* JG */
    case FSTEPS:
	fputs("fsteps\n", fp);
	break;			/* HOE */
    case HISTEPS:
	fputs("histeps\n", fp);
	break;			/* CAC */
    case VECTOR:
	fputs("vector\n", fp);
	break;
    case FINANCEBARS:
	fputs("financebars\n", fp);
	break;
    case CANDLESTICKS:
	fputs("candlesticks\n", fp);
	break;
    case BOXPLOT:
	fputs("boxplot\n", fp);
	break;
    case PM3DSURFACE:
	fputs("pm3d\n", fp);
	break;
    case LABELPOINTS:
	fputs("labels\n", fp);
	break;
    case IMAGE:
	fputs("image\n", fp);
	break;
    case RGBIMAGE:
	fputs("rgbimage\n", fp);
	break;
#ifdef EAM_OBJECTS
	case CIRCLES:
	fputs("circles\n", fp);
	break;
	case ELLIPSES:
	fputs("ellipses\n", fp);
	break;
#endif
    case SURFACEGRID:
	fputs("surfaces\n", fp);
	break;
    case PARALLELPLOT:
	fputs("parallelaxes\n", fp);
	break;
    case PLOT_STYLE_NONE:
    default:
	fputs("---error!---\n", fp);
    }
}

void save_dashtype(FILE *fp, int d_type, const t_dashtype *dt)
{
    /* this is indicated by LT_AXIS (lt 0) instead */
    if (d_type == DASHTYPE_AXIS)
	return;

    fprintf(fp, " dashtype");
    if (d_type == DASHTYPE_CUSTOM) {
	if (dt->dstring[0] != '\0')
	    fprintf(fp, " \"%s\"", dt->dstring);
	if (fp == stderr || dt->dstring[0] == '\0') {
	    int i;
	    fputs(" (", fp);
	    for (i = 0; i < DASHPATTERN_LENGTH && dt->pattern[i] > 0; i++)
		fprintf(fp, i ? ", %.2f" : "%.2f", dt->pattern[i]);
	    fputs(")", fp);
	}
    } else if (d_type == DASHTYPE_SOLID) {
	fprintf(fp, " solid");
    } else {
	fprintf(fp, " %d", d_type + 1);
    }
}

void
save_linetype(FILE *fp, lp_style_type *lp, TBOOLEAN show_point)
{
    if (lp->l_type == LT_NODRAW)
	fprintf(fp, " lt nodraw");
    else if (lp->l_type == LT_BLACK)
	fprintf(fp, " lt black");
    else if (lp->l_type == LT_BACKGROUND)
	fprintf(fp, " lt bgnd");
    else if (lp->l_type < 0)
	fprintf(fp, " lt %d", lp->l_type+1);

    else if (lp->pm3d_color.type != TC_DEFAULT) {
	fprintf(fp, " linecolor");
	if (lp->pm3d_color.type == TC_LT)
    	    fprintf(fp, " %d", lp->pm3d_color.lt+1);
	else if (lp->pm3d_color.type == TC_LINESTYLE && lp->l_type == LT_COLORFROMCOLUMN)
	    fprintf(fp, " variable");
	else
    	    save_pm3dcolor(fp,&(lp->pm3d_color));
    }
    fprintf(fp, " linewidth %.3f", lp->l_width);

    save_dashtype(fp, lp->d_type, &lp->custom_dash_pattern);

    if (show_point) {
	if (lp->p_type == PT_CHARACTER)
	    fprintf(fp, " pointtype \"%s\"", (char *)(&lp->p_char));
	else
	    fprintf(fp, " pointtype %d", lp->p_type + 1);
	if (lp->p_size == PTSZ_VARIABLE)
	    fprintf(fp, " pointsize variable");
	else if (lp->p_size == PTSZ_DEFAULT)
	    fprintf(fp, " pointsize default");
	else
	    fprintf(fp, " pointsize %.3f", lp->p_size);
	fprintf(fp, " pointinterval %d", lp->p_interval);
    }

}


void
save_offsets(FILE *fp, char *lead)
{
    fprintf(fp, "%s %s%g, %s%g, %s%g, %s%g\n", lead,
	loff.scalex == graph ? "graph " : "", loff.x,
	roff.scalex == graph ? "graph " : "", roff.x,
	toff.scaley == graph ? "graph " : "", toff.y,
	boff.scaley == graph ? "graph " : "", boff.y);
}

void 
save_histogram_opts (FILE *fp)
{
    switch (histogram_opts.type) {
	default:
	case HT_CLUSTERED:
	    fprintf(fp,"clustered gap %d ",histogram_opts.gap); break;
	case HT_ERRORBARS:
	    fprintf(fp,"errorbars gap %d lw %g",histogram_opts.gap,histogram_opts.bar_lw); break;
	case HT_STACKED_IN_LAYERS:
	    fprintf(fp,"rowstacked "); break;
	case HT_STACKED_IN_TOWERS:
	    fprintf(fp,"columnstacked "); break;
    }
    if (fp == stderr)
	fprintf(fp,"\n\t\t");
    fprintf(fp,"title");
    save_textcolor(fp, &histogram_opts.title.textcolor);
    if (histogram_opts.title.font)
	fprintf(fp, " font \"%s\" ", histogram_opts.title.font);
    save_position(fp, &histogram_opts.title.offset, TRUE);
    fprintf(fp, "\n");
}

#ifdef EAM_OBJECTS

/* Save/show rectangle <tag> (0 means show all) */
void
save_object(FILE *fp, int tag)
{
    t_object *this_object;
    t_rectangle *this_rect;
    t_circle *this_circle;
    t_ellipse *this_ellipse;
    TBOOLEAN showed = FALSE;

    for (this_object = first_object; this_object != NULL; this_object = this_object->next) {
	if ((this_object->object_type == OBJ_RECTANGLE)
	    && (tag == 0 || tag == this_object->tag)) {
	    this_rect = &this_object->o.rectangle;
	    showed = TRUE;
	    fprintf(fp, "%sobject %2d rect ", (fp==stderr) ? "\t" : "set ",this_object->tag);

	    if (this_rect->type == 1) {
		fprintf(fp, "center ");
		save_position(fp, &this_rect->center, FALSE);
		fprintf(fp, " size ");
		save_position(fp, &this_rect->extent, FALSE);
	    } else {
		fprintf(fp, "from ");
		save_position(fp, &this_rect->bl, FALSE);
		fprintf(fp, " to ");
		save_position(fp, &this_rect->tr, FALSE);
	    }
	}

	else if ((this_object->object_type == OBJ_CIRCLE)
	    && (tag == 0 || tag == this_object->tag)) {
	    struct position *e = &this_object->o.circle.extent;
	    this_circle = &this_object->o.circle;
	    showed = TRUE;
	    fprintf(fp, "%sobject %2d circle ", (fp==stderr) ? "\t" : "set ",this_object->tag);

	    fprintf(fp, "center ");
	    save_position(fp, &this_circle->center, FALSE);
	    fprintf(fp, " size ");
	    fprintf(fp, "%s%g", e->scalex == first_axes ? "" : coord_msg[e->scalex], e->x);
	    fprintf(fp, " arc [%g:%g] ", this_circle->arc_begin, this_circle->arc_end);
	    fprintf(fp, this_circle->wedge ? "wedge " : "nowedge");
	}

	else if ((this_object->object_type == OBJ_ELLIPSE)
	    && (tag == 0 || tag == this_object->tag)) {
	    struct position *e = &this_object->o.ellipse.extent;
	    this_ellipse = &this_object->o.ellipse;
	    showed = TRUE;
	    fprintf(fp, "%sobject %2d ellipse ", (fp==stderr) ? "\t" : "set ",this_object->tag);
	    fprintf(fp, "center ");
	    save_position(fp, &this_ellipse->center, FALSE);
	    fprintf(fp, " size ");
	    fprintf(fp, "%s%g", e->scalex == first_axes ? "" : coord_msg[e->scalex], e->x);
	    fprintf(fp, ", %s%g", e->scaley == e->scalex ? "" : coord_msg[e->scaley], e->y);
	    fprintf(fp, "  angle %g", this_ellipse->orientation);
	    fputs(" units ", fp);
	    switch (this_ellipse->type) {
		case ELLIPSEAXES_XY:
		    fputs("xy", fp);
		    break;
		case ELLIPSEAXES_XX:
		    fputs("xx", fp);
		    break;
		case ELLIPSEAXES_YY:
		    fputs("yy", fp);
		    break;
	    }
	}

	else if ((this_object->object_type == OBJ_POLYGON)
	    && (tag == 0 || tag == this_object->tag)) {
	    t_polygon *this_polygon = &this_object->o.polygon;
	    int nv;
	    showed = TRUE;
	    fprintf(fp, "%sobject %2d polygon ", (fp==stderr) ? "\t" : "set ",this_object->tag);
	    if (this_polygon->vertex) {
		fprintf(fp, "from ");
		save_position(fp, &this_polygon->vertex[0], FALSE);
	    }
	    for (nv=1; nv < this_polygon->type; nv++) {
		fprintf(fp, (fp==stderr) ? "\n\t\t\t    to " : " to ");
		save_position(fp, &this_polygon->vertex[nv], FALSE);
	    }
	}

	/* Properties common to all objects */
	if (tag == 0 || tag == this_object->tag) {
	    fprintf(fp, "\n%sobject %2d ", (fp==stderr) ? "\t" : "set ",this_object->tag);
	    fprintf(fp, "%s ", this_object->layer > 0 ? "front" : this_object->layer < 0 ? "behind" : "back");
	    if (this_object->clip == OBJ_NOCLIP)
		fputs("noclip ", fp);
	    else 
		fputs("clip ", fp);

	    if (this_object->lp_properties.l_width)
		    fprintf(fp, "lw %.1f ",this_object->lp_properties.l_width);
	    if (this_object->lp_properties.d_type)
		    save_dashtype(fp, this_object->lp_properties.d_type,
					&this_object->lp_properties.custom_dash_pattern);
	    fprintf(fp, " fc ");
	    if (this_object->lp_properties.l_type == LT_DEFAULT)
		    fprintf(fp,"default");
	    else /* FIXME: Broke with removal of use_palette? */
		    save_pm3dcolor(fp, &this_object->lp_properties.pm3d_color);
	    fprintf(fp, " fillstyle ");
	    save_fillstyle(fp, &this_object->fillstyle);
	}

    }
    if (tag > 0 && !showed)
	int_error(c_token, "object not found");
}

#endif

