/*
 * gadgets.h,v 1.1.3.1 2000/05/03 21:47:15 hbb Exp
 */

/* GNUPLOT - gadgets.h */

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

#ifndef GNUPLOT_GADGETS_H
# define GNUPLOT_GADGETS_H

#include "syscfg.h"

#include "term_api.h"

/* Types and variables concerning graphical plot elements that are not
 * *terminal-specific, are used by both* 2D and 3D plots, and are not
 * *assignable to any particular * axis. I.e. they belong to neither
 * *term_api, graphics, graph3d, nor * axis .h files.
 */

/* #if... / #include / #define collection: */

/* Default point size is taken from the global "pointsize" variable */
#define PTSZ_DEFAULT    (-2)
#define PTSZ_VARIABLE   (-3)
#define AS_VARIABLE	(-3)

/* Type definitions */

/* Coordinate system specifications: x1/y1, x2/y2, graph-box relative
 * or screen relative coordinate systems */
typedef enum position_type {
    first_axes,
    second_axes,
    graph,
    screen,
    character
} position_type;

/* A full 3D position, with all 3 coordinates of possible using different axes.
 * Used for 'set label', 'set arrow' positions and various offsets.
 */
typedef struct position {
    enum position_type scalex,scaley,scalez;
    double x,y,z;
} t_position;

/* Linked list of structures storing 'set label' information */
typedef struct text_label {
    struct text_label *next;	/* pointer to next label in linked list */
    int tag;			/* identifies the label */
    t_position place;
    enum JUSTIFY pos;		/* left/center/right horizontal justification */
    int rotate;
    int layer;
    int boxed;			/* EAM_BOXED_TEXT */
    char *text;
    char *font;			/* Entry font added by DJL */
    struct t_colorspec textcolor;
    struct lp_style_type lp_properties;
    struct position offset;
    TBOOLEAN noenhanced;
    TBOOLEAN hypertext;
} text_label;

/* This is the default state for the axis, timestamp, and plot title labels
 * indicated by tag = -2 */
#define NONROTATABLE_LABEL_TAG -2
#define ROTATE_IN_3D_LABEL_TAG -3
#define VARIABLE_ROTATE_LABEL_TAG -4
#define EMPTY_LABELSTRUCT \
    {NULL, NONROTATABLE_LABEL_TAG, \
     {character, character, character, 0.0, 0.0, 0.0}, CENTRE, 0, 0, \
     0, \
     NULL, NULL, {TC_LT, -2, 0.0}, DEFAULT_LP_STYLE_TYPE, \
     {character, character, character, 0.0, 0.0, 0.0}, FALSE, \
     FALSE}

/* Datastructure for implementing 'set arrow' */
typedef enum arrow_type {
    arrow_end_absolute,
    arrow_end_relative,
    arrow_end_oriented
    } arrow_type;

typedef struct arrow_def {
    struct arrow_def *next;	/* pointer to next arrow in linked list */
    int tag;			/* identifies the arrow */
    arrow_type type;		/* how to interpret t_position end */
    t_position start;
    t_position end;
    double angle;		/* angle in degrees if type arrow_end_oriented */
    struct arrow_style_type arrow_properties;
} arrow_def;

#ifdef EAM_OBJECTS
/* The object types supported so far are OBJ_RECTANGLE, OBJ_CIRCLE, and OBJ_ELLIPSE */
typedef struct rectangle {
    int type;			/* 0 = corners;  1 = center + size */
    t_position center;		/* center */
    t_position extent;		/* width and height */
    t_position bl;		/* bottom left */
    t_position tr;		/* top right */
} t_rectangle;

#define DEFAULT_RADIUS (-1.0)
#define DEFAULT_ELLIPSE (-2.0)
typedef struct circle {
    int type;			/* not used */
    t_position center;		/* center */
    t_position extent;		/* radius */
    double arc_begin;
    double arc_end;
    TBOOLEAN wedge;		/* TRUE = connect arc ends to center */
} t_circle;

