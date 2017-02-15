#ifndef lint
static char *RCSid() { return RCSid("$Id: multiplot.c,v 1.2.2.1 2015/03/04 04:23:15 sfeam Exp $"); }
#endif

/* GNUPLOT - term.c */

/*[
 * Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
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
 * Bookkeeping and support routines for 'set multiplot layout ...'
 * Jul 2004 Volker Dobler     layout rows, columns
 * Feb 2013 Christoph Bersch  layout margins spacing
 * Mar 2014 Ethan A Merritt   refactor into separate file (used to be in term.c)
 */

#include "term_api.h"
#include "multiplot.h"

#include "command.h"
#include "gadgets.h"
#include "graphics.h"
#include "parse.h"
#include "util.h"


static void mp_layout_size_and_offset __PROTO((void));
static void mp_layout_margins_and_spacing __PROTO((void));
static void mp_layout_set_margin_or_spacing __PROTO((t_position *));

enum set_multiplot_id {
    S_MULTIPLOT_LAYOUT,
    S_MULTIPLOT_COLUMNSFIRST, S_MULTIPLOT_ROWSFIRST, S_MULTIPLOT_SCALE,
    S_MULTIPLOT_DOWNWARDS, S_MULTIPLOT_UPWARDS,
    S_MULTIPLOT_OFFSET, S_MULTIPLOT_TITLE,
    S_MULTIPLOT_MARGINS, S_MULTIPLOT_SPACING,
    S_MULTIPLOT_INVALID
};

static const struct gen_table set_multiplot_tbl[] =
{
    { "lay$out", S_MULTIPLOT_LAYOUT },
    { "col$umnsfirst", S_MULTIPLOT_COLUMNSFIRST },
    { "row$sfirst", S_MULTIPLOT_ROWSFIRST },
    { "down$wards", S_MULTIPLOT_DOWNWARDS },
    { "up$wards", S_MULTIPLOT_UPWARDS },
    { "sca$le", S_MULTIPLOT_SCALE },
    { "off$set", S_MULTIPLOT_OFFSET },
    { "ti$tle", S_MULTIPLOT_TITLE },
    { "ma$rgins", S_MULTIPLOT_MARGINS },
    { "spa$cing", S_MULTIPLOT_SPACING },
    { NULL, S_MULTIPLOT_INVALID }
};

# define MP_LAYOUT_DEFAULT {          \
    FALSE,	/* auto_layout */         \
    0,		/* current_panel */       \
    0, 0,	/* num_rows, num_cols */  \
    FALSE,	/* row_major */           \
    TRUE,	/* downwards */           \
    0, 0,	/* act_row, act_col */    \
    1, 1,	/* xscale, yscale */      \
    0, 0,	/* xoffset, yoffset */    \
    FALSE,	/* auto_layout_margins */ \
    {screen, screen, screen, 0.1, -1, -1},  /* lmargin */ \
    {screen, screen, screen, 0.9, -1, -1},  /* rmargin */ \
    {screen, screen, screen, 0.1, -1, -1},  /* bmargin */ \
    {screen, screen, screen, 0.9, -1, -1},  /* tmargin */ \
    {screen, screen, screen, 0.05, -1, -1}, /* xspacing */ \
    {screen, screen, screen, 0.05, -1, -1}, /* yspacing */ \
    0,0,0,0,	/* prev_ sizes and offsets */ \
    DEFAULT_MARGIN_POSITION, \
    DEFAULT_MARGIN_POSITION, \
    DEFAULT_MARGIN_POSITION, \
    DEFAULT_MARGIN_POSITION,  /* prev_ margins */ \
    EMPTY_LABELSTRUCT, 0.0 \
}

