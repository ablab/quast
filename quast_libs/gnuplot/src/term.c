#ifndef lint
static char *RCSid() { return RCSid("$Id: term.c,v 1.296.2.22 2016/04/15 18:00:40 sfeam Exp $"); }
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


 /* This module is responsible for looking after the terminal
  * drivers at the lowest level. Only this module (should)
  * know about all the various rules about interpreting
  * the terminal capabilities exported by the terminal
  * drivers in the table.
  *
  * Note that, as far as this module is concerned, a
  * terminal session lasts only until _either_ terminal
  * or output file changes. Before either is changed,
  * the terminal is shut down.
  *
  * Entry points : (see also term/README)
  *
  * term_set_output() : called when  set output  invoked
  *
  * term_initialise()  : optional. Prepare the terminal for first
  *                use. It protects itself against subsequent calls.
  *
  * term_start_plot() : called at start of graph output. Calls term_init
  *                     if necessary
  *
  * term_apply_lp_properties() : apply linewidth settings
  *
  * term_end_plot() : called at the end of a plot
  *
  * term_reset() : called during int_error handling, to shut
  *                terminal down cleanly
  *
  * term_start_multiplot() : called by   set multiplot
  *
  * term_end_multiplot() : called by  set nomultiplot
  *
  * term_check_multiplot_okay() : called just before an interactive
  *                        prompt is issued while in multiplot mode,
  *                        to allow terminal to suspend if necessary,
  *                        Raises an error if interactive multiplot
  *                       is not supported.
  */

#include "term_api.h"

#include "alloc.h"
#include "axis.h"
#ifndef NO_BITMAP_SUPPORT
#include "bitmap.h"
#endif
#include "command.h"
#include "driver.h"
#include "graphics.h"
#include "help.h"
#include "plot.h"
#include "tables.h"
#include "getcolor.h"
#include "term.h"
#include "util.h"
#include "version.h"
#include "misc.h"
#include "multiplot.h"

#ifdef USE_MOUSE
#include "mouse.h"
#else
/* Some terminals (svg canvas) can provide mousing information */
/* even if the interactive gnuplot session itself cannot.      */
long mouse_mode = 0;
char* mouse_alt_string = NULL;
#endif

#ifdef WIN32
/* FIXME: Prototypes are in win/wcommon.h */
FILE *open_printer __PROTO((void));     /* in wprinter.c */
void close_printer __PROTO((FILE * outfile));
# include "win/winmain.h"
# ifdef __MSC__
#  include <malloc.h>
#  include <io.h>
# else
#  include <alloc.h>
# endif                         /* MSC */
#endif /* _Windows */

static int termcomp __PROTO((const generic * a, const generic * b));

/* Externally visible variables */
/* the central instance: the current terminal's interface structure */
struct termentry *term = NULL;  /* unknown */

/* ... and its options string */
char term_options[MAX_LINE_LEN+1] = "";

/* the 'output' file name and handle */
char *outstr = NULL;            /* means "STDOUT" */
FILE *gpoutfile;

/* Output file where the PostScript output goes to. See term_api.h for more
   details.
*/
FILE *gppsfile = 0;
char *PS_psdir = NULL;

/* true if terminal has been initialized */
TBOOLEAN term_initialised;

/* The qt and wxt terminals cannot be used in the same session. */
/* Whichever one is used first to plot, this locks out the other. */
void *term_interlock = NULL;

/* true if "set monochrome" */
TBOOLEAN monochrome = FALSE;

/* true if in multiplot mode */
TBOOLEAN multiplot = FALSE;

/* text output encoding, for terminals that support it */
enum set_encoding_id encoding;
/* table of encoding names, for output of the setting */
const char *encoding_names[] = {
    "default", "iso_8859_1", "iso_8859_2", "iso_8859_9", "iso_8859_15",
    "cp437", "cp850", "cp852", "cp950", "cp1250", "cp1251", "cp1252", "cp1254",
    "koi8r", "koi8u", "sjis", "utf8", NULL };
/* 'set encoding' options */
const struct gen_table set_encoding_tbl[] =
{
    { "def$ault", S_ENC_DEFAULT },
    { "utf$8", S_ENC_UTF8 },
    { "iso$_8859_1", S_ENC_ISO8859_1 },
    { "iso_8859_2", S_ENC_ISO8859_2 },
    { "iso_8859_9", S_ENC_ISO8859_9 },
    { "iso_8859_15", S_ENC_ISO8859_15 },
    { "cp4$37", S_ENC_CP437 },
    { "cp850", S_ENC_CP850 },
    { "cp852", S_ENC_CP852 },
    { "cp950", S_ENC_CP950 },
    { "cp1250", S_ENC_CP1250 },
    { "cp1251", S_ENC_CP1251 },
    { "cp1252", S_ENC_CP1252 },
    { "cp1254", S_ENC_CP1254 },
    { "koi8$r", S_ENC_KOI8_R },
    { "koi8$u", S_ENC_KOI8_U },
    { "sj$is", S_ENC_SJIS },
    { NULL, S_ENC_INVALID }
};

const char *arrow_head_names[4] =
    {"nohead", "head", "backhead", "heads"};

enum { IPC_BACK_UNUSABLE = -2, IPC_BACK_CLOSED = -1 };
#ifdef PIPE_IPC
/* HBB 20020225: currently not used anywhere outside term.c */
static SELECT_TYPE_ARG1 ipc_back_fd = IPC_BACK_CLOSED;
#endif

/* resolution in dpi for converting pixels to size units */
int gp_resolution = 72;

/* Support for enhanced text mode. Declared extern in term_api.h */
char  enhanced_text[MAX_LINE_LEN+1] = "";
char *enhanced_cur_text = NULL;
double enhanced_fontscale = 1.0;
char enhanced_escape_format[16] = "";
double enhanced_max_height = 0.0, enhanced_min_height = 0.0;
/* flag variable to disable enhanced output of filenames, mainly. */
TBOOLEAN ignore_enhanced_text = FALSE;

/* Recycle count for user-defined linetypes */
int linetype_recycle_count = 0;
int mono_recycle_count = 0;


/* Internal variables */

/* true if terminal is in graphics mode */
static TBOOLEAN term_graphics = FALSE;

/* we have suspended the driver, in multiplot mode */
static TBOOLEAN term_suspended = FALSE;

/* true if? */
static TBOOLEAN opened_binary = FALSE;

/* true if require terminal to be initialized */
static TBOOLEAN term_force_init = FALSE;

/* internal pointsize for do_point */
static double term_pointsize=1;

/* Internal prototypes: */

static void term_suspend __PROTO((void));
static void term_close_output __PROTO((void));

static void null_linewidth __PROTO((double));
static void do_point __PROTO((unsigned int x, unsigned int y, int number));
static void do_pointsize __PROTO((double size));
static void line_and_point __PROTO((unsigned int x, unsigned int y, int number));
static void do_arrow __PROTO((unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey, int head));
static void null_dashtype __PROTO((int type, t_dashtype *custom_dash_pattern));

static int null_text_angle __PROTO((int ang));
static int null_justify_text __PROTO((enum JUSTIFY just));
static int null_scale __PROTO((double x, double y));
static void null_layer __PROTO((t_termlayer layer));
static int null_set_font __PROTO((const char *font));
static void null_set_color __PROTO((struct t_colorspec *colorspec));
static void options_null __PROTO((void));
static void graphics_null __PROTO((void));
static void UNKNOWN_null __PROTO((void));
static void MOVE_null __PROTO((unsigned int, unsigned int));
static void LINETYPE_null __PROTO((int));
static void PUTTEXT_null __PROTO((unsigned int, unsigned int, const char *));

static int strlen_tex __PROTO((const char *));

static char *stylefont __PROTO((const char *fontname, TBOOLEAN isbold, TBOOLEAN isitalic));

/* Used by terminals and by shared routine parse_term_size() */
typedef enum {
    PIXELS,
    INCHES,
    CM
} size_units;
static size_units parse_term_size __PROTO((float *xsize, float *ysize, size_units def_units));


#ifdef VMS
char *vms_init();
void vms_reset();
void term_mode_tek();
void term_mode_native();
void term_pasthru();
void term_nopasthru();
void fflush_binary();
# define FOPEN_BINARY(file) fopen(file, "wb", "rfm=fix", "bls=512", "mrs=512")
#else /* !VMS */
# define FOPEN_BINARY(file) fopen(file, "wb")
#endif /* !VMS */

#if defined(MSDOS) || defined(WIN32)
# if defined(__DJGPP__)
#  include <io.h>
# endif
# include <fcntl.h>
# ifndef O_BINARY
#  ifdef _O_BINARY
#   define O_BINARY _O_BINARY
#  else
#   define O_BINARY O_BINARY_is_not_defined
#  endif
# endif
#endif

#ifdef __EMX__
#include <io.h>
#include <fcntl.h>
#endif

#if defined(__WATCOMC__) || defined(__MSC__)
# include <io.h>	/* for setmode() */
#endif

#define NICE_LINE               0
#define POINT_TYPES             6

#ifndef DEFAULTTERM
# define DEFAULTTERM NULL
#endif

/* interface to the rest of gnuplot - the rules are getting
 * too complex for the rest of gnuplot to be allowed in
 */

#if defined(PIPES)
static TBOOLEAN output_pipe_open = FALSE;
#endif /* PIPES */

static void
term_close_output()
{
    FPRINTF((stderr, "term_close_output\n"));

    opened_binary = FALSE;

    if (!outstr)                /* ie using stdout */
	return;

#if defined(PIPES)
    if (output_pipe_open) {
	(void) pclose(gpoutfile);
	output_pipe_open = FALSE;
    } else
#endif /* PIPES */
#ifdef _Windows
    if (stricmp(outstr, "PRN") == 0)
	close_printer(gpoutfile);
    else
#endif
    if (gpoutfile != gppsfile)
	fclose(gpoutfile);

    gpoutfile = stdout;         /* Don't dup... */
    free(outstr);
    outstr = NULL;

    if (gppsfile)
	fclose(gppsfile);
    gppsfile = NULL;
}

#ifdef OS2
# define POPEN_MODE ("wb")
#else
# define POPEN_MODE ("w")
#endif

/* assigns dest to outstr, so it must be allocated or NULL
 * and it must not be outstr itself !
 */
void
term_set_output(char *dest)
{
    FILE *f = NULL;

    FPRINTF((stderr, "term_set_output\n"));
    assert(dest == NULL || dest != outstr);

    if (multiplot) {
	fputs("In multiplot mode you can't change the output\n", stderr);
	return;
    }
    if (term && term_initialised) {
	(*term->reset) ();
	term_initialised = FALSE;
	/* switch off output to special postscript file (if used) */
	gppsfile = NULL;
    }
    if (dest == NULL) {         /* stdout */
	term_close_output();
    } else {

#if defined(PIPES)
	if (*dest == '|') {
	    restrict_popen();
#ifdef _Windows
	    if (term && (term->flags & TERM_BINARY))
		f = popen(dest + 1, "wb");
	    else
		f = popen(dest + 1, "w");
#else
	    f = popen(dest + 1, "w");
#endif
	    if (f == (FILE *) NULL)
		os_error(c_token, "cannot create pipe; output not changed");
	    else
		output_pipe_open = TRUE;
	} else {
#endif /* PIPES */

#ifdef _Windows
	if (outstr && stricmp(outstr, "PRN") == 0) {
	    /* we can't call open_printer() while printer is open, so */
	    close_printer(gpoutfile);   /* close printer immediately if open */
	    gpoutfile = stdout; /* and reset output to stdout */
	    free(outstr);
	    outstr = NULL;
	}
	if (stricmp(dest, "PRN") == 0) {
	    if ((f = open_printer()) == (FILE *) NULL)
		os_error(c_token, "cannot open printer temporary file; output may have changed");
	} else
#endif

	{
#if defined (MSDOS)
	    if (outstr && (0 == stricmp(outstr, dest))) {
		/* On MSDOS, you cannot open the same file twice and
		 * then close the first-opened one and keep the second
		 * open, it seems. If you do, you get lost clusters
		 * (connection to the first version of the file is
		 * lost, it seems). */
		/* FIXME: this is not yet safe enough. You can fool it by
		 * specifying the same output file in two different ways
		 * (relative vs. absolute path to file, e.g.) */
		term_close_output();
	    }
#endif
	    if (term && (term->flags & TERM_BINARY))
		f = FOPEN_BINARY(dest);
	    else
		f = fopen(dest, "w");

	    if (f == (FILE *) NULL)
		os_error(c_token, "cannot open file; output not changed");
	}
#if defined(PIPES)
	}
#endif

	term_close_output();
	gpoutfile = f;
	outstr = dest;
	opened_binary = (term && (term->flags & TERM_BINARY));
    }
}