#define ELLIPSEAXES_XY (0)
#define ELLIPSEAXES_XX (1)
#define ELLIPSEAXES_YY (2)
typedef struct ellipse {
    int type;			/* mapping of axes: ELLIPSEAXES_XY, ELLIPSEAXES_XX or ELLIPSEAXES_YY */
    t_position center;		/* center */
    t_position extent;		/* major and minor axes */
    double orientation;		/* angle of first axis to horizontal */
} t_ellipse;

typedef struct polygon {
    int	type;			/* Number of vertices */
    t_position *vertex;		/* Array of vertices */
} t_polygon;

typedef enum en_clip_object {
    OBJ_CLIP,		/* Clip to graph unless coordinate type is screen */
    OBJ_NOCLIP,	/* Clip to canvas, never to graph */
    OBJ_ALWAYS_CLIP	/* Not yet implemented */
} t_clip_object;

/* Datastructure for 'set object' */
typedef struct object {
    struct object *next;
    int tag;
    int layer;			/* behind or back or front */
    int object_type;		/* OBJ_RECTANGLE */
    t_clip_object clip;         
    fill_style_type fillstyle;
    lp_style_type lp_properties;
    union o {t_rectangle rectangle; t_circle circle; t_ellipse ellipse; t_polygon polygon;} o;
} t_object;
#define OBJ_RECTANGLE (1)
#define OBJ_CIRCLE (2)
#define OBJ_ELLIPSE (3)
#define OBJ_POLYGON (4)
#endif

/* Datastructure implementing 'set dashtype' */
struct custom_dashtype_def {
    struct custom_dashtype_def *next;	/* pointer to next dashtype in linked list */
    int tag;			/* identifies the dashtype */
    int d_type;                 /* for DASHTYPE_SOLID or CUSTOM */
    struct t_dashtype dashtype;
};

/* Datastructure implementing 'set style line' */
struct linestyle_def {
    struct linestyle_def *next;	/* pointer to next linestyle in linked list */
    int tag;			/* identifies the linestyle */
    struct lp_style_type lp_properties;
};

/* Datastructure implementing 'set style arrow' */
struct arrowstyle_def {
    struct arrowstyle_def *next;/* pointer to next arrowstyle in linked list */
    int tag;			/* identifies the arrowstyle */
    struct arrow_style_type arrow_properties;
};

