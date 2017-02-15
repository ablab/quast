/*
 * $Id: tables.h,v 1.91.2.3 2016/08/27 20:50:14 sfeam Exp $
 */

/* GNUPLOT - tables.h */

/*[
 * Copyright 1999, 2004  Lars Hecking
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

#ifndef GNUPLOT_TABLES_H
#define GNUPLOT_TABLES_H

#include "syscfg.h"


typedef void (*parsefuncp_t) __PROTO((void));

struct gen_ftable {
    const char *key;
    parsefuncp_t value;
};

/* The basic structure */
struct gen_table {
    const char *key;
    int value;
};

/* options for plot/splot */
enum plot_id {
    P_INVALID,
    P_AXES, P_BINARY, P_EVERY, P_INDEX, P_MATRIX, P_SMOOTH, P_THRU,
    P_TITLE, P_NOTITLE, P_USING, P_WITH
};

/* options for plot ax[ei]s */
enum plot_axes_id {
    AXES_X1Y1, AXES_X2Y2, AXES_X1Y2, AXES_X2Y1, AXES_NONE
};

/* plot smooth parameters in plot.h */

/* options for 'save' command */
enum save_id { SAVE_INVALID, SAVE_FUNCS, SAVE_TERMINAL, SAVE_SET, SAVE_VARS };

/* options for 'show' and 'set' commands
 * this is rather big, we might be better off with a hash table */
enum set_id {
    S_INVALID,
    S_ACTIONTABLE, S_ALL, S_ANGLES, S_ARROW, S_AUTOSCALE, S_BARS, S_BIND, S_BORDER,
    S_BOXWIDTH, S_CLABEL, S_CLIP, S_CNTRPARAM, S_CNTRLABEL, S_CONTOUR, 
    S_COLOR, S_COLORSEQUENCE, S_DASHTYPE, S_DATA, S_DATAFILE,
    S_FUNCTIONS, S_DGRID3D, S_DUMMY, S_ENCODING, S_DECIMALSIGN, S_FIT,
    S_FONTPATH, S_FORMAT,
    S_GRID, S_HIDDEN3D, S_HISTORY, S_HISTORYSIZE, S_ISOSAMPLES, S_KEY,
    S_LABEL, S_LINK,
    S_LINESTYLE, S_LINETYPE, S_LOADPATH, S_LOCALE, S_LOGSCALE, S_MACROS,
    S_MAPPING, S_MARGIN, S_LMARGIN, S_RMARGIN, S_TMARGIN, S_BMARGIN, S_MISSING,
    S_MINUS_SIGN,
#ifdef USE_MOUSE
    S_MOUSE,
#endif
    S_MONOCHROME, S_MULTIPLOT, S_MX2TICS, S_NOMX2TICS, S_MXTICS, S_NOMXTICS,
    S_MY2TICS, S_NOMY2TICS, S_MYTICS, S_NOMYTICS,
    S_MZTICS, S_NOMZTICS, S_MRTICS, S_NOMRTICS,
    S_OFFSETS, S_ORIGIN, SET_OUTPUT, S_PARAMETRIC,
    S_PALETTE, S_PM3D, S_COLORBOX, S_COLORNAMES,
    S_CBLABEL, S_CBRANGE, S_CBTICS, S_NOCBTICS, S_MCBTICS, S_NOMCBTICS,
    S_CBDATA, S_CBDTICS, S_NOCBDTICS, S_CBMTICS, S_NOCBMTICS, S_OBJECT,
    S_PLOT, S_POINTINTERVALBOX, S_POINTSIZE, S_POLAR, S_PRINT, S_PSDIR,
    S_SAMPLES, S_SIZE, S_SURFACE, S_STYLE, 
    S_TABLE, S_TERMINAL, S_TERMOPTIONS,
    S_TICS, S_TICSCALE, S_TICSLEVEL, S_TIMEFMT, S_TIMESTAMP, S_TITLE,
    S_TRANGE, S_URANGE, S_VARIABLES, S_VERSION, S_VIEW, S_VRANGE,

    S_X2DATA, S_X2DTICS, S_NOX2DTICS, S_X2LABEL, S_X2MTICS, S_NOX2MTICS,
    S_X2RANGE, S_X2TICS, S_NOX2TICS,
    S_XDATA, S_XDTICS, S_NOXDTICS, S_XLABEL, S_XMTICS, S_NOXMTICS, S_XRANGE,
    S_XTICS, S_NOXTICS, S_XYPLANE,

