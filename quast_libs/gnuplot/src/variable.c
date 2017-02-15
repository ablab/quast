#ifndef lint
static char *RCSid() { return RCSid("$Id: variable.c,v 1.44 2013/07/02 22:19:09 sfeam Exp $"); }
#endif

/* GNUPLOT - variable.c */

/*[
 * Copyright 1999, 2004   Lars Hecking
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

/* The Death of Global Variables - part one. */

#include <string.h>

#include "variable.h"

#include "alloc.h"
#include "command.h"
#include "plot.h"
#include "util.h"
#include "term_api.h"

#define PATHSEP_TO_NUL(arg)			\
do {						\
    char *s = arg;				\
    while ((s = strchr(s, PATHSEP)) != NULL)	\
	*s++ = NUL;				\
} while (0)

#define PRINT_PATHLIST(start, limit)		\
do {						\
    char *s = start;				\
						\
    while (s < limit) {				\
	fprintf(stderr, "\"%s\" ", s);		\
	s += strlen(s) + 1;			\
    }						\
    fputc('\n',stderr);				\
} while (0)

/*
 * char *loadpath_handler (int, char *)
 *
 */
char *
loadpath_handler(int action, char *path)
{
    /* loadpath variable
     * the path elements are '\0' separated (!)
     * this way, reading out loadpath is very
     * easy to implement */
    static char *loadpath;
    /* index pointer, end of loadpath,
     * env section of loadpath, current limit, in that order */
    static char *p, *last, *envptr, *limit;
#ifdef X11
    char *appdir;
#endif

    switch (action) {
    case ACTION_CLEAR:
	/* Clear loadpath, fall through to init */
	FPRINTF((stderr, "Clear loadpath\n"));
	free(loadpath);
	loadpath = p = last = NULL;
	/* HBB 20000726: 'limit' has to be initialized to NULL, too! */
	limit = NULL;
    case ACTION_INIT:
	/* Init loadpath from environment */
	FPRINTF((stderr, "Init loadpath from environment\n"));
	assert(loadpath == NULL);
	if (!loadpath)
	{
	    char *envlib = getenv("GNUPLOT_LIB");
	    if (envlib) {
		int len = strlen(envlib);
		loadpath = gp_strdup(envlib);
		/* point to end of loadpath */
		last = loadpath + len;
		/* convert all PATHSEPs to \0 */
		PATHSEP_TO_NUL(loadpath);
	    }			/* else: NULL = empty */
	}			/* else: already initialised; int_warn (?) */
	/* point to env portion of loadpath */
	envptr = loadpath;
	break;
    case ACTION_SET:
	/* set the loadpath */
	FPRINTF((stderr, "Set loadpath\n"));
	if (path && *path != NUL) {
	    /* length of env portion */
	    size_t elen = last - envptr;
	    size_t plen = strlen(path);
	    if (loadpath && envptr) {
		/* we are prepending a path name; because
		 * realloc() preserves only the contents up
		 * to the minimum of old and new size, we move
		 * the part to be preserved to the beginning
		 * of the string; use memmove() because strings
		 * may overlap */
		memmove(loadpath, envptr, elen + 1);
	    }
	    loadpath = gp_realloc(loadpath, elen + 1 + plen + 1, "expand loadpath");
	    /* now move env part back to the end to make space for
	     * the new path */
	    memmove(loadpath + plen + 1, loadpath, elen + 1);
	    strcpy(loadpath, path);
	    /* separate new path(s) and env path(s) */
	    loadpath[plen] = PATHSEP;
	    /* adjust pointer to env part and last */
	    envptr = &loadpath[plen+1];
	    last = envptr + elen;
	    PATHSEP_TO_NUL(loadpath);
	}			/* else: NULL = empty */
	break;
    case ACTION_SHOW:
	/* print the current, full loadpath */
	FPRINTF((stderr, "Show loadpath\n"));
	if (loadpath) {
	    fputs("\tloadpath is ", stderr);
	    PRINT_PATHLIST(loadpath, envptr);
	    if (envptr) {
		/* env part */
		fputs("\tloadpath from GNUPLOT_LIB is ", stderr);
		PRINT_PATHLIST(envptr, last);
	    }
	} else
	    fputs("\tloadpath is empty\n", stderr);
#ifdef GNUPLOT_SHARE_DIR
	fprintf(stderr,"\tgnuplotrc is read from %s\n",GNUPLOT_SHARE_DIR);
#endif
#ifdef X11
	if ((appdir = getenv("XAPPLRESDIR"))) {
	    fprintf(stderr,"\tenvironmental path for X11 application defaults: \"%s\"\n",
		appdir);
	}
#ifdef XAPPLRESDIR
	else {
	    fprintf(stderr,"\tno XAPPLRESDIR found in the environment,\n");
	    fprintf(stderr,"\t    falling back to \"%s\"\n", XAPPLRESDIR);
	}
#endif
#endif
	break;
    case ACTION_SAVE:
	/* we don't save the load path taken from the
	 * environment, so don't go beyond envptr when
	 * extracting the path elements
	 */
	limit = envptr;
    case ACTION_GET:
	/* subsequent calls to get_loadpath() return all
	 * elements of the loadpath until exhausted
	 */
	FPRINTF((stderr, "Get loadpath\n"));
	if (!loadpath)
	    return NULL;
	if (!p) {
	    /* init section */
	    p = loadpath;
	    if (!limit)
		limit = last;
	} else {
	    /* skip over '\0' */
	    p += strlen(p) + 1;
	}
	if (p >= limit) 
	    limit = p = NULL;
	return p;
	break;
    case ACTION_NULL:
	/* just return */
    default:
	break;
    }

    /* should always be ignored - points to the
     * first path in the list */
    return loadpath;

}



