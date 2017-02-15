/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985 by Supoj Sutanthavibul
 * Parts Copyright (c) 1994 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 *
 * The X Consortium, and any party obtaining a copy of these files from
 * the X Consortium, directly or indirectly, is granted, free of charge, a
 * full and unrestricted irrevocable, world-wide, paid up, royalty-free,
 * nonexclusive right and license to deal in this software and
 * documentation files (the "Software"), including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons who receive
 * copies from any such party to do so, with the only requirement being
 * that this copyright notice remain intact.  This license includes without
 * limitation a license to do the foregoing actions under any patents of
 * the party supplying this software to the X Consortium.
 */

/* This file has been modified for use with Gnuplot 3.6 by
 * Ian MacPhedran.
 */

/* DEFAULT is used for many things - font, color etc */

#define		DEFAULT		      (-1)
#define		SOLID_LINE		0
#define		DASH_LINE		1
#define		DOTTED_LINE		2
#define		RUBBER_LINE		3
#define		PANEL_LINE		4

#define		Color			int

#define		BLACK			0
#define		BLUE			1
#define		GREEN			2
#define		CYAN			3
#define		RED			4
#define		MAGENTA			5
#define		YELLOW			6
#define		WHITE			7

/** VERY IMPORTANT:  The f_line, f_spline and f_arc objects all must have the
		components up to and including the arrows in the same order.
		This is for the get/put_generic_arrows() in e_edit.c.
**/

typedef struct f_point {
    int		    x, y;
}
		F_point;

typedef struct f_pos {
    int		    x, y;
}
		F_pos;

#define DEF_ARROW_WID (4 * ZOOM_FACTOR)
#define DEF_ARROW_HT (8 * ZOOM_FACTOR)

typedef struct f_arrow {
    int		    type;
    int		    style;
    float	    thickness;
    float	    wid;
    float	    ht;
}
		F_arrow;

typedef struct f_ellipse {
    int		    tagged;
    int		    distrib;
    int		    type;
#define					T_ELLIPSE_BY_RAD	1
#define					T_ELLIPSE_BY_DIA	2
#define					T_CIRCLE_BY_RAD		3
#define					T_CIRCLE_BY_DIA		4
    int		    style;
    int		    thickness;
    Color	    pen_color;
    Color	    fill_color;
    int		    fill_style;
    int		    depth;
    float	    style_val;
    int		    pen_style;
    float	    angle;
    int		    direction;
#define					UNFILLED	-1
    struct f_pos    center;
    struct f_pos    radiuses;
    struct f_pos    start;
    struct f_pos    end;
    struct f_ellipse *next;
}
		F_ellipse;

/* SEE NOTE AT TOP BEFORE CHANGING ANYTHING IN THE f_arc STRUCTURE */

typedef struct f_arc {
    int		    tagged;
    int		    distrib;
    int		    type;
				/* note: these arc types are the internal values */
				/* in the file, they are open=1, wedge=2 */
#define					T_OPEN_ARC		0
#define					T_PIE_WEDGE_ARC		1
    int		    style;
    int		    thickness;
    Color	    pen_color;
    Color	    fill_color;
    int		    fill_style;
    int		    depth;
    int		    pen_style;
    struct f_arrow *for_arrow;
    struct f_arrow *back_arrow;
/* THE PRECEDING VARS MUST BE IN THE SAME ORDER IN f_arc, f_line and f_spline */
    int		    cap_style;
    float	    style_val;
    int		    direction;
    struct {
	float		x, y;
    }		    center;
    struct f_pos    point[3];
    struct f_arc   *next;
}
		F_arc;

#define		CLOSED_PATH		0
#define		OPEN_PATH		1
#define		DEF_BOXRADIUS		7
#define		DEF_DASHLENGTH		4
#define		DEF_DOTGAP		3

typedef struct f_pic {
#ifndef PATH_MAX
#define PATH_MAX 128
#endif
    char	    file[PATH_MAX];
    int		    subtype;
#define T_PIC_EPS	1
#define T_PIC_BITMAP	2
#define T_PIC_PIXMAP	3
#define T_PIC_GIF	4
#define FileInvalid	-2
    int		    flipped;
    unsigned char   *bitmap;
    int		    numcols;		/* number of colors in cmap */
    float	    hw_ratio;
    int		    size_x, size_y;	/* fig units */
    struct f_pos    bit_size;		/* pixels */
    Color	    color;		/* only used for XBM */
    int		    pix_rotation, pix_width, pix_height, pix_flipped;
}
		F_pic;

extern char	EMPTY_PIC[];

/* SEE NOTE AT TOP BEFORE CHANGING ANYTHING IN THE f_line STRUCTURE */