static struct {
    TBOOLEAN auto_layout;  /* automatic layout if true */
    int current_panel;     /* initialized to 0, incremented after each plot */
    int num_rows;          /* number of rows in layout */
    int num_cols;          /* number of columns in layout */
    TBOOLEAN row_major;    /* row major mode if true, column major else */
    TBOOLEAN downwards;    /* prefer downwards or upwards direction */
    int act_row;           /* actual row in layout */
    int act_col;           /* actual column in layout */
    double xscale;         /* factor for horizontal scaling */
    double yscale;         /* factor for vertical scaling */
    double xoffset;        /* horizontal shift */
    double yoffset;        /* horizontal shift */
    TBOOLEAN auto_layout_margins;
    t_position lmargin, rmargin, bmargin, tmargin;
    t_position xspacing, yspacing;
    double prev_xsize, prev_ysize, prev_xoffset, prev_yoffset;
    t_position prev_lmargin, prev_rmargin, prev_tmargin, prev_bmargin;
			   /* values before 'set multiplot layout' */
    text_label title;    /* goes above complete set of plots */
    double title_height;   /* fractional height reserved for title */
} mp_layout = MP_LAYOUT_DEFAULT;

/* Helper routines */
void
multiplot_next()
{
    mp_layout.current_panel++;
    if (mp_layout.auto_layout) {
	if (mp_layout.row_major) {
	    mp_layout.act_row++;
	    if (mp_layout.act_row == mp_layout.num_rows) {
		mp_layout.act_row = 0;
		mp_layout.act_col++;
		if (mp_layout.act_col == mp_layout.num_cols) {
		    /* int_warn(NO_CARET,"will overplot first plot"); */
		    mp_layout.act_col = 0;
		}
	    }
	} else { /* column-major */
	    mp_layout.act_col++;
	    if (mp_layout.act_col == mp_layout.num_cols ) {
		mp_layout.act_col = 0;
		mp_layout.act_row++;
		if (mp_layout.act_row == mp_layout.num_rows ) {
		    /* int_warn(NO_CARET,"will overplot first plot"); */
		    mp_layout.act_row = 0;
		}
	    }
	}
	if (mp_layout.auto_layout_margins)
	    mp_layout_margins_and_spacing();
	else
	    mp_layout_size_and_offset();
    }
}

void
multiplot_previous()
{
    mp_layout.current_panel--;
    if (mp_layout.auto_layout) {
	if (mp_layout.row_major) {
	    mp_layout.act_row--;
	    if (mp_layout.act_row < 0) {
		mp_layout.act_row = mp_layout.num_rows-1;
		mp_layout.act_col--;
		if (mp_layout.act_col < 0) {
		    /* int_warn(NO_CARET,"will overplot first plot"); */
		    mp_layout.act_col = mp_layout.num_cols-1;
		}
	    }
	} else { /* column-major */
	    mp_layout.act_col--;
	    if (mp_layout.act_col < 0) {
		mp_layout.act_col = mp_layout.num_cols-1;
		mp_layout.act_row--;
		if (mp_layout.act_row < 0) {
		    /* int_warn(NO_CARET,"will overplot first plot"); */
		    mp_layout.act_row = mp_layout.num_rows-1;
		}
	    }
	}
	if (mp_layout.auto_layout_margins)
	    mp_layout_margins_and_spacing();
	else
	    mp_layout_size_and_offset();
    }
}

int
multiplot_current_panel()
{
    return mp_layout.current_panel;
}