struct path_table {
    const char *dir;
};

/* Yet, no special font paths for these operating systems:
 * MSDOS, NeXT, ultrix, VMS, _IBMR2, alliant
 *
 * Environmental variables are written as $(name).
 * Commands are written as $`command`.
 */

#if defined(OS2) && !defined(FONTPATHSET)
#  define FONTPATHSET
static const struct path_table fontpath_tbl[] =
{
    { "$(BOOTDIR)/PSFONTS" },
    /* X11 */
    { "$(X11ROOT)/X11R6/lib/X11/fonts/Type1" },
    { NULL }
};
#endif

#if defined(_Windows) && !defined(FONTPATHSET)
#  define FONTPATHSET
static const struct path_table fontpath_tbl[] =
{
    { "$(windir)\\fonts" },
    /* Ghostscript */
    { "c:\\gs\\fonts" },
    /* X11 */
    { "$(CYGWIN_ROOT)\\usr\\X11R6\\lib\\X11\\fonts\\Type1" },
#ifdef HAVE_KPSEXPAND
    /* fpTeX */
    { "$`kpsewhich -expand-path=$HOMETEXMF`\\fonts\\type1!" },
    { "$`kpsewhich -expand-path=$TEXMFLOCAL`\\fonts\\type1!" },
    { "$`kpsewhich -expand-path=$TEXMFMAIN`\\fonts\\type1!" },
    { "$`kpsewhich -expand-path=$TEXMFDIST`\\fonts\\type1!" },
#endif
    { NULL }
};
#endif

#if defined(__APPLE__) && !defined(FONTPATHSET)
#  define FONTPATHSET
static const struct path_table fontpath_tbl[] =
{
    { "/System/Library/Fonts!" },
    { "/Library/Fonts!" },
    { "$(HOME)/Library/Fonts!" },
    { NULL }
};
#endif

#if defined(VMS) && !defined(FONTPATHSET)
#  define FONTPATHSET
static const struct path_table fontpath_tbl[] =
{
    { "SYS$COMMON:[SYSFONT]!" },
    { NULL }
};
#endif

