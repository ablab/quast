/*
 * $Id: breaders.h,v 1.5 2009/08/28 05:16:31 sfeam Exp $
 */

/* GNUPLOT - binedf.h */

/*[
 * Copyright 2004  Petr Mikulik
 *
 * As part of the program Gnuplot, which is
 *
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

#ifndef GNUPLOT_BINEDF_H
# define GNUPLOT_BINEDF_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"

/* Prototypes of functions exported by breaders.c */

void edf_filetype_function __PROTO((void));
void png_filetype_function __PROTO((void));
void gif_filetype_function __PROTO((void));
void jpeg_filetype_function __PROTO((void));
int  df_libgd_get_pixel __PROTO((int i, int j, int component));

#endif /* GNUPLOT_BINEDF_H */