void
term_initialise()
{
    FPRINTF((stderr, "term_initialise()\n"));

    if (!term)
	int_error(NO_CARET, "No terminal defined");

    /* check if we have opened the output file in the wrong mode
     * (text/binary), if set term comes after set output
     * This was originally done in change_term, but that
     * resulted in output files being truncated
     */

    if (outstr && (term->flags & TERM_NO_OUTPUTFILE)) {
	if (interactive)
	    fprintf(stderr,"Closing %s\n",outstr);
	term_close_output();
    }

    if (outstr &&
	(((term->flags & TERM_BINARY) && !opened_binary) ||
	 ((!(term->flags & TERM_BINARY) && opened_binary)))) {
	/* this is nasty - we cannot just term_set_output(outstr)
	 * since term_set_output will first free outstr and we
	 * end up with an invalid pointer. I think I would
	 * prefer to defer opening output file until first plot.
	 */
	char *temp = gp_alloc(strlen(outstr) + 1, "temp file string");
	if (temp) {
	    FPRINTF((stderr, "term_initialise: reopening \"%s\" as %s\n",
		     outstr, term->flags & TERM_BINARY ? "binary" : "text"));
	    strcpy(temp, outstr);
	    term_set_output(temp);      /* will free outstr */
	    if (temp != outstr) {
		if (temp)
		    free(temp);
		temp = outstr;
	    }
	} else
	    fputs("Cannot reopen output file in binary", stderr);
	/* and carry on, hoping for the best ! */
    }
#if defined(MSDOS) || defined (_Windows) || defined(OS2)
# ifdef _Windows
    else if (!outstr && (term->flags & TERM_BINARY))
# else
    else if (!outstr && !interactive && (term->flags & TERM_BINARY))
# endif
	{
	    /* binary to stdout in non-interactive session... */
	    fflush(stdout);
	    setmode(fileno(stdout), O_BINARY);
	}
#endif


    if (!term_initialised || term_force_init) {
	FPRINTF((stderr, "- calling term->init()\n"));
	(*term->init) ();
	term_initialised = TRUE;
    }
}


void
term_start_plot()
{
    FPRINTF((stderr, "term_start_plot()\n"));

    if (!term_initialised)
	term_initialise();

    if (!term_graphics) {
	FPRINTF((stderr, "- calling term->graphics()\n"));
	(*term->graphics) ();
	term_graphics = TRUE;
    } else if (multiplot && term_suspended) {
	if (term->resume) {
	    FPRINTF((stderr, "- calling term->resume()\n"));
	    (*term->resume) ();
	}
	term_suspended = FALSE;
    }

    /* Sync point for epslatex text positioning */
    (*term->layer)(TERM_LAYER_RESET);

    /* Because PostScript plots may be viewed out of order, make sure */
    /* Each new plot makes no assumption about the previous palette.  */
    if (term->flags & TERM_IS_POSTSCRIPT)
	invalidate_palette();

    /* Set canvas size to full range of current terminal coordinates */
	canvas.xleft  = 0;
	canvas.xright = term->xmax - 1;
	canvas.ybot   = 0;
	canvas.ytop   = term->ymax - 1;

}

void
term_end_plot()
{
    FPRINTF((stderr, "term_end_plot()\n"));

    if (!term_initialised)
	return;

    /* Sync point for epslatex text positioning */
    (*term->layer)(TERM_LAYER_END_TEXT);

    if (!multiplot) {
	FPRINTF((stderr, "- calling term->text()\n"));
	(*term->text) ();
	term_graphics = FALSE;
    } else {
	multiplot_next();
    }
#ifdef VMS
    if (opened_binary)
	fflush_binary();
    else
#endif /* VMS */

	(void) fflush(gpoutfile);

#ifdef USE_MOUSE
    recalc_statusline();
    update_ruler();
#endif
}


static void
term_suspend()
{
    FPRINTF((stderr, "term_suspend()\n"));
    if (term_initialised && !term_suspended && term->suspend) {
	FPRINTF((stderr, "- calling term->suspend()\n"));
	(*term->suspend) ();
	term_suspended = TRUE;
    }
}

void
term_reset()
{
    FPRINTF((stderr, "term_reset()\n"));

#ifdef USE_MOUSE
    /* Make sure that ^C will break out of a wait for 'pause mouse' */
    paused_for_mouse = 0;
#ifdef WIN32
    kill_pending_Pause_dialog();
#endif
#endif

    if (!term_initialised)
	return;

    if (term_suspended) {
	if (term->resume) {
	    FPRINTF((stderr, "- calling term->resume()\n"));
	    (*term->resume) ();
	}
	term_suspended = FALSE;
    }
    if (term_graphics) {
	(*term->text) ();
	term_graphics = FALSE;
    }
    if (term_initialised) {
	(*term->reset) ();
	term_initialised = FALSE;
	/* switch off output to special postscript file (if used) */
	gppsfile = NULL;
    }
}

void
term_apply_lp_properties(struct lp_style_type *lp)
{
    /*  This function passes all the line and point properties to the
     *  terminal driver and issues the corresponding commands.
     *
     *  Alas, sometimes it might be necessary to give some help to
     *  this function by explicitly issuing additional '(*term)(...)'
     *  commands.
     */
    int lt = lp->l_type;
    int dt = lp->d_type;
    t_dashtype custom_dash_pattern = lp->custom_dash_pattern;
    t_colorspec colorspec = lp->pm3d_color;

    if ((lp->flags & LP_SHOW_POINTS)) {
	/* change points, too
	 * Currently, there is no 'pointtype' function.  For points
	 * there is a special function also dealing with (x,y) co-
	 * ordinates.
	 */
	if (lp->p_size < 0)
	    (*term->pointsize) (pointsize);
	else
	    (*term->pointsize) (lp->p_size);
    }
    /*  _first_ set the line width, _then_ set the line type !
     *  The linetype might depend on the linewidth in some terminals.
     */
    (*term->linewidth) (lp->l_width);

    /* The paradigm for handling linetype and dashtype in version 5 is */
    /* linetype < 0 (e.g. LT_BACKGROUND, LT_NODRAW) means some special */
    /* category that will be handled directly by term->linetype().     */
    /* linetype > 0 is now redundant. It used to encode both a color   */
    /* and a dash pattern.  Now we have separate mechanisms for those. */ 
    if (LT_COLORFROMCOLUMN < lt && lt < 0)
	(*term->linetype) (lt);
    else if (term->set_color == null_set_color) {
	(*term->linetype) (lt-1);
	return;
    } else /* All normal lines will be solid unless a dashtype is given */
	(*term->linetype) (LT_SOLID);

    /* Apply dashtype or user-specified dash pattern, which may override  */
    /* the terminal-specific dot/dash pattern belonging to this linetype. */
    if (lt == LT_AXIS)
	; /* LT_AXIS is a special linetype that may incorporate a dash pattern */
    else if (dt == DASHTYPE_CUSTOM)
	(*term->dashtype) (dt, &custom_dash_pattern);
    else if (dt == DASHTYPE_SOLID)
	(*term->dashtype) (dt, NULL);
    else if (dt >= 0)
	/* The null_dashtype() routine or a version 5 terminal's private  */
	/* dashtype routine converts this into a call to term->linetype() */
	/* yielding the same result as in version 4 except possibly for a */
	/* different line width.					  */
	(*term->dashtype) (dt, NULL);

    /* Finally adjust the color of the line */
    apply_pm3dcolor(&colorspec, term);
}

void
term_start_multiplot()
{
    FPRINTF((stderr, "term_start_multiplot()\n"));
    multiplot_start();
#ifdef USE_MOUSE
    UpdateStatusline();
#endif
}

void
term_end_multiplot()
{
    FPRINTF((stderr, "term_end_multiplot()\n"));
    if (!multiplot)
	return;

    if (term_suspended) {
	if (term->resume)
	    (*term->resume) ();
	term_suspended = FALSE;
    }

    multiplot_end();

    term_end_plot();
#ifdef USE_MOUSE
    UpdateStatusline();
#endif
}

void
term_check_multiplot_okay(TBOOLEAN f_interactive)
{
    FPRINTF((stderr, "term_multiplot_okay(%d)\n", f_interactive));

    if (!term_initialised)
	return;                 /* they've not started yet */

    /* make sure that it is safe to issue an interactive prompt
     * it is safe if
     *   it is not an interactive read, or
     *   the terminal supports interactive multiplot, or
     *   we are not writing to stdout and terminal doesn't
     *     refuse multiplot outright
     */
    if (!f_interactive || (term->flags & TERM_CAN_MULTIPLOT) ||
	((gpoutfile != stdout) && !(term->flags & TERM_CANNOT_MULTIPLOT))
	) {
	/* it's okay to use multiplot here, but suspend first */
	term_suspend();
	return;
    }
    /* uh oh: they're not allowed to be in multiplot here */

    term_end_multiplot();

    /* at this point we know that it is interactive and that the
     * terminal can either only do multiplot when writing to
     * to a file, or it does not do multiplot at all
     */

    if (term->flags & TERM_CANNOT_MULTIPLOT)
	int_error(NO_CARET, "This terminal does not support multiplot");
    else
	int_error(NO_CARET, "Must set output to a file or put all multiplot commands on one input line");
}


void
write_multiline(
    unsigned int x, unsigned int y,
    char *text,
    JUSTIFY hor,                /* horizontal ... */
    VERT_JUSTIFY vert,          /* ... and vertical just - text in hor direction despite angle */
    int angle,                  /* assume term has already been set for this */
    const char *font)           /* NULL or "" means use default */
{
    struct termentry *t = term;
    char *p = text;

    if (!p)
	return;

    /* EAM 9-Feb-2003 - Set font before calculating sizes */
    if (font && *font)
	(*t->set_font) (font);

    if (vert != JUST_TOP) {
	/* count lines and adjust y */
	int lines = 0;          /* number of linefeeds - one fewer than lines */
	while (*p) {
	    if (*p++ == '\n')
		++lines;
	}
	if (angle)
	    x -= (vert * lines * t->v_char) / 2;
	else
	    y += (vert * lines * t->v_char) / 2;
    }

    for (;;) {                  /* we will explicitly break out */

	if ((text != NULL) && (p = strchr(text, '\n')) != NULL)
	    *p = 0;             /* terminate the string */

	if ((*t->justify_text) (hor)) {
	    if (on_page(x, y))
		(*t->put_text) (x, y, text);
	} else {
	    int len = estimate_strlen(text);
	    int hfix, vfix;

	    if (angle == 0) {
		hfix = hor * t->h_char * len / 2;
		vfix = 0;
	    } else {
		/* Attention: This relies on the numeric values of enum JUSTIFY! */
		hfix = hor * t->h_char * len * cos(angle * DEG2RAD) / 2 + 0.5;
		vfix = hor * t->v_char * len * sin(angle * DEG2RAD) / 2 + 0.5;
	    }
		if (on_page(x - hfix, y - vfix))
		    (*t->put_text) (x - hfix, y - vfix, text);
	}
	if (angle == 90 || angle == TEXT_VERTICAL)
	    x += t->v_char;
	else if (angle == -90 || angle == -TEXT_VERTICAL)
	    x -= t->v_char;
	else
	    y -= t->v_char;

	if (!p)
	    break;
	else {
	    /* put it back */
	    *p = '\n';
	}

	text = p + 1;
    }                           /* unconditional branch back to the for(;;) - just a goto ! */

    if (font && *font)
	(*t->set_font) ("");

}


