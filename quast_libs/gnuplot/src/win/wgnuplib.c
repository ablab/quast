#ifndef lint
static char *RCSid() { return RCSid("$Id: wgnuplib.c,v 1.9 2014/05/09 22:14:12 broeker Exp $"); }
#endif

/* GNUPLOT - win/wgnuplib.c */
/*[
 * Copyright 1992, 1993, 1998, 2004   Russell Lang
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

/*
 * AUTHORS
 *
 *   Russell Lang
 */

#define STRICT
#include <ctype.h>
#include <windows.h>
#include <windowsx.h>
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"

HINSTANCE hdllInstance;
LPSTR szParentClass = "wgnuplot_parent";
LPSTR szTextClass = "wgnuplot_text";
LPSTR szPauseClass = "wgnuplot_pause";
LPSTR szGraphClass = "wgnuplot_graph";

/* Window ID */
struct WID {
	BOOL    used;
	HWND    hwnd;
	void  * ptr;
};
struct WID *widptr = NULL;
unsigned int nwid = 0;
HLOCAL hwid = 0;

void NEAR *
LocalAllocPtr(UINT flags, UINT size)
{
HLOCAL hlocal;
	hlocal = LocalAlloc(flags, size+1);
	return (char *)LocalLock(hlocal);
}

void NEAR *
LocalReAllocPtr(void NEAR * ptr, UINT flags, UINT size)
{
HLOCAL hlocal;
	hlocal = LocalHandle(ptr);
	LocalUnlock(hlocal);
	hlocal = LocalReAlloc(hlocal, size+1, flags);
	return (char *)LocalLock(hlocal);
}

void
LocalFreePtr(void NEAR *ptr)
{
HLOCAL hlocal;
	hlocal = LocalHandle(ptr);
	LocalUnlock(hlocal);
	LocalFree(hlocal);
	return;
}


/* ascii to int */
/* returns:
 *  A pointer to character past int if successful,
 *  otherwise NULL on failure.
 *  convert int is stored at pval.
 */
LPSTR
GetInt(LPSTR str, LPINT pval)
{
    int val = 0;
    BOOL negative = FALSE;
    BOOL success = FALSE;
    unsigned char ch;

    if (!str)
	return NULL;
    while (((ch = *str) != 0) && isspace(ch))
	str++;

    if (ch == '-') {
	negative = TRUE;
	str++;
    }
    while (((ch = *str) != 0) && isdigit(ch)) {
	success = TRUE;
	val = val * 10 + (ch - '0');
	str++;
    }

    if (success) {
	if (negative)
	    val = -val;
	*pval = val;
	return str;
    }
    return NULL;
}

