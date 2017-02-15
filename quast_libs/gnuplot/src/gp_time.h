/*
 * $Id: gp_time.h,v 1.9 2013/04/05 18:36:53 sfeam Exp $
 */

/* GNUPLOT - gp_time.h */

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

#ifndef GNUPLOT_GP_TIME_H
# define GNUPLOT_GP_TIME_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "stdfn.h"

/* Define the zero point for internal storage of time+date as some number of seconds */
/* Through gnuplot version 4.6 this was taken as 1-jan-2000, i.e. 30 years off from  */
/* the conventional unix epoch date 1-jan-1970. This caused problems when converting */
/* internal <-> external dates given in seconds, so now we change it to agree with   */
/* the rest of the unix world.							     */
#if (0)
#define ZERO_YEAR	2000
#define JAN_FIRST_WDAY 6	/* 1 jan 2000 was a Saturday (cal 1 2000 on unix) */
#define SEC_OFFS_SYS	946684800.0	/*  zero gnuplot (2000) - zero system (1970) */
#else
#define ZERO_YEAR	1970
#define JAN_FIRST_WDAY 4	/* 1 jan 1970 was a Thursday (cal 1 1970 on unix) */
#define SEC_OFFS_SYS	0.0	/* difference between internal and external epochs */
#endif

/* defines used for timeseries, seconds */
#define YEAR_SEC	31557600.0	/* avg, incl. leap year */
#define MON_SEC		2629800.0	/* YEAR_SEC / 12 */
#define WEEK_SEC	604800.0
#define DAY_SEC		86400.0

/* Type definitions */

/* Variables of time.c needed by other modules: */

/* Prototypes of functions exported by time.c */

/* string to *tm */
char * gstrptime __PROTO((char *, char *, struct tm *, double *));

/* seconds to string */
size_t gstrftime __PROTO((char *, size_t, const char *, double));

/* *tm to seconds */
double gtimegm __PROTO((struct tm *));

/* seconds to *tm */
int ggmtime __PROTO((struct tm *, double));



#endif /* GNUPLOT_GP_TIME_H */
