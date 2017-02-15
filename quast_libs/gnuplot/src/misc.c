#ifndef lint
static char *RCSid() { return RCSid("$Id: misc.c,v 1.188.2.10 2016/08/16 19:35:19 sfeam Exp $"); }
#endif

/* GNUPLOT - misc.c */

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

#include "misc.h"

#include "alloc.h"
#include "command.h"
#include "graphics.h"
#include "plot.h"
#include "tables.h"
#include "util.h"
#include "variable.h"
#include "axis.h"
#include "scanner.h"		/* so that scanner() can count curly braces */
#ifdef _Windows
# include <fcntl.h>
# if defined(__WATCOMC__) || defined(__MSC__)
#  include <io.h>        /* for setmode() */
# endif
#endif
#if defined(HAVE_DIRENT_H)
# include <sys/types.h>
# include <dirent.h>
#elif defined(_Windows)
# include <windows.h>
#endif

static char *recursivefullname __PROTO((const char *path, const char *filename, TBOOLEAN recursive));
static void prepare_call __PROTO((int calltype));

/* State information for load_file(), to recover from errors
 * and properly handle recursive load_file calls
 */
LFS *lf_head = NULL;		/* NULL if not in load_file */

/* these are global so that plot.c can load them for the -c option */
int call_argc;
char *call_args[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char *argname[] = {"ARG0","ARG1","ARG2","ARG3","ARG4","ARG5","ARG6","ARG7","ARG8","ARG9"};

/*
 * iso_alloc() allocates a iso_curve structure that can hold 'num'
 * points.
 */
struct iso_curve *
iso_alloc(int num)
{
    struct iso_curve *ip;
    ip = (struct iso_curve *) gp_alloc(sizeof(struct iso_curve), "iso curve");
    ip->p_max = (num >= 0 ? num : 0);
    ip->p_count = 0;
    if (num > 0) {
	ip->points = (struct coordinate GPHUGE *)
	    gp_alloc(num * sizeof(struct coordinate), "iso curve points");
	memset(ip->points, 0, num * sizeof(struct coordinate));
    } else
	ip->points = (struct coordinate GPHUGE *) NULL;
    ip->next = NULL;
    return (ip);
}

/*
 * iso_extend() reallocates a iso_curve structure to hold "num"
 * points. This will either expand or shrink the storage.
 */
void
iso_extend(struct iso_curve *ip, int num)
{
    if (num == ip->p_max)
	return;

    if (num > 0) {
	if (ip->points == NULL) {
	    ip->points = (struct coordinate GPHUGE *)
		gp_alloc(num * sizeof(struct coordinate), "iso curve points");
	} else {
	    ip->points = (struct coordinate GPHUGE *)
		gp_realloc(ip->points, num * sizeof(struct coordinate), "expanding curve points");
	}
	if (num > ip->p_max)
	    memset( &(ip->points[ip->p_max]), 0, (num - ip->p_max) * sizeof(struct coordinate));
	ip->p_max = num;
    } else {
	if (ip->points != (struct coordinate GPHUGE *) NULL)
	    free(ip->points);
	ip->points = (struct coordinate GPHUGE *) NULL;
	ip->p_max = 0;
    }
}

/*
 * iso_free() releases any memory which was previously malloc()'d to hold
 *   iso curve points.
 */
void
iso_free(struct iso_curve *ip)
{
    if (ip) {
	if (ip->points)
	    free((char *) ip->points);
	free((char *) ip);
    }
}

static void
prepare_call(int calltype)
{
    struct udvt_entry *udv;
    int argindex;
    if (calltype == 2) {
	call_argc = 0;
	while (!END_OF_COMMAND && call_argc <= 9) {
	    call_args[call_argc] = try_to_get_string();
	    if (!call_args[call_argc]) {
		int save_token = c_token;

		/* This catches call "file" STRINGVAR (expression) */
		if (type_udv(c_token) == STRING) {
		    call_args[call_argc] = gp_strdup(add_udv(c_token)->udv_value.v.string_val);
		    c_token++;

		/* Evaluates a parenthesized expression and store the result in a string */
		} else if (equals(c_token, "(")) {
		    char val_as_string[32];
		    struct value a;
		    const_express(&a);
		    switch(a.type) {
			case CMPLX: /* FIXME: More precision? Some way to provide a format? */
				sprintf(val_as_string, "%g", a.v.cmplx_val.real);
				call_args[call_argc] = gp_strdup(val_as_string);
				break;
			default:
				int_error(save_token, "Unrecognized argument type");
				break;
			case INTGR:	
				sprintf(val_as_string, "%d", a.v.int_val);
				call_args[call_argc] = gp_strdup(val_as_string);
				break;
		    } 

		/* old (pre version 5) style wrapping of bare tokens as strings */
		/* is still useful for passing unquoted numbers */
		} else {
		    m_capture(&call_args[call_argc], c_token, c_token);
		    c_token++;
		}
	    }
	    call_argc++;
	}
	lf_head->c_token = c_token;
	if (!END_OF_COMMAND)
	    int_error(++c_token, "too many arguments for 'call <file>'");

    } else if (calltype == 5) {
	/* lf_push() moved our call arguments from call_args[] to lf->call_args[] */
	/* call_argc was determined at program entry */
	for (argindex = 0; argindex < 10; argindex++) {
	    call_args[argindex] = lf_head->call_args[argindex];
	    lf_head->call_args[argindex] = NULL;	/* just to be safe */
	}

    } else {
	/* "load" command has no arguments */
	call_argc = 0;
    }

    /* Old-style "call" arguments were referenced as $0 ... $9 and $# */
    /* New-style has ARG0 = script-name, ARG1 ... ARG9 and ARGC */
    /* FIXME:  If we defined these on entry, we could use get_udv* here */
    udv = add_udv_by_name("ARGC");
    Ginteger(&(udv->udv_value), call_argc);
    udv->udv_undef = FALSE;
    udv = add_udv_by_name("ARG0");
    gpfree_string(&(udv->udv_value));
    Gstring(&(udv->udv_value), gp_strdup(lf_head->name));
    udv->udv_undef = FALSE;
    for (argindex = 1; argindex <= 9; argindex++) {
	char *arg = gp_strdup(call_args[argindex-1]);
	udv = add_udv_by_name(argname[argindex]);
	gpfree_string(&(udv->udv_value));
	Gstring(&(udv->udv_value), arg ? arg : gp_strdup(""));
	udv->udv_undef = FALSE;
    }
}

#ifdef OLD_STYLE_CALL_ARGS

const char *
expand_call_arg(int c)
{
    static char numstr[3];
    if (c == '$') {
	return "$";
    } else if (c == '#') {
	assert(call_argc >= 0 && call_argc <= 9);
	sprintf(numstr, "%i", call_argc);
	return numstr;
    } else if (c >= '0' && c <= '9') {
	int ind = c - '0';
	if (ind >= call_argc)
	    return "";
	else
	    return call_args[ind];
    } else {
	/* pass through unrecognized syntax elements that begin with $, e.g. datablock names */
	sprintf(numstr, "$%c", c);
	return numstr;
    }
    return NULL; /* Avoid compiler warning */
}


static void
expand_call_args(void)
{
    int il = 0;
    int len;
    char *rl;
    char *raw_line = gp_strdup(gp_input_line);

    rl = raw_line;
    *gp_input_line = '\0';
    while (*rl) {
	if (*rl == '$') {
	    const char *sub = expand_call_arg(*(++rl));
	    len = strlen(sub);
	    while (gp_input_line_len - il < len + 1)
		extend_input_line();
	    strcpy(gp_input_line + il, sub);
	    il += len;
	} else {
	    if (il + 1 > gp_input_line_len)
		extend_input_line();
	    gp_input_line[il++] = *rl;
	}
	rl++;
    }
    if (il + 1 > gp_input_line_len)
	extend_input_line();
    gp_input_line[il] = '\0';
    free(raw_line);
}
#endif /* OLD_STYLE_CALL_ARGS */

/*
 * load_file() is called from
 * (1) the "load" command, no arguments substitution is done
 * (2) the "call" command, arguments are substituted for $0, $1, etc.
 * (3) on program entry to load initialization files (acts like "load")
 * (4) to execute script files given on the command line (acts like "load")
 * (5) to execute a single script file given with -c (acts like "call")
 */
void
load_file(FILE *fp, char *name, int calltype)
{
    int len;

    int start, left;
    int more;
    int stop = FALSE;

    /* Provide a user-visible copy of the current line number in the input file */
    udvt_entry *gpval_lineno = add_udv_by_name("GPVAL_LINENO");
    Ginteger(&gpval_lineno->udv_value, 0);
    gpval_lineno->udv_undef = FALSE;

    lf_push(fp, name, NULL); /* save state for errors and recursion */

    if (fp == (FILE *) NULL) {
	int_error(NO_CARET, "Cannot open script file '%s'", name);
	return; /* won't actually reach here */
    }

    if (fp == stdin) {
	/* DBT 10-6-98  go interactive if "-" named as load file */
	interactive = TRUE;
	while (!com_line());
	(void) lf_pop();
	return;
    }

    /* We actually will read from a file */
    prepare_call(calltype);

    /* things to do after lf_push */
    inline_num = 0;
    /* go into non-interactive mode during load */
    /* will be undone below, or in load_file_error */
    interactive = FALSE;

    while (!stop) {	/* read all lines in file */
	left = gp_input_line_len;
	start = 0;
	more = TRUE;

	/* read one logical line */
	while (more) {
	    if (fgets(&(gp_input_line[start]), left, fp) == (char *) NULL) {
		stop = TRUE;	/* EOF in file */
		gp_input_line[start] = '\0';
		more = FALSE;
	    } else {
		inline_num++;
		gpval_lineno->udv_value.v.int_val = inline_num;	/* User visible copy */
		len = strlen(gp_input_line) - 1;
		if (gp_input_line[len] == '\n') {	/* remove any newline */
		    gp_input_line[len] = '\0';
		    /* Look, len was 1-1 = 0 before, take care here! */
		    if (len > 0)
			--len;
		    if (gp_input_line[len] == '\r') {	/* remove any carriage return */
			gp_input_line[len] = NUL;
			if (len > 0)
			    --len;
		    }
		} else if (len + 2 >= left) {
		    extend_input_line();
		    left = gp_input_line_len - len - 1;
		    start = len + 1;
		    continue;	/* don't check for '\' */
		}
		if (gp_input_line[len] == '\\') {
		    /* line continuation */
		    start = len;
		    left = gp_input_line_len - start;
		} else {
		    /* EAM May 2011 - handle multi-line bracketed clauses {...}.
		     * Introduces a requirement for scanner.c and scanner.h
		     * This code is redundant with part of do_line(),
		     * but do_line() assumes continuation lines come from stdin.
		     */

		    /* macros in a clause are problematic, as they are */
		    /* only expanded once even if the clause is replayed */
		    string_expand_macros();

		    /* Strip off trailing comment and count curly braces */
		    num_tokens = scanner(&gp_input_line, &gp_input_line_len);
		    if (gp_input_line[token[num_tokens].start_index] == '#') {
			gp_input_line[token[num_tokens].start_index] = NUL;
			start = token[num_tokens].start_index;
			left = gp_input_line_len - start;
		    }
		    /* Read additional lines if necessary to complete a
		     * bracketed clause {...}
		     */
		    if (curly_brace_count < 0)
			int_error(NO_CARET, "Unexpected }");
		    if (curly_brace_count > 0) {
			if ((len + 4) > gp_input_line_len)
			    extend_input_line();
			strcat(gp_input_line,";\n");
			start = strlen(gp_input_line);
			left = gp_input_line_len - start;
			continue;
		    }
		    
		    more = FALSE;
		}

	    }
	}

	/* process line */
	if (strlen(gp_input_line) > 0) {
#ifdef OLD_STYLE_CALL_ARGS
	    if (calltype == 2 || calltype == 5)
		expand_call_args();
#endif
	    screen_ok = FALSE;	/* make sure command line is echoed on error */
	    if (do_line())
		stop = TRUE;
	}
    }

    /* pop state */
    (void) lf_pop();		/* also closes file fp */
}

/* pop from load_file state stack
   FALSE if stack was empty
   called by load_file and load_file_error */
TBOOLEAN
lf_pop()
{
    LFS *lf;
    int argindex;
    struct udvt_entry *udv;

    if (lf_head == NULL)
	return (FALSE);

    lf = lf_head;
    if (lf->fp == NULL || lf->fp == stdin)
	/* Do not close stdin in the case that "-" is named as a load file */
	;
#if defined(PIPES)
    else if (lf->name != NULL && lf->name[0] == '<')
	pclose(lf->fp);
#endif
    else
	fclose(lf->fp);

    /* call arguments are not relevant when invoked from do_string_and_free */
    if (lf->cmdline == NULL) {
	for (argindex = 0; argindex < 10; argindex++) {
	    if (call_args[argindex])
		free(call_args[argindex]);
	    call_args[argindex] = lf->call_args[argindex];
	}
	call_argc = lf->call_argc;

	/* Restore ARGC and ARG0 ... ARG9 */
	if ((udv = get_udv_by_name("ARGC"))) {
	    Ginteger(&(udv->udv_value), call_argc);
	}
	if ((udv = get_udv_by_name("ARG0"))) {
	    gpfree_string(&(udv->udv_value));
	    Gstring(&(udv->udv_value),
		(lf->prev && lf->prev->name) ? gp_strdup(lf->prev->name) : gp_strdup(""));
	}
	for (argindex = 1; argindex <= 9; argindex++) {
	    if ((udv = get_udv_by_name(argname[argindex]))) {
		gpfree_string(&(udv->udv_value));
		Gstring(&(udv->udv_value), gp_strdup(call_args[argindex-1]));
		if (!call_args[argindex-1])
		    udv->udv_undef = TRUE;
	    }
	}
    }

    interactive = lf->interactive;
    inline_num = lf->inline_num;
    add_udv_by_name("GPVAL_LINENO")->udv_value.v.int_val = inline_num;
    if_depth = lf->if_depth;
    if_condition = lf->if_condition;
    if_open_for_else = lf->if_open_for_else;

    /* Restore saved input state and free the copy */
    if (lf->tokens) {
	num_tokens = lf->num_tokens;
	c_token = lf->c_token;
	assert(token_table_size >= lf->num_tokens+1);
	memcpy(token, lf->tokens,
	       (lf->num_tokens+1) * sizeof(struct lexical_unit));
	free(lf->tokens);
    }
    if (lf->input_line) {
	strcpy(gp_input_line, lf->input_line);
	free(lf->input_line);
    }
    free(lf->name);
    free(lf->cmdline);
    
    lf_head = lf->prev;
    free(lf);
    return (TRUE);
}

/* lf_push is called from two different contexts:
 *    load_file passes fp and file name (3rd param NULL)
 *    do_string_and_free passes cmdline (1st and 2nd params NULL)
 * In either case the routines lf_push/lf_pop save and restore state
 * information that may be changed by executing commands from a file
 * or from the passed command line.
 */
void
lf_push(FILE *fp, char *name, char *cmdline)
{
    LFS *lf;
    int argindex;

    lf = (LFS *) gp_alloc(sizeof(LFS), (char *) NULL);
    if (lf == (LFS *) NULL) {
	if (fp != (FILE *) NULL)
	    (void) fclose(fp);	/* it won't be otherwise */
	int_error(c_token, "not enough memory to load file");
    }
    lf->fp = fp;		/* save this file pointer */
    lf->name = name;
    lf->cmdline = cmdline;

    lf->interactive = interactive;	/* save current state */
    lf->inline_num = inline_num;	/* save current line number */
    lf->call_argc = call_argc;

    /* Call arguments are irrelevant if invoked from do_string_and_free */
    if (cmdline == NULL) {
	for (argindex = 0; argindex < 10; argindex++) {
	    lf->call_args[argindex] = call_args[argindex];
	    call_args[argindex] = NULL;	/* initially no args */
	}
    }
    lf->depth = lf_head ? lf_head->depth+1 : 0;	/* recursion depth */
    if (lf->depth > STACK_DEPTH)
	int_error(NO_CARET, "load/eval nested too deeply");
    lf->if_depth = if_depth;
    lf->if_open_for_else = if_open_for_else;
    lf->if_condition = if_condition;
    lf->c_token = c_token;
    lf->num_tokens = num_tokens;
    lf->tokens = gp_alloc((num_tokens+1) * sizeof(struct lexical_unit),
			  "lf tokens");
    memcpy(lf->tokens, token, (num_tokens+1) * sizeof(struct lexical_unit));
    lf->input_line = gp_strdup(gp_input_line);

    lf->prev = lf_head;		/* link to stack */
    lf_head = lf;
}

/* used for reread  vsnyder@math.jpl.nasa.gov */
FILE *
lf_top()
{
    if (lf_head == (LFS *) NULL)
	return ((FILE *) NULL);
    return (lf_head->fp);
}

/* called from main */
void
load_file_error()
{
    /* clean up from error in load_file */
    /* pop off everything on stack */
    while (lf_pop());
}

FILE *
loadpath_fopen(const char *filename, const char *mode)
{
    FILE *fp;

#if defined(PIPES)
    if (*filename == '<') {
	restrict_popen();
	if ((fp = popen(filename + 1, "r")) == (FILE *) NULL)
	    return (FILE *) 0;
    } else
#endif /* PIPES */
    if ((fp = fopen(filename, mode)) == (FILE *) NULL) {
	/* try 'loadpath' variable */
	char *fullname = NULL, *path;

	while ((path = get_loadpath()) != NULL) {
	    /* length of path, dir separator, filename, \0 */
	    fullname = gp_realloc(fullname, strlen(path) + 1 + strlen(filename) + 1, "loadpath_fopen");
	    strcpy(fullname, path);
	    PATH_CONCAT(fullname, filename);
	    if ((fp = fopen(fullname, mode)) != NULL) {
		free(fullname);
		fullname = NULL;
		/* reset loadpath internals!
		 * maybe this can be replaced by calling get_loadpath with
		 * a NULL argument and some loadpath_handler internal logic */
		while (get_loadpath());
		break;
	    }
	}

	if (fullname)
	    free(fullname);

    }

#ifdef _Windows
    if (fp != NULL)
	setmode(fileno(fp), _O_BINARY);
#endif
    return fp;
}


/* Harald Harders <h.harders@tu-bs.de> */
/* Thanks to John Bollinger <jab@bollingerbands.com> who has tested the
   windows part */
static char *
recursivefullname(const char *path, const char *filename, TBOOLEAN recursive)
{
    char *fullname = NULL;
    FILE *fp;

    /* length of path, dir separator, filename, \0 */
    fullname = gp_alloc(strlen(path) + 1 + strlen(filename) + 1,
			"recursivefullname");
    strcpy(fullname, path);
    PATH_CONCAT(fullname, filename);

    if ((fp = fopen(fullname, "r")) != NULL) {
	fclose(fp);
	return fullname;
    } else {
	free(fullname);
	fullname = NULL;
    }

    if (recursive) {
#ifdef HAVE_DIRENT_H
	DIR *dir;
	struct dirent *direntry;
	struct stat buf;

	dir = opendir(path);
	if (dir) {
	    while ((direntry = readdir(dir)) != NULL) {
		char *fulldir = gp_alloc(strlen(path) + 1 + strlen(direntry->d_name) + 1,
					 "fontpath_fullname");
		strcpy(fulldir, path);
#  if defined(VMS)
		if (fulldir[strlen(fulldir) - 1] == ']')
		    fulldir[strlen(fulldir) - 1] = '\0';
		strcpy(&(fulldir[strlen(fulldir)]), ".");
		strcpy(&(fulldir[strlen(fulldir)]), direntry->d_name);
		strcpy(&(fulldir[strlen(fulldir)]), "]");
#  else
		PATH_CONCAT(fulldir, direntry->d_name);
#  endif
		stat(fulldir, &buf);
		if ((S_ISDIR(buf.st_mode)) &&
		    (strcmp(direntry->d_name, ".") != 0) &&
		    (strcmp(direntry->d_name, "..") != 0)) {
		    fullname = recursivefullname(fulldir, filename, TRUE);
		    if (fullname != NULL)
			break;
		}
		free(fulldir);
	    }
	    closedir(dir);
	}
#elif defined(_Windows)
	HANDLE filehandle;
	WIN32_FIND_DATA finddata;
	char *pathwildcard = gp_alloc(strlen(path) + 2, "fontpath_fullname");

	strcpy(pathwildcard, path);
	PATH_CONCAT(pathwildcard, "*");

	filehandle = FindFirstFile(pathwildcard, &finddata);
	free(pathwildcard);
	if (filehandle != INVALID_HANDLE_VALUE)
	    do {
		if ((finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
		    (strcmp(finddata.cFileName, ".") != 0) &&
		    (strcmp(finddata.cFileName, "..") != 0)) {
		    char *fulldir = gp_alloc(strlen(path) + 1 + strlen(finddata.cFileName) + 1,
					     "fontpath_fullname");
		    strcpy(fulldir, path);
		    PATH_CONCAT(fulldir, finddata.cFileName);

		    fullname = recursivefullname(fulldir, filename, TRUE);
		    free(fulldir);
		    if (fullname != NULL)
			break;
		}
	    } while (FindNextFile(filehandle, &finddata) != 0);
	FindClose(filehandle);

#else
	int_warn(NO_CARET, "Recursive directory search not supported\n\t('%s!')", path);
#endif
    }
    return fullname;
}


/* may return NULL */
char *
fontpath_fullname(const char *filename)
{
    FILE *fp;
    char *fullname = NULL;

#if defined(PIPES)
    if (*filename == '<') {
	os_error(NO_CARET, "fontpath_fullname: No Pipe allowed");
    } else
#endif /* PIPES */
    if ((fp = fopen(filename, "r")) == (FILE *) NULL) {
	/* try 'fontpath' variable */
	char *tmppath, *path = NULL;

	while ((tmppath = get_fontpath()) != NULL) {
	    TBOOLEAN subdirs = FALSE;
	    path = gp_strdup(tmppath);
	    if (path[strlen(path) - 1] == '!') {
		path[strlen(path) - 1] = '\0';
		subdirs = TRUE;
	    }			/* if */
	    fullname = recursivefullname(path, filename, subdirs);
	    if (fullname != NULL) {
		while (get_fontpath());
		free(path);
		break;
	    }
	    free(path);
	}

    } else
	fullname = gp_strdup(filename);

    return fullname;
}


/* Push current terminal.
 * Called 1. in main(), just after init_terminal(),
 *        2. from load_rcfile(),
 *        3. anytime by user command "set term push".
 */
static char *push_term_name = NULL;
static char *push_term_opts = NULL;

void
push_terminal(int is_interactive)
{
    if (term) {
	free(push_term_name);
	free(push_term_opts);
	push_term_name = gp_strdup(term->name);
	push_term_opts = gp_strdup(term_options);
	if (is_interactive)
	    fprintf(stderr, "   pushed terminal %s %s\n", push_term_name, push_term_opts);
    } else {
	if (is_interactive)
	    fputs("\tcurrent terminal type is unknown\n", stderr);
    }
}

/* Pop the terminal.
 * Called anytime by user command "set term pop".
 */
void
pop_terminal()
{
    if (push_term_name != NULL) {
	char *s;
	int i = strlen(push_term_name) + 11;
	if (push_term_opts) {
	    /* do_string() does not like backslashes -- thus remove them */
	    for (s=push_term_opts; *s; s++)
		if (*s=='\\' || *s=='\n') *s=' ';
	    i += strlen(push_term_opts);
	}
	s = gp_alloc(i, "pop");
	i = interactive;
	interactive = 0;
	sprintf(s,"set term %s %s", push_term_name, (push_term_opts ? push_term_opts : ""));
	do_string_and_free(s);
	interactive = i;
	if (interactive)
	    fprintf(stderr,"   restored terminal is %s %s\n", term->name, ((*term_options) ? term_options : ""));
    } else
	fprintf(stderr,"No terminal has been pushed yet\n");
}


/* Parse a plot style. Used by 'set style {data|function}' and by (s)plot.  */
enum PLOT_STYLE
get_style()
{
    /* defined in plot.h */
    enum PLOT_STYLE ps;

    c_token++;

    ps = lookup_table(&plotstyle_tbl[0], c_token);

    c_token++;

    if (ps == PLOT_STYLE_NONE)
	int_error(c_token, "unrecognized plot type");

    return ps;
}

/* Parse options for style filledcurves and fill fco accordingly.
 * If no option given, then set fco->opt_given to 0.
 */
void
get_filledcurves_style_options(filledcurves_opts *fco)
{
    int p;
    p = lookup_table(&filledcurves_opts_tbl[0], c_token);

    if (p == FILLEDCURVES_ABOVE) {
	fco->oneside = 1;
	p = lookup_table(&filledcurves_opts_tbl[0], ++c_token);
    } else if (p == FILLEDCURVES_BELOW) {
	fco->oneside = -1;
	p = lookup_table(&filledcurves_opts_tbl[0], ++c_token);
    } else
	fco->oneside = 0;

    if (p == -1) {
	fco->opt_given = 0;
	return;			/* no option given */
    } else
	fco->opt_given = 1;

    c_token++;

    fco->closeto = p;
    fco->at = 0;
    if (!equals(c_token, "="))
	return;
    /* parameter required for filledcurves x1=... and friends */
    if (p < FILLEDCURVES_ATXY)
	fco->closeto += 4;
    c_token++;
    fco->at = real_expression();
    if (p != FILLEDCURVES_ATXY)
	return;
    /* two values required for FILLEDCURVES_ATXY */
    if (!equals(c_token, ","))
	int_error(c_token, "syntax is xy=<x>,<y>");
    c_token++;
    fco->aty = real_expression();
    return;
}

/* Print filledcurves style options to a file (used by 'show' and 'save'
 * commands).
 */
void
filledcurves_options_tofile(filledcurves_opts *fco, FILE *fp)
{
    if (!fco->opt_given)
	return;
    if (fco->oneside)
	fputs(fco->oneside > 0 ? "above " : "below ", fp);
    if (fco->closeto == FILLEDCURVES_CLOSED) {
	fputs("closed", fp);
	return;
    }
    if (fco->closeto <= FILLEDCURVES_Y2) {
	fputs(filledcurves_opts_tbl[fco->closeto].key, fp);
	return;
    }
    if (fco->closeto <= FILLEDCURVES_ATY2) {
	fprintf(fp, "%s=%g", filledcurves_opts_tbl[fco->closeto - 4].key, fco->at);
	return;
    }
    if (fco->closeto == FILLEDCURVES_ATXY) {
	fprintf(fp, "xy=%g,%g", fco->at, fco->aty);
	return;
    }
}

TBOOLEAN
need_fill_border(struct fill_style_type *fillstyle)
{
    struct lp_style_type p;
    p.pm3d_color = fillstyle->border_color;

    if (p.pm3d_color.type == TC_LT) {
	/* Doesn't want a border at all */
	if (p.pm3d_color.lt == LT_NODRAW)
	    return FALSE;
	load_linetype(&p, p.pm3d_color.lt+1);
    }

    /* Wants a border in a new color */
    if (p.pm3d_color.type != TC_DEFAULT)
	apply_pm3dcolor(&p.pm3d_color,term);
    
    return TRUE;
}

int
parse_dashtype(struct t_dashtype *dt)
{
    int res = DASHTYPE_SOLID;
    int j = 0;
    int k = 0;
    char *dash_str = NULL;

    /* Erase any previous contents */
    memset(dt, 0, sizeof(struct t_dashtype));

    /* Fill in structure based on keyword ... */ 
    if (equals(c_token, "solid")) {
	res = DASHTYPE_SOLID;
	c_token++;

    /* Or numerical pattern consisting of pairs solid,empty,solid,empty... */
    } else if (equals(c_token, "(")) {
	c_token++;
	while (!END_OF_COMMAND) {
	    if (j >= DASHPATTERN_LENGTH) {
		int_error(c_token, "too many pattern elements");
	    }
	    dt->pattern[j++] = real_expression();	/* The solid portion */
	    if (!equals(c_token++, ","))
		int_error(c_token, "expecting comma");
	    dt->pattern[j++] = real_expression();	/* The empty portion */
	    if (equals(c_token, ")"))
		break;
	    if (!equals(c_token++, ","))
		int_error(c_token, "expecting comma");
	}

	if (!equals(c_token, ")"))
	    int_error(c_token, "expecting , or )");

	c_token++;
	res = DASHTYPE_CUSTOM;

    /* Or string representing pattern elements ... */
    } else if ((dash_str = try_to_get_string())) {
#define DSCALE 10.
	while (dash_str[j] && (k < DASHPATTERN_LENGTH || dash_str[j] == ' ')) {
	    /* .      Dot with short space 
	     * -      Dash with regular space
	     * _      Long dash with regular space
	     * space  Don't add new dash, just increase last space */
	    switch (dash_str[j]) {
	    case '.':
		dt->pattern[k++] = 0.2 * DSCALE;
		dt->pattern[k++] = 0.5 * DSCALE;
		break;
	    case '-':
		dt->pattern[k++] = 1.0 * DSCALE;
		dt->pattern[k++] = 1.0 * DSCALE;
		break;
	    case '_':
		dt->pattern[k++] = 2.0 * DSCALE;
		dt->pattern[k++] = 1.0 * DSCALE;
		break;
	    case ' ':
		if (k > 0)
		dt->pattern[k-1] += 1.0 * DSCALE;
		break;
	    default:
		int_error(c_token - 1, "expecting one of . - _ or space");
	    }
	    j++;
#undef  DSCALE
	}
	/* truncate dash_str if we ran out of space in the array representation */
	dash_str[j] = '\0';
	strncpy(dt->dstring, dash_str, sizeof(dt->dstring)-1);
	free(dash_str);
	res = DASHTYPE_CUSTOM;

    /* Or index of previously defined dashtype */
    /* FIXME: Is the index enough or should we copy its contents into this one? */
    /* FIXME: What happens if there is a recursive definition? */
    } else {
	res = int_expression();
	if (res < 0)
	    int_error(c_token - 1, "dashtype must be non-negative");
	if (res == 0)
	    res = DASHTYPE_AXIS;
	else
	    res = res - 1;
    }

    return res;
}

/*
 * destination_class tells us whether we are filling in a line style ('set style line'),
 * a persistant linetype ('set linetype') or an ad hoc set of properties for a single
 * use ('plot ... lc foo lw baz').
 * allow_point controls whether we accept a point attribute in this lp_style.
 */
int
lp_parse(struct lp_style_type *lp, lp_class destination_class, TBOOLEAN allow_point)
{
    /* keep track of which options were set during this call */
    int set_lt = 0, set_pal = 0, set_lw = 0; 
    int set_pt = 0, set_ps  = 0, set_pi = 0;
    int set_dt = 0;
    int new_lt = 0;

    /* EAM Mar 2010 - We don't want properties from a user-defined default
     * linetype to override properties explicitly set here.  So fill in a
     * local lp_style_type as we go and then copy over the specifically
     * requested properties on top of the default ones.                                           
     */
    struct lp_style_type newlp = *lp;
	
    if ((destination_class == LP_ADHOC)
    && (almost_equals(c_token, "lines$tyle") || equals(c_token, "ls"))) {
	c_token++;
	lp_use_properties(lp, int_expression());
    } 
    
    while (!END_OF_COMMAND) {

	/* This special case is to flag an attemp to "set object N lt <lt>",
	 * which would otherwise be accepted but ignored, leading to confusion
	 * FIXME:  Couldn't this be handled at a higher level?
	 */
	if ((destination_class == LP_NOFILL)
	&&  (equals(c_token,"lt") || almost_equals(c_token,"linet$ype"))) {
	    int_error(c_token, "object linecolor must be set using fillstyle border");
	}

	if (almost_equals(c_token, "linet$ype") || equals(c_token, "lt")) {
	    if (set_lt++)
		break;
	    if (destination_class == LP_TYPE)
		int_error(c_token, "linetype definition cannot use linetype");
	    c_token++;
	    if (almost_equals(c_token, "rgb$color")) {
		if (set_pal++)
		    break;
		c_token--;
		parse_colorspec(&(newlp.pm3d_color), TC_RGB);
	    } else
		/* both syntaxes allowed: 'with lt pal' as well as 'with pal' */
		if (almost_equals(c_token, "pal$ette")) {
		    if (set_pal++)
			break;
		    c_token--;
		    parse_colorspec(&(newlp.pm3d_color), TC_Z);
		} else if (equals(c_token,"bgnd")) {
		    *lp = background_lp;
		    c_token++;
		} else if (equals(c_token,"black")) {
		    *lp = default_border_lp;
		    c_token++;
		} else if (equals(c_token,"nodraw")) {
		    lp->l_type = LT_NODRAW;
		    c_token++;
		} else {
		    /* These replace the base style */
		    new_lt = int_expression();
		    lp->l_type = new_lt - 1;
		    /* user may prefer explicit line styles */
		    if (prefer_line_styles && (destination_class != LP_STYLE))
			lp_use_properties(lp, new_lt);
		    else
			load_linetype(lp, new_lt);
		}
	} /* linetype, lt */

	/* both syntaxes allowed: 'with lt pal' as well as 'with pal' */
	if (almost_equals(c_token, "pal$ette")) {
	    if (set_pal++)
		break;
	    c_token--;
	    parse_colorspec(&(newlp.pm3d_color), TC_Z);
	    continue;
	}

	/* This is so that "set obj ... lw N fc <colorspec>" doesn't eat up the
	 * fc colorspec as a line property.  We need to parse it later as a
	 * _fill_ property. Also prevents "plot ... fc <col1> fs <foo> lw <baz>"
	 * from generating an error claiming redundant line properties.
	 */
	if ((destination_class == LP_NOFILL || destination_class == LP_ADHOC)
	&&  (equals(c_token,"fc") || almost_equals(c_token,"fillc$olor")))
	    break;

	if (equals(c_token,"lc") || almost_equals(c_token,"linec$olor")
	||  equals(c_token,"fc") || almost_equals(c_token,"fillc$olor")
	   ) {
	    if (set_pal++)
		break;
	    c_token++;
	    if (almost_equals(c_token, "rgb$color") || isstring(c_token)) {
		c_token--;
		parse_colorspec(&(newlp.pm3d_color), TC_RGB);
	    } else if (almost_equals(c_token, "pal$ette")) {
		c_token--;
		parse_colorspec(&(newlp.pm3d_color), TC_Z);
	    } else if (equals(c_token,"bgnd")) {
		newlp.pm3d_color.type = TC_LT;
		newlp.pm3d_color.lt = LT_BACKGROUND;
		c_token++;
	    } else if (equals(c_token,"black")) {
		newlp.pm3d_color.type = TC_LT;
		newlp.pm3d_color.lt = LT_BLACK;
		c_token++;
	    } else if (almost_equals(c_token, "var$iable")) {
		c_token++;
		newlp.l_type = LT_COLORFROMCOLUMN;
		newlp.pm3d_color.type = TC_LINESTYLE;
	    } else {
		/* Pull the line colour from a default linetype, but */
		/* only if we are not in the middle of defining one! */
		if (destination_class != LP_STYLE) {
		    struct lp_style_type temp;
		    load_linetype(&temp, int_expression());
		    newlp.pm3d_color = temp.pm3d_color;
		} else {
		    newlp.pm3d_color.type = TC_LT;
		    newlp.pm3d_color.lt = int_expression() - 1;
		}
	    }
	    continue;
	}

	if (almost_equals(c_token, "linew$idth") || equals(c_token, "lw")) {
	    if (set_lw++)
		break;
	    c_token++;
	    newlp.l_width = real_expression();
	    if (newlp.l_width < 0)
		newlp.l_width = 0;
	    continue;
	}

	if (equals(c_token,"bgnd")) {
	    if (set_lt++)
		break;;
	    c_token++;
	    *lp = background_lp;
	    continue;
	}

	if (equals(c_token,"black")) {
	    if (set_lt++)
		break;;
	    c_token++;
	    *lp = default_border_lp;
	    continue;
	}

	if (almost_equals(c_token, "pointt$ype") || equals(c_token, "pt")) {
	    if (allow_point) {
		char *symbol;
		if (set_pt++)
		    break;
		c_token++;
		if ((symbol = try_to_get_string())) {
		    newlp.p_type = PT_CHARACTER;
		    /* An alternative mechanism would be to use
		     * utf8toulong(&newlp.p_char, symbol);
		     */
		    strncpy((char *)(&newlp.p_char), symbol, 3);
		    /* Truncate ascii text to single character */
		    if ((((char *)&newlp.p_char)[0] & 0x80) == 0)
			((char *)&newlp.p_char)[1] = '\0';
		    /* UTF-8 characters may use up to 3 bytes */
		    ((char *)&newlp.p_char)[3] = '\0';
		    free(symbol);
		} else {
		    newlp.p_type = int_expression() - 1;
		}
	    } else {
		int_warn(c_token, "No pointtype specifier allowed, here");
		c_token += 2;
	    }
	    continue;
	}

	if (almost_equals(c_token, "points$ize") || equals(c_token, "ps")) {
	    if (allow_point) {
		if (set_ps++)
		    break;
		c_token++;
		if (almost_equals(c_token, "var$iable")) {
		    newlp.p_size = PTSZ_VARIABLE;
		    c_token++;
		} else if (almost_equals(c_token, "def$ault")) {
		    newlp.p_size = PTSZ_DEFAULT;
		    c_token++;
		} else {
		    newlp.p_size = real_expression();
		    if (newlp.p_size < 0)
			newlp.p_size = 0;
		}
	    } else {
		int_warn(c_token, "No pointsize specifier allowed, here");
		c_token += 2;
	    }
	    continue;
	}

	if (almost_equals(c_token, "pointi$nterval") || equals(c_token, "pi")) {
	    c_token++;
	    if (allow_point) {
		newlp.p_interval = int_expression();
		set_pi = 1;
	    } else {
		int_warn(c_token, "No pointinterval specifier allowed, here");
		int_expression();
	    }
	    continue;
	}

	if (almost_equals(c_token, "dasht$ype") || equals(c_token, "dt")) {
	    int tmp;
	    if (set_dt++)
		break;
	    c_token++;
	    tmp = parse_dashtype(&newlp.custom_dash_pattern);
	    /* Pull the dashtype from the list of already defined dashtypes, */
	    /* but only if it we didn't get an explicit one back from parse_dashtype */ 
	    if (tmp == DASHTYPE_AXIS)
		lp->l_type = LT_AXIS;
	    if (tmp >= 0)
		tmp = load_dashtype(&newlp.custom_dash_pattern, tmp + 1);
	    newlp.d_type = tmp;
	    continue;
	}


	/* caught unknown option -> quit the while(1) loop */
	break;
    }

    if (set_lt > 1 || set_pal > 1 || set_lw > 1 || set_pt > 1 || set_ps > 1 || set_dt > 1)
	int_error(c_token, "duplicated arguments in style specification");

    if (set_pal) {
	lp->pm3d_color = newlp.pm3d_color;
	/* hidden3d uses this to decide that a single color surface is wanted */
	lp->flags |= LP_EXPLICIT_COLOR;
    } else {
	lp->flags &= ~LP_EXPLICIT_COLOR;
    }
    if (set_lw)
	lp->l_width = newlp.l_width;
    if (set_pt) {
	lp->p_type = newlp.p_type;
	lp->p_char = newlp.p_char;
    }
    if (set_ps)
	lp->p_size = newlp.p_size;
    if (set_pi)
	lp->p_interval = newlp.p_interval;
    if (newlp.l_type == LT_COLORFROMCOLUMN)
	lp->l_type = LT_COLORFROMCOLUMN;
    if (set_dt) {
	lp->d_type = newlp.d_type;
	lp->custom_dash_pattern = newlp.custom_dash_pattern;
    }	
		

    return new_lt;
}

/* <fillstyle> = {empty | solid {<density>} | pattern {<n>}} {noborder | border {<lt>}} */
const struct gen_table fs_opt_tbl[] = {
    {"e$mpty", FS_EMPTY},
    {"s$olid", FS_SOLID},
    {"p$attern", FS_PATTERN},
    {NULL, -1}
};

void
parse_fillstyle(struct fill_style_type *fs, int def_style, int def_density, int def_pattern, 
		t_colorspec def_bordertype)
{
    TBOOLEAN set_fill = FALSE;
    TBOOLEAN set_border = FALSE;
    TBOOLEAN transparent = FALSE;

    /* Set defaults */
    fs->fillstyle = def_style;
    fs->filldensity = def_density;
    fs->fillpattern = def_pattern;
    fs->border_color = def_bordertype;

    if (END_OF_COMMAND)
	return;
    if (!equals(c_token, "fs") && !almost_equals(c_token, "fill$style"))
	return;
    c_token++;

    while (!END_OF_COMMAND) {
	int i;

	if (almost_equals(c_token, "trans$parent")) {
	    transparent = TRUE;
	    c_token++;
	    continue;
	}

	i = lookup_table(fs_opt_tbl, c_token);
	switch (i) {
	    default:
		 break;

	    case FS_EMPTY:
	    case FS_SOLID:
	    case FS_PATTERN:

		if (set_fill && fs->fillstyle != i)
		    int_error(c_token, "conflicting option");
		fs->fillstyle = i;
		set_fill = TRUE;
		c_token++;
		
		if (isanumber(c_token) || type_udv(c_token) == INTGR || type_udv(c_token) == CMPLX) {
		    if (fs->fillstyle == FS_SOLID) {
			/* user sets 0...1, but is stored as an integer 0..100 */
			fs->filldensity = 100.0 * real_expression() + 0.5;
			if (fs->filldensity < 0)
			    fs->filldensity = 0;
			if (fs->filldensity > 100)
			    fs->filldensity = 100;
		    } else if (fs->fillstyle == FS_PATTERN) {
			fs->fillpattern = int_expression();
			if (fs->fillpattern < 0)
			    fs->fillpattern = 0;
		    } else
			int_error(c_token, "this fill style does not have a parameter");
		}
		continue;
	}

	if (almost_equals(c_token, "bo$rder")) {
	    if (set_border && fs->border_color.lt == LT_NODRAW)
		int_error(c_token, "conflicting option");
	    fs->border_color.type = TC_DEFAULT;
	    set_border = TRUE;
	    c_token++;
	    if (END_OF_COMMAND)
		continue;
	    if (equals(c_token,"-") || isanumber(c_token)) {
		fs->border_color.type = TC_LT;
		fs->border_color.lt = int_expression() - 1;
	    } else if (equals(c_token,"lc") || almost_equals(c_token,"linec$olor")) {
		parse_colorspec(&fs->border_color, TC_Z);
	    } else if (equals(c_token,"rgb")
		   ||  equals(c_token,"lt") || almost_equals(c_token,"linet$ype")) {
		c_token--;
		parse_colorspec(&fs->border_color, TC_Z);
	    }
	    continue;
	} else if (almost_equals(c_token, "nobo$rder")) {
	    if (set_border && fs->border_color.lt != LT_NODRAW)
		int_error(c_token, "conflicting option");
	    fs->border_color.type = TC_LT;
	    fs->border_color.lt = LT_NODRAW;
	    set_border = TRUE;
	    c_token++;
	    continue;
	}

	/* Keyword must belong to someone else */
	break;
    }
    if (transparent) {
	if (fs->fillstyle == FS_SOLID)
	    fs->fillstyle = FS_TRANSPARENT_SOLID;
        else if (fs->fillstyle == FS_PATTERN)
	    fs->fillstyle = FS_TRANSPARENT_PATTERN;
    }
}

/*
 * Parse the sub-options of text color specification
 *   { def$ault | lt <linetype> | pal$ette { cb <val> | frac$tion <val> | z }
 * The ordering of alternatives shown in the line above is kept in the symbol definitions
 * TC_DEFAULT TC_LT TC_LINESTYLE TC_RGB TC_CB TC_FRAC TC_Z TC_VARIABLE (0 1 2 3 4 5 6 7)
 * and the "options" parameter to parse_colorspec limits legal input to the
 * corresponding point in the series. So TC_LT allows only default or linetype
 * coloring, while TC_Z allows all coloring options up to and including pal z
 */
void
parse_colorspec(struct t_colorspec *tc, int options)
{
    c_token++;
    if (END_OF_COMMAND)
	int_error(c_token, "expected colorspec");
    if (almost_equals(c_token,"def$ault")) {
	c_token++;
	tc->type = TC_DEFAULT;
    } else if (equals(c_token,"bgnd")) {
	c_token++;
	tc->type = TC_LT;
	tc->lt = LT_BACKGROUND;
    } else if (equals(c_token,"black")) {
	c_token++;
	tc->type = TC_LT;
	tc->lt = LT_BLACK;
    } else if (equals(c_token,"lt")) {
	struct lp_style_type lptemp;
	c_token++;
	if (END_OF_COMMAND)
	    int_error(c_token, "expected linetype");
	tc->type = TC_LT;
	tc->lt = int_expression()-1;
	if (tc->lt < LT_BACKGROUND) {
	    tc->type = TC_DEFAULT;
	    int_warn(c_token,"illegal linetype");
	}

	/*
	 * July 2014 - translate linetype into user-defined linetype color.
	 * This is a CHANGE!
	 * FIXME: calling load_linetype here may obviate the need to call it
	 * many places in the higher level code.  They could be removed.
	 */
	load_linetype(&lptemp, tc->lt + 1);
	*tc = lptemp.pm3d_color;
    } else if (options <= TC_LT) {
	tc->type = TC_DEFAULT;
	int_error(c_token, "only tc lt <n> possible here");
    } else if (equals(c_token,"ls") || almost_equals(c_token,"lines$tyle")) {
	c_token++;
	tc->type = TC_LINESTYLE;
	tc->lt = real_expression();
    } else if (almost_equals(c_token,"rgb$color")) {
	c_token++;
	tc->type = TC_RGB;
	if (almost_equals(c_token, "var$iable")) {
	    tc->value = -1.0;
	    c_token++;
	} else {
	    tc->value = 0.0;
	    tc->lt = parse_color_name();
	}
    } else if (almost_equals(c_token,"pal$ette")) {
	c_token++;
	if (equals(c_token,"z")) {
	    /* The actual z value is not yet known, fill it in later */
	    if (options >= TC_Z) {
		tc->type = TC_Z;
	    } else {
		tc->type = TC_DEFAULT;
		int_error(c_token, "palette z not possible here");
	    }
	    c_token++;
	} else if (equals(c_token,"cb")) {
	    tc->type = TC_CB;
	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "expected cb value");
	    tc->value = real_expression();
	} else if (almost_equals(c_token,"frac$tion")) {
	    tc->type = TC_FRAC;
	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "expected palette fraction");
	    tc->value = real_expression();
	    if (tc->value < 0. || tc->value > 1.0)
		int_error(c_token, "palette fraction out of range");
	} else {
	    /* END_OF_COMMAND or palette <blank> */
	    if (options >= TC_Z)
		tc->type = TC_Z;
	}
    } else if (options >= TC_VARIABLE && almost_equals(c_token,"var$iable")) {
	tc->type = TC_VARIABLE;
	c_token++;

    /* New: allow to skip the rgb keyword, as in  'plot $foo lc "blue"' */
    } else if (isstring(c_token)) {
	tc->type = TC_RGB;
	tc->lt = parse_color_name();

    } else {
	int_error(c_token, "colorspec option not recognized");
    }
}