static void
do_point(unsigned int x, unsigned int y, int number)
{
    int htic, vtic;
    struct termentry *t = term;

    /* use solid lines for point symbols */
    if (term->dashtype != null_dashtype)
	term->dashtype(DASHTYPE_SOLID, NULL);

    if (number < 0) {           /* do dot */
	(*t->move) (x, y);
	(*t->vector) (x, y);
	return;
    }
    number %= POINT_TYPES;
    /* should be in term_tbl[] in later version */
    htic = (term_pointsize * t->h_tic / 2);
    vtic = (term_pointsize * t->v_tic / 2);

    /* point types 1..4 are same as in postscript, png and x11
       point types 5..6 are "similar"
       (note that (number) equals (pointtype-1)
    */
    switch (number) {
    case 4:                     /* do diamond */
	(*t->move) (x - htic, y);
	(*t->vector) (x, y - vtic);
	(*t->vector) (x + htic, y);
	(*t->vector) (x, y + vtic);
	(*t->vector) (x - htic, y);
	(*t->move) (x, y);
	(*t->vector) (x, y);
	break;
    case 0:                     /* do plus */
	(*t->move) (x - htic, y);
	(*t->vector) (x - htic, y);
	(*t->vector) (x + htic, y);
	(*t->move) (x, y - vtic);
	(*t->vector) (x, y - vtic);
	(*t->vector) (x, y + vtic);
	break;
    case 3:                     /* do box */
	(*t->move) (x - htic, y - vtic);
	(*t->vector) (x - htic, y - vtic);
	(*t->vector) (x + htic, y - vtic);
	(*t->vector) (x + htic, y + vtic);
	(*t->vector) (x - htic, y + vtic);
	(*t->vector) (x - htic, y - vtic);
	(*t->move) (x, y);
	(*t->vector) (x, y);
	break;
    case 1:                     /* do X */
	(*t->move) (x - htic, y - vtic);
	(*t->vector) (x - htic, y - vtic);
	(*t->vector) (x + htic, y + vtic);
	(*t->move) (x - htic, y + vtic);
	(*t->vector) (x - htic, y + vtic);
	(*t->vector) (x + htic, y - vtic);
	break;
    case 5:                     /* do triangle */
	(*t->move) (x, y + (4 * vtic / 3));
	(*t->vector) (x - (4 * htic / 3), y - (2 * vtic / 3));
	(*t->vector) (x + (4 * htic / 3), y - (2 * vtic / 3));
	(*t->vector) (x, y + (4 * vtic / 3));
	(*t->move) (x, y);
	(*t->vector) (x, y);
	break;
    case 2:                     /* do star */
	(*t->move) (x - htic, y);
	(*t->vector) (x - htic, y);
	(*t->vector) (x + htic, y);
	(*t->move) (x, y - vtic);
	(*t->vector) (x, y - vtic);
	(*t->vector) (x, y + vtic);
	(*t->move) (x - htic, y - vtic);
	(*t->vector) (x - htic, y - vtic);
	(*t->vector) (x + htic, y + vtic);
	(*t->move) (x - htic, y + vtic);
	(*t->vector) (x - htic, y + vtic);
	(*t->vector) (x + htic, y - vtic);
	break;
    }
}

static void
do_pointsize(double size)
{
    term_pointsize = (size >= 0 ? size : 1);
}


/*
 * general point routine
 */
static void
line_and_point(unsigned int x, unsigned int y, int number)
{
    /* temporary(?) kludge to allow terminals with bad linetypes
       to make nice marks */

    (*term->linetype) (NICE_LINE);
    do_point(x, y, number);
}

/*
 * general arrow routine
 *
 * I set the angle between the arrowhead and the line 15 degree.
 * The length of arrowhead varies depending on the line length
 * within the the range [0.3*(the-tic-length), 2*(the-tic-length)].
 * No head is printed if the arrow length is zero.
 *
 *            Yasu-hiro Yamazaki(hiro@rainbow.physics.utoronto.ca)
 *            Jul 1, 1993
 */

#define COS15 (0.96593)         /* cos of 15 degree */
#define SIN15 (0.25882)         /* sin of 15 degree */

#define HEAD_LONG_LIMIT  (2.0)  /* long  limit of arrowhead length */
#define HEAD_SHORT_LIMIT (0.3)  /* short limit of arrowhead length */
				/* their units are the "tic" length */

#define HEAD_COEFF  (0.3)       /* default value of head/line length ratio */

int curr_arrow_headlength; /* access head length + angle without changing API */
double curr_arrow_headangle;    /* angle in degrees */
double curr_arrow_headbackangle;  /* angle in degrees */
arrowheadfill curr_arrow_headfilled;      /* arrow head filled or not */
TBOOLEAN curr_arrow_headfixedsize;        /* Adapt the head size for short arrows or not */

static void
do_arrow(
    unsigned int usx, unsigned int usy,   /* start point */
    unsigned int uex, unsigned int uey,   /* end point (point of arrowhead) */
    int headstyle)
{
    /* Clipping and angle calculations do not work if coords are unsigned! */
    int sx = (int)usx;
    int sy = (int)usy;
    int ex = (int)uex;
    int ey = (int)uey;

    struct termentry *t = term;
    float len_tic = ((double) (t->h_tic + t->v_tic)) / 2.0;
    /* average of tic sizes */
    /* (dx,dy) : vector from end to start */
    double dx = sx - ex;
    double dy = sy - ey;
    double len_arrow = sqrt(dx * dx + dy * dy);
    gpiPoint head_points[5];
    int xm = 0, ym = 0;
    BoundingBox *clip_save;
    t_arrow_head head = (t_arrow_head)((headstyle < 0) ? -headstyle : headstyle);
	/* negative headstyle means draw heads only, no shaft */

    /* The arrow shaft was clipped already in do_clip_arrow() but we still */
    /* need to clip the head here. */
    clip_save = clip_area;
    if (term->flags & TERM_CAN_CLIP)
	clip_area = NULL;
    else
	clip_area = &canvas;

    /* Calculate and draw arrow heads.
     * Draw no head for arrows with length = 0, or, to be more specific,
     * length < DBL_EPSILON, because len_arrow will almost always be != 0.
     */
    if ((head != NOHEAD) && fabs(len_arrow) >= DBL_EPSILON) {
	int x1, y1, x2, y2;
	if (curr_arrow_headlength <= 0) {
	    /* An arrow head with the default size and angles */
	    double coeff_shortest = len_tic * HEAD_SHORT_LIMIT / len_arrow;
	    double coeff_longest = len_tic * HEAD_LONG_LIMIT / len_arrow;
	    double head_coeff = GPMAX(coeff_shortest,
				      GPMIN(HEAD_COEFF, coeff_longest));
	    /* we put the arrowhead marks at 15 degrees to line */
	    x1 = (int) ((COS15 * dx - SIN15 * dy) * head_coeff);
	    y1 = (int) ((SIN15 * dx + COS15 * dy) * head_coeff);
	    x2 = (int) ((COS15 * dx + SIN15 * dy) * head_coeff);
	    y2 = (int) ((-SIN15 * dx + COS15 * dy) * head_coeff);
	    /* backangle defaults to 90 deg */
	    xm = (int) ((x1 + x2)/2);
	    ym = (int) ((y1 + y2)/2);
	} else {
	    /* An arrow head with the length + angle specified explicitly.	*/
	    /* Assume that if the arrow is shorter than the arrowhead, this is	*/
	    /* because of foreshortening in a 3D plot.                  	*/
	    double alpha = curr_arrow_headangle * DEG2RAD;
	    double beta = curr_arrow_headbackangle * DEG2RAD;
	    double phi = atan2(-dy,-dx); /* azimuthal angle of the vector */
	    double backlen, effective_length;
	    double dx2, dy2;

	    effective_length = curr_arrow_headlength;
	    if (!curr_arrow_headfixedsize && (curr_arrow_headlength > len_arrow/2.)) {
		effective_length = len_arrow/2.;
		alpha = atan(tan(alpha)*((double)curr_arrow_headlength/effective_length));
		beta = atan(tan(beta)*((double)curr_arrow_headlength/effective_length));
	    }
	    backlen = sin(alpha) / sin(beta);

	    /* anticlock-wise head segment */
	    x1 = -(int)(effective_length * cos( alpha - phi ));
	    y1 =  (int)(effective_length * sin( alpha - phi ));
	    /* clock-wise head segment */
	    dx2 = -effective_length * cos( phi + alpha );
	    dy2 = -effective_length * sin( phi + alpha );
	    x2 = (int) (dx2);
	    y2 = (int) (dy2);
	    /* back point */
	    xm = (int) (dx2 + backlen*effective_length * cos( phi + beta ));
	    ym = (int) (dy2 + backlen*effective_length * sin( phi + beta ));
	}

	if ((head & END_HEAD) && !clip_point(ex, ey)) {
	    head_points[0].x = ex + xm;
	    head_points[0].y = ey + ym;
	    head_points[1].x = ex + x1;
	    head_points[1].y = ey + y1;
	    head_points[2].x = ex;
	    head_points[2].y = ey;
	    head_points[3].x = ex + x2;
	    head_points[3].y = ey + y2;
	    head_points[4].x = ex + xm;
	    head_points[4].y = ey + ym;
	    if (curr_arrow_headfilled >= AS_FILLED) {
		/* draw filled forward arrow head */
		head_points->style = FS_OPAQUE;
		if (t->filled_polygon)
		    (*t->filled_polygon) (5, head_points);
	    }
	    /* draw outline of forward arrow head */
	    if (curr_arrow_headfilled == AS_NOFILL) {
		draw_clip_polygon(3, head_points+1);
	    } else if (curr_arrow_headfilled != AS_NOBORDER) {
		draw_clip_polygon(5, head_points);
	    }
	}

	/* backward arrow head */
	if ((head & BACKHEAD) && !clip_point(sx,sy)) {
	    head_points[0].x = sx - xm;
	    head_points[0].y = sy - ym;
	    head_points[1].x = sx - x1;
	    head_points[1].y = sy - y1;
	    head_points[2].x = sx;
	    head_points[2].y = sy;
	    head_points[3].x = sx - x2;
	    head_points[3].y = sy - y2;
	    head_points[4].x = sx - xm;
	    head_points[4].y = sy - ym;
	    if (curr_arrow_headfilled >= AS_FILLED) {
		/* draw filled backward arrow head */
		head_points->style = FS_OPAQUE;
		if (t->filled_polygon)
		    (*t->filled_polygon) (5, head_points);
	    }
	    /* draw outline of backward arrow head */
	    if (curr_arrow_headfilled == AS_NOFILL) {
		draw_clip_polygon(3, head_points+1);
	    } else if (curr_arrow_headfilled != AS_NOBORDER) {
		draw_clip_polygon(5, head_points);
	    }
	}
    }

    /* Draw the line for the arrow. */
    if (headstyle >= 0) {
	if ((head & BACKHEAD)
	&&  (fabs(len_arrow) >= DBL_EPSILON) && (curr_arrow_headfilled != AS_NOFILL) ) {
	    sx -= xm;
	    sy -= ym;
	}
	if ((head & END_HEAD)
	&&  (fabs(len_arrow) >= DBL_EPSILON) && (curr_arrow_headfilled != AS_NOFILL) ) {
	    ex += xm;
	    ey += ym;
	}
	draw_clip_line(sx, sy, ex, ey);
    }

    /* Restore previous clipping box */
    clip_area = clip_save;

}

#ifdef EAM_OBJECTS
/* Generic routine for drawing circles or circular arcs.          */
/* If this feature proves useful, we can add a new terminal entry */
/* point term->arc() to the API and let terminals either provide  */
/* a private implemenation or use this generic one.               */

