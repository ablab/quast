#ifndef lint
static char *RCSid() { return RCSid("$Id: xref.c,v 1.13 2011/02/28 11:39:35 markisch Exp $"); }
#endif

/* GNUPLOT - xref.c */

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
 * this file is used by doc2ipf, doc2html, doc2rtf and doc2info
 *
 * MUST be included after termdoc.c (since termdoc.c redefines fgets() )
 *
 * it contains functions needed to handle xrefs, most of them from
 *     doc2rtf (most likely) by Maurice Castro
 *  or doc2ipf by Roger Fearick
 *  or doc2html by Russel Lang
 *
 * I have modified the functions a little to make them more flexible
 * (lookup returns list instead of list->line) or let them work with all
 * four programs (adding three parameters to refs).
 *
 * I switched the search order of lookup. Makes more sense to me
 *
 * Stefan Bodewig 1/29/1996
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define DOCS_XREF_MAIN

#include "syscfg.h"
#include "stdfn.h"
#include "doc2x.h"
#include "xref.h"

struct LIST *list = NULL;
struct LIST *head = NULL;

struct LIST *keylist = NULL;
struct LIST *keyhead = NULL;

void dump_list __PROTO((void));

int maxlevel = 0;		/* how deep are the topics nested? */
int listitems = 0;		/* number of topics */

/* for debugging (invoke from gdb !) */
void
dump_list()
{
    struct LIST *element = head;
    while (element) {
	fprintf(stderr, "%p level %d, line %d, \"%s\"\n", element,
		element->level, element->line, element->string);
	element = element->next;
    }
}


generic *
xmalloc(size_t size)
{
    generic *p = malloc(size);

    if (!p) {
        fprintf(stderr, "Malloc failed\n");
	exit(EXIT_FAILURE);
    }
    return p;
}

/* scan the file and build a list of line numbers where particular levels are */
void
parse(FILE *a)
{
    static char line[MAX_LINE_LEN+1];
    char *c;
    int lineno = 0;
    int lastline = 0;

    /* insert a special level 0 listitem
     * this one is the starting point for the table of contents in the html
     * version and the Top-Node of the info version.
     *
     * Added this to support multiple level 1 items.     --SB
     */
    listitems = 1;
    head = (list = (struct LIST *) xmalloc(sizeof(struct LIST)));
    list->prev = NULL;
    list->line = 0;
    list->level = 0;
    list->string = (char *) xmalloc(1);
    list->string[0] = NUL;
    list->next = NULL;

    while (get_line(line, sizeof(line), a)) {
	lineno++;
	if (isdigit((int)line[0])) {	/* start of new section */
	    listitems++;

	    if (list == NULL) {	/* impossible with the new level 0 item */
		head = (list = (struct LIST *) xmalloc(sizeof(struct LIST)));
		list->prev = NULL;
	    } else {
		list->next = (struct LIST *) xmalloc(sizeof(struct LIST));
		list->next->prev = list;
		list = list->next;
		list->next = NULL;
	    }

	    list->line = lastline = lineno;
	    list->level = line[0] - '0';
	    list->string = (char *) xmalloc(strlen(line) + 1);
	    c = strtok(&(line[1]), "\n");
	    strcpy(list->string, c);
	    list->next = NULL;
	    if (list->level > maxlevel)
		maxlevel = list->level;
	}
	if (line[0] == '?') {	/* keywords */
	    if (keylist == NULL) {
		keyhead = (keylist = (struct LIST *) xmalloc(sizeof(struct LIST)));
		keylist->prev = NULL;
	    } else {
		keylist->next = (struct LIST *) xmalloc(sizeof(struct LIST));
		keylist->next->prev = keylist;
		keylist = keylist->next;
	    }

	    keylist->line = lastline;
	    keylist->level = list->level;
	    c = strtok(&(line[1]), "\n");
	    if (c == NULL || *c == '\0')
		c = list->string;
	    keylist->string = (char *) malloc(strlen(c) + 1);
	    strcpy(keylist->string, c);
	    keylist->next = NULL;
	}
    }
    rewind(a);
}