/* For 'set style parallelaxis' */
struct pa_style {
    lp_style_type lp_properties;/* used to draw the axes themselves */
    int layer;			/* front/back */
};
#define DEFAULT_PARALLEL_AXIS_STYLE \
	{{0, LT_BLACK, 0, DASHTYPE_SOLID, 0, 2.0, 0.0, 0, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, LAYER_FRONT }

/* The stacking direction of the key box: (vertical, horizontal) */
typedef enum en_key_stack_direction {
    GPKEY_VERTICAL,
    GPKEY_HORIZONTAL
} t_key_stack_direction;

/* The region, with respect to the border, key is located: (inside, outside) */
typedef enum en_key_region {
    GPKEY_AUTO_INTERIOR_LRTBC,   /* Auto placement, left/right/top/bottom/center */
    GPKEY_AUTO_EXTERIOR_LRTBC,   /* Auto placement, left/right/top/bottom/center */
    GPKEY_AUTO_EXTERIOR_MARGIN,  /* Auto placement, margin plus lrc or tbc */
    GPKEY_USER_PLACEMENT         /* User specified placement */
} t_key_region;

/* If exterior, there are 12 possible auto placements.  Since
   left/right/center with top/bottom/center can only define 9
   locations, further subdivide the exterior region into four
   subregions for which left/right/center (TMARGIN/BMARGIN)
   and top/bottom/center (LMARGIN/RMARGIN) creates 12 locations. */
typedef enum en_key_ext_region {
    GPKEY_TMARGIN,
    GPKEY_BMARGIN,
    GPKEY_LMARGIN,
    GPKEY_RMARGIN
} t_key_ext_region;

/* Key sample to the left or the right of the plot title? */
typedef enum en_key_sample_positioning {
    GPKEY_LEFT,
    GPKEY_RIGHT
} t_key_sample_positioning;

typedef struct {
    int opt_given; /* option given / not given (otherwise default) */
    int closeto;   /* from list FILLEDCURVES_CLOSED, ... */
    double at;	   /* value for FILLEDCURVES_AT... */
    double aty;	   /* the other value for FILLEDCURVES_ATXY */
    int oneside;   /* -1 if fill below bound only; +1 if fill above bound only */
} filledcurves_opts;
#define EMPTY_FILLEDCURVES_OPTS { 0, 0, 0.0, 0.0, 0 }

typedef struct histogram_style {
    int type;		/* enum t_histogram_type */
    int gap;		/* set style hist gap <n> (space between clusters) */
    int clustersize;	/* number of datasets in this histogram */
    double start;	/* X-coord of first histogram entry */
    double end;		/* X-coord of last histogram entry */
    int startcolor;	/* LT_UNDEFINED or explicit color for first entry */
    int startpattern;	/* LT_UNDEFINED or explicit pattern for first entry */
    double bar_lw;	/* linewidth for error bars */
    struct histogram_style *next;
    struct text_label title;
} histogram_style;
typedef enum histogram_type {
	HT_NONE,
	HT_STACKED_IN_LAYERS,
	HT_STACKED_IN_TOWERS,
	HT_CLUSTERED,
	HT_ERRORBARS
} t_histogram_type;
#define DEFAULT_HISTOGRAM_STYLE { HT_CLUSTERED, 2, 1, 0.0, 0.0, LT_UNDEFINED, LT_UNDEFINED, 0, NULL, EMPTY_LABELSTRUCT }

typedef enum en_boxplot_factor_labels {
	BOXPLOT_FACTOR_LABELS_OFF,
	BOXPLOT_FACTOR_LABELS_AUTO,
	BOXPLOT_FACTOR_LABELS_X,
	BOXPLOT_FACTOR_LABELS_X2
} t_boxplot_factor_labels;

#define DEFAULT_BOXPLOT_FACTOR -1

typedef struct boxplot_style {
    int limit_type;	/* 0 = multiple of interquartile 1 = fraction of points */
    double limit_value;
    TBOOLEAN outliers;
    int pointtype;
    int plotstyle;	/* CANDLESTICKS or FINANCEBARS */
    double separation;	/* of boxplots if there are more than one factors */
    t_boxplot_factor_labels labels;	/* Which axis to put the tic labels if there are factors */
    TBOOLEAN sort_factors;	/* Sort factors in alphabetical order? */
} boxplot_style;
extern boxplot_style boxplot_opts;
#define DEFAULT_BOXPLOT_STYLE { 0, 1.5, TRUE, 6, CANDLESTICKS, 1.0, BOXPLOT_FACTOR_LABELS_AUTO, FALSE }

#ifdef EAM_BOXED_TEXT
typedef struct textbox_style {
    TBOOLEAN opaque;	/* True if the box is background-filled before writing into it */
    TBOOLEAN noborder;	/* True if you want fill only, no lines */
    double xmargin;	/* fraction of default margin to use */
    double ymargin;	/* fraction of default margin to use */
} textbox_style;
#define DEFAULT_TEXTBOX_STYLE { FALSE, FALSE, 1.0, 1.0 }
#endif

/***********************************************************/
/* Variables defined by gadgets.c needed by other modules. */
/***********************************************************/

/* bounding box position, in terminal coordinates */
typedef struct {
    int xleft;
    int xright;
    int ybot;
    int ytop;
} BoundingBox;

/* EAM Feb 2003 - Move all global variables related to key into a */
/* single structure. Eventually this will allow multiple keys.    */

typedef enum keytitle_type {
    NOAUTO_KEYTITLES, FILENAME_KEYTITLES, COLUMNHEAD_KEYTITLES
} keytitle_type;

typedef struct {
    TBOOLEAN visible;		/* Do we show this key at all? */
    t_key_region region;	/* if so: where? */
    t_key_ext_region margin;	/* if exterior: where outside? */
    struct position user_pos;	/* if user specified position, this is it */
    VERT_JUSTIFY vpos;		/* otherwise these guide auto-positioning */
    JUSTIFY hpos;
    t_key_sample_positioning just;
    t_key_stack_direction stack_dir;
    double swidth;		/* 'width' of the linestyle sample line in the key */
    double vert_factor;		/* user specified vertical spacing multiplier */
    double width_fix;		/* user specified additional (+/-) width of key titles */
    double height_fix;
    keytitle_type auto_titles;	/* auto title curves unless plotted 'with notitle' */
    TBOOLEAN front;		/* draw key in a second pass after the rest of the graph */
    TBOOLEAN reverse;		/* key back to front */
    TBOOLEAN invert;		/* key top to bottom */
    TBOOLEAN enhanced;		/* enable/disable enhanced text of key titles */
    struct lp_style_type box;	/* linetype of box around key:  */
    char *font;			/* Will be used for both key title and plot titles */
    struct t_colorspec textcolor;	/* Will be used for both key title and plot titles */
    BoundingBox bounds;
    int maxcols;		/* maximum no of columns for horizontal keys */
    int maxrows;		/* maximum no of rows for vertical keys */
    text_label title;		/* holds title line for the key as a whole */
} legend_key;

extern legend_key keyT;

#define DEFAULT_KEYBOX_LP {0, LT_NODRAW, 0, DASHTYPE_SOLID, 0, 1.0, PTSZ_DEFAULT, 0, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}

#define DEFAULT_KEY_POSITION { graph, graph, graph, 0.9, 0.9, 0. }

#define DEFAULT_KEY_PROPS \
		{ TRUE, \
		GPKEY_AUTO_INTERIOR_LRTBC, GPKEY_RMARGIN, \
		DEFAULT_KEY_POSITION, \
		JUST_TOP, RIGHT, \
		GPKEY_RIGHT, GPKEY_VERTICAL, \
		4.0, 1.0, 0.0, 0.0, \
		FILENAME_KEYTITLES, \
		FALSE, FALSE, FALSE, TRUE, \
		DEFAULT_KEYBOX_LP, \
		NULL, {TC_LT, LT_BLACK, 0.0}, \
		{0,0,0,0}, 0, 0, \
		EMPTY_LABELSTRUCT}


/*
 * EAM Jan 2006 - Move colorbox structure definition to here from color.h
 * in order to be able to use struct position
 */

#define SMCOLOR_BOX_NO      'n'
#define SMCOLOR_BOX_DEFAULT 'd'
#define SMCOLOR_BOX_USER    'u'

typedef struct {
  char where;
    /* where
	SMCOLOR_BOX_NO .. do not draw the colour box
	SMCOLOR_BOX_DEFAULT .. draw it at default position and size
	SMCOLOR_BOX_USER .. draw it at the position given by user
    */
  char rotation; /* 'v' or 'h' vertical or horizontal box */
  char border; /* if non-null, a border will be drawn around the box (default) */
  int border_lt_tag;
  int layer; /* front or back */
  int xoffset;	/* To adjust left or right, e.g. for y2tics */
  struct position origin;
  struct position size;
  TBOOLEAN invert;	/* gradient low->high runs top->bot rather than bot->top */
  BoundingBox bounds;
} color_box_struct;

extern color_box_struct color_box;
extern color_box_struct default_color_box;

/* Holder for various image properties */
typedef struct t_image {
    t_imagecolor type; /* See above */
    TBOOLEAN fallback; /* true == don't use terminal-specific code */
    unsigned int ncols, nrows; /* image dimensions */
} t_image;

extern BoundingBox plot_bounds;	/* Plot Boundary */
extern BoundingBox canvas; 	/* Writable area on terminal */
extern BoundingBox *clip_area;	/* Current clipping box */

extern float xsize;		/* x scale factor for size */
extern float ysize;		/* y scale factor for size */
extern float zsize;		/* z scale factor for size */
extern float xoffset;		/* x origin setting */
extern float yoffset;		/* y origin setting */
extern float aspect_ratio;	/* 1.0 for square */
extern int aspect_ratio_3D;	/* 2 for equal scaling of x and y; 3 for z also */

/* plot border autosizing overrides, in characters (-1: autosize) */
extern t_position lmargin, bmargin, rmargin, tmargin;
#define DEFAULT_MARGIN_POSITION {character, character, character, -1, -1, -1}

extern struct custom_dashtype_def *first_custom_dashtype;

extern struct arrow_def *first_arrow;

extern struct text_label *first_label;

extern struct linestyle_def *first_linestyle;
extern struct linestyle_def *first_perm_linestyle;
extern struct linestyle_def *first_mono_linestyle;

extern struct arrowstyle_def *first_arrowstyle;

extern struct pa_style parallel_axis_style;

#ifdef EAM_OBJECTS
extern struct object *first_object;
#endif

extern text_label title;

extern text_label timelabel;
#ifndef DEFAULT_TIMESTAMP_FORMAT
/* asctime() format */
# define DEFAULT_TIMESTAMP_FORMAT "%a %b %d %H:%M:%S %Y"
#endif
extern int timelabel_rotate;
extern int timelabel_bottom;

extern TBOOLEAN	polar;

#define ZERO 1e-8		/* default for 'zero' set option */
extern double zero;		/* zero threshold, not 0! */

extern double pointsize;
extern double pointintervalbox;

#define SOUTH		1 /* 0th bit */
#define WEST		2 /* 1th bit */
#define NORTH		4 /* 2th bit */
#define EAST		8 /* 3th bit */
#define border_east	(draw_border & EAST)
#define border_west	(draw_border & WEST)
#define border_south	(draw_border & SOUTH)
#define border_north	(draw_border & NORTH)
#define border_complete	((draw_border & 15) == 15)
extern int draw_border;
extern int user_border;
extern int border_layer;

extern struct lp_style_type border_lp;
extern const struct lp_style_type background_lp;
extern const struct lp_style_type default_border_lp;

extern TBOOLEAN	clip_lines1;
extern TBOOLEAN	clip_lines2;
extern TBOOLEAN	clip_points;

#define SAMPLES 100		/* default number of samples for a plot */
extern int samples_1;
extern int samples_2;

extern double ang2rad; /* 1 or pi/180 */

extern enum PLOT_STYLE data_style;
extern enum PLOT_STYLE func_style;

extern TBOOLEAN parametric;

/* If last plot was a 3d one. */
extern TBOOLEAN is_3d_plot;

/* A macro to check whether 2D functionality is allowed in the last plot:
   either the plot is a 2D plot, or it is a suitably oriented 3D plot (e.g. map).
*/
#define ALMOST2D      \
    ( !is_3d_plot || splot_map || \
      ( fabs(fmod(surface_rot_z,90.0))<0.1  \
        && fabs(fmod(surface_rot_x,180.0))<0.1 ) )

typedef enum E_Refresh_Allowed {
   E_REFRESH_NOT_OK = 0,
   E_REFRESH_OK_2D = 2,
   E_REFRESH_OK_3D = 3
} TRefresh_Allowed;

extern TRefresh_Allowed refresh_ok;
# define SET_REFRESH_OK(ok, nplots) do { \
   refresh_ok = (ok); \
   refresh_nplots = (nplots); \
} while(0)
extern int refresh_nplots;