void
multiplot_start()
{
    TBOOLEAN set_spacing = FALSE;
    TBOOLEAN set_margins = FALSE;

    c_token++;

    /* Only a few options are possible if we are already in multiplot mode */
    /* So far we have "next".  Maybe also "previous", "clear"? */
    if (multiplot) {
	if (equals(c_token, "next")) {
	    c_token++;
	    if (!mp_layout.auto_layout)
		int_error(c_token, "only valid inside an auto-layout multiplot");
	    multiplot_next();
	    return;
	} else if (almost_equals(c_token, "prev$ious")) {
	    c_token++;
	    if (!mp_layout.auto_layout)
		int_error(c_token, "only valid inside an auto-layout multiplot");
	    multiplot_previous();
	    return;
	} else {
	    term_end_multiplot();
	}
    }

    /* FIXME: more options should be reset/initialized each time */
    mp_layout.auto_layout = FALSE;
    mp_layout.auto_layout_margins = FALSE;
    mp_layout.current_panel = 0;
    mp_layout.title.noenhanced = FALSE;
    free(mp_layout.title.text);
    mp_layout.title.text = NULL;
    free(mp_layout.title.font);
    mp_layout.title.font = NULL;

    /* Parse options */
    while (!END_OF_COMMAND) {

	if (almost_equals(c_token, "ti$tle")) {
	    c_token++;
	    mp_layout.title.text = try_to_get_string();
 	    continue;
       }

       if (equals(c_token, "font")) {
	    c_token++;
	    mp_layout.title.font = try_to_get_string();
	    continue;
	}

        if (almost_equals(c_token,"enh$anced")) {
            mp_layout.title.noenhanced = FALSE;
            c_token++;
            continue;
        }

        if (almost_equals(c_token,"noenh$anced")) {
            mp_layout.title.noenhanced = TRUE;
            c_token++;
            continue;
        }

	if (almost_equals(c_token, "lay$out")) {
	    if (mp_layout.auto_layout)
		int_error(c_token, "too many layout commands");
	    else
		mp_layout.auto_layout = TRUE;

	    c_token++;
	    if (END_OF_COMMAND) {
		int_error(c_token,"expecting '<num_cols>,<num_rows>'");
	    }

	    /* read row,col */
	    mp_layout.num_rows = int_expression();
	    if (END_OF_COMMAND || !equals(c_token,",") )
		int_error(c_token, "expecting ', <num_cols>'");

	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "expecting <num_cols>");
	    mp_layout.num_cols = int_expression();

	    /* remember current values of the plot size and the margins */
	    mp_layout.prev_xsize = xsize;
	    mp_layout.prev_ysize = ysize;
	    mp_layout.prev_xoffset = xoffset;
	    mp_layout.prev_yoffset = yoffset;
	    mp_layout.prev_lmargin = lmargin;
	    mp_layout.prev_rmargin = rmargin;
	    mp_layout.prev_bmargin = bmargin;
	    mp_layout.prev_tmargin = tmargin;

	    mp_layout.act_row = 0;
	    mp_layout.act_col = 0;

	    continue;
	}

	/* The remaining options are only valid for auto-layout mode */
	if (!mp_layout.auto_layout)
	    int_error(c_token, "only valid in the context of an auto-layout command");

	switch(lookup_table(&set_multiplot_tbl[0],c_token)) {
	    case S_MULTIPLOT_COLUMNSFIRST:
		mp_layout.row_major = TRUE;
		c_token++;
		break;
	    case S_MULTIPLOT_ROWSFIRST:
		mp_layout.row_major = FALSE;
		c_token++;
		break;
	    case S_MULTIPLOT_DOWNWARDS:
		mp_layout.downwards = TRUE;
		c_token++;
		break;
	    case S_MULTIPLOT_UPWARDS:
		mp_layout.downwards = FALSE;
		c_token++;
		break;
	    case S_MULTIPLOT_SCALE:
		c_token++;
		mp_layout.xscale = real_expression();
		mp_layout.yscale = mp_layout.xscale;
		if (!END_OF_COMMAND && equals(c_token,",") ) {
		    c_token++;
		    if (END_OF_COMMAND) {
			int_error(c_token, "expecting <yscale>");
		    }
		    mp_layout.yscale = real_expression();
		}
		break;
	    case S_MULTIPLOT_OFFSET:
		c_token++;
		mp_layout.xoffset = real_expression();
		mp_layout.yoffset = mp_layout.xoffset;
		if (!END_OF_COMMAND && equals(c_token,",") ) {
		    c_token++;
		    if (END_OF_COMMAND) {
			int_error(c_token, "expecting <yoffset>");
		    }
		    mp_layout.yoffset = real_expression();
		}
		break;
	    case S_MULTIPLOT_MARGINS:
		c_token++;
		if (END_OF_COMMAND)
		    int_error(c_token,"expecting '<left>,<right>,<bottom>,<top>'");
		
		mp_layout.lmargin.scalex = screen;
		mp_layout_set_margin_or_spacing(&(mp_layout.lmargin));
		if (!END_OF_COMMAND && equals(c_token,",") ) {
		    c_token++;
		    if (END_OF_COMMAND)
			int_error(c_token, "expecting <right>");

		    mp_layout.rmargin.scalex = mp_layout.lmargin.scalex;
		    mp_layout_set_margin_or_spacing(&(mp_layout.rmargin));
		} else {
		    int_error(c_token, "expecting <right>");
		}
		if (!END_OF_COMMAND && equals(c_token,",") ) {
		    c_token++;
		    if (END_OF_COMMAND)
			int_error(c_token, "expecting <top>");

		    mp_layout.bmargin.scalex = mp_layout.rmargin.scalex;
		    mp_layout_set_margin_or_spacing(&(mp_layout.bmargin));
		} else {
		    int_error(c_token, "expecting <bottom>");
		}
		if (!END_OF_COMMAND && equals(c_token,",") ) {
		    c_token++;
		    if (END_OF_COMMAND)
			int_error(c_token, "expecting <bottom>");

		    mp_layout.tmargin.scalex = mp_layout.bmargin.scalex;
		    mp_layout_set_margin_or_spacing(&(mp_layout.tmargin));
		} else {
		    int_error(c_token, "expection <top>");
		}
		set_margins = TRUE;
		break;
	    case S_MULTIPLOT_SPACING:
		c_token++;
		if (END_OF_COMMAND)
		    int_error(c_token,"expecting '<xspacing>,<yspacing>'");
		mp_layout.xspacing.scalex = screen;
		mp_layout_set_margin_or_spacing(&(mp_layout.xspacing));
		mp_layout.yspacing = mp_layout.xspacing;

		if (!END_OF_COMMAND && equals(c_token, ",")) {
		    c_token++;
		    if (END_OF_COMMAND)
			int_error(c_token, "expecting <yspacing>");
		    mp_layout_set_margin_or_spacing(&(mp_layout.yspacing));
		}
		set_spacing = TRUE;
		break;
	    default:
		int_error(c_token,"invalid or duplicate option");
		break;
	}
    }

    if (set_spacing || set_margins) {
	if (set_spacing && set_margins) {
	    if (mp_layout.lmargin.x >= 0 && mp_layout.rmargin.x >= 0 
	    &&  mp_layout.tmargin.x >= 0 && mp_layout.bmargin.x >= 0 
	    &&  mp_layout.xspacing.x >= 0 && mp_layout.yspacing.x >= 0)
		mp_layout.auto_layout_margins = TRUE;
	    else
		int_error(NO_CARET, "must give positive margin and spacing values");
	} else if (set_spacing) {
	    int_warn(NO_CARET, "must give margins and spacing, continue with auto margins.");
	} else if (set_margins) {
	    mp_layout.auto_layout_margins = TRUE;
	    mp_layout.xspacing.scalex = screen;
	    mp_layout.xspacing.x = 0.05;
	    mp_layout.yspacing.scalex = screen;
	    mp_layout.yspacing.x = 0.05;
	    int_warn(NO_CARET, "must give margins and spacing, continue with spacing of 0.05");
	}
	/* Sanity check that screen tmargin is > screen bmargin */
	if (mp_layout.bmargin.scalex == screen && mp_layout.tmargin.scalex == screen)
	    if (mp_layout.bmargin.x > mp_layout.tmargin.x) {
		double tmp = mp_layout.bmargin.x;
		mp_layout.bmargin.x = mp_layout.tmargin.x;
		mp_layout.tmargin.x = tmp;
	    }
    }

    /* If we reach here, then the command has been successfully parsed.
     * Aug 2013: call term_start_plot() before setting multiplot so that
     * the wxt and qt terminals will reset the plot count to 0 before
     * ignoring subsequent TERM_LAYER_RESET requests. 
     */
    term_start_plot();
    multiplot = TRUE;
    fill_gpval_integer("GPVAL_MULTIPLOT", 1);

    /* Place overall title before doing anything else */
    if (mp_layout.title.text) {
	double tmpx, tmpy;
	unsigned int x, y;
	char *p = mp_layout.title.text;

	map_position_r(&(mp_layout.title.offset), &tmpx, &tmpy, "mp title");
	x = term->xmax  / 2 + tmpx;
	y = term->ymax - term->v_char + tmpy;;

	ignore_enhanced(mp_layout.title.noenhanced);
	apply_pm3dcolor(&(mp_layout.title.textcolor), term);
	write_multiline(x, y, mp_layout.title.text,
			CENTRE, JUST_TOP, 0, mp_layout.title.font);
	reset_textcolor(&(mp_layout.title.textcolor), term);
	ignore_enhanced(FALSE);

	/* Calculate fractional height of title compared to entire page */
	/* If it would fill the whole page, forget it! */
	for (y=1; *p; p++)
	    if (*p == '\n')
		y++;

	/* Oct 2012 - v_char depends on the font used */
	if (mp_layout.title.font && *mp_layout.title.font)
	    term->set_font(mp_layout.title.font);
	mp_layout.title_height = (double)(y * term->v_char) / (double)term->ymax;
	if (mp_layout.title.font && *mp_layout.title.font)
	    term->set_font("");

	if (mp_layout.title_height > 0.9)
	    mp_layout.title_height = 0.05;
    } else {
	mp_layout.title_height = 0.0;
    }

    if (mp_layout.auto_layout_margins)
	mp_layout_margins_and_spacing();
    else
	mp_layout_size_and_offset();
}

