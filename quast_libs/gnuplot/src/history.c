#ifndef lint
static char *RCSid() { return RCSid("$Id: history.c,v 1.31.2.1 2016/03/31 03:47:56 sfeam Exp $"); }
#endif

/* GNUPLOT - history.c */

/*[
 * Copyright 1986 - 1993, 1999, 2004   Thomas Williams, Colin Kelley
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

#include <stdio.h>

#include "gp_hist.h"

#include "alloc.h"
#include "plot.h"
#include "util.h"

int gnuplot_history_size = HISTORY_SIZE;
TBOOLEAN history_quiet = FALSE;
TBOOLEAN history_full = FALSE;


#if defined(READLINE) && !defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDITLINE)

struct hist *history = NULL;     /* no history yet */
struct hist *cur_entry = NULL;

/* add line to the history */
void
add_history(char *line)
{
    static struct hist *first_entry = NULL; 
	/* this points to first entry in history list, 
	    whereas "history" points to last entry     */
    static long int hist_count = 0;
	/* number of entries in history list */
    struct hist *entry;

    entry = history;
    if (!history_full)
    while (entry != NULL) {
	/* Don't store duplicate entries */
	if (!strcmp(entry->line, line)) {
	    /* cmd lines are equal, relink entry that was found last */
	    if (entry->next == NULL) {
		/* previous command repeated, no change */
		return;
	    }
	    if (entry->prev == NULL) {
		/* current cmd line equals the first in the history */
		(entry->next)->prev = NULL;
		first_entry = entry->next;
		history->next = entry;
		entry->prev = history;
		entry->next = NULL;
		history = entry;
		return;
	    }
	    /* bridge over entry's vacancy, then move it to the end */
	    (entry->prev)->next = entry->next;
	    (entry->next)->prev = entry->prev;
	    entry->prev = history;
	    history->next = entry;
	    entry->next = NULL;
	    history = entry;
	    return;
	}
	entry = entry->prev;
    }				/* end of not-storing duplicated entries */

#if (0)
    /* This code block breaks "history !<number>" because it renumbers the  */
    /* entries before retrieving the requested number.  Also it potentially */
    /* truncates the list on every command contrary to the documentattion.  */

    /* limit size of history list to "gnuplot_history_size" */
    if (gnuplot_history_size != -1) {
	while ((hist_count >= gnuplot_history_size) && (first_entry != NULL)) {
    
	    entry = first_entry;

	    /* remove first entry from chain */
	    first_entry = first_entry->next;
	    if (first_entry) {
	        first_entry->prev = NULL;
            } 
	    hist_count--;

	    /* remove references */
	    if (cur_entry == entry)
		cur_entry = first_entry;
	    if (history == entry) {
		cur_entry = history = NULL;
		hist_count = 0;
	    }

	    free( entry->line );
	    free( entry );           
	}
    }
#endif

    entry = (struct hist *) gp_alloc(sizeof(struct hist), "history");
    entry->line = gp_strdup(line);

    entry->prev = history;
    entry->next = NULL;
    if (history != NULL) {
	history->next = entry;
    } else {
	first_entry = entry;
    } 
    history = entry;
    hist_count++;
}


/*
 * New functions for browsing the history. They are called from command.c
 * when the user runs the 'history' command
 */

/* write <n> last entries of the history to the file <filename>
 * Input parameters:
 *    n > 0 ... write only <n> last entries; otherwise all entries
 *    filename == NULL ... write to stdout; otherwise to the filename
 *    filename == "" ... write to stdout, but without entry numbers
 *    mode ... should be "w" or "a" to select write or append for file,
 *	       ignored if history is written to a pipe
*/
void
write_history_n(const int n, const char *filename, const char *mode)
{
    struct hist *entry = history, *start = NULL;
    FILE *out = stdout;
#ifdef PIPES
    int is_pipe = 0; /* not filename but pipe to an external program */
#endif
    int hist_entries = 0;
    int hist_index = 1;

    if (entry == NULL)
	return;			/* no history yet */

    /* find the beginning of the history and count nb of entries */
    while (entry->prev != NULL) {
	entry = entry->prev;
	hist_entries++;
	if (n <= 0 || hist_entries <= n)
	    start = entry;	/* listing will start from this entry */
    }
    entry = start;
    hist_index = (n > 0) ? GPMAX(hist_entries - n, 0) + 1 : 1;

    /* now write the history */
    if (filename != NULL && filename[0]) {
#ifdef PIPES
	if (filename[0]=='|') {
	    restrict_popen();
	    out = popen(filename+1, "w");
	    is_pipe = 1;
	} else
#endif
	out = fopen(filename, mode);
    }
    if (!out) {
	/* cannot use int_error() because we are just exiting gnuplot:
	   int_error(NO_CARET, "cannot open file for saving the history");
	*/
	fprintf(stderr, "Warning: cannot open file %s for saving the history.", filename);
    } else {
	while (entry != NULL) {
	    /* don't add line numbers when writing to file
	    * to make file loadable */
	    if (filename) {
		if (filename[0]==0) fputs(" ", out);
		fprintf(out, "%s\n", entry->line);
	    } else
		fprintf(out, "%5i  %s\n", hist_index++, entry->line);
	    entry = entry->next;
	}
	if (filename != NULL && filename[0]) {
#ifdef PIPES
	    if (is_pipe)
		pclose(out);
	    else
#endif
	    fclose(out);
	}
    }
}


