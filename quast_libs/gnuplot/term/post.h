/*
 * $Id: post.h,v 1.12 2011/11/07 18:40:48 markisch Exp $
 */

/* GNUPLOT - post.h */

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

#ifndef TERM_POST_H
# define TERM_POST_H

/* Needed by terminals which output postscript
 * (post.trm and pslatex.trm)
 */

#ifdef PSLATEX_DRIVER
TERM_PUBLIC void PSTEX_common_init __PROTO((void));
TERM_PUBLIC void PSTEX_reopen_output __PROTO((void));
TERM_PUBLIC void EPSLATEX_common_init __PROTO((void));
TERM_PUBLIC void EPSLATEX_reopen_output __PROTO((char *));
#endif

#define PS_POINT_TYPES 8

/* assumes landscape */
#define PS_XMAX (10*720)
#define PS_YMAX (7*720)
#define PS_YMAX_OLDSTYLE (6*720)

/* These seem to be unnecessary, thus commented out */
/*
#define PS_XLAST (PS_XMAX - 1)
#define PS_YLAST (PS_YMAX - 1)
*/

#define PS_VTIC (PS_YMAX/80)
#define PS_HTIC (PS_YMAX/80)

/* scale is 1pt = 10 units */
#define PS_SC 10

/* EAM March 2010 allow user to rescale fonts */
#define PS_SCF (PS_SC * ps_params->fontscale)

/* linewidth = 0.5 pts */
#define PS_LW (0.5*PS_SC)

/* character size defaults: */
/* 14 pt for postscript */
#define PS_VCHAR (14*PS_SC)
#define PS_HCHAR (14*PS_SC*6/10)
/* 10 pt for ps(la)tex */
#define PSTEX_VCHAR (10*PS_SC)
#define PSTEX_HCHAR (10*PS_SC*6/10)
/* 11 pt for epslatex */
#define EPSLATEX_VCHAR (11*PS_SC)
#define EPSLATEX_HCHAR (11*PS_SC*6/10)

/* additional LaTeX header information for epslatex terminal */
#ifdef PSLATEX_DRIVER
TERM_PUBLIC char *epslatex_header;
#endif

#endif /* TERM_POST_H */