void
multiplot_end()
{
    multiplot = FALSE;
    fill_gpval_integer("GPVAL_MULTIPLOT", 0);
    /* reset plot size, origin and margins to values before 'set
       multiplot layout' */
    if (mp_layout.auto_layout) {
	xsize = mp_layout.prev_xsize;
	ysize = mp_layout.prev_ysize;
	xoffset = mp_layout.prev_xoffset;
	yoffset = mp_layout.prev_yoffset;

	lmargin = mp_layout.prev_lmargin;
	rmargin = mp_layout.prev_rmargin;
	bmargin = mp_layout.prev_bmargin;
	tmargin = mp_layout.prev_tmargin;
    }
    /* reset automatic multiplot layout */
    mp_layout.auto_layout = FALSE;
    mp_layout.auto_layout_margins = FALSE;
    mp_layout.xscale = mp_layout.yscale = 1.0;
    mp_layout.xoffset = mp_layout.yoffset = 0.0;
    mp_layout.lmargin.scalex = mp_layout.rmargin.scalex = screen;
    mp_layout.bmargin.scalex = mp_layout.tmargin.scalex = screen;
    mp_layout.lmargin.x = mp_layout.rmargin.x = mp_layout.bmargin.x = mp_layout.tmargin.x = -1;
    mp_layout.xspacing.scalex = mp_layout.yspacing.scalex = screen;
    mp_layout.xspacing.x = mp_layout.yspacing.x = -1;

    if (mp_layout.title.text) {
	free(mp_layout.title.text);
	mp_layout.title.text = NULL;
    }
}

