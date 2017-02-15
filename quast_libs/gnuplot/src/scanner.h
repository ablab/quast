/*
 * $Id: scanner.h,v 1.8 2011/11/28 19:49:53 sfeam Exp $
 */

/* GNUPLOT - scanner.h */

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

#ifndef GNUPLOT_SCANNER_H
# define GNUPLOT_SCANNER_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "stdfn.h"

/* Type definitions */

/* Variables of scanner.c needed by other modules: */

extern int curly_brace_count;

/* Prototypes of functions exported by scanner.c */

TBOOLEAN legal_identifier __PROTO((char *p));
int scanner __PROTO((char **expression, size_t *line_lengthp));

#endif /* GNUPLOT_SCANNER_H */
