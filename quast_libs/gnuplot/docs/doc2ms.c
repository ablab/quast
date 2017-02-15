#ifndef lint
static char *RCSid() { return RCSid("$Id: doc2ms.c,v 1.19.6.1 2015/06/28 20:15:03 broeker Exp $"); }
#endif

/* GNUPLOT - doc2ms.c */

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
 * doc2ms.c  -- program to convert Gnuplot .DOC format to *roff -ms document
 * From hlp2ms by Thomas Williams
 *
 * Modified by Russell Lang, 2nd October 1989
 * to make vms help level 1 and 2 create the same ms section level.
 *
 * Modified to become doc2ms by David Kotz (David.Kotz@Dartmouth.edu) 12/89
 * Added table and backquote support.
 *
 * usage:  doc2ms [file.doc [file.ms]]
 *
 *   where file.doc is a VMS .DOC file, and file.ms will be a [nt]roff
 *     document suitable for printing with nroff -ms or troff -ms
 *
 * typical usage for GNUPLOT:
 *
 *   doc2ms gnuplot.doc | tbl | eqn | troff -ms
 *
 * or
 *
 *   doc2ms gnuplot.doc | groff -ms -et >gnuplot.ps
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"
#include "stdfn.h"
#include "doc2x.h"

#define LINE_SKIP		3

void init __PROTO((FILE *, char *));
void convert __PROTO((FILE *, FILE *));
void process_line __PROTO((char *, FILE *));
void section __PROTO((char *, FILE *));
void putms __PROTO((char *, FILE *));
void putms_verb __PROTO((char *, FILE *));
void finish __PROTO((FILE *));

static TBOOLEAN intable = FALSE;

int
main (int argc, char **argv)
{
    char *titlepage_filename = "titlepag.ms";
    FILE *infile;
    FILE *outfile;
    infile = stdin;
    outfile = stdout;
    
    if (argc > 4) {
	fprintf(stderr, "Usage: %s [infile [outfile [titlefile]]]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if (argc >= 2) {
	if ((infile = fopen(argv[1], "r")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for reading\n",
		    argv[0], argv[1]);
	    exit(EXIT_FAILURE);
	}
    }
    if (argc >= 3) {
	if ((outfile = fopen(argv[2], "w")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for writing\n",
		    argv[0], argv[2]);
	    exit(EXIT_FAILURE);
	}
    }
    if (argc == 4) {
	FILE *check_titlepage;
	if (! (check_titlepage = fopen(argv[3], "r"))) {
	    fprintf(stderr, "%s: Can't open %s for reading\n",
		    argv[0], argv[3]);
	    exit(EXIT_FAILURE);
	}
	titlepage_filename = argv[3];
	fclose(check_titlepage);
    }
    init(outfile, titlepage_filename);
    convert(infile, outfile);
    finish(outfile);
    return EXIT_SUCCESS;
}


void
init(FILE *b, char *t)
{
    /* in nroff, increase line length by 8 and don't adjust lines */
    (void) fprintf(b, "\
.if n \\{.nr LL +8m\n.na \\}\n\
.nr PO +0.3i\n\
.so %s\n\
.pn 1\n\
.bp\n\
.ta 1.5i 3.0i 4.5i 6.0i 7.5i\n\
\\&\n.sp 3\n.PP\n", t);

    /* following line commented out by rjl
       (void) fputs(".so intro\n",b);
     */
}


void
convert( FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN+1];

    while (get_line(line, sizeof(line), a)) {
	process_line(line, b);
    }
}