long
parse_color_name()
{
    char *string;
    long color = -2;

    /* Terminal drivers call this after seeing a "background" option */
    if (almost_equals(c_token,"rgb$color") && almost_equals(c_token-1,"back$ground"))
	c_token++;
    if ((string = try_to_get_string())) {
	int iret;
	iret = lookup_table_nth(pm3d_color_names_tbl, string);
	if (iret >= 0)
	    color = pm3d_color_names_tbl[iret].value;
	else if (string[0] == '#')
	    iret = sscanf(string,"#%lx",&color);
	else if (string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))
	    iret = sscanf(string,"%lx",&color);
	free(string);
	if (color == -2)
	    int_error(c_token, "unrecognized color name and not a string \"#AARRGGBB\" or \"0xAARRGGBB\"");
    } else {
	color = int_expression();
    }

    return (unsigned int)(color);
}

/* arrow parsing...
 *
 * allow_as controls whether we are allowed to accept arrowstyle in
 * the current context [ie not when doing a  set style arrow command]
 */

void
arrow_use_properties(struct arrow_style_type *arrow, int tag)
{
    /*  This function looks for an arrowstyle defined by 'tag' and
     *  copies its data into the structure 'ap'. */
    struct arrowstyle_def *this;

    this = first_arrowstyle;
    while (this != NULL) {
	if (this->tag == tag) {
	    *arrow = this->arrow_properties;
	    return;
	} else {
	    this = this->next;
	}
    }

    /* tag not found: */
    default_arrow_style(arrow);
    int_warn(NO_CARET,"arrowstyle %d not found", tag);
}