typedef struct f_line {
    int		    tagged;
    int		    distrib;
    int		    type;
#define					T_POLYLINE	1
#define					T_BOX		2
#define					T_POLYGON	3
#define					T_ARC_BOX	4
#define					T_PIC_BOX	5
    int		    style;
    int		    thickness;
    Color	    pen_color;
    Color	    fill_color;
    int		    fill_style;
    int		    depth;
    int		    pen_style;
    struct f_arrow *for_arrow;
    struct f_arrow *back_arrow;
/* THE PRECEDING VARS MUST BE IN THE SAME ORDER IN f_arc, f_line and f_spline */
    int		    cap_style;		/* line cap style - Butt, Round, Bevel */
#define					CAP_BUTT	0
#define					CAP_ROUND	1
#define					CAP_PROJECT	2
    struct f_point *points;	/* this must immediately follow cap_style */
    int		    join_style;		/* join style - Miter, Round, Bevel */
#define					JOIN_MITER	0
#define					JOIN_ROUND	1
#define					JOIN_BEVEL	2
    float	    style_val;
    int		    radius;		/* corner radius for T_ARC_BOX */
    struct f_pic   *pic;
    struct f_line  *next;
}
		F_line;

typedef struct f_text {
    int		    tagged;
    int		    distrib;
    int		    type;
#define					T_LEFT_JUSTIFIED	0
#define					T_CENTER_JUSTIFIED	1
#define					T_RIGHT_JUSTIFIED	2
    int		    font;
/*    PIX_FONT	    fontstruct; */
    int		    size;	/* point size */
    Color	    color;
    int		    depth;
    float	    angle;	/* in radians */

    int		    flags;
#define					RIGID_TEXT		1
#define					SPECIAL_TEXT		2
#define					PSFONT_TEXT		4
#define					HIDDEN_TEXT		8

    int		    ascent;	/* Fig units */
    int		    length;	/* Fig units */
    int		    descent;	/* from XTextExtents(), not in file */
    int		    base_x;
    int		    base_y;
    int		    pen_style;
    char	   *cstring;
    struct f_text  *next;
}
		F_text;

#define MAXFONT(T) (psfont_text(T) ? NUM_FONTS : NUM_LATEX_FONTS)

#define		rigid_text(t) \
			(t->flags == DEFAULT \
				|| (t->flags & RIGID_TEXT))

#define		special_text(t) \
			((t->flags != DEFAULT \
				&& (t->flags & SPECIAL_TEXT)))

#define		psfont_text(t) \
			(t->flags != DEFAULT \
				&& (t->flags & PSFONT_TEXT))

#define		hidden_text(t) \
			(t->flags != DEFAULT \
				&& (t->flags & HIDDEN_TEXT))

#define		text_length(t) \
			(hidden_text(t) ? hidden_text_length : t->length)

#define		using_ps	(cur_textflags & PSFONT_TEXT)

typedef struct f_control {
    float	    lx, ly, rx, ry;
    struct f_control *next;
}
		F_control;

/* SEE NOTE AT TOP BEFORE CHANGING ANYTHING IN THE f_spline STRUCTURE */

#define		int_spline(s)		(s->type & 0x2)
#define		normal_spline(s)	(!(s->type & 0x2))
#define		closed_spline(s)	(s->type & 0x1)
#define		open_spline(s)		(!(s->type & 0x1))

typedef struct f_spline {
    int		    tagged;
    int		    distrib;
    int		    type;
#define					T_OPEN_NORMAL	0
#define					T_CLOSED_NORMAL 1
#define					T_OPEN_INTERP	2
#define					T_CLOSED_INTERP 3
    int		    style;
    int		    thickness;
    Color	    pen_color;
    Color	    fill_color;
    int		    fill_style;
    int		    depth;
    int		    pen_style;
    struct f_arrow *for_arrow;
    struct f_arrow *back_arrow;
/* THE PRECEDING VARS MUST BE IN THE SAME ORDER IN f_arc, f_line and f_spline */
    int		    cap_style;
    /*
     * For T_OPEN_NORMAL and T_CLOSED_NORMAL points are control points while
     * they are knots for T_OPEN_INTERP and T_CLOSED_INTERP whose control
     * points are stored in controls.
     */
    struct f_point *points;	/* this must immediately follow cap_style */
    float	    style_val;
    struct f_control *controls;
    struct f_spline *next;
}
		F_spline;

typedef struct f_compound {
    int		    tagged;
    int		    distrib;
    struct f_pos    nwcorner;
    struct f_pos    secorner;
    struct f_line  *lines;
    struct f_ellipse *ellipses;
    struct f_spline *splines;
    struct f_text  *texts;
    struct f_arc   *arcs;
    struct f_compound *compounds;
    struct f_compound *next;
}
		F_compound;

typedef struct f_linkinfo {
    struct f_line  *line;
    struct f_point *endpt;
    struct f_point *prevpt;
    int		    two_pts;
    struct f_linkinfo *next;
}
		F_linkinfo;

/* separate the "type" and the "style" from the cur_arrowtype */
#define		ARROW_TYPE(x)	((x)==0? 0 : ((x)+1)/2)
#define		ARROW_STYLE(x)	((x)==0? 0 : ((x)+1)%2)