/* obviously the same routine as in GNU readline, according to code from
 * plot.c:#if defined(HAVE_LIBREADLINE) && defined(GNUPLOT_HISTORY)
 */
void
write_history(char *filename)
{
    write_history_n(0, filename, "w");
}


/* routine to read history entries from a file,
 * this complements write_history and is necessary for
 * saving of history when we are not using libreadline
 */
void
read_history(char *filename)
{
    FILE *hist_file;
    
    if ((hist_file = fopen( filename, "r" ))) {
    	while (!feof(hist_file)) {
	    char *pline, line[MAX_LINE_LEN+1];
	    pline = fgets(line, MAX_LINE_LEN, hist_file);
	    if (pline) {
		/* remove trailing linefeed */
		if ((pline = strrchr(line, '\n')))
		    *pline = '\0';
		if ((pline = strrchr(line, '\r')))
		    *pline = '\0';

	    	add_history(line);
	    }
	}
	fclose(hist_file);
    }
}

/* finds and returns a command from the history list by number */
const char *
history_find_by_number(int n)
{
    struct hist *entry = history;
    int hist_entries = 0;
    int index;

    if (history == NULL || n <= 0)
	return NULL;	/* no history yet */

    /* count number of entries */
    for (entry = history; entry->prev != NULL; entry = entry->prev)
	hist_entries++;

    if (n > hist_entries)
	return NULL;

    /* step backwards to the requested entry */
    for (entry = history->prev, index = hist_entries; --index > 0; entry = entry->prev)
	if (index + 1 == n)
	    return entry->line;

    return NULL;	/* should not happen */
}


/* finds and returns a command from the history which starts with <cmd>
 * (ignores leading spaces in <cmd>)
 * Returns NULL if nothing found
 */
const char *
history_find(char *cmd)
{
    struct hist *entry = history;
    size_t len;
    char *line;

    if (entry == NULL)
	return NULL;		/* no history yet */
    if (*cmd == '"')
	cmd++;			/* remove surrounding quotes */
    if (!*cmd)
	return NULL;

    len = strlen(cmd);

    if (cmd[len - 1] == '"')
	cmd[--len] = 0;
    if (!*cmd)
	return NULL;

    /* search through the history */
    while (entry != NULL) {
	line = entry->line;
	while (isspace((unsigned char) *line))
	    line++;		/* skip leading spaces */
	if (!strncmp(cmd, line, len))	/* entry found */
	    return line;
	entry = entry->prev;
    }
    return NULL;
}


/* finds and print all occurencies of commands from the history which
 * start with <cmd>
 * (ignores leading spaces in <cmd>)
 * Returns 1 on success, 0 if no such entry exists
 */
int
history_find_all(char *cmd)
{
    struct hist *entry = history;
    int hist_index = 1;
    char res = 0;
    int len;
    char *line;

    if (entry == NULL)
	return 0;		/* no history yet */
    if (*cmd == '"')
	cmd++;			/* remove surrounding quotes */
    if (!*cmd)
	return 0;
    len = strlen(cmd);
    if (cmd[len - 1] == '"')
	cmd[--len] = 0;
    if (!*cmd)
	return 0;
    /* find the beginning of the history */
    while (entry->prev != NULL)
	entry = entry->prev;
    /* search through the history */
    while (entry != NULL) {
	line = entry->line;
	while (isspace((unsigned char) *line))
	    line++;		/* skip leading spaces */
	if (!strncmp(cmd, line, len)) {	/* entry found */
	    printf("%5i  %s\n", hist_index, line);
	    res = 1;
	}
	entry = entry->next;
	hist_index++;
    }
    return res;
}

#elif defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)

/* Save history to file, or write to stdout or pipe.
 * For pipes, only "|" works, pipes starting with ">" get a strange 
 * filename like in the non-readline version.
 *
 * Peter Weilbacher, 28Jun2004
 */

