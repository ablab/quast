#ifndef lint
static char *RCSid() { return RCSid("$Id: dynarray.c,v 1.11.10.1 2015/04/23 20:10:30 sfeam Exp $"); }
#endif

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

/* This module implements a dynamically growing array of arbitrary
 * elements parametrized by their sizeof(). There is no 'access
 * function', i.e. you'll have to access the elements of the
 * dynarray->v memory block by hand. It's implemented in OO-style,
 * even though this is C, not C++.  In particular, every function
 * takes a pointer to a data structure type 'dynarray', which mimics
 * the 'this' pointer of an object. */

#include "dynarray.h"

#include "alloc.h"
#include "util.h"		/* for graph_error() */

/* The 'constructor' of a dynarray object: initializes all the
 * variables to well-defined startup values */
void
init_dynarray(dynarray *this, size_t entry_size, long size, long increment)
{
    this->v = 0;		/* preset value, in case gp_alloc fails */
    if (size)
	this->v = gp_alloc(entry_size*size, "init dynarray");
    this->size = size;
    this->end = 0;
    this->increment = increment;
    this->entry_size = entry_size;
}

/* The 'destructor'; sets all crucial elements of the structure to
 * well-defined values to avoid problems by use of bad pointers... */
void
free_dynarray(dynarray *this)
{
    free(this->v);		/* should work, even if gp_alloc failed */
    this->v = 0;
    this->end = this->size = 0;
}

/* Set the size of the dynamical array to a new, fixed value */
void
resize_dynarray(dynarray *this, long newsize)
{
    if (!this->v)
	graph_error("resize_dynarray: dynarray wasn't initialized!");

    if (newsize == 0)
	free_dynarray(this);
    else {
	this->v = gp_realloc(this->v, this->entry_size * newsize, "extend dynarray");
	this->size = newsize;
    }
}

/* Increase the size of the dynarray by a given amount */
void
extend_dynarray(dynarray *this, long increment)
{
    resize_dynarray(this, this->size + increment);
}

/* Get pointer to the element one past the current end of the dynamic
 * array. Resize it if necessary. Returns a pointer-to-void to that
 * element. */
void GPHUGE *
nextfrom_dynarray(dynarray *this)
{
    if (!this->v)
	graph_error("nextfrom_dynarray: dynarray wasn't initialized!");

    if (this->end >= this->size)
	extend_dynarray(this, this->increment);
    return (void *)((char *)(this->v) + this->entry_size * (this->end++));
}

/* Release the element at the current end of the dynamic array, by
 * moving the 'end' index one element backwards */
void
droplast_dynarray(dynarray *this)
{
    if (!this->v)
	graph_error("droplast_dynarray: dynarray wasn't initialized!");

    if (this->end)
	this->end--;
}