void
do_arc(
    unsigned int cx, unsigned int cy, /* Center */
    double radius, /* Radius */
    double arc_start, double arc_end, /* Limits of arc in degress */
    int style, TBOOLEAN wedge)
{
    gpiPoint vertex[250];
    int i, segments;
    double aspect;
    TBOOLEAN complete_circle;

    /* Protect against out-of-range values */
    while (arc_start < 0)
	arc_start += 360.;
    while (arc_end > 360.)
	arc_end -= 360.;

    /* Always draw counterclockwise */
    while (arc_end < arc_start)
	arc_end += 360.;

    /* Choose how finely to divide this arc into segments */
    /* FIXME: INC=2 causes problems for gnuplot_x11 */
#   define INC 3.
    segments = (arc_end - arc_start) / INC;
    if (segments < 1)
	segments = 1;

    /* Calculate the vertices */
    aspect = (double)term->v_tic / (double)term->h_tic;
#ifdef WIN32
    if (strcmp(term->name, "windows") == 0)
	aspect = 1.;
#endif
    for (i=0; i<segments; i++) {
	vertex[i].x = cx + cos(DEG2RAD * (arc_start + i*INC)) * radius;
	vertex[i].y = cy + sin(DEG2RAD * (arc_start + i*INC)) * radius * aspect;
    }
#   undef INC
    vertex[segments].x = cx + cos(DEG2RAD * arc_end) * radius;
    vertex[segments].y = cy + sin(DEG2RAD * arc_end) * radius * aspect;

    if (fabs(arc_end - arc_start) > .1
    &&  fabs(arc_end - arc_start) < 359.9) {
	vertex[++segments].x = cx;
	vertex[segments].y = cy;
	vertex[++segments].x = vertex[0].x;
	vertex[segments].y = vertex[0].y;
	complete_circle = FALSE;
    } else
	complete_circle = TRUE;

    if (style) { /* Fill in the center */
	gpiPoint fillarea[250];
	int in;

	clip_polygon(vertex, fillarea, segments, &in);
	fillarea[0].style = style;
	if (term->filled_polygon)
	    term->filled_polygon(in, fillarea);

    } else { /* Draw the arc */
	if (!wedge && !complete_circle)
	    segments -= 2;
	draw_clip_polygon(segments+1, vertex);
    }
}
#endif /* EAM_OBJECTS */



#define TERM_PROTO
#define TERM_BODY
#define TERM_PUBLIC static

#include "term.h"

#undef TERM_PROTO
#undef TERM_BODY
#undef TERM_PUBLIC


/* Dummy functions for unavailable features */
/* return success if they asked for default - this simplifies code
 * where param is passed as a param. Client can first pass it here,
 * and only if it fails do they have to see what was trying to be done
 */

/* change angle of text.  0 is horizontal left to right.
   * 1 is vertical bottom to top (90 deg rotate)
 */
static int
null_text_angle(int ang)
{
    return (ang == 0);
}

/* change justification of text.
 * modes are LEFT (flush left), CENTRE (centred), RIGHT (flush right)
 */
static int
null_justify_text(enum JUSTIFY just)
{
    return (just == LEFT);
}


/* Change scale of plot.
 * Parameters are x,y scaling factors for this plot.
 * Some terminals (eg latex) need to do scaling themselves.
 */
static int
null_scale(double x, double y)
{
    (void) x;                   /* avoid -Wunused warning */
    (void) y;
    return FALSE;               /* can't be done */
}

static void
null_layer(t_termlayer layer)
{
    (void) layer;               /* avoid -Wunused warning */
}

static void
options_null()
{
    term_options[0] = '\0';     /* we have no options */
}

static void
graphics_null()
{
    fprintf(stderr,
	    "WARNING: Plotting with an 'unknown' terminal.\n"
	    "No output will be generated. Please select a terminal with 'set terminal'.\n");
}

static void
UNKNOWN_null()
{
}

static void
MOVE_null(unsigned int x, unsigned int y)
{
    (void) x;                   /* avoid -Wunused warning */
    (void) y;
}

static void
LINETYPE_null(int t)
{
    (void) t;                   /* avoid -Wunused warning */
}

static void
PUTTEXT_null(unsigned int x, unsigned int y, const char *s)
{
    (void) s;                   /* avoid -Wunused warning */
    (void) x;
    (void) y;
}


static void
null_linewidth(double s)
{
    (void) s;                   /* avoid -Wunused warning */
}

static int
null_set_font(const char *font)
{
    (void) font;		/* avoid -Wunused warning */
    return FALSE;		/* Never used!! */
}

static void
null_set_color(struct t_colorspec *colorspec)
{
    if (colorspec->type == TC_LT)
	term->linetype(colorspec->lt);
}

static void
null_dashtype(int type, t_dashtype *custom_dash_pattern)
{
    (void) custom_dash_pattern;	/* ignore */
    /*
     * If the terminal does not support user-defined dashtypes all we can do
     * do is fall through to the old (pre-v5) assumption that the dashtype,
     * if any, is part of the linetype.  We also assume that the color will
     * be adjusted after this.
     */
    if (type <= 0)
	type = LT_SOLID;
    term->linetype(type);
}

/* setup the magic macros to compile in the right parts of the
 * terminal drivers included by term.h
 */
#define TERM_TABLE
#define TERM_TABLE_START(x) ,{
#define TERM_TABLE_END(x)   }


/*
 * term_tbl[] contains an entry for each terminal.  "unknown" must be the
 *   first, since term is initialized to 0.
 */
static struct termentry term_tbl[] =
{
    {"unknown", "Unknown terminal type - not a plotting device",
     100, 100, 1, 1,
     1, 1, options_null, UNKNOWN_null, UNKNOWN_null,
     UNKNOWN_null, null_scale, graphics_null, MOVE_null, MOVE_null,
     LINETYPE_null, PUTTEXT_null}

#include "term.h"

};

#define TERMCOUNT (sizeof(term_tbl) / sizeof(term_tbl[0]))

void
list_terms()
{
    int i;
    char *line_buffer = gp_alloc(BUFSIZ, "list_terms");
    int sort_idxs[TERMCOUNT];

    /* sort terminal types alphabetically */
    for( i = 0; i < TERMCOUNT; i++ )
	sort_idxs[i] = i;
    qsort( sort_idxs, TERMCOUNT, sizeof(int), termcomp );
    /* now sort_idxs[] contains the sorted indices */

    StartOutput();
    strcpy(line_buffer, "\nAvailable terminal types:\n");
    OutLine(line_buffer);

    for (i = 0; i < TERMCOUNT; i++) {
	sprintf(line_buffer, "  %15s  %s\n",
		term_tbl[sort_idxs[i]].name,
		term_tbl[sort_idxs[i]].description);
	OutLine(line_buffer);
    }

    EndOutput();
    free(line_buffer);
}

/* Return string with all terminal names.
   Note: caller must free the returned names after use.
*/
char*
get_terminals_names()
{
    int i;
    char *buf = gp_alloc(TERMCOUNT*15, "all_term_names"); /* max 15 chars per name */
    char *names;
    int sort_idxs[TERMCOUNT];

    /* sort terminal types alphabetically */
    for( i = 0; i < TERMCOUNT; i++ )
	sort_idxs[i] = i;
    qsort( sort_idxs, TERMCOUNT, sizeof(int), termcomp );
    /* now sort_idxs[] contains the sorted indices */

    strcpy(buf, " "); /* let the string have leading and trailing " " in order to search via strstrt(GPVAL_TERMINALS, " png "); */
    for (i = 0; i < TERMCOUNT; i++)
	sprintf(buf+strlen(buf), "%s ", term_tbl[sort_idxs[i]].name);
    names = gp_alloc(strlen(buf)+1, "all_term_names2");
    strcpy(names, buf);
    free(buf);
    return names;
}

static int
termcomp(const generic *arga, const generic *argb)
{
    const int *a = arga;
    const int *b = argb;

    return( strcasecmp( term_tbl[*a].name, term_tbl[*b].name ) );
}

/* set_term: get terminal number from name on command line
 * will change 'term' variable if successful
 */
struct termentry *
set_term()
{
    struct termentry *t = NULL;
    char *input_name = NULL;

    if (!END_OF_COMMAND) {
	input_name = gp_input_line + token[c_token].start_index;
	t = change_term(input_name, token[c_token].length);
	if (!t && isstringvalue(c_token) && (input_name = try_to_get_string())) {
	    t = change_term(input_name, strlen(input_name));
	    free(input_name);
	} else {
	    c_token++;
	}
    }

    if (!t) {
	change_term("unknown", 7);
	int_error(c_token-1, "unknown or ambiguous terminal type; type just 'set terminal' for a list");
    }

    /* otherwise the type was changed */
    return (t);
}

/* change_term: get terminal number from name and set terminal type
 *
 * returns NULL for unknown or ambiguous, otherwise is terminal
 * driver pointer
 */
struct termentry *
change_term(const char *origname, int length)
{
    int i;
    struct termentry *t = NULL;
    TBOOLEAN ambiguous = FALSE;

    /* For backwards compatibility only */
    char *name = (char *)origname;
    if (!strncmp(origname,"X11",length)) {
	name = "x11";
	length = 3;
    }

#ifdef HAVE_CAIROPDF
    /* To allow "set term eps" as short for "set term epscairo" */
    if (!strncmp(origname,"eps",length)) {
	name = "epscairo";
	length = 8;
    }
#endif

    for (i = 0; i < TERMCOUNT; i++) {
	if (!strncmp(name, term_tbl[i].name, length)) {
	    if (t)
		ambiguous = TRUE;
	    t = term_tbl + i;
	    /* Exact match is always accepted */
	    if (length == strlen(term_tbl[i].name)) {
		ambiguous = FALSE;
		break;
	    }
	}
    }

    if (!t || ambiguous)
	return (NULL);

    /* Success: set terminal type now */

    term = t;
    term_initialised = FALSE;

    if (term->scale != null_scale)
	fputs("Warning: scale interface is not null_scale - may not work with multiplot\n", stderr);

    /* check that optional fields are initialised to something */
    if (term->text_angle == 0)
	term->text_angle = null_text_angle;
    if (term->justify_text == 0)
	term->justify_text = null_justify_text;
    if (term->point == 0)
	term->point = do_point;
    if (term->arrow == 0)
	term->arrow = do_arrow;
    if (term->pointsize == 0)
	term->pointsize = do_pointsize;
    if (term->linewidth == 0)
	term->linewidth = null_linewidth;
    if (term->layer == 0)
	term->layer = null_layer;
    if (term->tscale <= 0)
	term->tscale = 1.0;
    if (term->set_font == 0)
	term->set_font = null_set_font;
    if (term->set_color == 0) {
	term->set_color = null_set_color;
	term->flags |= TERM_NULL_SET_COLOR;
    }
    if (term->dashtype == 0)
	term->dashtype = null_dashtype;

    if (interactive)
	fprintf(stderr, "Terminal type set to '%s'\n", term->name);

    /* Invalidate any terminal-specific structures that may be active */
    invalidate_palette();

    return (t);
}

/*
 * Routine to detect what terminal is being used (or do anything else
 * that would be nice).  One anticipated (or allowed for) side effect
 * is that the global ``term'' may be set.
 * The environment variable GNUTERM is checked first; if that does
 * not exist, then the terminal hardware is checked, if possible,
 * and finally, we can check $TERM for some kinds of terminals.
 * A default can be set with -DDEFAULTTERM=myterm in the Makefile
 * or #define DEFAULTTERM myterm in term.h
 */
