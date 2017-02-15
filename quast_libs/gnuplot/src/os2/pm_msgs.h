/*
 * $Id: pm_msgs.h,v 1.4 2005/10/05 08:05:41 mikulik Exp $
 */

/* GNUPLOT - pm_msgs.h */

/*[
 * Copyright 1992, 1993, 1998, 2004   Roger Fearick
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
 * Message codes for communication between pm.trm (gnuplot) and gclient.c
 * (gnupmdrv).
 */

#ifndef PM_CMDS_H
#define PM_CMDS_H

/* graphics commands */
#define SET_GRAPHICS	'G'
#define SET_TEXT	'E'
#define SET_LINE	'L'
#define SET_FILLBOX	'B'
#define SET_LINEWIDTH	'W'
#define SET_ANGLE	'A'
#define SET_JUSTIFY	'J'
#define SET_POINTMODE	'D'
#define SET_FONT	'F'
#define GR_QUERY_FONT   'g'
#define SET_OPTIONS	'O'
#define SET_SPECIAL	'o'  /* used for special options */
#define SET_MENU	'#'
#define GR_QUERY	'Q'
#define GR_SUSPEND	'E'	/*'s' */
#define GR_RESUME	'r'
#define GR_MOVE		'M'
#define GR_DRAW		'V'
#define GR_RESET	'R'
#define GR_TEXT		'T'
#define GR_ENH_TEXT	'x'
#define GR_PAUSE 	'P'
#define GR_HELP		'H'
#define GR_MOUSECAPABLE 'm' /* PM: say gnupmdrv we are mouseable */
#define PUT_TMPTEXT	't'
#define SET_RULER	'u'
#define SET_CURSOR	'c'
#define SET_CLIPBOARD	'l'
#define GR_MAKE_PALETTE		'p'
#define GR_RELEASE_PALETTE	'e'
#define GR_SET_COLOR		'C'
#define GR_SET_RGBCOLOR		'b'
#define GR_FILLED_POLYGON	'f'
#define GR_IMAGE		'i'
#define GR_RGB_IMAGE	'I'

#endif

