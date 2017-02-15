/*
 * $Id: readline.h,v 1.11 2008/12/12 21:06:13 sfeam Exp $
 */

/* GNUPLOT - readline.h */

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

#ifndef GNUPLOT_READLINE_H
# define GNUPLOT_READLINE_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
/* Type definitions */

/* Variables of readline.c needed by other modules: */

/* Prototypes of functions exported by readline.c */

#if defined(HAVE_LIBREADLINE)
# include "stdfn.h"	/* <readline/readline.h> needs stdio.h... */
# include <readline/readline.h>
#endif
#if defined(HAVE_LIBEDITLINE)
# include <editline/readline.h>
#endif

#if defined(HAVE_LIBEDITLINE)
int getc_wrapper __PROTO((FILE* fp));
#endif

#if defined(READLINE) && !defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDITLINE)
char *readline __PROTO((const char *));
#endif

/*
 * The following 'readline_ipc' routine is usual 'readline' for OS2_IPC,
 * and a special one for IPC communication.
 */
char *readline_ipc __PROTO((const char*));

#endif /* GNUPLOT_READLINE_H */
