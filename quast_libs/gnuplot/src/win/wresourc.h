/*
 * $Id: wresourc.h,v 1.19 2014/01/04 15:46:38 markisch Exp $
 */

/* GNUPLOT - win/wresourc.h */

/*[
 * Copyright 1992 - 1993, 1998, 2004  Maurice Castro, Russell Lang
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
 *   Maurice Castro
 *   Russell Lang
 */

/* This contains items internal to wgnuplot.dll
   that are used by the Resource Compiler */

/* wmenu.c */
#define ID_PROMPT 300
#define ID_ANSWER 302
#define NUMMENU 512
#define ID_TOOLBAR NUMMENU+42
#define ID_TEXTSTATUS NUMMENU+43
#define ID_GRAPHSTATUS NUMMENU+44

/* wpause.c */

/* wtext.c */
#define AB_ICON 250
#define AB_TEXT1 251
#define AB_TEXT2 252
#define AB_TEXT3 253

/* wgraph.c */
#define M_GRAPH_TO_TOP NUMMENU+1
#define M_CHOOSE_FONT  NUMMENU+2
#define M_BACKGROUND   NUMMENU+3
#define M_COLOR        NUMMENU+4
#define M_COPY_CLIP    NUMMENU+5
#define M_LINESTYLE    NUMMENU+6
#define M_PRINT        NUMMENU+7
#define M_WRITEINI     NUMMENU+8
#define M_PASTE        NUMMENU+9
#define M_ABOUT        NUMMENU+10
#define M_REBUILDTOOLS NUMMENU+11
#define M_COMMANDLINE  NUMMENU+12
#define M_SAVE_AS_EMF  NUMMENU+13
#define M_DOUBLEBUFFER NUMMENU+14
#define M_OVERSAMPLE   NUMMENU+15
#define M_GDIPLUS      NUMMENU+16
#define M_ANTIALIASING NUMMENU+17
#define M_POLYAA       NUMMENU+18
#define M_PATTERNAA    NUMMENU+19
#define M_FASTROTATE   NUMMENU+20
#define M_HIDEGRID     NUMMENU+21
#define M_HIDEPLOT     NUMMENU+22
/* Warning: need MAXPLOTSHIDE free slots */
/* wtext.c */
#define M_SYSCOLORS    NUMMENU+50
#define M_WRAP         NUMMENU+51


/* wprinter.c */
#define PSIZE_SBOX 100
#define PSIZE_DEF 101
#define PSIZE_OTHER 102
#define PSIZE_DEFX 103
#define PSIZE_DEFY 104
#define PSIZE_X 105
#define PSIZE_Y 106
#define PSIZE_OFFBOX 107
#define PSIZE_OFFX 108
#define PSIZE_OFFY 109
#define CANCEL_PCDONE 120

/* wgraph.c */
/* line style dialog box */
#define LS_LINENUM 200
#define LS_MONOBOX 201
#define LS_COLORBOX 202
#define LS_MONOSTYLE 203
#define LS_MONOWIDTH 204
#define LS_CHOOSECOLOR 205
#define LS_COLORSAMPLE 206
#define LS_COLORSTYLE 207
#define LS_COLORWIDTH 208
#define LS_DEFAULT 209

/* HBB 20010208: cursors for mouse-enabled wgnuplot */
#define IDC_SCALING    222
#define IDC_ROTATING   223

