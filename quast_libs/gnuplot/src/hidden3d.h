/*
 * $Id: hidden3d.h,v 1.13 2013/06/27 19:37:14 sfeam Exp $
 */

/* GNUPLOT - hidden3d.h */

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

#ifndef GNUPLOT_HIDDEN3D_H
# define GNUPLOT_HIDDEN3D_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "stdfn.h"
#include "graph3d.h"
#include "util3d.h"

/* Type definitions */

#define PT_ARROWHEAD -10
#define PT_BACKARROW -11

/* Variables of hidden3d.c needed by other modules: */

extern TBOOLEAN disable_mouse_z;

/* Prototypes of functions exported by hidden3d.c */

void set_hidden3doptions __PROTO((void));
void show_hidden3doptions __PROTO((void));
void reset_hidden3doptions __PROTO((void));
void save_hidden3doptions __PROTO((FILE *fp));
void init_hidden_line_removal __PROTO((void));
void reset_hidden_line_removal __PROTO((void));
void term_hidden_line_removal __PROTO((void));
void plot3d_hidden __PROTO((struct surface_points *plots, int pcount));
void draw_line_hidden __PROTO((p_vertex, p_vertex, lp_style_type *));
void draw_label_hidden __PROTO((p_vertex, lp_style_type *, int, int));

#endif /* GNUPLOT_HIDDEN3D_H */
