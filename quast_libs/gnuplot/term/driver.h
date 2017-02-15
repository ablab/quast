/* $Id: driver.h,v 1.22 2006/10/08 21:11:08 sfeam Exp $ */

/* GNUPLOT - driver.h */

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


#ifndef TERM_DRIVER_H
#define TERM_DRIVER_H

#include "syscfg.h"

#include <stdio.h>

/* functions provided by term.c */

static void do_point __PROTO((unsigned int x, unsigned int y, int number));
static void line_and_point __PROTO((unsigned int x, unsigned int y, int number));
static int null_text_angle __PROTO((int ang));
static int null_justify_text __PROTO((enum JUSTIFY just));
static int null_scale __PROTO((double x, double y));
static void options_null __PROTO((void));
static void UNKNOWN_null __PROTO((void));
/* static int set_font_null __PROTO((const char *s));     */ /* unused */
#define set_font_null NULL

extern FILE *gpoutfile;
extern struct termentry *term;

/* for use by all drivers */
#ifndef NEXT
#define sign(x) ((x) >= 0 ? 1 : -1)
#else
/* it seems that sign as macro causes some conflict with precompiled headers */
static int sign(int x)
{
    return x >= 0 ? 1 : -1;
}
#endif /* NEXT */

/* abs as macro is now uppercase, there are conflicts with a few C compilers
   that have abs as macro, even though ANSI defines abs as function
   (int abs(int)). Most calls to ABS in term/ could be changed to abs if
   they use only int arguments and others to fabs, but for the time being,
   all calls are done via the macro */
#ifndef ABS
# define ABS(x) ((x) >= 0 ? (x) : -(x))
#endif /* ABS */

#define NICE_LINE		0
#define POINT_TYPES		6

#endif /* TERM_DRIVER_H */
