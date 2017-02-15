/*
 * $Id: doc2x.h,v 1.8 2004/04/13 17:23:31 broeker Exp $
 *
 */

/* GNUPLOT - doc2x.h */

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

#ifndef DOCS_DOC2X_H
# define DOCS_DOC2X_H

#include "stdfn.h"		/* HBB 990828: safe_strncpy() prototype */

/* Various defines and macros */
#ifndef MAX_LINE_LEN
# define MAX_LINE_LEN 1023
#endif

#ifndef MAX_NAME_LEN
# define MAX_NAME_LEN 255
#endif

#ifdef HAVE_STRINGIZE
# define START_HELP(driver) "C#" #driver ,
# define END_HELP(driver)   ,"C#",
#else
# define START_HELP(driver)     /* nought */
# define END_HELP(driver)   ,
#endif

#if defined(DOCS_TERMDOC_MAIN) || defined(DOCS_XREF_MAIN)
extern char *termtext[];
#else

/* a complete lie, but they dont need it ! */
# define TERM_DRIVER_H
# define TERM_HELP

char *termtext[] = {
# ifdef ALL_TERM_DOC
#  include "allterm.h"
# else
#  include "term.h"
# endif
    NULL
};
#endif /* !DOCS_TERMDOC_MAIN */

/* From termdoc.c */
#ifndef DOCS_TERMDOC_MAIN
extern int termdoc_lineno;
extern char termdoc_filename[];
#endif

/* We are using the fgets() replacement from termdoc.c */
#ifndef DOCS_TERMDOC_MAIN
extern
#endif
       char *get_line __PROTO((char *, int, FILE *));

#endif /* DOCS_DOC2X_H */
