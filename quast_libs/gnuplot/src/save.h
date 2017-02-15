/*
 * $Id: save.h,v 1.18.2.1 2014/11/08 04:52:25 sfeam Exp $
 */

/* GNUPLOT - save.h */

/*[
 * Copyright 1999, 2004   Thomas Williams, Colin Kelley
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

#ifndef GNUPLOT_SAVE_H
# define GNUPLOT_SAVE_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "stdfn.h"

#include "axis.h"

/* Type definitions */

/* Variables of save.c needed by other modules: */

/* Prototypes of functions exported by save.c */
void save_functions __PROTO((FILE *fp));
void save_variables __PROTO((FILE *fp));
void save_set __PROTO((FILE *fp));
void save_term __PROTO((FILE *fp));
void save_all __PROTO((FILE *fp));
void save_position __PROTO((FILE *, struct position *, TBOOLEAN));
void save_range __PROTO((FILE *, AXIS_INDEX));
void save_textcolor __PROTO((FILE *, const struct t_colorspec *));
void save_pm3dcolor __PROTO((FILE *, const struct t_colorspec *));
void save_fillstyle __PROTO((FILE *, const struct fill_style_type *));
void save_offsets __PROTO((FILE *, char *));
void save_histogram_opts __PROTO((FILE *fp));
#ifdef EAM_OBJECTS
void save_object __PROTO((FILE *, int));
#endif
void save_style_parallel __PROTO((FILE *));
void save_data_func_style __PROTO((FILE *, const char *, enum PLOT_STYLE));
void save_linetype __PROTO((FILE *, lp_style_type *, TBOOLEAN));
void save_dashtype __PROTO((FILE *, int, const t_dashtype *));
void save_num_or_time_input __PROTO((FILE *, double x, AXIS_INDEX axis));

#endif /* GNUPLOT_SAVE_H */