/* Fallback: Should work for unix */
#ifndef FONTPATHSET
static const struct path_table fontpath_tbl[] =
{
#ifdef HAVE_KPSEXPAND
    /* teTeX or TeXLive */
    { "$`kpsexpand '$HOMETEXMF'`/fonts/type1!" },
    { "$`kpsexpand '$TEXMFLOCAL'`/fonts/type1!" },
    { "$`kpsexpand '$TEXMFMAIN'`/fonts/type1!" },
    { "$`kpsexpand '$TEXMFDIST'`/fonts/type1!" },
#endif
    /* Linux paths */
    { "/usr/X11R6/lib/X11/fonts/Type1" },
    { "/usr/X11R6/lib/X11/fonts/truetype" },
    /* HP-UX */
    { "/usr/lib/X11/fonts!"},
    /* Ghostscript */
    { "/usr/share/ghostscript/fonts" },
    { "/usr/local/share/ghostscript/fonts" },
    { NULL }
};
#endif

#undef FONTPATHSET

static TBOOLEAN fontpath_init_done = FALSE;

/*
 * char *fontpath_handler (int, char *)
 *
 */
char *
fontpath_handler(int action, char *path)
{
    /* fontpath variable
     * the path elements are '\0' separated (!)
     * this way, reading out fontpath is very
     * easy to implement */
    static char *fontpath;
    /* index pointer, end of fontpath,
     * env section of fontpath, current limit, in that order */
    static char *p, *last, *envptr, *limit;

    if (!fontpath_init_done) {
	fontpath_init_done = TRUE;
	init_fontpath();
    }

    switch (action) {
    case ACTION_CLEAR:
	/* Clear fontpath, fall through to init */
	FPRINTF((stderr, "Clear fontpath\n"));
	free(fontpath);
	fontpath = p = last = NULL;
	/* HBB 20000726: 'limit' has to be initialized to NULL, too! */
	limit = NULL;
    case ACTION_INIT:
	/* Init fontpath from environment */
	FPRINTF((stderr, "Init fontpath from environment\n"));
	assert(fontpath == NULL);
	if (!fontpath)
	{
	    char *envlib = getenv("GNUPLOT_FONTPATH");
	    if (envlib) {
		/* get paths from environment */
		int len = strlen(envlib);
		fontpath = gp_strdup(envlib);
		/* point to end of fontpath */
		last = fontpath + len;
		/* convert all PATHSEPs to \0 */
		PATHSEP_TO_NUL(fontpath);
	    }
#if defined(HAVE_DIRENT_H) || defined(_Windows)
	    else {
		/* set hardcoded paths */
		const struct path_table *curr_fontpath = fontpath_tbl;

		while (curr_fontpath->dir) {
		    char *currdir = NULL;
		    char *envbeg = NULL;
#  if defined(PIPES)
		    char *cmdbeg = NULL;
#  endif
		    TBOOLEAN subdirs = FALSE;

		    currdir = gp_strdup( curr_fontpath->dir );

		    while ( (envbeg=strstr(currdir, "$("))
#  if defined(PIPES)
			    || (cmdbeg=strstr( currdir, "$`" ))
#  endif
			    ) {
			/* Read environment variables */
			if (envbeg) {
			    char *tmpdir = NULL;
			    char *envend = NULL, *envval = NULL;
			    unsigned int envlen;
			    envend = strchr(envbeg+2,')');
			    envend[0] = '\0';
			    envval = getenv(envbeg+2);
			    envend[0] = ')';
			    envlen = envval ? strlen(envval) : 0;
			    tmpdir = gp_alloc(strlen(currdir)+envlen
					      +envbeg-envend+1,
					      "expand fontpath");
			    strncpy(tmpdir,currdir,envbeg-currdir);
			    if (envval)
				strcpy(tmpdir+(envbeg-currdir),envval);
			    strcpy(tmpdir+(envbeg-currdir+envlen), envend+1);

			    free(currdir);
			    currdir = tmpdir;
			}
#  if defined(PIPES)
			/* Read environment variables */
			else if (cmdbeg) {
			    char *tmpdir = NULL;
			    char *envend = NULL;
			    char envval[256];
			    unsigned int envlen;
			    FILE *fcmd;
			    envend = strchr(cmdbeg+2,'`');
			    envend[0] = '\0';
			    restrict_popen();
			    fcmd = popen(cmdbeg+2,"r");
			    if (fcmd) {
				fgets(envval,255,fcmd);
				if (envval[strlen(envval)-1]=='\n')
				    envval[strlen(envval)-1]='\0';
				pclose(fcmd);
			    }
			    envend[0] = '`';
			    envlen = strlen(envval);
			    tmpdir = gp_alloc(strlen(currdir)+envlen
					      +cmdbeg-envend+1,
					      "expand fontpath");
			    strncpy(tmpdir,currdir,cmdbeg-currdir);
			    if (*envval)
				strcpy(tmpdir+(cmdbeg-currdir),envval);
			    strcpy(tmpdir+(cmdbeg-currdir+envlen), envend+1);

			    free(currdir);
			    currdir = tmpdir;
			}
#  endif
		    }

		    if ( currdir[strlen(currdir)-1] == '!' ) {
			/* search subdirectories */
			/* delete ! from directory name */
			currdir[strlen(currdir)-1] = '\0';
			subdirs = TRUE;
		    }

		    if ( existdir( currdir ) ) {
			size_t plen;
			if ( subdirs )
			    /* add ! to directory name again */
			    currdir[strlen(currdir)] = '!';
			plen = strlen(currdir);
			if (fontpath) {
			    size_t elen = strlen(fontpath);
			    fontpath = gp_realloc(fontpath,
						  elen + 1 + plen + 1,
						  "expand fontpath");
			    last = fontpath+elen;
			    *last = PATHSEP;
			    ++last;
			    *last = '\0';
			} else {
			    fontpath = gp_alloc(plen + 1,
						"expand fontpath");
			    last = fontpath;
			}

			strcpy(last, currdir );
			last += plen;
		    }
		    curr_fontpath++;
		    if (currdir) {
			free(currdir);
			currdir = NULL;
		    }
		}
		/* convert all PATHSEPs to \0 */
		if (fontpath)
		    PATHSEP_TO_NUL(fontpath);
	    }
#endif /* HAVE_DIRENT_H */

	}			/* else: already initialised; int_warn (?) */
	/* point to env portion of fontpath */
	envptr = fontpath;
	break;
    case ACTION_SET:
	/* set the fontpath */
	FPRINTF((stderr, "Set fontpath\n"));
	if (path && *path != NUL) {
	    /* length of env portion */
	    size_t elen = last - envptr;
	    size_t plen = strlen(path);
	    if (fontpath && envptr) {
		/* we are prepending a path name; because
		 * realloc() preserves only the contents up
		 * to the minimum of old and new size, we move
		 * the part to be preserved to the beginning
		 * of the string; use memmove() because strings
		 * may overlap */
		memmove(fontpath, envptr, elen + 1);
	    }
	    fontpath = gp_realloc(fontpath, elen + 1 + plen + 1, "expand fontpath");
	    /* now move env part back to the end to make space for
	     * the new path */
	    memmove(fontpath + plen + 1, fontpath, elen + 1);
	    strcpy(fontpath, path);
	    /* separate new path(s) and env path(s) */
	    fontpath[plen] = PATHSEP;
	    /* adjust pointer to env part and last */
	    envptr = &fontpath[plen+1];
	    last = envptr + elen;
	    PATHSEP_TO_NUL(fontpath);
	}			/* else: NULL = empty */
	break;
    case ACTION_SHOW:
	/* print the current, full fontpath */
	FPRINTF((stderr, "Show fontpath\n"));
	if (fontpath) {
	    fputs("\tfontpath is ", stderr);
	    PRINT_PATHLIST(fontpath, envptr);
	    if (envptr) {
		/* env part */
		fputs("\tsystem fontpath is ", stderr);
		PRINT_PATHLIST(envptr, last);
	    }
	} else
	    fputs("\tfontpath is empty\n", stderr);
	break;
    case ACTION_SAVE:
	/* we don't save the font path taken from the
	 * environment, so don't go beyond envptr when
	 * extracting the path elements
	 */
	limit = envptr;
    case ACTION_GET:
	/* subsequent calls to get_fontpath() return all
	 * elements of the fontpath until exhausted
	 */
	FPRINTF((stderr, "Get fontpath\n"));
	if (!fontpath)
	    return NULL;
	if (!p) {
	    /* init section */
	    p = fontpath;
	    if (!limit)
		limit = last;
	} else {
	    /* skip over '\0' */
	    p += strlen(p) + 1;
	}
	if (p >= limit)
	    limit = p = NULL;
	return p;
    case ACTION_NULL:
	/* just return */
    default:
	break;
    }

    /* should always be ignored - points to the
     * first path in the list */
    return fontpath;

}