    S_Y2DATA, S_Y2DTICS, S_NOY2DTICS, S_Y2LABEL, S_Y2MTICS, S_NOY2MTICS,
    S_Y2RANGE, S_Y2TICS, S_NOY2TICS,
    S_YDATA, S_YDTICS, S_NOYDTICS, S_YLABEL, S_YMTICS, S_NOYMTICS, S_YRANGE,
    S_YTICS, S_NOYTICS,

    S_ZDATA, S_ZDTICS, S_NOZDTICS, S_ZLABEL, S_ZMTICS, S_NOZMTICS, S_ZRANGE,
    S_ZTICS, S_NOZTICS,

    S_RTICS, S_NORTICS, S_RRANGE, S_RAXIS, S_PAXIS,

    S_ZERO, S_ZEROAXIS, S_XZEROAXIS, S_X2ZEROAXIS, S_YZEROAXIS, S_Y2ZEROAXIS,
    S_ZZEROAXIS
};

enum set_hidden3d_id {
    S_HI_INVALID,
    S_HI_DEFAULTS, S_HI_OFFSET, S_HI_NOOFFSET, S_HI_TRIANGLEPATTERN,
    S_HI_UNDEFINED, S_HI_NOUNDEFINED, S_HI_ALTDIAGONAL, S_HI_NOALTDIAGONAL,
    S_HI_BENTOVER, S_HI_NOBENTOVER,
    S_HI_FRONT, S_HI_BACK
};

enum set_key_id {
    S_KEY_INVALID,
    S_KEY_TOP, S_KEY_BOTTOM, S_KEY_LEFT, S_KEY_RIGHT, S_KEY_CENTER,
    S_KEY_VERTICAL, S_KEY_HORIZONTAL, S_KEY_OVER, S_KEY_UNDER, S_KEY_MANUAL,
    S_KEY_INSIDE, S_KEY_OUTSIDE, S_KEY_ABOVE, S_KEY_BELOW,
    S_KEY_TMARGIN, S_KEY_BMARGIN, S_KEY_LMARGIN, S_KEY_RMARGIN,
    S_KEY_LLEFT, S_KEY_RRIGHT, S_KEY_REVERSE, S_KEY_NOREVERSE,
    S_KEY_INVERT, S_KEY_NOINVERT,
    S_KEY_ENHANCED, S_KEY_NOENHANCED,
    S_KEY_BOX, S_KEY_NOBOX, S_KEY_SAMPLEN, S_KEY_SPACING, S_KEY_WIDTH,
    S_KEY_HEIGHT, S_KEY_TITLE, S_KEY_NOTITLE,
    S_KEY_FONT, S_KEY_TEXTCOLOR,
    S_KEY_AUTOTITLES, S_KEY_NOAUTOTITLES,
    S_KEY_DEFAULT, S_KEY_ON, S_KEY_OFF,
    S_KEY_MAXCOLS, S_KEY_MAXROWS,
    S_KEY_FRONT, S_KEY_NOFRONT
};

enum set_colorbox_id {
    S_COLORBOX_INVALID,
    S_COLORBOX_VERTICAL, S_COLORBOX_HORIZONTAL,
    S_COLORBOX_DEFAULT, S_COLORBOX_USER,
    S_COLORBOX_BORDER, S_COLORBOX_BDEFAULT, S_COLORBOX_NOBORDER,
    S_COLORBOX_ORIGIN, S_COLORBOX_SIZE,
    S_COLORBOX_INVERT, S_COLORBOX_NOINVERT,
    S_COLORBOX_FRONT, S_COLORBOX_BACK
};

enum set_palette_id {
    S_PALETTE_INVALID,
    S_PALETTE_POSITIVE, S_PALETTE_NEGATIVE,
    S_PALETTE_GRAY, S_PALETTE_COLOR, S_PALETTE_RGBFORMULAE,
    S_PALETTE_NOPS_ALLCF, S_PALETTE_PS_ALLCF, S_PALETTE_MAXCOLORS,
    S_PALETTE_DEFINED, S_PALETTE_FILE, S_PALETTE_FUNCTIONS,
    S_PALETTE_MODEL, S_PALETTE_GAMMA, S_PALETTE_CUBEHELIX
};

