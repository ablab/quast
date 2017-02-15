/*
 * $Id: standard.h,v 1.12 2008/03/30 03:03:48 sfeam Exp $
 */

/* GNUPLOT - standard.h */

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

#ifndef GNUPLOT_STANDARD_H
# define GNUPLOT_STANDARD_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "eval.h"

/* Type definitions */

/* Variables of standard.c needed by other modules: */

/* Prototypes of functions exported by standard.c */

/* These are the more 'usual' functions built into the stack machine */
void f_real __PROTO((union argument *x));
void f_imag __PROTO((union argument *x));
void f_int __PROTO((union argument *x));
void f_arg __PROTO((union argument *x));
void f_conjg __PROTO((union argument *x));
void f_sin __PROTO((union argument *x));
void f_cos __PROTO((union argument *x));
void f_tan __PROTO((union argument *x));
void f_asin __PROTO((union argument *x));
void f_acos __PROTO((union argument *x));
void f_atan __PROTO((union argument *x));
void f_atan2 __PROTO((union argument *x));
void f_sinh __PROTO((union argument *x));
void f_cosh __PROTO((union argument *x));
void f_tanh __PROTO((union argument *x));
void f_asinh __PROTO((union argument *x));
void f_acosh __PROTO((union argument *x));
void f_atanh __PROTO((union argument *x));
void f_ellip_first __PROTO((union argument *x));
void f_ellip_second __PROTO((union argument *x));
void f_ellip_third __PROTO((union argument *x));
void f_void __PROTO((union argument *x));
void f_abs __PROTO((union argument *x));
void f_sgn __PROTO((union argument *x));
void f_sqrt __PROTO((union argument *x));
void f_exp __PROTO((union argument *x));
void f_log10 __PROTO((union argument *x));
void f_log __PROTO((union argument *x));
void f_floor __PROTO((union argument *x));
void f_ceil __PROTO((union argument *x));
void f_besj0 __PROTO((union argument *x));
void f_besj1 __PROTO((union argument *x));
void f_besy0 __PROTO((union argument *x));
void f_besy1 __PROTO((union argument *x));
void f_exists __PROTO((union argument *x));   /* exists("foo") */

void f_tmsec __PROTO((union argument *x));
void f_tmmin __PROTO((union argument *x));
void f_tmhour __PROTO((union argument *x));
void f_tmmday __PROTO((union argument *x));
void f_tmmon __PROTO((union argument *x));
void f_tmyear __PROTO((union argument *x));
void f_tmwday __PROTO((union argument *x));
void f_tmyday __PROTO((union argument *x));

#endif /* GNUPLOT_STANDARD_H */
