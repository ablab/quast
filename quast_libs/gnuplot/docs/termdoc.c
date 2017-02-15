#ifndef lint
static char *RCSid() { return RCSid("$Id: termdoc.c,v 1.15 2004/07/01 17:10:03 broeker Exp $"); }
#endif

/* GNUPLOT - termdoc.c */

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
 * AUTHORS
 *
 *  David Denholm - 1996
 */

/* this file provides a replacement for fgets() which inserts all the
 * help from the terminal drivers line by line at the < in the
 * gnuplot.doc file. This way, doc2* dont need to know what's going
 * on, and think it's all coming from one place.
 *
 * Can be compiled as a standalone program to generate the raw
 * .doc test, when compiled with -DTEST_TERMDOC
 *
 * Strips comment lines {so none of doc2* need to bother}
 * but magic comments beginning  C#  are used as markers
 * for line number recording (as c compilers)
 * We set BEGIN_HELP macro to "C#<driver>" as a special marker.
 *
 * Hmm - this is turning more and more into a preprocessor !
 * gnuplot.doc now has multiple top-level entries, but
 * some help systems (eg VMS) cannot tolerate this.
 * As a complete bodge, conditional on TBOOLEAN single_top_level == TRUE,
 * we accept only the first 1, and map any subsequent 1's to 2's
 * At present, this leaves a bogus, empty section called
 * commands, but that's a small price to pay to get it
 * working properly
 */

#include "syscfg.h"

#define DOCS_TERMDOC_MAIN

#include "stdfn.h"
#include "gp_types.h"
#include "doc2x.h"

/* because we hide details of including terminal drivers,
 * we provide line numbers and file names
 */

int termdoc_lineno;
char termdoc_filename[80];

TBOOLEAN single_top_level;

char *
get_line( char *buffer, int max, FILE *fp)
{
    static int line = -1;	/* not going yet */
    static int level = 0;	/* terminals are at level 1 - we add this */
    static int save_lineno;	/* for saving lineno */
    static int seen_a_one = 0;

    if (line == -1) {

	/* we are reading from file */

	{
	  read_another_line:	/* come here if a comment is read */

	    if (!fgets(buffer, max, fp))
		return NULL;	/* EOF */
	    ++termdoc_lineno;
	    if (buffer[0] == 'C') {
		if (buffer[1] == '#') {
		    /* should not happen in gnuplot.doc, but... */
		    safe_strncpy(termdoc_filename, buffer + 2, sizeof(termdoc_filename));
		    termdoc_filename[strlen(termdoc_filename) - 1] = NUL;
		    termdoc_lineno = 0;
		}
		goto read_another_line;		/* skip comments */
	    }
	}

	if (single_top_level == TRUE) {
	    if (buffer[0] == '1') {
		if (seen_a_one) {
		    buffer[0] = '2';
		}
		seen_a_one = 1;
	    }
	}
	if (buffer[0] != '<')
	    return buffer;	/* the simple case */

	/* prepare to return text from the terminal drivers */
	save_lineno = termdoc_lineno;
	termdoc_lineno = -1;	/* dont count the C# */
	level = buffer[1] - '1';
	line = 0;
    }
    /* we're sending lines from terminal help */

    /* process and skip comments. Note that the last line
     * will invariably be a comment !
     */

    while (termtext[line][0] == 'C') {
	if (termtext[line][1] == '#') {
	    safe_strncpy(termdoc_filename, termtext[line] + 2, sizeof(termdoc_filename));
	    termdoc_lineno = 0;
	}
	++termdoc_lineno;

	if (!termtext[++line]) {
	    /* end of text : need to return a line from
	     * the file. Recursive call is best way out
	     */
	    termdoc_lineno = save_lineno;
	    /* we've done the last line, so get next line from file */
	    line = -1;
	    return get_line(buffer, max, fp);
	}
    }


    /* termtext[line] is the next line of text.
     * more efficient to return pointer, but we need to modify it
     */

    ++termdoc_lineno;
    safe_strncpy(buffer, termtext[line], max);
    /* dodgy; can overrun buffer; lh */
    /* strncat(buffer, "\n", max); */
    if (strlen(buffer) == (max - 1))
        buffer[max-2] = '\n';
    else
        strcat(buffer, "\n");

    if (isdigit((int)buffer[0]))
	buffer[0] += level;

    if (!termtext[++line]) {
	/* end of terminal help : return to input file next time
	 * last (pseudo-)line in each terminal should be a comment,
	 * so we shouldn't get here, but...
	 */
	termdoc_lineno = save_lineno;
	/* we've done the last line, so get next line from file */
	line = -1;
    }
    return buffer;
}


/* Safe, '\0'-terminated version of strncpy()
 * safe_strncpy(dest, src, n), where n = sizeof(dest)
 * This is basically the old fit.c(copy_max) function
 */

char *
safe_strncpy( char *d, const char *s, size_t n)
{
    char *ret;

    ret = strncpy(d, s, n);
    if (strlen(s) >= n)
        d[n-1] = NUL;

    return ret;
}


#ifdef TEST_TERMDOC
int
main()
{
    char line[256];

    while (get_line(line, sizeof(line), stdin))
	printf("%s:%d:%s", termdoc_filename, termdoc_lineno, line);
    return 0;
}
#endif