/* not set or shown directly, but controlled by 'set locale'
 * defined in national.h
 */

char full_month_names[12][32] =
{ FMON01, FMON02, FMON03, FMON04, FMON05, FMON06, FMON07, FMON08, FMON09, FMON10, FMON11, FMON12 };
char abbrev_month_names[12][8] =
{ AMON01, AMON02, AMON03, AMON04, AMON05, AMON06, AMON07, AMON08, AMON09, AMON10, AMON11, AMON12 };

char full_day_names[7][32] =
{ FDAY0, FDAY1, FDAY2, FDAY3, FDAY4, FDAY5, FDAY6 };
char abbrev_day_names[7][8] =
{ ADAY0, ADAY1, ADAY2, ADAY3, ADAY4, ADAY5, ADAY6 };

char *
locale_handler(int action, char *newlocale)
{
    struct tm tm;
    int i;

    switch(action) {
    case ACTION_CLEAR:
    case ACTION_INIT:
	free(current_locale);
#ifdef HAVE_LOCALE_H
	setlocale(LC_TIME, "");
	setlocale(LC_CTYPE, "");
	current_locale = gp_strdup(setlocale(LC_TIME,NULL));
#else
	current_locale = gp_strdup(INITIAL_LOCALE);
#endif
	break;

    case ACTION_SET:
#ifdef HAVE_LOCALE_H
	if (setlocale(LC_TIME, newlocale)) {
	    free(current_locale);
	    current_locale = gp_strdup(setlocale(LC_TIME,NULL));
	} else {
	    int_error(c_token, "Locale not available");
	}

	/* we can do a *lot* better than this ; eg use system functions
	 * where available; create values on first use, etc
	 */
	memset(&tm, 0, sizeof(struct tm));
	for (i = 0; i < 7; ++i) {
	    tm.tm_wday = i;		/* hope this enough */
	    strftime(full_day_names[i], sizeof(full_day_names[i]), "%A", &tm);
	    strftime(abbrev_day_names[i], sizeof(abbrev_day_names[i]), "%a", &tm);
	}
	for (i = 0; i < 12; ++i) {
	    tm.tm_mon = i;		/* hope this enough */
	    strftime(full_month_names[i], sizeof(full_month_names[i]), "%B", &tm);
	    strftime(abbrev_month_names[i], sizeof(abbrev_month_names[i]), "%b", &tm);
	}
#else
	current_locale = gp_realloc(current_locale, strlen(newlocale) + 1, "locale");
	strcpy(current_locale, newlocale);
#endif /* HAVE_LOCALE_H */
	break;

    case ACTION_SHOW:
#ifdef HAVE_LOCALE_H
	fprintf(stderr, "\tgnuplot LC_CTYPE   %s\n", setlocale(LC_CTYPE,NULL));
	fprintf(stderr, "\tgnuplot encoding   %s\n", encoding_names[encoding]);
	fprintf(stderr, "\tgnuplot LC_TIME    %s\n", setlocale(LC_TIME,NULL));
	fprintf(stderr, "\tgnuplot LC_NUMERIC %s\n", numeric_locale ? numeric_locale : "C");
#else
	fprintf(stderr, "\tlocale is \"%s\"\n", current_locale);
#endif
	break;
    
    case ACTION_GET:
    default:
	break;
    }

    return current_locale;
}