/* thanks to osupyr!alden (Dave Alden) for the original GNUTERM code */
void
init_terminal()
{
    char *term_name = DEFAULTTERM;
#if (defined(MSDOS) && !defined(_Windows)) || defined(NEXT) || defined(SUN) || defined(X11)
    char *env_term = NULL;      /* from TERM environment var */
#endif
#ifdef X11
    char *display = NULL;
#endif
    char *gnuterm = NULL;

    /* GNUTERM environment variable is primary */
    gnuterm = getenv("GNUTERM");
    if (gnuterm != (char *) NULL) {
	term_name = gnuterm;
    } else {

#ifdef VMS
	term_name = vms_init();
#endif /* VMS */

#ifdef NEXT
	env_term = getenv("TERM");
	if (term_name == (char *) NULL
	    && env_term != (char *) NULL && strcmp(env_term, "next") == 0)
	    term_name = "next";
#endif /* NeXT */

#ifdef __BEOS__
	env_term = getenv("TERM");
	if (term_name == (char *) NULL
	    && env_term != (char *) NULL && strcmp(env_term, "beterm") == 0)
	    term_name = "be";
#endif /* BeOS */

#ifdef SUN
	env_term = getenv("TERM");      /* try $TERM */
	if (term_name == (char *) NULL
	    && env_term != (char *) NULL && strcmp(env_term, "sun") == 0)
	    term_name = "sun";
#endif /* SUN */

#ifdef WIN32
#ifdef WXWIDGETS
	/* let the wxWidgets terminal be the default when available */
	if (term_name == (char *) NULL)
	    term_name = "wxt";
#endif
#endif

#ifdef QTTERM
	if (term_name == (char *) NULL)
	    term_name = "qt";
#endif

#ifdef WXWIDGETS
	if (term_name == (char *) NULL)
	    term_name = "wxt";
#endif

#ifdef _Windows
	if (term_name == (char *) NULL)
	    term_name = "win";
#endif /* _Windows */

#if defined(__APPLE__) && defined(__MACH__) && defined(HAVE_FRAMEWORK_AQUATERM)
	/* Mac OS X with AquaTerm installed */
	term_name = "aqua";
#endif

#ifdef X11
	env_term = getenv("TERM");      /* try $TERM */
	if (term_name == (char *) NULL
	    && env_term != (char *) NULL && strcmp(env_term, "xterm") == 0)
	    term_name = "x11";
	display = getenv("DISPLAY");
	if (term_name == (char *) NULL && display != (char *) NULL)
	    term_name = "x11";
	if (X11_Display)
	    term_name = "x11";
#endif /* x11 */

#ifdef DJGPP
	term_name = "svga";
#endif

#ifdef GRASS
	term_name = "grass";
#endif

#ifdef OS2
/* amai: Note that we do some checks above and now overwrite any
   results. Perhaps we may disable checks above!? */
#ifdef X11
/* WINDOWID is set in sessions like xterm, etc.
   DISPLAY is also mandatory. */
	env_term = getenv("WINDOWID");
	display  = getenv("DISPLAY");
	if ((env_term != (char *) NULL) && (display != (char *) NULL))
	    term_name = "x11";
	else
#endif          /* X11 */
	    term_name = "pm";
#endif /*OS2 */

/* set linux terminal only if LINUX_setup was successfull, if we are on X11
   LINUX_setup has failed, also if we are logged in by network */
#ifdef LINUXVGA
	if (LINUX_graphics_allowed)
#if defined(VGAGL) && defined (THREEDKIT)
	    term_name = "vgagl";
#else
	    term_name = "linux";
#endif
#endif /* LINUXVGA */
    }

    /* We have a name, try to set term type */
    if (term_name != NULL && *term_name != '\0') {
	int namelength = strlen(term_name);
	struct udvt_entry *name = add_udv_by_name("GNUTERM");

	Gstring(&name->udv_value, gp_strdup(term_name));
	name->udv_undef = FALSE;

	if (strchr(term_name,' '))
	    namelength = strchr(term_name,' ') - term_name;

	/* Force the terminal to initialize default fonts, etc.	This prevents */
	/* segfaults and other strangeness if you set GNUTERM to "post" or    */
	/* "png" for example. However, calling X11_options() is expensive due */
	/* to the fork+execute of gnuplot_x11 and x11 can tolerate not being  */
	/* initialized until later.                                           */
	/* Note that gp_input_line[] is blank at this point.	              */
	if (change_term(term_name, namelength)) {
	    if (strcmp(term->name,"x11"))
		term->options();
	    return;
	}
	fprintf(stderr, "Unknown or ambiguous terminal name '%s'\n", term_name);
    }
    change_term("unknown", 7);
}


/* test terminal by drawing border and text */
/* called from command test */
void
test_term()
{
    struct termentry *t = term;
    const char *str;
    int x, y, xl, yl, i;
    int xmax_t, ymax_t, x0, y0;
    char label[MAX_ID_LEN];
    int key_entry_height;
    int p_width;
    TBOOLEAN already_in_enhanced_text_mode;
    static t_colorspec black = BLACK_COLORSPEC;

    already_in_enhanced_text_mode = t->flags & TERM_ENHANCED_TEXT;
    if (!already_in_enhanced_text_mode)
	do_string("set termopt enh");

    term_start_plot();
    screen_ok = FALSE;
    xmax_t = (t->xmax * xsize);
    ymax_t = (t->ymax * ysize);
    x0 = (xoffset * t->xmax);
    y0 = (yoffset * t->ymax);

    p_width = pointsize * t->h_tic;
    key_entry_height = pointsize * t->v_tic * 1.25;
    if (key_entry_height < t->v_char)
	key_entry_height = t->v_char;

    /* Sync point for epslatex text positioning */
    (*t->layer)(TERM_LAYER_FRONTTEXT);

    /* border linetype */
    (*t->linewidth) (1.0);
    (*t->linetype) (LT_BLACK);
    newpath();
    (*t->move) (x0, y0);
    (*t->vector) (x0 + xmax_t - 1, y0);
    (*t->vector) (x0 + xmax_t - 1, y0 + ymax_t - 1);
    (*t->vector) (x0, y0 + ymax_t - 1);
    (*t->vector) (x0, y0);
    closepath();

    /* Echo back the current terminal type */
    if (!strcmp(term->name,"unknown"))
	int_error(NO_CARET, "terminal type is unknown");
    else {
	char tbuf[64];
	(void) (*t->justify_text) (LEFT);
	sprintf(tbuf,"%s  terminal test", term->name);
	(*t->put_text) (x0 + t->h_char * 2, y0 + ymax_t - t->v_char, tbuf);
	sprintf(tbuf, "gnuplot version %s.%s  ", gnuplot_version, gnuplot_patchlevel);
	(*t->put_text) (x0 + t->h_char * 2, y0 + ymax_t - t->v_char * 2.25, tbuf);
    }

    (*t->linetype) (LT_AXIS);
    (*t->move) (x0 + xmax_t / 2, y0);
    (*t->vector) (x0 + xmax_t / 2, y0 + ymax_t - 1);
    (*t->move) (x0, y0 + ymax_t / 2);
    (*t->vector) (x0 + xmax_t - 1, y0 + ymax_t / 2);
    /* test width and height of characters */
    (*t->linetype) (LT_SOLID);
    newpath();
    (*t->move) (x0 + xmax_t / 2 - t->h_char * 10, y0 + ymax_t / 2 + t->v_char / 2);
    (*t->vector) (x0 + xmax_t / 2 + t->h_char * 10, y0 + ymax_t / 2 + t->v_char / 2);
    (*t->vector) (x0 + xmax_t / 2 + t->h_char * 10, y0 + ymax_t / 2 - t->v_char / 2);
    (*t->vector) (x0 + xmax_t / 2 - t->h_char * 10, y0 + ymax_t / 2 - t->v_char / 2);
    (*t->vector) (x0 + xmax_t / 2 - t->h_char * 10, y0 + ymax_t / 2 + t->v_char / 2);
    closepath();
    (*t->put_text) (x0 + xmax_t / 2 - t->h_char * 10, y0 + ymax_t / 2,
		    "12345678901234567890");
    (*t->put_text) (x0 + xmax_t / 2 - t->h_char * 10, y0 + ymax_t / 2 + t->v_char * 1.4,
		    "test of character width:");
    (*t->linetype) (LT_BLACK);

    /* Test for enhanced text */
    if (t->flags & TERM_ENHANCED_TEXT) {
	char *tmptext1 =   "Enhanced text:   {x@_{0}^{n+1}}";
	char *tmptext2 = "&{Enhanced text:  }{/:Bold Bold}{/:Italic  Italic}";  
	(*t->put_text) (x0 + xmax_t * 0.5, y0 + ymax_t * 0.40, tmptext1);
	(*t->put_text) (x0 + xmax_t * 0.5, y0 + ymax_t * 0.35, tmptext2);
	(*t->set_font)("");
	if (!already_in_enhanced_text_mode)
	    do_string("set termopt noenh");
    }

    /* test justification */
    (void) (*t->justify_text) (LEFT);
    (*t->put_text) (x0 + xmax_t / 2, y0 + ymax_t / 2 + t->v_char * 6, "left justified");
    str = "centre+d text";
    if ((*t->justify_text) (CENTRE))
	(*t->put_text) (x0 + xmax_t / 2,
			y0 + ymax_t / 2 + t->v_char * 5, str);
    else
	(*t->put_text) (x0 + xmax_t / 2 - strlen(str) * t->h_char / 2,
			y0 + ymax_t / 2 + t->v_char * 5, str);
    str = "right justified";
    if ((*t->justify_text) (RIGHT))
	(*t->put_text) (x0 + xmax_t / 2,
			y0 + ymax_t / 2 + t->v_char * 4, str);
    else
	(*t->put_text) (x0 + xmax_t / 2 - strlen(str) * t->h_char,
			y0 + ymax_t / 2 + t->v_char * 4, str);
    /* test text angle */
    (*t->linetype)(1);
    str = "rotated ce+ntred text";
    if ((*t->text_angle) (TEXT_VERTICAL)) {
	if ((*t->justify_text) (CENTRE))
	    (*t->put_text) (x0 + t->v_char,
			    y0 + ymax_t / 2, str);
	else
	    (*t->put_text) (x0 + t->v_char,
			    y0 + ymax_t / 2 - strlen(str) * t->h_char / 2, str);
	(*t->justify_text) (LEFT);
	str = " rotated by +45 deg";
	(*t->text_angle)(45);
	(*t->put_text)(x0 + t->v_char * 3, y0 + ymax_t / 2, str);
	(*t->justify_text) (LEFT);
	str = " rotated by -45 deg";
	(*t->text_angle)(-45);
	(*t->put_text)(x0 + t->v_char * 2, y0 + ymax_t / 2, str);
    } else {
	(void) (*t->justify_text) (LEFT);
	(*t->put_text) (x0 + t->h_char * 2, y0 + ymax_t / 2 - t->v_char * 2, "can't rotate text");
    }
    (void) (*t->justify_text) (LEFT);
    (void) (*t->text_angle) (0);

    /* test tic size */
    (*t->linetype)(2);
    (*t->move) ((unsigned int) (x0 + xmax_t / 2 + t->h_tic * (1 + axis_array[FIRST_X_AXIS].ticscale)), y0 + (unsigned int) ymax_t - 1);
    (*t->vector) ((unsigned int) (x0 + xmax_t / 2 + t->h_tic * (1 + axis_array[FIRST_X_AXIS].ticscale)),
		  (unsigned int) (y0 + ymax_t - axis_array[FIRST_X_AXIS].ticscale * t->v_tic));
    (*t->move) ((unsigned int) (x0 + xmax_t / 2), y0 + (unsigned int) (ymax_t - t->v_tic * (1 + axis_array[FIRST_X_AXIS].ticscale)));
    (*t->vector) ((unsigned int) (x0 + xmax_t / 2 + axis_array[FIRST_X_AXIS].ticscale * t->h_tic),
		  (unsigned int) (y0 + ymax_t - t->v_tic * (1 + axis_array[FIRST_X_AXIS].ticscale)));
    (void) (*t->justify_text) (RIGHT);
    (*t->put_text) (x0 + (unsigned int) (xmax_t / 2 - 1* t->h_char),
		    y0 + (unsigned int) (ymax_t - t->v_char),
		    "show ticscale");
    (void) (*t->justify_text) (LEFT);
    (*t->linetype)(LT_BLACK);

    /* test line and point types */
    x = x0 + xmax_t - t->h_char * 7 - p_width;
    y = y0 + ymax_t - key_entry_height;
    (*t->pointsize) (pointsize);
    for (i = -2; y > y0 + key_entry_height; i++) {
	struct lp_style_type ls = DEFAULT_LP_STYLE_TYPE;
	ls.l_width = 1;
	load_linetype(&ls,i+1);
	term_apply_lp_properties(&ls);

	(void) sprintf(label, "%d", i + 1);
	if ((*t->justify_text) (RIGHT))
	    (*t->put_text) (x, y, label);
	else
	    (*t->put_text) (x - strlen(label) * t->h_char, y, label);
	(*t->move) (x + t->h_char, y);
	(*t->vector) (x + t->h_char * 5, y);
	if (i >= -1)
	    (*t->point) (x + t->h_char * 6 + p_width / 2, y, i);
	y -= key_entry_height;
    }

    /* test some arrows */
    (*t->linewidth) (1.0);
    (*t->linetype) (0);
    (*t->dashtype) (DASHTYPE_SOLID, NULL);
    x = x0 + xmax_t * .28;
    y = y0 + ymax_t * .5;
    xl = t->h_tic * 7;
    yl = t->v_tic * 7;
    i = curr_arrow_headfilled;
    curr_arrow_headfilled = AS_NOFILL;
    (*t->arrow) (x, y, x + xl, y, END_HEAD);
    curr_arrow_headfilled = 1;
    (*t->arrow) (x, y, x - xl, y, END_HEAD);
    curr_arrow_headfilled = 2;
    (*t->arrow) (x, y, x, y + yl, END_HEAD);
    curr_arrow_headfilled = AS_EMPTY;
    (*t->arrow) (x, y, x, y - yl, END_HEAD);
    curr_arrow_headfilled = AS_NOBORDER;
    xl = t->h_tic * 5;
    yl = t->v_tic * 5;
    (*t->arrow) (x - xl, y - yl, x + xl, y + yl, END_HEAD | BACKHEAD);
    (*t->arrow) (x - xl, y + yl, x, y, NOHEAD);
    curr_arrow_headfilled = AS_EMPTY;
    (*t->arrow) (x, y, x + xl, y - yl, BACKHEAD);
    curr_arrow_headfilled = i;

    /* test line widths */
    (void) (*t->justify_text) (LEFT);
    xl = xmax_t / 10;
    yl = ymax_t / 25;
    x = x0 + xmax_t * .075;
    y = y0 + yl;

    for (i=1; i<7; i++) {
	(*t->linewidth) ((float)(i)); (*t->linetype)(LT_BLACK);
	(*t->move) (x, y); (*t->vector) (x+xl, y);
	sprintf(label,"  lw %1d", i);
	(*t->put_text) (x+xl, y, label);
	y += yl;
    }
    (*t->put_text) (x, y, "linewidth");

    /* test native dashtypes (_not_ the 'set mono' sequence) */
    (void) (*t->justify_text) (LEFT);
    xl = xmax_t / 10;
    yl = ymax_t / 25;
    x = x0 + xmax_t * .3;
    y = y0 + yl;
    
    for (i=0; i<5; i++) {
 	(*t->linewidth) (1.0);
	(*t->linetype) (LT_SOLID);
	(*t->dashtype) (i, NULL); 
	(*t->set_color)(&black);
	(*t->move) (x, y); (*t->vector) (x+xl, y);
	sprintf(label,"  dt %1d", i+1);
	(*t->put_text) (x+xl, y, label);
	y += yl;
    }
    (*t->put_text) (x, y, "dashtype");

    /* test fill patterns */
    x = x0 + xmax_t * 0.5;
    y = y0;
    xl = xmax_t / 40;
    yl = ymax_t / 8;
    (*t->linewidth) ((float)(1));
    (*t->linetype)(LT_BLACK);
    (*t->justify_text) (CENTRE);
    (*t->put_text)(x+xl*7, y + yl+t->v_char*1.5, "pattern fill");
    for (i=0; i<9; i++) {
	int style = ((i<<4) + FS_PATTERN);
	if (t->fillbox)
	    (*t->fillbox) ( style, x, y, xl, yl );
	newpath();
	(*t->move)  (x,y);
	(*t->vector)(x,y+yl);
	(*t->vector)(x+xl,y+yl);
	(*t->vector)(x+xl,y);
	(*t->vector)(x,y);
	closepath();
	sprintf(label, "%2d", i);
	(*t->put_text)(x+xl/2, y+yl+t->v_char*0.5, label);
	x += xl * 1.5;
    }

    {
	int cen_x = x0 + (int)(0.70 * xmax_t);
	int cen_y = y0 + (int)(0.83 * ymax_t);
	int radius = xmax_t / 20;

	/* test pm3d -- filled_polygon(), but not set_color() */
	if (t->filled_polygon) {
	    int i, j;
#define NUMBER_OF_VERTICES 6
	    int n = NUMBER_OF_VERTICES;
	    gpiPoint corners[NUMBER_OF_VERTICES+1];
#undef  NUMBER_OF_VERTICES

	    for (j=0; j<=1; j++) {
		int ix = cen_x + j*radius;
		int iy = cen_y - j*radius/2;
		for (i = 0; i < n; i++) {
		    corners[i].x = ix + radius * cos(2*M_PI*i/n);
		    corners[i].y = iy + radius * sin(2*M_PI*i/n);
		}
		corners[n].x = corners[0].x;
		corners[n].y = corners[0].y;
		if (j == 0) {
		    (*t->linetype)(2);
		    corners->style = FS_OPAQUE;
		} else {
		    (*t->linetype)(1);
		    corners->style = FS_TRANSPARENT_SOLID + (50<<4);
		}
		term->filled_polygon(n+1, corners);
	    }
	    str = "filled polygons:";
	} else
	    str = "No filled polygons";
	(*t->linetype)(LT_BLACK);
	i = ((*t->justify_text) (CENTRE)) ? 0 : t->h_char * strlen(str) / 2;
	(*t->put_text) (cen_x + i, cen_y + radius + t->v_char * 0.5, str);
    }

    term_end_plot();
}