/* look up a topic in text reference */
/*
 * Original version from doc2rtf (|| ipf || html) scanned keylist before list.
 * This way we get a reference to `plot` for the topic `splot` instead
 * of one to `splot`. Switched the search order -SB.
 */
struct LIST *
lookup(char *s)
{
    char *c;
    char tokstr[MAX_LINE_LEN+1];
    char *match;
    int l;

    strcpy(tokstr, s);

    /* first try titles */
    match = strtok(tokstr, " \n\t");
    if (match == NULL) {
	fprintf(stderr, "Error in lookup(\"%s\")\n", s);

	/* there should a line number, but it is local to parse()  */
	fprintf(stderr,
		"Possible missing link character (`) near above line number\n");
	exit(3);
    }
    l = 0;			/* level */

    list = head;
    while (list != NULL) {
	c = list->string;
	while (isspace((int)(*c)))
	    c++;
	if (!strcmp(match, c)) {
	    l = list->level;
	    match = strtok(NULL, "\n\t ");
	    if (match == NULL) {
		return (list);
	    }
	}
	if (l > list->level)
	    break;
	list = list->next;
    }

    /* then try the ? keyword entries */
    keylist = keyhead;
    while (keylist != NULL) {
	c = keylist->string;
	while (isspace((int)(*c)))
	    c++;
	if (!strcmp(s, c))
	    return (keylist);
	keylist = keylist->next;
    }

    return (NULL);
}

/*
 * find title-entry for keyword-entry
 */
struct LIST *
lkup_by_number(int line)
{
    struct LIST *run = head;

    while (run->next && run->next->line <= line)
	run = run->next;

    if (run->next)
	return run;
    else
	return NULL;
}

/*
 * free the whole list (I never trust the OS -SB)
 */
void
list_free()
{
    struct LIST *run;

    for (run = head; run->next; run = run->next)
	; /* do nothing */

    for (run = run->prev; run; run = run->prev) {
	free(run->next->string);
	free(run->next);
    }
    free(head->string);
    free(head);

    for (run = keyhead; run->next; run = run->next)
	; /* do nothing */
    for (run = run->prev; run; run = run->prev) {
	free(run->next->string);
	free(run->next);
    }
    free(keyhead->string);
    free(keyhead);
}


/* search through the list to find any references */
/*
 * writes a menu of all subtopics of the topic located at l
 * format must contain %s for the title of the subtopic and may contain
 * a %d for the line number of the subtopic (used by doc2html and doc2rtf
 * The whole menu is preceeded by start and gets the trailer end
 */
void
refs( int l, FILE *f, char *start, char *end, char *format)
{
    int curlevel, i;
    char *c;
    int inlist = FALSE;

    /* find current line */
    list = head;
    while (list->line != l)
	list = list->next;
    curlevel = list->level;
    list = list->next;		/* look at next element before going on */

    if ((start != NULL) && (list != NULL) && (list->level > curlevel)) {
	/* don't write start if there's no menue at all */
	inlist = TRUE;
	fprintf(f, "%s", start);
    }
    while (list != NULL) {
	/* we are onto the next topic so stop */
	if (list->level <= curlevel)
	    break;
	/* these are the next topics down the list */
	if (list->level == curlevel + 1) {
	    c = list->string;
	    while (isspace((int)(*c)))
		c++;		/* strip leading whitespace */

	    if (format != NULL) {
		for (i = 0; format[i] != '%' && format[i] != '\0'; i++);
		if (format[i] != '\0') {
		    if (format[i + 1] == 'd') {
			/* line number has to be printed first */
			fprintf(f, format, list->line, c);
		    }
		    else {
			++i;
			for (; format[i] != '%' && format[i] != '\0'; i++);
			if (format[i] != '\0')	/* line number is second */
			    fprintf(f, format, c, list->line);
			else	/* no line number at all */
			    fprintf(f, format, c);
		    }
		}
	    }
	}
	list = list->next;
    }
    if (inlist && end)		/* trailer */
	fprintf(f, "%s", end);
}