extern TBOOLEAN volatile_data;

/* WINDOWID to be filled by terminals running on X11 (x11, wxt, qt, ...) */
extern int current_x11_windowid;

/* Plot layer definitions are collected here. */
/* Someday they might actually be used.       */
#define LAYER_BEHIND     -1
#define LAYER_BACK        0
#define LAYER_FRONT       1
#define LAYER_FOREGROUND  2	/* not currently used */
#define LAYER_PLOTLABELS 99

/* Functions exported by gadgets.c */

/* moved here from util3d: */
void draw_clip_line __PROTO((int, int, int, int));
void draw_clip_polygon __PROTO((int , gpiPoint *));
void draw_clip_arrow __PROTO((int, int, int, int, int));
void clip_polygon __PROTO((gpiPoint *, gpiPoint *, int , int *));
int clip_point __PROTO((unsigned int, unsigned int));
void clip_put_text __PROTO((unsigned int, unsigned int, char *));

/* moved here from graph3d: */
void clip_move __PROTO((unsigned int x, unsigned int y));
void clip_vector __PROTO((unsigned int x, unsigned int y));

/* Common routines for setting line or text color from t_colorspec */
void apply_pm3dcolor __PROTO((struct t_colorspec *tc, const struct termentry *t));
void reset_textcolor __PROTO((const struct t_colorspec *tc, const struct termentry *t));

