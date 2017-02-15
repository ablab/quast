#ifndef lint
static char *RCSid() { return RCSid("$Id: doc2gih.c,v 1.16 2007/10/24 00:47:51 sfeam Exp $"); }
#endif

/* GNUPLOT - doc2gih.c */

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
 * doc2gih.c  -- program to convert Gnuplot .DOC format to gnuplot
 * interactive help (.GIH) format.
 *
 * This involves stripping all lines with a leading digit or
 * a leading @, #, or %.
 * Modified by Russell Lang from hlp2ms.c by Thomas Williams
 *
 * usage:  doc2gih [file.doc [file.gih]]
 *
 * Original version by David Kotz used the following one line script!
 * sed '/^[0-9@#%]/d' file.doc > file.gih
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
    FILE *outfile;

    infile = stdin;
    outfile = stdout;

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

    convert(infile, outfile);

    return EXIT_SUCCESS;
}


void
convert (FILE *inf, FILE *outf)
{
    static char line[MAX_LINE_LEN+1];

    while (get_line(line, sizeof(line), inf))
        process_line(line, outf);
}


void
process_line(char *line, FILE *b)
{
    static int line_count = 0;

    line_count++;

    switch (line[0]) {		/* control character */
    case '?':{			/* interactive help entry */
	    (void) fputs(line, b);
	    break;
	}
    case '@':{			/* start/end table */
	    break;		/* ignore */
	}
    case '#':{			/* latex table entry */
	    break;		/* ignore */
	}
    case '=':{			/* latex index entry */
	    break;		/* ignore */
	}
    case 'F':{			/* latex embedded figure */
	    break;		/* ignore */
	}
    case '%':{			/* troff table entry */
	    break;		/* ignore */
	}
    case '^':{			/* html entry */
	    break;		/* ignore */
	}
    case '\n':			/* empty text line */
    case ' ':{			/* normal text line */
	    (void) fputs(line, b);
	    break;
	}
    default:{
	    if (isdigit((int)line[0])) {	/* start of section */
		/* ignore */
	    } else
		fprintf(stderr, "unknown control code '%c' in column 1, line %d\n",
			line[0], line_count);
	    break;
	}
    }
}