/* Helper function for multiplot auto layout to issue size and offset cmds */
static void
mp_layout_size_and_offset(void)
{
    if (!mp_layout.auto_layout) return;

    /* fprintf(stderr,"col==%d row==%d\n",mp_layout.act_col,mp_layout.act_row); */
    /* the 'set size' command */
    xsize = mp_layout.xscale / mp_layout.num_cols;
    ysize = mp_layout.yscale / mp_layout.num_rows;

    /* the 'set origin' command */
    xoffset = (double)(mp_layout.act_col) / mp_layout.num_cols;
    if (mp_layout.downwards)
	yoffset = 1.0 - (double)(mp_layout.act_row+1) / mp_layout.num_rows;
    else
	yoffset = (double)(mp_layout.act_row) / mp_layout.num_rows;
    /* fprintf(stderr,"xoffset==%g  yoffset==%g\n", xoffset,yoffset); */

    /* Allow a little space at the top for a title */
    if (mp_layout.title.text) {
	ysize *= (1.0 - mp_layout.title_height);
	yoffset *= (1.0 - mp_layout.title_height);
    }

    /* corrected for x/y-scaling factors and user defined offsets */
    xoffset -= (mp_layout.xscale-1)/(2*mp_layout.num_cols);
    yoffset -= (mp_layout.yscale-1)/(2*mp_layout.num_rows);
    /* fprintf(stderr,"  xoffset==%g  yoffset==%g\n", xoffset,yoffset); */
    xoffset += mp_layout.xoffset;
    yoffset += mp_layout.yoffset;
    /* fprintf(stderr,"  xoffset==%g  yoffset==%g\n", xoffset,yoffset); */
}

