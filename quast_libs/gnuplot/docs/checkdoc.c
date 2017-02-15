#ifndef lint
static char *RCSid() { return RCSid("$Id: checkdoc.c,v 1.15 2007/11/13 17:56:15 sfeam Exp $"); }
#endif

/* GNUPLOT - checkdoc.c */

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
 * checkdoc -- check a doc file for correctness of first column.
 *
 * Prints out lines that have an illegal first character.
 * First character must be space, digit, or ?, @, #, %,
 * or line must be empty.
 *
 * usage: checkdoc [docfile]
 * Modified by Russell Lang from hlp2ms.c by Thomas Williams
 *
 * Original version by David Kotz used the following one line script!
 * sed -e '/^$/d' -e '/^[ 0-9?@#%]/d' gnuplot.doc
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"
#include "stdfn.h"
#include "doc2x.h"

void convert __PROTO((FILE *, FILE *));
void process_line __PROTO((char *, FILE *));

int
main (int argc, char **argv)
{
    FILE *infile;
    infile = stdin;

    if (argc > 2) {
	fprintf(stderr, "Usage: %s [infile]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if (argc == 2)
	if ((infile = fopen(argv[1], "r")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for reading\n",
		    argv[0], argv[1]);
	    exit(EXIT_FAILURE);
	}
    convert(infile, stdout);
    return EXIT_SUCCESS;
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
    /* check matching backticks within a paragraph */

    static int count = 0;

    if (line[0] == ' ') {
	char *p = line;

	/* skip/count leading spaces */

	while (*++p == ' ');

	if (*p == '\n') {
	    /* it is not clear if this is an error, but it is an
	     * inconsistency, so flag it
	     */
	    fprintf(b, "spaces-only line %s:%d\n", termdoc_filename, termdoc_lineno);
	} else {
	    /* accumulate count of backticks. Do not check odd-ness
	     * until end of paragraph (non-space in column 1)
	     */
	    for (; *p; ++p)
		if (*p == '`')
		    ++count;
	}
    } else {
	if (count & 1) {
	    fprintf(b,
		    "mismatching backticks before %s:%d\n",
		    termdoc_filename, termdoc_lineno);
	}
	count = 0;
    }

    if (strchr(line, '\t'))
	fprintf(b, "tab character in line %s:%d\n", termdoc_filename, termdoc_lineno);

    switch (line[0]) {		/* control character */
    case '?':{			/* interactive help entry */
	    break;		/* ignore */
	}
    case '<':{			/* term docs */
	    break;		/* ignore */
	}
    case '@':{			/* start/end table */
	    break;		/* ignore */
	}
    case '#':{			/* latex table entry */
	    break;		/* ignore */
	}
    case '%':{			/* troff table entry */
	    break;		/* ignore */
	}
    case '^':{			/* html entry */
	    break;		/* ignore */
	}
    case '=':{			/* index entry */
	    break;		/* ignore */
	}
    case 'F':{			/* included figure */
	    break;		/* ignore */
	}
    case '\n':			/* empty text line */
    case ' ':{			/* normal text line */
	    break;
	}
    default:{
	    if (isdigit((int)line[0])) {	/* start of section */
		/* ignore */
	    } else
		/* output bad line */
		fprintf(b, "%s:%d:%s", termdoc_filename, termdoc_lineno, line);
	    break;
	}
    }
}