void
arrow_parse(
    struct arrow_style_type *arrow,
    TBOOLEAN allow_as)
{
    int set_layer=0, set_line=0, set_head=0;
    int set_headsize=0, set_headfilled=0;

    /* Use predefined arrow style */
    if (allow_as && (almost_equals(c_token, "arrows$tyle") ||
		     equals(c_token, "as"))) {
	c_token++;
	if (almost_equals(c_token, "var$iable")) {
	    arrow->tag = AS_VARIABLE;
	    c_token++;
	} else {
	    arrow_use_properties(arrow, int_expression());
	}
	return;
    }

    /* No predefined arrow style; read properties from command line */
    /* avoid duplicating options */
    while (!END_OF_COMMAND) {
	if (equals(c_token, "nohead")) {
	    if (set_head++)
		break;
	    c_token++;
	    arrow->head = NOHEAD;
	    continue;
	}
	if (equals(c_token, "head")) {
	    if (set_head++)
		break;
	    c_token++;
	    arrow->head = END_HEAD;
	    continue;
	}
	if (equals(c_token, "backhead")) {
	    if (set_head++)
		break;
	    c_token++;
	    arrow->head = BACKHEAD;
	    continue;
	}
	if (equals(c_token, "heads")) {
	    if (set_head++)
		break;
	    c_token++;
	    arrow->head = BACKHEAD | END_HEAD;
	    continue;
	}

	if (almost_equals(c_token, "nobo$rder")) {
	    if (set_headfilled++)
		break;
	    c_token++;
	    arrow->headfill = AS_NOBORDER;
	    continue;
	}
	if (almost_equals(c_token, "fill$ed")) {
	    if (set_headfilled++)
		break;
	    c_token++;
	    arrow->headfill = AS_FILLED;
	    continue;
	}
	if (almost_equals(c_token, "empty")) {
	    if (set_headfilled++)
		break;
	    c_token++;
	    arrow->headfill = AS_EMPTY;
	    continue;
	}
	if (almost_equals(c_token, "nofill$ed")) {
	    if (set_headfilled++)
		break;
	    c_token++;
	    arrow->headfill = AS_NOFILL;
	    continue;
	}

	if (equals(c_token, "size")) {
	    struct position hsize;
	    if (set_headsize++)
		break;
	    hsize.scalex = hsize.scaley = hsize.scalez = first_axes;
	    /* only scalex used; scaley is angle of the head in [deg] */
	    c_token++;
	    if (END_OF_COMMAND)
		int_error(c_token, "head size expected");
	    get_position(&hsize);
	    arrow->head_length = hsize.x;
	    arrow->head_lengthunit = hsize.scalex;
	    arrow->head_angle = hsize.y;
	    arrow->head_backangle = hsize.z;
	    /* invalid backangle --> default of 90.0 degrees */
	    if (arrow->head_backangle <= arrow->head_angle)
		arrow->head_backangle = 90.0;
	    
	    /* Assume adjustable size but check for 'fixed' instead */
	    arrow->head_fixedsize = FALSE;
	    continue;
	}

	if (almost_equals(c_token, "fix$ed")) {
	    arrow->head_fixedsize = TRUE;
	    c_token++;
	    continue;
	}

	if (equals(c_token, "back")) {
	    if (set_layer++)
		break;
	    c_token++;
	    arrow->layer = LAYER_BACK;
	    continue;
	}
	if (equals(c_token, "front")) {
	    if (set_layer++)
		break;
	    c_token++;
	    arrow->layer = LAYER_FRONT;
	    continue;
	}

	/* pick up a line spec - allow ls, but no point. */
	{
	    int stored_token = c_token;
	    lp_parse(&arrow->lp_properties, LP_ADHOC, FALSE);
	    if (stored_token == c_token || set_line++)
		break;
	    continue;
	}

	/* unknown option caught -> quit the while(1) loop */
	break;
    }

    if (set_layer>1 || set_line>1 || set_head>1 || set_headsize>1 || set_headfilled>1)
	int_error(c_token, "duplicated arguments in style specification");
}

void
get_image_options(t_image *image)
{
    if (almost_equals(c_token, "pix$els") || equals(c_token, "failsafe")) {
	c_token++;
	image->fallback = TRUE;
    }

}