extern fill_style_type default_fillstyle;

#ifdef EAM_OBJECTS
/*       Warning: C89 does not like the union initializers     */
extern struct object default_rectangle;
#define DEFAULT_RECTANGLE_STYLE { NULL, -1, 0, OBJ_RECTANGLE, OBJ_CLIP,	\
	{FS_SOLID, 100, 0, BLACK_COLORSPEC},   			\
	{0, LT_BACKGROUND, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, 0, BACKGROUND_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{.rectangle = {0, {0,0,0,0.,0.,0.}, {0,0,0,0.,0.,0.}, {0,0,0,0.,0.,0.}, {0,0,0,0.,0.,0.}}} }

extern struct object default_circle;
#define DEFAULT_CIRCLE_STYLE { NULL, -1, 0, OBJ_CIRCLE, OBJ_CLIP, \
	{FS_SOLID, 100, 0, BLACK_COLORSPEC},   			\
	{0, LT_BACKGROUND, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, 0, BACKGROUND_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{.circle = {1, {0,0,0,0.,0.,0.}, {graph,0,0,0.02,0.,0.}, 0., 360., TRUE }} }

extern struct object default_ellipse;
#define DEFAULT_ELLIPSE_STYLE { NULL, -1, 0, OBJ_ELLIPSE, OBJ_CLIP, \
	{FS_SOLID, 100, 0, BLACK_COLORSPEC},   			\
	{0, LT_BACKGROUND, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, 0, BACKGROUND_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{.ellipse = {ELLIPSEAXES_XY, {0,0,0,0.,0.,0.}, {graph,graph,0,0.05,0.03,0.}, 0. }} }

