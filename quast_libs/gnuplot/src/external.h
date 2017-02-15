/*
 * $Id: external.h,v 1.1 2014/02/28 00:24:20 sfeam Exp $
 */
/* GNUPLOT - external.h */

/*[
 * Copyright 2002 Stephan Boettcher
 *
 * Gnuplot license:
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
 *
 * Alternative license:
 *
 * As an alternative to distributing code in this file under the gnuplot license,
 * you may instead comply with the terms below. In this case, redistribution and
 * use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
]*/

#ifndef GNUPLOT_EXTERNAL_H
# define GNUPLOT_EXTERNAL_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "gp_types.h"
#include "eval.h"

#ifdef HAVE_EXTERNAL_FUNCTIONS

/* Prototypes from file "external.c" */
void f_calle __PROTO((union argument *x));
struct at_type *external_at __PROTO((const char *));
void external_free __PROTO((struct at_type *));

#if defined(WIN32)

# include <windows.h>
# include <stdio.h>
typedef void *gp_dll_t;

# define DLL_PATHSEP "\\"
# define DLL_EXT  ".dll"
# define DLL_OPEN(f) ((void *)LoadLibrary((f)));
# define DLL_CLOSE(dl) ((void)FreeLibrary((HINSTANCE)dl))
# define DLL_SYM(dl, sym) ((void *)GetProcAddress((HINSTANCE)dl, (sym)))
# define DLL_ERROR(dl) "dynamic library error"


#elif defined(HAVE_DLFCN_H)

# include <dlfcn.h>
typedef void *gp_dll_t;

# define DLL_PATHSEP "/"
# define DLL_EXT  ".so"
# define DLL_OPEN(f) dlopen((f), RTLD_NOW);
# define DLL_CLOSE(dl) dlclose(dl)
# define DLL_SYM(dl, sym) dlsym((dl),(sym))
# define DLL_ERROR(dl) dlerror()


#elif defined(HAVE_DL_H)

# include <dl.h>
typedef shl_t gp_dll_t;

# define DLL_PATHSEP "/"
# define DLL_EXT  ".so"
# define DLL_OPEN(f) shl_load((f), BIND_IMMEDIATE, 0);
# define DLL_CLOSE(dl) shl_unload(dl)
__inline__ static DLL_SYM(gp_dll_t dl, const char *sym)
{
  void *a;
  if (shl_findsym(&dl, sym, TYPE_PROCEDURE, &a))
    return a;
  else
    return 0x0;
}
# define DLL_ERROR(dl) strerror(errno)


#else /* No DLL */

#  error "HAVE_EXTERNAL_FUNCTIONS requires a DLL lib"

#endif /* No DLL */
#endif /* HAVE_EXTERNAL_FUNCTIONS */

#endif /* GNUPLOT_EXTERNAL_H */