#ifdef VMS
/* these are needed to modify terminal characteristics */
# ifndef VWS_XMAX
   /* avoid duplicate warning; VWS includes these */
#  include <descrip.h>
#  include <ssdef.h>
# endif                         /* !VWS_MAX */
# include <iodef.h>
# include <ttdef.h>
# include <tt2def.h>
# include <dcdef.h>
# include <stat.h>
# include <fab.h>
/* If you use WATCOM C or a very strict ANSI compiler, you may have to
 * delete or comment out the following 3 lines: */
# ifndef TT2$M_DECCRT3          /* VT300 not defined as of VAXC v2.4 */
#  define TT2$M_DECCRT3 0X80000000
# endif
static unsigned short chan;
static int old_char_buf[3], cur_char_buf[3];
$DESCRIPTOR(sysoutput_desc, "SYS$OUTPUT");

/* Look first for decw$display (decterms do regis).  Determine if we
 * have a regis terminal and save terminal characteristics */
char *
vms_init()
{
    /* Save terminal characteristics in old_char_buf and
       initialise cur_char_buf to current settings. */
    int i;
#ifdef X11
    if (getenv("DECW$DISPLAY"))
	return ("x11");
#endif
    atexit(vms_reset);
    sys$assign(&sysoutput_desc, &chan, 0, 0);
    sys$qiow(0, chan, IO$_SENSEMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
    for (i = 0; i < 3; ++i)
	cur_char_buf[i] = old_char_buf[i];
    sys$dassgn(chan);

    /* Test if terminal is regis */
    if ((cur_char_buf[2] & TT2$M_REGIS) == TT2$M_REGIS)
	return ("regis");
    return (NULL);
}

/* set terminal to original state */
void
vms_reset()
{
    int i;

    sys$assign(&sysoutput_desc, &chan, 0, 0);
    sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
    for (i = 0; i < 3; ++i)
	cur_char_buf[i] = old_char_buf[i];
    sys$dassgn(chan);
}

/* set terminal mode to tektronix */
void
term_mode_tek()
{
    long status;

    if (gpoutfile != stdout)
	return;                 /* don't modify if not stdout */
    sys$assign(&sysoutput_desc, &chan, 0, 0);
    cur_char_buf[0] = 0x004A0000 | DC$_TERM | (TT$_TEK401X << 8);
    cur_char_buf[1] = (cur_char_buf[1] & 0x00FFFFFF) | 0x18000000;

    cur_char_buf[1] &= ~TT$M_CRFILL;
    cur_char_buf[1] &= ~TT$M_ESCAPE;
    cur_char_buf[1] &= ~TT$M_HALFDUP;
    cur_char_buf[1] &= ~TT$M_LFFILL;
    cur_char_buf[1] &= ~TT$M_MECHFORM;
    cur_char_buf[1] &= ~TT$M_NOBRDCST;
    cur_char_buf[1] &= ~TT$M_NOECHO;
    cur_char_buf[1] &= ~TT$M_READSYNC;
    cur_char_buf[1] &= ~TT$M_REMOTE;
    cur_char_buf[1] |= TT$M_LOWER;
    cur_char_buf[1] |= TT$M_TTSYNC;
    cur_char_buf[1] |= TT$M_WRAP;
    cur_char_buf[1] &= ~TT$M_EIGHTBIT;
    cur_char_buf[1] &= ~TT$M_MECHTAB;
    cur_char_buf[1] &= ~TT$M_SCOPE;
    cur_char_buf[1] |= TT$M_HOSTSYNC;

    cur_char_buf[2] &= ~TT2$M_APP_KEYPAD;
    cur_char_buf[2] &= ~TT2$M_BLOCK;
    cur_char_buf[2] &= ~TT2$M_DECCRT3;
    cur_char_buf[2] &= ~TT2$M_LOCALECHO;
    cur_char_buf[2] &= ~TT2$M_PASTHRU;
    cur_char_buf[2] &= ~TT2$M_REGIS;
    cur_char_buf[2] &= ~TT2$M_SIXEL;
    cur_char_buf[2] |= TT2$M_BRDCSTMBX;
    cur_char_buf[2] |= TT2$M_EDITING;
    cur_char_buf[2] |= TT2$M_INSERT;
    cur_char_buf[2] |= TT2$M_PRINTER;
    cur_char_buf[2] &= ~TT2$M_ANSICRT;
    cur_char_buf[2] &= ~TT2$M_AVO;
    cur_char_buf[2] &= ~TT2$M_DECCRT;
    cur_char_buf[2] &= ~TT2$M_DECCRT2;
    cur_char_buf[2] &= ~TT2$M_DRCS;
    cur_char_buf[2] &= ~TT2$M_EDIT;
    cur_char_buf[2] |= TT2$M_FALLBACK;

    status = sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
    if (status == SS$_BADPARAM) {
	/* terminal fallback utility not installed on system */
	cur_char_buf[2] &= ~TT2$M_FALLBACK;
	sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
    } else {
	if (status != SS$_NORMAL)
	    lib$signal(status, 0, 0);
    }
    sys$dassgn(chan);
}

/* set terminal mode back to native */
void
term_mode_native()
{
    int i;

    if (gpoutfile != stdout)
	return;                 /* don't modify if not stdout */
    sys$assign(&sysoutput_desc, &chan, 0, 0);
    sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, old_char_buf, 12, 0, 0, 0, 0);
    for (i = 0; i < 3; ++i)
	cur_char_buf[i] = old_char_buf[i];
    sys$dassgn(chan);
}

/* set terminal mode pasthru */
void
term_pasthru()
{
    if (gpoutfile != stdout)
	return;                 /* don't modify if not stdout */
    sys$assign(&sysoutput_desc, &chan, 0, 0);
    cur_char_buf[2] |= TT2$M_PASTHRU;
    sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
    sys$dassgn(chan);
}

/* set terminal mode nopasthru */
void
term_nopasthru()
{
    if (gpoutfile != stdout)
	return;                 /* don't modify if not stdout */
    sys$assign(&sysoutput_desc, &chan, 0, 0);
    cur_char_buf[2] &= ~TT2$M_PASTHRU;
    sys$qiow(0, chan, IO$_SETMODE, 0, 0, 0, cur_char_buf, 12, 0, 0, 0, 0);
    sys$dassgn(chan);
}

void
fflush_binary()
{
    typedef short int INT16;    /* signed 16-bit integers */
    INT16 k;            /* loop index */

    if (gpoutfile != stdout) {
	/* Stupid VMS fflush() raises error and loses last data block
	   unless it is full for a fixed-length record binary file.
	   Pad it here with NULL characters. */
	for (k = (INT16) ((*gpoutfile)->_cnt); k > 0; --k)
	    putc('\0', gpoutfile);
	fflush(gpoutfile);
    }
}
#endif /* VMS */

/*
 * This is an abstraction of the enhanced text mode originally written
 * for the postscript terminal driver by David Denholm and Matt Heffron.
 * I have split out a terminal-independent recursive syntax-parser
 * routine that can be shared by all drivers that want to add support
 * for enhanced text mode.
 *
 * A driver that wants to make use of this common framework must provide
 * three new entries in TERM_TABLE:
 *      void *enhanced_open   (char *fontname, double fontsize, double base,
 *                             TBOOLEAN widthflag, TBOOLEAN showflag,
 *                             int overprint)
 *      void *enhanced_writec (char c)
 *      void *enhanced_flush  ()
 *
 * Each driver also has a separate ENHXX_put_text() routine that replaces
 * the normal (term->put_text) routine while in enhanced mode.
 * This routine must initialize the following globals used by the shared code:
 *      enhanced_fontscale      converts font size to device resolution units
 *      enhanced_escape_format  used to process octal escape characters \xyz
 *
 * I bent over backwards to make the output of the revised code identical
 * to the output of the original postscript version.  That means there is
 * some cruft left in here (enhanced_max_height for one thing) that is
 * probably irrelevant to any new drivers using the code.
 *
 * Ethan A Merritt - November 2003
 */