#define DEFAULT_POLYGON_STYLE { NULL, -1, 0, OBJ_POLYGON, OBJ_CLIP, \
	{FS_SOLID, 100, 0, BLACK_COLORSPEC},   			\
	{0, LT_BLACK, 0, DASHTYPE_SOLID, 0, 1.0, 0.0, 0, BLACK_COLORSPEC, DEFAULT_DASHPATTERN}, \
	{.polygon = {0, NULL} } }

#endif

/* filledcurves style options set by 'set style [data|func] filledcurves opts' */
extern filledcurves_opts filledcurves_opts_data;
extern filledcurves_opts filledcurves_opts_func;

/* Prefer line styles over plain line types */
#if TRUE || defined(BACKWARDS_COMPATIBLE)
extern TBOOLEAN prefer_line_styles;
#else
#define prefer_line_styles FALSE
#endif

extern histogram_style histogram_opts;

#ifdef EAM_BOXED_TEXT
extern textbox_style textbox_opts;
#endif

void default_arrow_style __PROTO((struct arrow_style_type *arrow));
void apply_head_properties __PROTO((struct arrow_style_type *arrow_properties));

void free_labels __PROTO((struct text_label *tl));

void get_offsets __PROTO((struct text_label *this_label,
	struct termentry *t, int *htic, int *vtic));
void write_label __PROTO((unsigned int x, unsigned int y, struct text_label *label));
int label_width __PROTO((const char *, int *));

#endif /* GNUPLOT_GADGETS_H */
