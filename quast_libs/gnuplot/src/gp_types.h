/*
 * $Id: gp_types.h,v 1.63.2.1 2014/12/05 18:09:34 sfeam Exp $
 */

/* GNUPLOT - gp_types.h */

/*[
 * Copyright 2000, 2004   Thomas Williams, Colin Kelley
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

#ifndef GNUPLOT_GPTYPES_H
#define GNUPLOT_GPTYPES_H

#include "syscfg.h"

#define MAX_ID_LEN 50		/* max length of an identifier */
#define MAX_LINE_LEN 1024	/* maximum number of chars allowed on line */

#define DEG2RAD (M_PI / 180.0)

/* type_udv() will return 0 rather than type if udv does not exist */
enum DATA_TYPES {
	INTGR=1,
	CMPLX,
	STRING,
	DATABLOCK,
	INVALID_VALUE,	/* used only for error return by external functions */
	INVALID_NAME	/* used only to trap errors in linked axis function definition */
};

enum MODE_PLOT_TYPE {
	MODE_QUERY, MODE_PLOT, MODE_SPLOT
};

enum PLOT_TYPE {
	FUNC, DATA, FUNC3D, DATA3D, NODATA
};

/* we explicitly assign values to the types, such that we can
 * perform bit tests to see if the style involves points and/or lines
 * bit 0 (val 1) = line, bit 1 (val 2) = point, bit 2 (val 4)= error
 * This allows rapid decisions about the sample drawn into the key,
 * for example.
 */
/* HBB 20010610: new enum, to make mnemonic names for these flags
 * accessible everywhere */
typedef enum e_PLOT_STYLE_FLAGS {
    PLOT_STYLE_HAS_LINE      = (1<<0),
    PLOT_STYLE_HAS_POINT     = (1<<1),
    PLOT_STYLE_HAS_ERRORBAR  = (1<<2),
    PLOT_STYLE_HAS_FILL      = (1<<3),
    PLOT_STYLE_BITS          = (1<<4)
} PLOT_STYLE_FLAGS;

typedef enum PLOT_STYLE {
    LINES        =  0*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    POINTSTYLE   =  1*PLOT_STYLE_BITS + PLOT_STYLE_HAS_POINT,
    IMPULSES     =  2*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    LINESPOINTS  =  3*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_LINE),
    DOTS         =  4*PLOT_STYLE_BITS + 0,
    XERRORBARS   =  5*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    YERRORBARS   =  6*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    XYERRORBARS  =  7*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    BOXXYERROR   =  8*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_FILL),
    BOXES        =  9*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_FILL),
    BOXERROR     = 10*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_FILL),
    STEPS        = 11*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    FILLSTEPS    = 11*PLOT_STYLE_BITS + PLOT_STYLE_HAS_FILL,
    FSTEPS       = 12*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    HISTEPS      = 13*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    VECTOR       = 14*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    CANDLESTICKS = 15*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_ERRORBAR | PLOT_STYLE_HAS_FILL),
    FINANCEBARS  = 16*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    XERRORLINES  = 17*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    YERRORLINES  = 18*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    XYERRORLINES = 19*PLOT_STYLE_BITS + (PLOT_STYLE_HAS_LINE | PLOT_STYLE_HAS_POINT | PLOT_STYLE_HAS_ERRORBAR),
    FILLEDCURVES = 21*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE + PLOT_STYLE_HAS_FILL,
    PM3DSURFACE  = 22*PLOT_STYLE_BITS + 0,
    LABELPOINTS  = 23*PLOT_STYLE_BITS + 0,
    HISTOGRAMS   = 24*PLOT_STYLE_BITS + PLOT_STYLE_HAS_FILL,
    IMAGE        = 25*PLOT_STYLE_BITS + 0,
    RGBIMAGE     = 26*PLOT_STYLE_BITS + 0,
    RGBA_IMAGE   = 27*PLOT_STYLE_BITS + 0,
    CIRCLES      = 28*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE + PLOT_STYLE_HAS_FILL,
    BOXPLOT      = 29*PLOT_STYLE_BITS + PLOT_STYLE_HAS_FILL + PLOT_STYLE_HAS_POINT,
    ELLIPSES     = 30*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE + PLOT_STYLE_HAS_FILL,
    SURFACEGRID  = 31*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    PARALLELPLOT = 32*PLOT_STYLE_BITS + PLOT_STYLE_HAS_LINE,
    TABLESTYLE   = 33*PLOT_STYLE_BITS,
    PLOT_STYLE_NONE = -1
} PLOT_STYLE;

typedef enum PLOT_SMOOTH {
    SMOOTH_NONE,
    SMOOTH_ACSPLINES,
    SMOOTH_BEZIER,
    SMOOTH_CSPLINES,
    SMOOTH_SBEZIER,
    SMOOTH_UNIQUE,
    SMOOTH_UNWRAP,
    SMOOTH_FREQUENCY,
    SMOOTH_CUMULATIVE,
    SMOOTH_KDENSITY,
    SMOOTH_CUMULATIVE_NORMALISED,
    SMOOTH_MONOTONE_CSPLINE
} PLOT_SMOOTH;

/* FIXME HBB 20000521: 'struct value' and its part, 'cmplx', should go
 * into one of scanner/internal/standard/util .h, but I've yet to
 * decide which of them */

struct cmplx {
	double real, imag;
};

typedef struct value {
    enum DATA_TYPES type;
    union {
	int int_val;
	struct cmplx cmplx_val;
	char *string_val;
	char **data_array;
    } v;
} t_value;

/* Defines the type of a coordinate */
/* INRANGE and OUTRANGE points have an x,y point associated with them */
typedef enum coord_type {
    INRANGE,			/* inside plot boundary */
    OUTRANGE,			/* outside plot boundary, but defined */
    UNDEFINED,			/* not defined at all */
    EXCLUDEDRANGE		/* would be inside plot, but excluded for other reasons */
				/* e.g. in polar mode and outside of trange[tmin:tmax] */
} coord_type;


/* These fields of 'struct coordinate' used for storing the color of 3D data
 * points (if requested by NEED_PALETTE(this_plot), for instance).
 */
#define CRD_COLOR yhigh
#define CRD_R yhigh
#define CRD_G xlow
#define CRD_B xhigh
#define CRD_A ylow
/* The field of 'struct coordinate' used for storing the point size in plot
 * style POINTSTYLE with variable point size
 */
#define CRD_PTSIZE xlow


typedef struct coordinate {
    enum coord_type type;	/* see above */
    coordval x, y, z;
    coordval ylow, yhigh;	/* ignored in 3d */
    coordval xlow, xhigh;	/* also ignored in 3d */
} coordinate;

typedef enum lp_class {
	LP_TYPE   = 0,	/* lp_style_type defined by 'set linetype' */
	LP_STYLE  = 1,	/* lp_style_type defined by 'set style line' */
	LP_ADHOC  = 2,	/* lp_style_type used for single purpose */
	LP_NOFILL = 3	/* special treatment of fillcolor */
} lp_class;

#endif /* GNUPLOT_GPTYPES_H */
