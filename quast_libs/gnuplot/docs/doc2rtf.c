#ifndef lint
static char *RCSid() { return RCSid("$Id: doc2rtf.c,v 1.16 2007/10/24 00:47:51 sfeam Exp $"); }
#endif

/* GNUPLOT - doc2rtf.c */

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
 * doc2rtf.c  -- program to convert Gnuplot .DOC format to MS windows
 * help (.rtf) format.
 *
 * This involves stripping all lines with a leading digit or
 * a leading @, #, or %.
 * Modified by Maurice Castro from doc2gih.c by Thomas Williams
 *
 * usage:  doc2rtf file.doc file.rtf [-d]
 *
 */

/* note that tables must begin in at least the second column to */
/* be formatted correctly and tabs are forbidden */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"
#include "stdfn.h"
#define MAX_LINE_LEN 1023
#include "doc2x.h"
#include "xref.h"

static TBOOLEAN debug = FALSE;

void footnote __PROTO((int, char *, FILE *));
void convert __PROTO((FILE *, FILE *));
void process_line __PROTO((char *, FILE *));

int
main (int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;
    if (argc == 4 && argv[3][0] == '-' && argv[3][1] == 'd')
	debug = TRUE;

    if (argc != 3 && !debug) {
	fprintf(stderr, "Usage: %s infile outfile\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if ((infile = fopen(argv[1], "r")) == (FILE *) NULL) {
	fprintf(stderr, "%s: Can't open %s for reading\n",
		argv[0], argv[1]);
	exit(EXIT_FAILURE);
    }
    if ((outfile = fopen(argv[2], "w")) == (FILE *) NULL) {
	fprintf(stderr, "%s: Can't open %s for writing\n",
		argv[0], argv[2]);
	exit(EXIT_FAILURE);
    }
    parse(infile);
    convert(infile, outfile);
    return (EXIT_SUCCESS);
}

/* generate an RTF footnote with reference char c and text s */
void
footnote(int c, char *s, FILE *b)
{
    fprintf(b, "%c{\\footnote %c %s}\n", c, c, s);
}

void
convert(FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN+1];

    /* generate rtf header */
    fprintf(b, "{\\rtf1\\ansi ");	/* vers 1 rtf, ansi char set */
    fprintf(b, "\\deff0");	/* default font font 0 */
    /* font table: font 0 proportional, font 1 fixed */
    fprintf(b, "{\\fonttbl{\\f0\\fswiss Arial;}{\\f1\\fmodern Courier New;}}\n");

    /* process each line of the file */
    while (get_line(line, sizeof(line), a)) {
	process_line(line, b);
    }

    /* close final page and generate trailer */
    fprintf(b, "}{\\plain \\page}\n");
    /* fprintf(b,"}\n"); */ /* HBB: HACK ALERT: only without this, hc31 works */
    list_free();
}

