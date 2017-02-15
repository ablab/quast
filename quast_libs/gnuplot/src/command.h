/*
 * $Id: command.h,v 1.66.2.2 2015/05/22 23:34:06 sfeam Exp $
 */

/* GNUPLOT - command.h */

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

#ifndef GNUPLOT_COMMAND_H
# define GNUPLOT_COMMAND_H

#include "gp_types.h"
#include "stdfn.h"

extern char *gp_input_line;
extern size_t gp_input_line_len;

extern int inline_num;

extern int if_depth;			/* old if/else syntax only */
extern TBOOLEAN if_open_for_else;	/* new if/else syntax only */
extern TBOOLEAN if_condition;		/* used by both old and new syntax */

typedef struct lexical_unit {	/* produced by scanner */
    TBOOLEAN is_token;		/* true if token, false if a value */
    struct value l_val;
    int start_index;		/* index of first char in token */
    int length;			/* length of token in chars */
} lexical_unit;

extern struct lexical_unit *token;
extern int token_table_size;
extern int plot_token;
#define END_OF_COMMAND (c_token >= num_tokens || equals(c_token,";"))

extern char *replot_line;

/* flag to disable `replot` when some data are sent through stdin;
 * used by mouse/hotkey capable terminals */
extern TBOOLEAN replot_disabled;

#ifdef USE_MOUSE
extern int paused_for_mouse;	/* Flag the end condition we are paused until */
#define PAUSE_BUTTON1   001		/* Mouse button 1 */
#define PAUSE_BUTTON2   002		/* Mouse button 2 */
#define PAUSE_BUTTON3   004		/* Mouse button 3 */
#define PAUSE_CLICK	007		/* Any button click */
#define PAUSE_KEYSTROKE 010		/* Any keystroke */
#define PAUSE_WINCLOSE	020		/* Window close event */
#define PAUSE_ANY       077		/* Terminate on any of the above */
#endif

/* output file for the print command */
extern FILE *print_out;
extern struct udvt_entry * print_out_var;
extern char *print_out_name;

extern struct udft_entry *dummy_func;

#ifndef STDOUT
# define STDOUT 1
#endif

#if defined(MSDOS) && defined(DJGPP)
extern char HelpFile[];         /* patch for do_help  - AP */
#endif /* MSDOS */

#ifdef _Windows
# define SET_CURSOR_WAIT SetCursor(LoadCursor((HINSTANCE) NULL, IDC_WAIT))
# define SET_CURSOR_ARROW SetCursor(LoadCursor((HINSTANCE) NULL, IDC_ARROW))
#else
# define SET_CURSOR_WAIT        /* nought, zilch */
# define SET_CURSOR_ARROW       /* nought, zilch */
#endif

/* Include code to support deprecated "call" syntax. */
#ifdef BACKWARDS_COMPATIBLE
#define OLD_STYLE_CALL_ARGS
#endif

/* input data, parsing variables */
extern int num_tokens, c_token;

void raise_lower_command __PROTO((int));
void raise_command __PROTO((void));
void lower_command __PROTO((void));
#ifdef OS2
extern void pm_raise_terminal_window __PROTO((void));
extern void pm_lower_terminal_window __PROTO((void));
#endif
#ifdef X11
extern void x11_raise_terminal_window __PROTO((int));
extern void x11_raise_terminal_group __PROTO((void));
extern void x11_lower_terminal_window __PROTO((int));
extern void x11_lower_terminal_group __PROTO((void));
#endif
#ifdef _Windows
extern void win_raise_terminal_window __PROTO((int));
extern void win_raise_terminal_group __PROTO((void));
extern void win_lower_terminal_window __PROTO((int));
extern void win_lower_terminal_group __PROTO((void));
#endif
#ifdef WXWIDGETS
extern void wxt_raise_terminal_window __PROTO((int));
extern void wxt_raise_terminal_group __PROTO((void));
extern void wxt_lower_terminal_window __PROTO((int));
extern void wxt_lower_terminal_group __PROTO((void));
#endif
extern void string_expand_macros __PROTO((void));

#ifdef USE_MOUSE
void bind_command __PROTO((void));
void restore_prompt __PROTO((void));
#else
#define bind_command()
#endif
void refresh_request __PROTO((void));
void refresh_command __PROTO((void));
void call_command __PROTO((void));
void changedir_command __PROTO((void));
void clear_command __PROTO((void));
void eval_command __PROTO((void));
void exit_command __PROTO((void));
void help_command __PROTO((void));
void history_command __PROTO((void));
void do_command __PROTO((void));
void if_command __PROTO((void));
void else_command __PROTO((void));
void import_command __PROTO((void));
void invalid_command __PROTO((void));
void link_command __PROTO((void));
void load_command __PROTO((void));
void begin_clause __PROTO((void));
void clause_reset_after_error __PROTO((void));
void end_clause __PROTO((void));
void null_command __PROTO((void));
void pause_command __PROTO((void));
void plot_command __PROTO((void));
void print_command __PROTO((void));
void printerr_command __PROTO((void));
void pwd_command __PROTO((void));
void replot_command __PROTO((void));
void reread_command __PROTO((void));
void save_command __PROTO((void));
void screendump_command __PROTO((void));
void splot_command __PROTO((void));
void stats_command __PROTO((void));
void system_command __PROTO((void));
void test_command __PROTO((void));
void update_command __PROTO((void));
void do_shell __PROTO((void));
void undefine_command __PROTO((void));
void while_command __PROTO((void));

/* Prototypes for functions exported by command.c */
void extend_input_line __PROTO((void));
void extend_token_table __PROTO((void));
int com_line __PROTO((void));
int do_line __PROTO((void));
void do_string __PROTO((const char* s));
void do_string_and_free __PROTO((char* s));
#ifdef USE_MOUSE
void toggle_display_of_ipc_commands __PROTO((void));
int display_ipc_commands __PROTO((void));
void do_string_replot __PROTO((const char* s));
#endif
#ifdef VMS                     /* HBB 990829: used only on VMS */
void done __PROTO((int status));
#endif
void define __PROTO((void));

void replotrequest __PROTO((void)); /* used in command.c & mouse.c */

void print_set_output __PROTO((char *, TBOOLEAN, TBOOLEAN)); /* set print output file */
char *print_show_output __PROTO((void)); /* show print output file */

/* Activate/deactive effects of 'set view map' before 'splot'/'plot',
 * respectively. Required for proper mousing during 'set view map';
 * actually it won't be necessary if gnuplot keeps a copy of all variables for
 * the current plot and don't share them with 'set' commands.
 *   These routines need to be executed before each plotrequest() and
 * plot3drequest().
 */
void splot_map_activate __PROTO((void));
void splot_map_deactivate __PROTO((void));

int do_system_func __PROTO((const char *cmd, char **output));

#endif /* GNUPLOT_COMMAND_H */