#ifdef DEBUG_ENH
#define ENH_DEBUG(x) printf x;
#else
#define ENH_DEBUG(x)
#endif

void
do_enh_writec(int c)
{
    /* note: c is meant to hold a char, but is actually an int, for
     * the same reasons applying to putc() and friends */
    *enhanced_cur_text++ = c;
}

/*
 * Process a bit of string, and return the last character used.
 * p is start of string
 * brace is TRUE to keep processing to }, FALSE to do one character only
 * fontname & fontsize are obvious
 * base is the current baseline
 * widthflag is TRUE if the width of this should count,
 *              FALSE for zero width boxes
 * showflag is TRUE if this should be shown,
 *             FALSE if it should not be shown (like TeX \phantom)
 * overprint is 0 for normal operation,
 *              1 for the underprinted text (included in width calculation),
 *              2 for the overprinted text (not included in width calc)
 *              (overprinted text is centered horizontally on underprinted text
 */

const char *
enhanced_recursion(
    const char *p,
    TBOOLEAN brace,
    char *fontname,
    double fontsize,
    double base,
    TBOOLEAN widthflag,
    TBOOLEAN showflag,
    int overprint)
{
    TBOOLEAN wasitalic, wasbold;

    /* Keep track of the style of the font passed in at this recursion level */
    wasitalic = (strstr(fontname, ":Italic") != NULL);
    wasbold = (strstr(fontname, ":Bold") != NULL);

    FPRINTF((stderr, "RECURSE WITH \"%s\", %d %s %.1f %.1f %d %d %d",
		p, brace, fontname, fontsize, base, widthflag, showflag, overprint));

    /* Start each recursion with a clean string */
    (term->enhanced_flush)();

    if (base + fontsize > enhanced_max_height) {
	enhanced_max_height = base + fontsize;
	ENH_DEBUG(("Setting max height to %.1f\n", enhanced_max_height));
    }

    if (base < enhanced_min_height) {
	enhanced_min_height = base;
	ENH_DEBUG(("Setting min height to %.1f\n", enhanced_min_height));
    }

    while (*p) {
	float shift;

	/*
	 * EAM Jun 2009 - treating bytes one at a time does not work for multibyte
	 * encodings, including utf-8. If we hit a byte with the high bit set, test
	 * whether it starts a legal UTF-8 sequence and if so copy the whole thing.
	 * Other multibyte encodings are still a problem.
	 * Gnuplot's other defined encodings are all single-byte; for those we
	 * really do want to treat one byte at a time.
	 */
	if ((*p & 0x80) && (encoding == S_ENC_DEFAULT || encoding == S_ENC_UTF8)) {
	    unsigned long utf8char;
	    const char *nextchar = p;

	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    if (utf8toulong(&utf8char, &nextchar)) {	/* Legal UTF8 sequence */
		while (p < nextchar)
		    (term->enhanced_writec)(*p++);
		p--;
	    } else {					/* Some other multibyte encoding? */
		(term->enhanced_writec)(*p);
	    }
/* shige : for Shift_JIS */
	} else if ((*p & 0x80) && (encoding == S_ENC_SJIS)) {
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    (term->enhanced_writec)(*(p++));
	    (term->enhanced_writec)(*p);
	} else

	switch (*p) {
	case '}'  :
	    /*{{{  deal with it*/
	    if (brace)
		return (p);

	    int_warn(NO_CARET, "enhanced text parser - spurious }");
	    break;
	    /*}}}*/

	case '_'  :
	case '^'  :
	    /*{{{  deal with super/sub script*/
	    shift = (*p == '^') ? 0.5 : -0.3;
	    (term->enhanced_flush)();
	    p = enhanced_recursion(p + 1, FALSE, fontname, fontsize * 0.8,
			      base + shift * fontsize, widthflag,
			      showflag, overprint);
	    break;
	    /*}}}*/
	case '{'  :
	    {
		TBOOLEAN isitalic = FALSE, isbold = FALSE, isnormal = FALSE;
		const char *start_of_fontname = NULL;
		const char *end_of_fontname = NULL;
		char *localfontname = NULL;
		char ch;
		float f = fontsize, ovp;

		/* Mar 2014 - this will hold "fontfamily{:Italic}{:Bold}" */
		char *styledfontname = NULL;

		/*{{{  recurse (possibly with a new font) */

		ENH_DEBUG(("Dealing with {\n"));

		/* get vertical offset (if present) for overprinted text */
		while (*++p == ' ');
		if (overprint == 2) {
		    char *end;
		    ovp = (float)strtod(p,&end);
		    p = end;
		    if (term->flags & TERM_IS_POSTSCRIPT)
			base = ovp*f;
		    else
			base += ovp*f;
		}
		--p;            /* HBB 20001021: bug fix: 10^{2} broken */

		if (*++p == '/') {
		    /* then parse a fontname, optional fontsize */
		    while (*++p == ' ')
			;       /* do nothing */
		    if (*p=='-') {
			while (*++p == ' ')
			    ;   /* do nothing */
		    }
		    start_of_fontname = p;
		    while ((ch = *p) > ' ' && ch != '=' && ch != '*' && ch != '}' && ch != ':')
			++p;
		    end_of_fontname = p;
		    do {
			if (ch == '=') {
			    /* get optional font size */
			    char *end;
			    p++;
			    ENH_DEBUG(("Calling strtod(\"%s\") ...", p));
			    f = (float)strtod(p, &end);
			    p = end;
			    ENH_DEBUG(("Returned %.1f and \"%s\"\n", f, p));

			    if (f == 0)
				f = fontsize;
			    else
				f *= enhanced_fontscale;  /* remember the scaling */

			    ENH_DEBUG(("Font size %.1f\n", f));
			} else if (ch == '*') {
			    /* get optional font size scale factor */
			    char *end;
			    p++;
			    ENH_DEBUG(("Calling strtod(\"%s\") ...", p));
			    f = (float)strtod(p, &end);
			    p = end;
			    ENH_DEBUG(("Returned %.1f and \"%s\"\n", f, p));

			    if (f)
				f *= fontsize;  /* apply the scale factor */
			    else
				f = fontsize;

			    ENH_DEBUG(("Font size %.1f\n", f));
			} else if (ch == ':') {
			    /* get optional style markup attributes */
			    p++;
			    if (!strncmp(p,"Bold",4))
				isbold = TRUE;
			    if (!strncmp(p,"Italic",6))
				isitalic = TRUE;
			    if (!strncmp(p,"Normal",6))
				isnormal = TRUE;
			    while (isalpha((unsigned char)*p)) {p++;}
			}
		    } while (((ch = *p) == '=') || (ch == ':') || (ch == '*'));

		    if (ch == '}')
			int_warn(NO_CARET,"bad syntax in enhanced text string");

		    if (*p == ' ')	/* Eat up a single space following a font spec */
			++p;
		    if (!start_of_fontname || (start_of_fontname == end_of_fontname)) {
			/* Use the font name passed in to us */
			localfontname = gp_strdup(fontname);
		    } else {
			/* We found a new font name {/Font ...} */
			int len = end_of_fontname - start_of_fontname;
			localfontname = gp_alloc(len+1,"localfontname");
			strncpy(localfontname, start_of_fontname, len);
			localfontname[len] = '\0';
		    }
		}
		/*}}}*/

		/* Collect cumulative style markup before passing it in the font name */
		isitalic = (wasitalic || isitalic) && !isnormal;
		isbold = (wasbold || isbold) && !isnormal;

		styledfontname = stylefont(localfontname ? localfontname : fontname,
					    isbold, isitalic);

		p = enhanced_recursion(p, TRUE, styledfontname, f, base,
				  widthflag, showflag, overprint);

		(term->enhanced_flush)();

		free(styledfontname);
		free(localfontname);

		break;
	    } /* case '{' */
	case '@' :
	    /*{{{  phantom box - prints next 'char', then restores currentpoint */
	    (term->enhanced_flush)();
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, 3);
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, showflag, overprint);
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, 4);
	    break;
	    /*}}}*/

	case '&' :
	    /*{{{  character skip - skips space equal to length of character(s) */
	    (term->enhanced_flush)();

	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, FALSE, overprint);
	    break;
	    /*}}}*/

	case '~' :
	    /*{{{ overprinted text */
	    /* the second string is overwritten on the first, centered
	     * horizontally on the first and (optionally) vertically
	     * shifted by an amount specified (as a fraction of the
	     * current fontsize) at the beginning of the second string

	     * Note that in this implementation neither the under- nor
	     * overprinted string can contain syntax that would result
	     * in additional recursions -- no subscripts,
	     * superscripts, or anything else, with the exception of a
	     * font definition at the beginning of the text */

	    (term->enhanced_flush)();
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      widthflag, showflag, 1);
	    (term->enhanced_flush)();
	    if (!*p)
	        break;
	    p = enhanced_recursion(++p, FALSE, fontname, fontsize, base,
			      FALSE, showflag, 2);

	    overprint = 0;   /* may not be necessary, but just in case . . . */
	    break;
	    /*}}}*/

	case '('  :
	case ')'  :
	    /*{{{  an escape and print it */
	    /* special cases */
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    if (term->flags & TERM_IS_POSTSCRIPT)
		(term->enhanced_writec)('\\');
	    (term->enhanced_writec)(*p);
	    break;
	    /*}}}*/

	case '\\'  :
	    /*{{{  Enhanced mode always uses \xyz as an octal character representation
		   but each terminal type must give us the actual output format wanted.
		   pdf.trm wants the raw character code, which is why we use strtol();
		   most other terminal types want some variant of "\\%o". */
	    if (p[1] >= '0' && p[1] <= '7') {
		char *e, escape[16], octal[4] = {'\0','\0','\0','\0'};

		(term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
		octal[0] = *(++p);
		if (p[1] >= '0' && p[1] <= '7') {
		    octal[1] = *(++p);
		    if (p[1] >= '0' && p[1] <= '7')
			octal[2] = *(++p);
		}
		sprintf(escape, enhanced_escape_format, strtol(octal,NULL,8));
		for (e=escape; *e; e++) {
		    (term->enhanced_writec)(*e);
		}
		break;
	    /* This was the original (prior to version 4) enhanced text code specific */
	    /* to the reserved characters of PostScript.  Some of it was mis-applied  */
	    /* to other terminal types until fixed in Mar 2012.                       */
	    } else if (term->flags & TERM_IS_POSTSCRIPT) {
		if (p[1]=='\\' || p[1]=='(' || p[1]==')') {
		    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
		    (term->enhanced_writec)('\\');
		} else if (strchr("^_@&~{}",p[1]) == NULL) {
		    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
		    (term->enhanced_writec)('\\');
		    (term->enhanced_writec)('\\');
		    break;
		}
	    }
	    ++p;

	    /* HBB 20030122: Avoid broken output if there's a \
	     * exactly at the end of the line */
	    if (*p == '\0') {
		int_warn(NO_CARET, "enhanced text parser -- spurious backslash");
		break;
	    }

	    /* SVG requires an escaped '&' to be passed as something else */
	    /* FIXME: terminal-dependent code does not belong here */
	    if (*p == '&' && encoding == S_ENC_DEFAULT && !strcmp(term->name, "svg")) {
		(term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
		(term->enhanced_writec)('\376');
		break;
	    }

	    /* just go and print it (fall into the 'default' case) */
	    /*}}}*/
	default:
	    /*{{{  print it */
	    (term->enhanced_open)(fontname, fontsize, base, widthflag, showflag, overprint);
	    (term->enhanced_writec)(*p);
	    /*}}}*/
	} /* switch (*p) */

	/* like TeX, we only do one character in a recursion, unless it's
	 * in braces
	 */

	if (!brace) {
	    (term->enhanced_flush)();
	    return(p);  /* the ++p in the outer copy will increment us */
	}

	if (*p) /* only not true if { not terminated, I think */
	    ++p;
    } /* while (*p) */

    (term->enhanced_flush)();
    return p;
}

/* Strip off anything trailing the requested font name,
 * then add back markup requests.
 */