void
process_line(char *line, FILE *b)
{
    static int line_count = 0;
    static char line2[MAX_LINE_LEN+1];
    static int last_line;
    int i;
    int j;
    static int startpage = 1;
    char str[MAX_LINE_LEN+1];
    char topic[MAX_LINE_LEN+1];
    int k, l;
    static int tabl = 0;
    static int para = 0;
    static int llpara = 0;
    static int inquote = FALSE;
    static int inref = FALSE;
    struct LIST *klist;

    line_count++;

    i = 0;
    j = 0;
    while (line[i] != NUL) {
	switch (line[i]) {
	case '\\':
	case '{':
	case '}':
	    line2[j] = '\\';
	    j++;
	    line2[j] = line[i];
	    break;
	case '\r':
	case '\n':
	    break;
	case '`':		/* backquotes mean boldface or link */
	    if (line[1] == ' ')	/* tabular line */
		line2[j] = line[i];
	    else if ((!inref) && (!inquote)) {
		k = i + 1;	/* index into current string */
		l = 0;		/* index into topic string */
		while ((line[k] != '`') && (line[k] != NUL))
		    topic[l++] = line[k++];
		topic[l] = NUL;
		klist = lookup(topic);
		if (klist && (k = klist->line) > 0 && (k != last_line)) {
		    line2[j++] = '{';
		    line2[j++] = '\\';
		    line2[j++] = 'u';
		    line2[j++] = 'l';
		    line2[j++] = 'd';
		    line2[j++] = 'b';
		    line2[j] = ' ';
		    inref = k;
		} else {
		    if (debug)
			fprintf(stderr, "Can't make link for \042%s\042 on line %d\n", topic, line_count);
		    line2[j++] = '{';
		    line2[j++] = '\\';
		    line2[j++] = 'b';
		    line2[j] = ' ';
		    inquote = TRUE;
		}
	    } else {
		if (inquote && inref)
		    fprintf(stderr, "Warning: Reference Quote conflict line %d\n", line_count);
		if (inquote) {
		    line2[j] = '}';
		    inquote = FALSE;
		}
		if (inref) {
		    /* must be inref */
		    sprintf(topic, "%d", inref);
		    line2[j++] = '}';
		    line2[j++] = '{';
		    line2[j++] = '\\';
		    line2[j++] = 'v';
		    line2[j++] = ' ';
		    line2[j++] = 'l';
		    line2[j++] = 'o';
		    line2[j++] = 'c';
		    k = 0;
		    while (topic[k] != NUL)
			line2[j++] = topic[k++];
		    line2[j] = '}';
		    inref = 0;
		}
	    }
	    break;
	default:
	    line2[j] = line[i];
	}
	i++;
	j++;
	line2[j] = NUL;
    }

    i = 1;

    switch (line[0]) {		/* control character */
    case '?':{			/* interactive help entry */
	    if ((line2[1] != NUL) && (line2[1] != ' '))
		footnote('K', &(line2[1]), b);
	    break;
	}
    case '@':{			/* start/end table */
	    break;		/* ignore */
	}
    case '^':{			/* html link escape */
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
	fprintf(b, "\\par\n");
	llpara = para;
	para = 0;
	tabl = 0;
	break;
    case ' ':{			/* normal text line */
	    if ((line2[1] == NUL) || (line2[1] == '\n')) {
		fprintf(b, "\\par\n");
		llpara = para;
		para = 0;
		tabl = 0;
	    }
	    if (line2[1] == ' ') {
		if (!tabl) {
		    fprintf(b, "\\par\n");
		}
		fprintf(b, "{\\pard \\plain \\f1\\fs20 ");
		fprintf(b, "%s", &line2[1]);
		fprintf(b, "}\\par\n");
		llpara = 0;
		para = 0;
		tabl = 1;
	    } else {
		if (!para) {
		    if (llpara)
			fprintf(b, "\\par\n");	/* blank line between paragraphs */
		    llpara = 0;
		    para = 1;	/* not in para so start one */
		    tabl = 0;
		    fprintf(b, "\\pard \\plain \\qj \\fs20 \\f0 ");
		}
		fprintf(b, "%s \n", &line2[1]);
	    }
	    break;
	}
    default:{
	    if (isdigit((int)line[0])) {	/* start of section */
		if (startpage) {	/* use new level 0 item */
		    refs(0, b, "\\par", NULL, "\\par{\\uldb %s}{\\v loc%d}\n");
		    fprintf(b, "}{\\plain \\page}\n");
		} else {
		    refs(last_line, b, "\\par", NULL, "\\par{\\uldb %s}{\\v loc%d}\n");
		    fprintf(b, "}{\\plain \\page}\n");
		}
		para = 0;	/* not in a paragraph */
		tabl = 0;
		last_line = line_count;
		startpage = 0;
		fprintf(b, "{\n");
		sprintf(str, "browse:%05d", line_count);
		footnote('+', str, b);
		footnote('$', &(line2[1]), b);	/* title */
		fprintf(b, "{\\b \\fs24 %s}\\plain\\par\\par\n", &(line2[1]));
		/* output unique ID */
		sprintf(str, "loc%d", line_count);
		footnote('#', str, b);
	    } else
		fprintf(stderr, "unknown control code '%c' in column 1, line %d\n",
			line[0], line_count);
	    break;
	}
    }
}