void
write_history_list(const int num, const char *const filename, const char *mode)
{
    const HIST_ENTRY *list_entry;
    FILE *out = stdout;
    int is_pipe = 0;
    int is_file = 0;
    int is_quiet = 0;
    int i, istart;

    if (filename && filename[0] ) {
        /* good filename given and not quiet */
#ifdef PIPES
        if (filename[0]=='|') {
	    restrict_popen();
            out = popen(filename+1, "w");
            is_pipe = 1;
        } else
#endif
	{
            if (! (out = fopen(filename, mode) ) ) {
                int_warn(NO_CARET, "Cannot open file to save history, using standard output.\n");
                out = stdout;
            } else {
		is_file = 1;
	    }
        }

    } else if (filename && !filename[0])
        is_quiet = 1;

    /* Determine starting point and output in loop.
     * For some reason the readline functions append_history() 
     * and write_history() do not work they way I thought they did...
     */
    if (num > 0)
        istart = history_length - num;
    else
	istart = 1;
    if (istart <= 0 || istart > history_length)
	istart = 1;

    for (i = istart; (list_entry = history_get(i)); i++) {
        /* don't add line numbers when writing to file to make file loadable */
	if (!is_file && !is_quiet)
	    fprintf(out, "%5i ", i + history_base - 1);
	fprintf(out, "  %s\n", list_entry->line);
    }

#ifdef PIPES
    if (is_pipe) pclose(out);
#endif
    if (is_file) fclose(out);
}

/* This is the function getting called in command.c */
void
write_history_n(const int n, const char *filename, const char *mode)
{
    write_history_list(n, filename, mode);
}

/* finds and returns a command from the history list by number */
const char *
history_find_by_number(int n)
{
    if (0 < n && n < history_length)
	return history_get(n)->line;
    else
	return NULL;
}

/* finds and returns a command from the history which starts with <cmd>
 * Returns NULL if nothing found
 *
 * Peter Weilbacher, 28Jun2004
 */
const char *
history_find(char *cmd)
{
    int len;

    /* quote removal, copied from non-readline version */
    if (*cmd == '"') cmd++;
    if (!*cmd) return 0;
    len = strlen(cmd);
    if (cmd[len - 1] == '"') cmd[--len] = 0;
    if (!*cmd) return 0;
    /* printf ("searching for '%s'\n", cmd); */

    /* Anchored backward search for prefix */
    if (history_search_prefix(cmd, -1) == 0)
        return current_history()->line;
    return NULL;
}

/* finds and print all occurencies of commands from the history which
 * start with <cmd>
 * Returns the number of found entries on success,
 * and 0 if no such entry exists
 *
 * Peter Weilbacher 28Jun2004
 */
int
history_find_all(char *cmd)
{
    int len;
    int found;
    int number = 0; /* each found entry increases this */

    /* quote removal, copied from non-readline version */
    if (*cmd == '"') cmd++;
    if (!*cmd) return 0;
    len = strlen(cmd);
    if (cmd[len - 1] == '"') cmd[--len] = 0;
    if (!*cmd) return 0;
    /* printf ("searching for all occurrences of '%s'\n", cmd); */

    /* Output matching history entries in chronological order (not backwards
     * so we have to start at the beginning of the history list.
     */
#if defined(HAVE_LIBREADLINE)
    found = history_set_pos(0);
    if (found == -1) {
        fprintf(stderr, "ERROR (history_find_all): could not rewind history\n");
        return 0;
    }
#else /* HAVE_LIBEDITLINE */
    /* libedit's history_set_pos() does not work properly,
       so we manually go to oldest entry */
    while (next_history());
#endif
    do {
        found = history_search_prefix(cmd, 1); /* Anchored backward search for prefix */
        if (found == 0) {
            number++;
#if defined(HAVE_LIBREADLINE)
            printf("%5i  %s\n", where_history() + history_base, current_history()->line);
            /* go one step back or you find always the same entry. */
            if (!history_set_pos(where_history() + 1))
                break; /* finished if stepping didn't work */
#else /* HAVE_LIBEDITLINE */
            /* libedit's history indices are reversed wrt GNU readline */
            printf("%5i  %s\n", history_length - where_history() + history_base, current_history()->line);
            /* go one step back or you find always the same entry. */
            if (!previous_history())
                break; /* finished if stepping didn't work */
#endif
        } /* (found == 0) */
    } while (found > -1);

    return number;
}

#endif /* READLINE && !HAVE_LIBREADLINE && !HAVE_LIBEDITLINE */
