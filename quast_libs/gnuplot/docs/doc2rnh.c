#ifndef lint
static char *RCSid() { return RCSid("$Id: doc2rnh.c,v 1.18 2011/12/08 20:53:03 sfeam Exp $"); }
#endif

/* GNUPLOT - doc2rnh.c */

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
 * doc2rnh.c  -- program to convert Gnuplot .DOC format to
 *               Digital Standard Runoff for VMS HELP files
 *               (gnuplot.doc, including the terminal documentation
 *                is no longer formated for VMS HELP by default)
 *
 * From hlp2ms by Thomas Williams
 *
 * Modified by Russell Lang, 2nd October 1989
 * to make vms help level 1 and 2 create the same ms section level.
 *
 * Modified to become doc2ms by David Kotz (David.Kotz@Dartmouth.edu) 12/89
 * Added table and backquote support.
 *
 * Adapted from doc2ms.c (the unix 'runoff' text-processor)
 * by Lucas Hart 3/97
 *
 * right margin is adjusted two spaces for each level to compensate
 * for the indentation by VMS HELP
 *
 * the page width can be adjusted by changing the value of DSR_RM
 * usage: $ MCR []doc2rnh gnuplot.doc gnuplot.rnh
 *        $ RUNOFF gnuplot.rnh
 *
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"
#include "stdfn.h"
#include "doc2x.h"

extern TBOOLEAN single_top_level;

#define LINE_SKIP		3
#define DSR_RM		70

void init __PROTO((FILE *));
void convert __PROTO((FILE *, FILE *));
void process_line __PROTO((char *, FILE *));
void section __PROTO((char *, FILE *));
void putrnh __PROTO((char *, FILE *));
void putrnh_ __PROTO((char *, FILE *));
void finish __PROTO((FILE *));

static TBOOLEAN intable = FALSE;
static TBOOLEAN rnh_table = FALSE;
static TBOOLEAN initial_entry = FALSE;

int
main (int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;
    infile = stdin;
    outfile = stdout;

    single_top_level = TRUE;

    if (argc > 3) {
	fprintf(stderr, "Usage: %s [infile [outfile]]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if (argc >= 2) {
	if ((infile = fopen(argv[1], "r")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for reading\n",
		    argv[0], argv[1]);
	    exit(EXIT_FAILURE);
	}
    }
    if (argc == 3) {
	if ((outfile = fopen(argv[2], "w")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for writing\n",
		    argv[0], argv[2]);
	    exit(EXIT_FAILURE);
	}
    }
    init(outfile);
    convert(infile, outfile);
    finish(outfile);
    return EXIT_SUCCESS;
}


void
init(FILE *b)
{
    /*     */
    (void) fputs("\
.no paging\n\
.no flags all\n\
.left margin 1\n\
.right margin 70\n\
.no justify\n", b);
}


void
convert(FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN+1];

    while (get_line(line, sizeof(line), a)) {
	process_line(line, b);
    }
}