enum set_pm3d_id {
    S_PM3D_INVALID,
    S_PM3D_AT,
    S_PM3D_INTERPOLATE,
    S_PM3D_SCANSFORWARD, S_PM3D_SCANSBACKWARD, S_PM3D_SCANS_AUTOMATIC,
    S_PM3D_DEPTH,
    S_PM3D_FLUSH, S_PM3D_FTRIANGLES, S_PM3D_NOFTRIANGLES,
    S_PM3D_CLIP_1IN, S_PM3D_CLIP_4IN,
    S_PM3D_MAP, S_PM3D_BORDER, S_PM3D_NOBORDER, S_PM3D_HIDDEN, S_PM3D_NOHIDDEN,
    S_PM3D_SOLID, S_PM3D_NOTRANSPARENT, S_PM3D_NOSOLID, S_PM3D_TRANSPARENT,
    S_PM3D_IMPLICIT, S_PM3D_NOEXPLICIT, S_PM3D_NOIMPLICIT, S_PM3D_EXPLICIT,
    S_PM3D_WHICH_CORNER
};

enum test_id {
    TEST_INVALID,
    TEST_TERMINAL,
    TEST_PALETTE
};

enum show_style_id {
    SHOW_STYLE_INVALID,
    SHOW_STYLE_DATA, SHOW_STYLE_FUNCTION, SHOW_STYLE_LINE,
    SHOW_STYLE_FILLING, SHOW_STYLE_ARROW, 
    SHOW_STYLE_CIRCLE, SHOW_STYLE_ELLIPSE, SHOW_STYLE_RECTANGLE,
    SHOW_STYLE_INCREMENT, SHOW_STYLE_HISTOGRAM, SHOW_STYLE_BOXPLOT,
    SHOW_STYLE_PARALLEL
#ifdef EAM_BOXED_TEXT
    , SHOW_STYLE_TEXTBOX
#endif
};

enum filledcurves_opts_id {
    FILLEDCURVES_CLOSED,
    FILLEDCURVES_X1, FILLEDCURVES_Y1, FILLEDCURVES_X2, FILLEDCURVES_Y2,
    /* requirement: FILLEDCURVES_ATX1 = FILLEDCURVES_X1+4 */
    FILLEDCURVES_ATX1, FILLEDCURVES_ATY1, FILLEDCURVES_ATX2, FILLEDCURVES_ATY2,
    FILLEDCURVES_ATXY,
    FILLEDCURVES_ATR,
    FILLEDCURVES_ABOVE, FILLEDCURVES_BELOW,
    FILLEDCURVES_BETWEEN
};

extern const struct gen_table command_tbl[];
extern const struct gen_table plot_axes_tbl[];
extern const struct gen_table plot_smooth_tbl[];
extern const struct gen_table dgrid3d_mode_tbl[];
extern const struct gen_table save_tbl[];
extern const struct gen_table set_tbl[];
extern const struct gen_table test_tbl[];
extern const struct gen_table set_key_tbl[];
extern const struct gen_table set_colorbox_tbl[];
extern const struct gen_table set_palette_tbl[];
extern const struct gen_table set_pm3d_tbl[];
extern const struct gen_table color_model_tbl[];
extern const struct gen_table set_hidden3d_tbl[];
extern const struct gen_table show_style_tbl[];
extern const struct gen_table plotstyle_tbl[];

/* EAM Nov 2008 - this is now dynamic, so we can add colors on the fly */
extern       struct gen_table *user_color_names_tbl;
extern       struct gen_table *pm3d_color_names_tbl;
extern const int num_predefined_colors;
extern int num_userdefined_colors;

extern const struct gen_ftable command_ftbl[];

extern const struct gen_table filledcurves_opts_tbl[];

/* Function prototypes */
int lookup_table __PROTO((const struct gen_table *, int));
parsefuncp_t lookup_ftable __PROTO((const struct gen_ftable *, int));
int lookup_table_entry __PROTO((const struct gen_table *tbl, const char *search_str));
int lookup_table_nth __PROTO((const struct gen_table *tbl, const char *search_str));
int lookup_table_nth_reverse __PROTO((const struct gen_table *tbl, int table_len, const char *search_str));
const char * reverse_table_lookup __PROTO((const struct gen_table *tbl, int entry));


#endif /* GNUPLT_TABLES_H */