char *
stylefont(const char *fontname, TBOOLEAN isbold, TBOOLEAN isitalic)
{
    char *div;
    char *markup = gp_alloc( strlen(fontname) + 16, "font markup");
    strcpy(markup, fontname);
    if ((div = strchr(markup,':')))
	*div = '\0';
    if (isbold)
	strcat(markup, ":Bold");
    if (isitalic)
	strcat(markup, ":Italic");

    FPRINTF((stderr, "MARKUP FONT: %s -> %s\n", fontname, markup));
    return markup;
}

/* Called after the end of recursion to check for errors */
void
enh_err_check(const char *str)
{
    if (*str == '}')
	int_warn(NO_CARET, "enhanced text mode parser - ignoring spurious }");
    else
	int_warn(NO_CARET, "enhanced text mode parsing error");
}

/*
 * Text strings containing control information for enhanced text mode
 * contain more characters than will actually appear in the output.
 * This makes it hard to estimate how much horizontal space on the plot
 * (e.g. in the key box) must be reserved to hold them.  To approximate
 * the eventual length we switch briefly to the dummy terminal driver
 * "estimate.trm" and then switch back to the current terminal.
 * If better, perhaps terminal-specific methods of estimation are
 * developed later they can be slotted into this one call site.
 */
int
estimate_strlen(char *text)
{
int len;

    if ((term->flags & TERM_IS_LATEX))
	len = strlen_tex(text);
    else

#ifdef GP_ENH_EST
    if (strchr(text,'\n') || (term->flags & TERM_ENHANCED_TEXT)) {
	struct termentry *tsave = term;
	term = &ENHest;
	term->put_text(0,0,text);
	len = term->xmax;
	FPRINTF((stderr,"Estimating length %d height %g for enhanced text string \"%s\"\n",
		len, (double)(term->ymax)/10., text));
	term = tsave;
    } else if (encoding == S_ENC_UTF8)
	len = strwidth_utf8(text);
    else
#endif
	len = strlen(text);

    return len;
}

/* 
 * Use estimate.trm to mock up a non-enhanced approximation of the
 * original string.
 */
char *
estimate_plaintext(char *enhancedtext)
{
    if (enhancedtext == NULL)
	return NULL;
    estimate_strlen(enhancedtext);
    return ENHest_plaintext;
}

void
ignore_enhanced(TBOOLEAN flag)
{
    /* Force a return to the default font */
    if (flag && !ignore_enhanced_text) {
	ignore_enhanced_text = TRUE;
	term->set_font("");
    }
    ignore_enhanced_text = flag;
}

/* Simple-minded test for whether the point (x,y) is in bounds for the current terminal.
 * Some terminals can do their own clipping, and can clip partial objects.
 * If the flag TERM_CAN_CLIP is set, we skip this relative crude test and let the
 * driver or the hardware handle clipping.
 */
TBOOLEAN
on_page(int x, int y)
{
    if (term->flags & TERM_CAN_CLIP)
	return TRUE;

    if ((0 < x && x < term->xmax) && (0 < y && y < term->ymax))
	return TRUE;

    return FALSE;
}

/* Utility routine for drivers to accept an explicit size for the
 * output image.
 */
size_units
parse_term_size( float *xsize, float *ysize, size_units default_units )
{
    size_units units = default_units;

    if (END_OF_COMMAND)
	int_error(c_token, "size requires two numbers:  xsize, ysize");
    *xsize = real_expression();
    if (almost_equals(c_token,"in$ches")) {
	c_token++;
	units = INCHES;
    } else if (equals(c_token,"cm")) {
	c_token++;
	units = CM;
    }
    switch (units) {
    case INCHES:	*xsize *= gp_resolution; break;
    case CM:		*xsize *= (float)gp_resolution / 2.54; break;
    case PIXELS:
    default:		 break;
    }

    if (!equals(c_token++,","))
	int_error(c_token, "size requires two numbers:  xsize, ysize");
    *ysize = real_expression();
    if (almost_equals(c_token,"in$ches")) {
	c_token++;
	units = INCHES;
    } else if (equals(c_token,"cm")) {
	c_token++;
	units = CM;
    }
    switch (units) {
    case INCHES:	*ysize *= gp_resolution; break;
    case CM:		*ysize *= (float)gp_resolution / 2.54; break;
    case PIXELS:
    default:		 break;
    }

    if (*xsize < 1 || *ysize < 1)
	int_error(c_token, "size: out of range");

    return units;
}

/*
 * Wrappers for newpath and closepath
 */

void
newpath()
{
    if (term->path)
	(*term->path)(0);
}

void
closepath()
{
    if (term->path)
	(*term->path)(1);
}

/* Squeeze all fill information into the old style parameter.
 * The terminal drivers know how to extract the information.
 * We assume that the style (int) has only 16 bit, therefore we take
 * 4 bits for the style and allow 12 bits for the corresponding fill parameter.
 * This limits the number of styles to 16 and the fill parameter's
 * values to the range 0...4095, which seems acceptable.
 */
int
style_from_fill(struct fill_style_type *fs)
{
    int fillpar, style;

    switch( fs->fillstyle ) {
    case FS_SOLID:
    case FS_TRANSPARENT_SOLID:
	fillpar = fs->filldensity;
	style = ((fillpar & 0xfff) << 4) + fs->fillstyle;
	break;
    case FS_PATTERN:
    case FS_TRANSPARENT_PATTERN:
	fillpar = fs->fillpattern;
	style = ((fillpar & 0xfff) << 4) + fs->fillstyle;
	break;
    case FS_EMPTY:
    default:
	/* solid fill with background color */
	style = FS_EMPTY;
	break;
    }

    return style;
}


/*
 * Load dt with the properties of a user-defined dashtype.
 * Return: DASHTYPE_SOLID or DASHTYPE_CUSTOM or a positive number
 * if no user-defined dashtype was found.
 */
int
load_dashtype(struct t_dashtype *dt, int tag)
{
    struct custom_dashtype_def *this;
    struct t_dashtype loc_dt = DEFAULT_DASHPATTERN;

    this = first_custom_dashtype;
    while (this != NULL) {
	if (this->tag == tag) {
	    *dt = this->dashtype;
	    memcpy(dt->dstring, this->dashtype.dstring, sizeof(dt->dstring));
	    return this->d_type;
	} else {
	    this = this->next;
	}
    }

    /* not found, fall back to default, terminal-dependent dashtype */
    *dt = loc_dt;
    return tag - 1;
}


void
lp_use_properties(struct lp_style_type *lp, int tag)
{
    /*  This function looks for a linestyle defined by 'tag' and copies
     *  its data into the structure 'lp'.
     */

    struct linestyle_def *this;
    int save_flags = lp->flags;

    this = first_linestyle;
    while (this != NULL) {
	if (this->tag == tag) {
	    *lp = this->lp_properties;
	    lp->flags = save_flags;
	    return;
	} else {
	    this = this->next;
	}
    }

    /* No user-defined style with this tag; fall back to default line type. */
    load_linetype(lp, tag);
}


/*
 * Load lp with the properties of a user-defined linetype
 */
void
load_linetype(struct lp_style_type *lp, int tag)
{
    struct linestyle_def *this;
    TBOOLEAN recycled = FALSE;

recycle:

    if ((tag > 0) && (monochrome || (term->flags & TERM_MONOCHROME))) {
	for (this = first_mono_linestyle; this; this = this->next) {
	    if (tag == this->tag) {
		*lp = this->lp_properties;
		return;
	    }
	}

	/* This linetype wasn't defined explicitly.		*/
	/* Should we recycle one of the first N linetypes?	*/
	if (tag > mono_recycle_count && mono_recycle_count > 0) {
	    tag = (tag-1) % mono_recycle_count + 1;
	    goto recycle;
	}

	return;
    }

    this = first_perm_linestyle;
    while (this != NULL) {
	if (this->tag == tag) {
	    /* Always load color, width, and dash properties */
	    lp->l_type = this->lp_properties.l_type;
	    lp->l_width = this->lp_properties.l_width;
	    lp->pm3d_color = this->lp_properties.pm3d_color;
	    lp->d_type = this->lp_properties.d_type;
	    lp->custom_dash_pattern = this->lp_properties.custom_dash_pattern;

	    /* Needed in version 5.0 to handle old terminals (pbm hpgl ...) */
	    /* with no support for user-specified colors */
	    if (term->set_color == null_set_color)
		lp->l_type = tag;

	    /* Do not recycle point properties. */
	    /* FIXME: there should be a separate command "set pointtype cycle N" */
	    if (!recycled) {
	    	lp->p_type = this->lp_properties.p_type;
	    	lp->p_interval = this->lp_properties.p_interval;
	    	lp->p_char = this->lp_properties.p_char;
	    	lp->p_size = this->lp_properties.p_size;
	    }
	    return;
	} else {
	    this = this->next;
	}
    }

    /* This linetype wasn't defined explicitly.		*/
    /* Should we recycle one of the first N linetypes?	*/
    if (tag > linetype_recycle_count && linetype_recycle_count > 0) {
	tag = (tag-1) % linetype_recycle_count + 1;
	recycled = TRUE;
	goto recycle;
    }

    /* No user-defined linetype with this tag; fall back to default line type. */
    /* NB: We assume that the remaining fields of lp have been initialized. */
    lp->l_type = tag - 1;
    lp->pm3d_color.type = TC_LT;
    lp->pm3d_color.lt = lp->l_type;
    lp->d_type = DASHTYPE_SOLID;
    lp->p_type = (tag <= 0) ? -1 : tag - 1;
}

/*
 * Version 5 maintains a parallel set of linetypes for "set monochrome" mode.
 * This routine allocates space and initializes the default set.
 */
void
init_monochrome()
{
    struct lp_style_type mono_default[] = DEFAULT_MONO_LINETYPES;

    if (first_mono_linestyle == NULL) {
	int i, n = sizeof(mono_default) / sizeof(struct lp_style_type);
	struct linestyle_def *new;
	/* copy default list into active list */
	for (i=n; i>0; i--) {
	    new = gp_alloc(sizeof(struct linestyle_def), NULL);
	    new->next = first_mono_linestyle;
	    new->lp_properties = mono_default[i-1];
	    new->tag = i;
	    first_mono_linestyle = new;
	}
    }
}

/*
 * Totally bogus estimate of TeX string lengths.
 * Basically
 * - don't count anything inside square braces
 * - count regexp \[a-zA-z]* as a single character
 * - ignore characters {}$^_
 */
int
strlen_tex(const char *str)
{
    const char *s = str;
    int len = 0;

    if (!strpbrk(s, "{}$[]\\")) {
	len = strlen(s);
	FPRINTF((stderr,"strlen_tex(\"%s\") = %d\n",s,len));
	return len;
    }

    while (*s) {
	switch (*s) {
	case '[':
		while (*s && *s != ']') s++;
		s++;
		break;
	case '\\':
		s++;
		while (*s && isalpha((unsigned char)*s)) s++;
		len++;
		break;
	case '{':
	case '}':
	case '$':
	case '_':
	case '^':
		s++;
		break;
	default:
		s++;
		len++;
	}
    }


    FPRINTF((stderr,"strlen_tex(\"%s\") = %d\n",str,len));
    return len;
}

/* The check for asynchronous events such as hotkeys and mouse clicks is
 * normally done in term->waitforinput() while waiting for the next input
 * from the command line.  If input is currently coming from a file or 
 * pipe instead, as with a "load" command, then this path would not be
 * triggered automatically and these events would back up until input
 * returned to the command line.  These code paths can explicitly call
 * check_for_mouse_events() so that event processing is handled sooner.
 */
void
check_for_mouse_events()
{
#ifdef USE_MOUSE
    if (term_initialised && term->waitforinput) {
	term->waitforinput(TERM_ONLY_CHECK_MOUSING);
    }
#endif
#ifdef WIN32
    /* Process windows GUI events (e.g. for text window, or wxt and windows terminals) */
    WinMessageLoop();
    /* On Windows, Ctrl-C only sets this flag. */
    /* The next block duplicates the behaviour of inter(). */
    if (ctrlc_flag) {
    ctrlc_flag = FALSE;
	term_reset();
	putc('\n', stderr);
	fprintf(stderr, "Ctrl-C detected!\n");
	bail_to_command_line();	/* return to prompt */
    }
#endif
}