void
process_line(char *line, FILE *b)
{
    switch (line[0]) {		/* control character */
    case '?':{			/* interactive help entry */
	    break;		/* ignore */
	}
    case '@':{			/* start/end table */
	    if (rnh_table) {
		(void) fputs(".end literal\n", b);
		rnh_table = FALSE;
		intable = FALSE;
	    } else {
/*                       (void) fputs(".literal\n",b);  */
		intable = TRUE;
		rnh_table = FALSE;
		initial_entry = TRUE;
	    }
	    /* ignore rest of line */
	    break;
	}
    case '^':{			/* html table entry */
	    break;		/* ignore */
	}
    case '=':			/* latex index entry */
    case 'F':			/* latex embedded figure */
    case '#':{			/* latex table entry */
	    break;		/* ignore */
	}
    case '%':{			/* troff table entry */
	    break;		/* ignore */
	}
    case '\n':			/* empty text line */
    case ' ':{			/* normal text line */

/* most tables are simple; no flags means no protected characters
 * other than period (command indicator) in first column
 *
 * However, for ease of maintainence, two tables have sublevels
 * and descriptions, corresponding to the printed table entries,
 * encapsulated by the table markers.  Therefore we need to
 * do some more work.
 *
 * Doc2hlp just ignores the table headings and treats
 * lower levels irrespectively, but we need to break
 * the level designators out of the table format.
 *
 *  The first entry in a table will have either
 *     - a level number in the first column => remainder of text in
 *                                             table is help text
 *     - spaces in the first two columns => rest of text is literal
 *                                          to be placed in table format
 *
 */

/* use the "cleartext" table or other text in tables */

/*                if (intable)  { */ /* its already literal */
	    if (rnh_table) {	/* its a literal */
		putrnh(line + 1, b);
		break;
	    }
	    switch (line[1]) {
	    case ' ':{
		    if ((intable) && (initial_entry)) {
			rnh_table = TRUE;
			initial_entry = FALSE;
			fputs(".literal\n", b);
			putrnh(line + 1, b);
			break;
		    }
		    /* verbatim mode */
		    fputs(".literal\n", b);
		    putrnh(line + 1, b);
		    fputs(".end literal\n", b);
		    break;
		}

/*
 *  "." in first column is the DSR command character;
 *  therefore, include the preceeding " "
 */
	    case '.':{
		    putrnh(line, b);
		    break;
		}
	    default:{
		    if (line[0] == '\n')
			fputs(".skip\n", b);	/* totally blank line */
		    else
			putrnh(line + 1, b);
		    break;
		}
		break;
	    }
	    break;
	}
    default:{
	    if (isdigit((int) line[0])) {	/* start of section */

/* some HELP text is surrounded by table flags */
/* doc2rnh will ignore the flags */

		if (intable) {
		    if (initial_entry) {
			initial_entry = FALSE;
			rnh_table = FALSE;
		    }
		}
		section(line, b);
	    } else
		fprintf(stderr, "unknown control code '%c' in column 1\n",
			line[0]);
	    break;
	}
    }
}


/* process a line with a digit control char */
/* starts a new [sub]section */
/* We want to retain section number, so its simpler than w/ TeX or roff */

void
section(char *line, FILE *b)
{
    int sh_i;
    static int old = 1;
/*
   (void) sscanf(line,"%d",&sh_i);
   *
   * check to make sure this works with terminals also
 */
    sh_i = atoi(line);

    if (sh_i > old) {
	do
	    if (old != 1)	/* this line added by rjl */
		(void) fputs(".rm-2\n", b);
	while (++old < sh_i);
    } else if (sh_i < old) {
	do
	    if (sh_i != 1)	/* this line added by rjl */
		(void) fputs(".rm+2\n", b);
	while (--old > sh_i);
    }
    /* added by dfk to capitalize section headers */
    /* Header name starts at [2] */
/* omit for online documentation
 *    if (islower(line[2]))
 *       line[2] = toupper(line[2]);
 */
    old = sh_i;

    (void) fputs(".indent -1;\n", b);
    (void) putrnh_(line, b);
    (void) fputs(".br;\n", b);
}

/*
 * dummy function in case we need to convert some characters in
 * output string ala doc2tex and doc2ms
 */

void
putrnh(char *s, FILE *file)
{
    (void) fputs(s, file);
}

/*
 * LBR$OUTPUT_HELP treats spaces and "/"s as list separators for topics,
 * but they are used in gnuplot.doc for the printed docs; convert to
 * "_" and "|"   Modeled after section heading conversions in doc2tex
 * and doc2ms.
 *
 */

void
putrnh_(char *s, FILE *file)
{
    int i, s_len, last_chr;

    last_chr = s_len = strlen(s);

    for (i = s_len - 1; i > 2; i--) {	/* any trailing spaces to drop? */
	if (s[i] != ' ') {
	    last_chr = i;
	    break;
	}
    }

    for (i = 0; i <= last_chr; i++) {
	if (i > 2) {
	    switch (s[i]) {
	    case ' ':
		(void) fputc('_', file);
		break;
	    case '/':
		(void) fputc('|', file);
		break;
	    default:
		(void) fputc(s[i], file);
	    }
	} else {
	    (void) fputc(s[i], file);
	}
    }
}

void
finish(FILE *b)
{
    return;
}