void
process_line( char *line, FILE *b)
{
    switch (line[0]) {		/* control character */
    case '?':{			/* interactive help entry */
	    break;		/* ignore */
	}
    case '@':{			/* start/end table */
	    if (intable) {
		(void) fputs(".TE\n.KE\n", b);
		(void) fputs(".EQ\ndelim off\n.EN\n\n", b);
		intable = FALSE;
	    } else {
		(void) fputs("\n.EQ\ndelim $$\n.EN\n", b);
		(void) fputs(".KS\n.TS\ncenter box tab (@) ;\n", b);
		/* moved to gnuplot.doc by RCC
		   (void) fputs("c c l .\n", b);
		 */
		intable = TRUE;
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
	    if (intable)
		(void) fputs(line + 1, b);	/* copy directly */
	    else
		fprintf(stderr, "error: %% line found outside of table\n");
	    break;
	}
    case '\n':			/* empty text line */
    case ' ':{			/* normal text line */
	    if (intable)
		break;		/* ignore while in table */
	    switch (line[1]) {
	    case ' ':{
		    /* verbatim mode */
		    fputs(".br\n", b);
		    putms_verb(line + 1, b);
		    fputs(".br\n", b);
		    break;
		}
	    case '\'':{
		    fputs("\\&", b);
		    putms(line + 1, b);
		    break;
		}
	    case '.':{		/* hide leading . from ms */
		    fputs("\\&", b);
		    putms(line + 1, b);
		    break;
		}
	    default:{
		    if (line[0] == '\n')
			putms(line, b);		/* handle totally blank line */
		    else
			putms(line + 1, b);
		    break;
		}
		break;
	    }
	    break;
	}
    default:{
	    if (isdigit((int)line[0])) {	/* start of section */
		if (!intable)	/* ignore while in table */
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

void
section( char *line, FILE *b)
{
    static char string[MAX_LINE_LEN+1];
    int sh_i;
    static int old = 1;


    (void) sscanf(line, "%d %[^\n]s", &sh_i, string);

    (void) fprintf(b, ".sp %d\n", (sh_i == 1) ? LINE_SKIP : LINE_SKIP - 1);

    if (sh_i > old) {
	do
	    if (old != 1)	/* this line added by rjl */
		(void) fputs(".RS\n.IP\n", b);
	while (++old < sh_i);
    } else if (sh_i < old) {
	do
	    if (sh_i != 1)	/* this line added by rjl */
		(void) fputs(".RE\n.br\n", b);
	while (--old > sh_i);
    }
    /* added by dfk to capitalize section headers */
    if (islower((int)string[0]))
	string[0] = toupper(string[0]);

    /* next 3 lines added by rjl */
    if (sh_i != 1)
	(void) fprintf(b, ".NH %d\n%s\n.sp 1\n.LP\n", sh_i - 1, string);
    else
	(void) fprintf(b, ".NH %d\n%s\n.sp 1\n.LP\n", sh_i, string);
    old = sh_i;

    (void) fputs(".XS\n", b);
    (void) fputs(string, b);
    (void) fputs("\n.XE\n", b);
}

void
putms( char *s, FILE *file)
{
    static TBOOLEAN inquote = FALSE;

    while (*s != NUL) {
	switch (*s) {
	case '`':{		/* backquote -> boldface */
		if (inquote) {
		    fputs("\\fR", file);
		    inquote = FALSE;
		} else {
		    fputs("\\fB", file);
		    inquote = TRUE;
		}
		break;
	    }
	case '\\':{		/* backslash */
		fputs("\\\\", file);
		break;
	    }
	case '\'':{		/* single quote */
		fputs("\\&'", file);
		break;
	    }
	default:{
		fputc(*s, file);
		break;
	    }
	}
	s++;
    }
}

/*
 * convert a verbatim line to troff input style, i.e. convert "\" to "\\"
 * (added by Alexander Lehmann 01/30/93)
 */

void
putms_verb( char *s, FILE *file)
{
    while (*s != '\0') {
	if (*s == '\\') {
	    fputc('\\', file);
	}
	fputc(*s, file);
	s++;
    }
}

/* spit out table of contents */
void
finish(FILE *b)
{
    fputs("\
.pn 1\n\
.ds RH %\n\
.af % i\n\
.bp\n.PX\n", b);
}