#define		ARROW_SIZE	sizeof(struct f_arrow)
#define		POINT_SIZE	sizeof(struct f_point)
#define		CONTROL_SIZE	sizeof(struct f_control)
#define		ELLOBJ_SIZE	sizeof(struct f_ellipse)
#define		ARCOBJ_SIZE	sizeof(struct f_arc)
#define		LINOBJ_SIZE	sizeof(struct f_line)
#define		TEXOBJ_SIZE	sizeof(struct f_text)
#define		SPLOBJ_SIZE	sizeof(struct f_spline)
#define		COMOBJ_SIZE	sizeof(struct f_compound)
#define		PIC_SIZE	sizeof(struct f_pic)
#define		LINKINFO_SIZE	sizeof(struct f_linkinfo)

/**********************  object codes  **********************/

#define		O_COLOR_DEF	0
#define		O_ELLIPSE	1
#define		O_POLYLINE	2
#define		O_SPLINE	3
/* HBB 990329: quick hack: 'O_TEXT' is in use by <fcntl.h> header
 * on DOS/Windows platforms. Renamed to OBJ_TEXT */
#define         OBJ_TEXT          4
#define		O_ARC		5
#define		O_COMPOUND	6
#define		O_END_COMPOUND	-O_COMPOUND
#define		O_ALL_OBJECT	99

/********************* object masks for update  ************************/

#define M_NONE			0x000
#define M_POLYLINE_POLYGON	0x001
#define M_POLYLINE_LINE		0x002
#define M_POLYLINE_BOX		0x004	/* includes ARCBOX */
#define M_SPLINE_O_NORMAL	0x008
#define M_SPLINE_C_NORMAL	0x010
#define M_SPLINE_O_INTERP	0x020
#define M_SPLINE_C_INTERP	0x040
#define M_TEXT_NORMAL		0x080
#define M_TEXT_HIDDEN		0x100
#define M_ARC			0x200
#define M_ELLIPSE		0x400
#define M_COMPOUND		0x800

#define M_TEXT		(M_TEXT_HIDDEN | M_TEXT_NORMAL)
#define M_SPLINE_O	(M_SPLINE_O_NORMAL | M_SPLINE_O_INTERP)
#define M_SPLINE_C	(M_SPLINE_C_NORMAL | M_SPLINE_C_INTERP)
#define M_SPLINE_NORMAL (M_SPLINE_O_NORMAL | M_SPLINE_C_NORMAL)
#define M_SPLINE_INTERP (M_SPLINE_O_INTERP | M_SPLINE_C_INTERP)
#define M_SPLINE	(M_SPLINE_NORMAL | M_SPLINE_INTERP)
#define M_POLYLINE	(M_POLYLINE_LINE | M_POLYLINE_POLYGON | M_POLYLINE_BOX)
#define M_VARPTS_OBJECT (M_POLYLINE_LINE | M_POLYLINE_POLYGON | M_SPLINE)
#define M_OPEN_OBJECT	(M_POLYLINE_LINE | M_SPLINE_O | M_ARC)
#define M_ROTATE_ANGLE	(M_VARPTS_OBJECT | M_ARC | M_TEXT | M_COMPOUND | M_ELLIPSE)
#define M_ELLTEXTANGLE	(M_ELLIPSE | M_TEXT)
#define M_OBJECT	(M_ELLIPSE | M_POLYLINE | M_SPLINE | M_TEXT | M_ARC)
#define M_NO_TEXT	(M_ELLIPSE | M_POLYLINE | M_SPLINE | M_COMPOUND | M_ARC)
#define M_ALL		(M_OBJECT | M_COMPOUND)

/************************  Objects  **********************/

extern F_compound objects;

/************  global working pointers ************/

extern F_line		*cur_l, *new_l, *old_l;
extern F_arc		*cur_a, *new_a, *old_a;
extern F_ellipse	*cur_e, *new_e, *old_e;
extern F_text		*cur_t, *new_t, *old_t;
extern F_spline		*cur_s, *new_s, *old_s;
extern F_compound	*cur_c, *new_c, *old_c;
extern F_point		*first_point, *cur_point;
extern F_linkinfo	*cur_links;

/*************** object attribute settings ***********/

/*  Lines  */
extern int	cur_linewidth;
extern int	cur_linestyle;
extern int	cur_joinstyle;
extern int	cur_capstyle;
extern float	cur_dashlength;
extern float	cur_dotgap;
extern float	cur_styleval;
extern Color	cur_fillcolor, cur_pencolor;
extern int	cur_fillstyle, cur_penstyle;
extern int	cur_boxradius;
extern int	cur_arrowmode;
extern int	cur_arrowtype;
extern int	cur_arctype;

/* Text */
extern int	cur_fontsize;	/* font size */
extern int	cur_latex_font;
extern int	cur_ps_font;
extern int	cur_textjust;
extern int	cur_textflags;

/* Misc */
extern float	cur_elltextangle;
