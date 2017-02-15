/*
 * $Id: mouse.h,v 1.26 2014/07/23 05:45:49 sfeam Exp $
 */

/* GNUPLOT - mouse.h */

/*[
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
 *   Original Software (October 1999 - January 2000):
 *     Pieter-Tjerk de Boer <ptdeboer@cs.utwente.nl>
 *     Petr Mikulik <mikulik@physics.muni.cz>
 *     Johannes Zellner <johannes@zellner.org>
 */


#ifndef _HAVE_MOUSE_H
#define _HAVE_MOUSE_H

#include "mousecmn.h"

#include "syscfg.h"

/* Zoom queue
*/
struct t_zoom {
  double xmin, ymin, xmax, ymax;
  double x2min, y2min, x2max, y2max;
  struct t_zoom *prev, *next;
};

typedef struct mouse_setting_t {
    int on;                /* ...                                         */
    int doubleclick;       /* Button1 double / single click resolution    */
    int annotate_zoom_box; /* draw coordinates at zoom box                */
    int label;             /* draw real gnuplot labels on Button 2        */
    int polardistance;     /* display dist. to ruler in polar coordinates */
    int verbose;           /* display ipc commands                        */
    int warp_pointer;      /* warp pointer after starting a zoom box      */
    double xmzoom_factor;  /* scale factor for +/- zoom on x		  */
    double ymzoom_factor;  /* scale factor for +/- zoom on y		  */
    char *fmt;             /* fprintf format for printing numbers         */
    char *labelopts;       /* label options                               */
} mouse_setting_t;


#ifdef OS2
        /* don't start with mouse on default -- clashes with arrow keys on command line */
#define DEFAULT_MOUSE_MODE    0
#else
        /* start with mouse on by default */
#define DEFAULT_MOUSE_MODE    1
#endif

#define DEFAULT_MOUSE_SETTING { \
    DEFAULT_MOUSE_MODE,         \
    300, /* ms */               \
    1, 0, 0, 0, 0,              \
    1.0, 1.0,			\
    mouse_fmt_default,          \
    NULL                        \
}

extern mouse_setting_t default_mouse_setting;
extern mouse_setting_t mouse_setting;
extern char mouse_fmt_default[];

/* enum of GP_ -keycodes has moved to mousecmn.h so that it can be
 * accessed by standalone terminals too */


/* FIXME HBB 20010207: Codestyle violation, again. */
#ifdef _MOUSE_C
/* the following table must match exactly the
 * enum's of GP_ and end with a NULL pointer! */
static char* special_keys[] = {
    "GP_FIRST_KEY", /* keep this dummy there */
    "Linefeed",
    "Clear",
    "Pause",
    "Scroll_Lock",
    "Sys_Req",
    "Insert",
    "Home",
    "Left",
    "Up",
    "Right",
    "Down",
    "PageUp",
    "PageDown",
    "End",
    "Begin",
    "KP_Space",
    "KP_Tab",
    "KP_F1",
    "KP_F2",
    "KP_F3",
    "KP_F4",

    /* see KP_0 - KP_9 */
    "KP_Insert",
    "KP_End",
    "KP_Down",
    "KP_PageDown",
    "KP_Left",
    "KP_Begin",
    "KP_Right",
    "KP_Home",
    "KP_Up",
    "KP_PageUp",

    "KP_Delete",
    "KP_Equal",
    "KP_Multiply",
    "KP_Add",
    "KP_Separator",
    "KP_Subtract",
    "KP_Decimal",
    "KP_Divide",
    "KP_0",
    "KP_1",
    "KP_2",
    "KP_3",
    "KP_4",
    "KP_5",
    "KP_6",
    "KP_7",
    "KP_8",
    "KP_9",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "Close",
    "Button1",
    "GP_LAST_KEY",
    (char*) 0 /* must be the last line */
};
#endif /* _MOUSE_C */

enum {
    MOUSE_COORDINATES_REAL = 0,
    MOUSE_COORDINATES_REAL1, /* w/o brackets */
    MOUSE_COORDINATES_FRACTIONAL,
#if 0
    MOUSE_COORDINATES_PIXELS,
    MOUSE_COORDINATES_SCREEN,
#endif
    MOUSE_COORDINATES_TIMEFMT,
    MOUSE_COORDINATES_XDATE,
    MOUSE_COORDINATES_XTIME,
    MOUSE_COORDINATES_XDATETIME,
    MOUSE_COORDINATES_ALT    /* alternative format as specified by the user */
};

/* FIXME HBB 20010207: Codestyle violation: these should be in mouse.c! */
#if defined(_MOUSE_C)
    long mouse_mode = MOUSE_COORDINATES_REAL;
    char* mouse_alt_string = (char*) 0;
#else
    extern long mouse_mode;
    extern char* mouse_alt_string;
#endif


void event_plotdone __PROTO((void));
void recalc_statusline __PROTO((void));
void update_ruler __PROTO((void));
void set_ruler __PROTO((TBOOLEAN on, int mx, int my));
void UpdateStatusline __PROTO((void));
void do_event __PROTO((struct gp_event_t *ge));
int plot_mode __PROTO((int mode));
void event_reset __PROTO((struct gp_event_t *ge));

/* bind prototype(s) */

void bind_process __PROTO((char* lhs, char* rhs, TBOOLEAN allwindows));
void bind_remove_all __PROTO((void));

#endif /* !_HAVE_MOUSE_H */