/* Helper function for multiplot auto layout to set the explicit plot margins, 
   if requested with 'margins' and 'spacing' options. */
static void
mp_layout_margins_and_spacing(void)
{
    /* width and height of a single sub plot. */
    double tmp_width, tmp_height;
    double leftmargin, rightmargin, topmargin, bottommargin, xspacing, yspacing;

    if (!mp_layout.auto_layout_margins) return;

    if (mp_layout.lmargin.scalex == screen)
	leftmargin = mp_layout.lmargin.x;
    else
	leftmargin = (mp_layout.lmargin.x * term->h_char) / term->xmax;

    if (mp_layout.rmargin.scalex == screen)
	rightmargin = mp_layout.rmargin.x;
    else
	rightmargin = 1 - (mp_layout.rmargin.x * term->h_char) / term->xmax;

    if (mp_layout.tmargin.scalex == screen)
	topmargin = mp_layout.tmargin.x;
    else
	topmargin = 1 - (mp_layout.tmargin.x * term->v_char) / term->ymax;

    if (mp_layout.bmargin.scalex == screen)
	bottommargin = mp_layout.bmargin.x;
    else
	bottommargin = (mp_layout.bmargin.x * term->v_char) / term->ymax;

    if (mp_layout.xspacing.scalex == screen)
	xspacing = mp_layout.xspacing.x;
    else
	xspacing = (mp_layout.xspacing.x * term->h_char) / term->xmax;

    if (mp_layout.yspacing.scalex == screen)
	yspacing = mp_layout.yspacing.x;
    else
	yspacing = (mp_layout.yspacing.x * term->v_char) / term->ymax;

    tmp_width = (rightmargin - leftmargin - (mp_layout.num_cols - 1) * xspacing) 
	        / mp_layout.num_cols;
    tmp_height = (topmargin - bottommargin - (mp_layout.num_rows - 1) * yspacing) 
	        / mp_layout.num_rows;

    lmargin.x = leftmargin + mp_layout.act_col * (tmp_width + xspacing);
    lmargin.scalex = screen;
    rmargin.x = lmargin.x + tmp_width;
    rmargin.scalex = screen;

    if (mp_layout.downwards) {
	bmargin.x = bottommargin + (mp_layout.num_rows - mp_layout.act_row - 1) 
	            * (tmp_height + yspacing);
    } else {
	bmargin.x = bottommargin + mp_layout.act_row * (tmp_height + yspacing);
    }
    bmargin.scalex = screen;
    tmargin.x = bmargin.x + tmp_height;
    tmargin.scalex = screen;
}

static void
mp_layout_set_margin_or_spacing(t_position *margin)
{
    margin->x = -1;

    if (END_OF_COMMAND)
	return;

    if (almost_equals(c_token, "sc$reen")) {
	margin->scalex = screen;
	c_token++;
    } else if (almost_equals(c_token, "char$acter")) {
	margin->scalex = character;
	c_token++;
    }

    margin->x = real_expression();
    if (margin->x < 0)
	margin->x = -1;

    if (margin->scalex == screen) {
	if (margin->x < 0)
	    margin->x = 0;
	if (margin->x > 1)
	    margin->x = 1;
    }
}

