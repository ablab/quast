/*
 * $Id: dynarray.h,v 1.7 2004/04/13 17:23:53 broeker Exp $
 */

/* GNUPLOT - dynarray.h */

/*[
 * Copyright 1999, 2004   Hans-Bernhard Broeker
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

#ifndef DYNARRAY__H
# define DYNARRAY__H

# include "syscfg.h"
# include "stdfn.h"

typedef struct dynarray {
    long size;			/* alloced size of the array */
    long end;			/* index of first unused entry */
    long increment;		/* amount to increment size by, on realloc */
    size_t entry_size;		/* size of the entries in this array */
    void GPHUGE *v;		/* the vector itself */
} dynarray;

/* Prototypes */
void init_dynarray __PROTO((dynarray * array, size_t element, long size, long increment));
void free_dynarray __PROTO((dynarray * array));
void extend_dynarray __PROTO((dynarray * array, long increment));
void resize_dynarray __PROTO((dynarray * array, long newsize));
void GPHUGE *nextfrom_dynarray __PROTO((dynarray * array));
void droplast_dynarray __PROTO((dynarray * array));

#endif /* DYNARRAY_H */
