/*
 * $Id: util3d.h,v 1.20.2.1 2015/10/31 04:39:20 sfeam Exp $
 */

/* GNUPLOT - util3d.h */

/*[
 * Copyright 1986 - 1993, 1998, 1999, 2004   Thomas Williams, Colin Kelley
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

#ifndef GNUPLOT_UTIL3D_H
# define GNUPLOT_UTIL3D_H

/* HBB 990828: moved all those variable decl's and #defines to new
 * file "graph3d.h", as the definitions are in graph3d.c, not in
 * util3d.c. Include that file from here, to ensure everything is
 * known */

#include "graph3d.h"

/* All the necessary information about one vertex. */
typedef struct vertex {
    coordval x, y, z;		/* vertex coordinates */
    lp_style_type *lp_style;	/* where to find point symbol type (if any) */
    coordval real_z;
    struct text_label *label;
#ifdef HIDDEN3D_VAR_PTSIZE	/* Needed for variable pointsize, but takes space */
    struct coordinate *original;
#endif
} vertex;
typedef vertex GPHUGE * p_vertex;


/* Utility macros for vertices: */
#define FLAG_VERTEX_AS_UNDEFINED(v) \
  do { (v).z = -2.0; } while (0)
#define VERTEX_IS_UNDEFINED(v) ((v).z == -2.0)
#define V_EQUAL(a,b) ( GE(0.0, fabs((a)->x - (b)->x) + \
   fabs((a)->y - (b)->y) + fabs((a)->z - (b)->z)) )


/* Maps from normalized space to terminal coordinates */
#define TERMCOORD(v,xvar,yvar)			\
{						\
    xvar = ((int)((v)->x * xscaler)) + xmiddle;	\
    yvar = ((int)((v)->y * yscaler)) + ymiddle;	\
}

/* Prototypes of functions exported by "util3d.c" */

void edge3d_intersect __PROTO((coordinate *, coordinate *, double *, double *, double *));
TBOOLEAN two_edge3d_intersect __PROTO((coordinate *, coordinate *, double *, double *, double *));
void mat_scale __PROTO((double sx, double sy, double sz, double mat[4][4]));
void mat_rot_x __PROTO((double teta, double mat[4][4]));
void mat_rot_z __PROTO((double teta, double mat[4][4]));
void mat_mult __PROTO((double mat_res[4][4], double mat1[4][4], double mat2[4][4]));
void map3d_xyz __PROTO((double x, double y, double z, p_vertex out));
void map3d_xy __PROTO((double x, double y, double z, int *xt, int *yt));
void map3d_xy_double __PROTO((double x, double y, double z, double *xt, double *yt));
void draw3d_line __PROTO((p_vertex, p_vertex, struct lp_style_type *));
void draw3d_line_unconditional __PROTO((p_vertex, p_vertex, struct lp_style_type *, t_colorspec));
void draw3d_point __PROTO((p_vertex v, struct lp_style_type *lp));
void apply_3dhead_properties __PROTO((struct arrow_style_type *arrow_properties));


/* HBB NEW 20031218: 3D polyline support */
void polyline3d_start __PROTO((p_vertex v1));
void polyline3d_next __PROTO((p_vertex v2, struct lp_style_type *lp));

#endif /* GNUPLOT_UTIL3D_H */
