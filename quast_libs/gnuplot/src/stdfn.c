#ifndef lint
static char *RCSid() { return RCSid("$Id: stdfn.c,v 1.30 2014/03/20 00:58:35 markisch Exp $"); }
#endif

/* GNUPLOT - stdfn.c */

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


/* This module collects various functions, which were previously scattered
 * all over the place. In a future implementation of gnuplot, each of
 * these functions will probably reside in their own file in a subdirectory.
 * - Lars Hecking
 */

#include "stdfn.h"

#ifdef WIN32
/* the WIN32 API has a Sleep function that does not consume CPU cycles */
#include <windows.h>
#ifndef HAVE_DIRENT_H
#include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#endif
#endif
#ifdef NEED_CEXP
#include <math.h>
#include <complex.h>
#endif

/*
 * ANSI C functions
 */

/* memcpy() */

#ifndef HAVE_MEMCPY
# ifndef HAVE_BCOPY
/*
 * cheap and slow version of memcpy() in case you don't have one
 */

char *
memcpy(char *dest, char *src, size_t len)
{
    while (len--)
	*dest++ = *src++;

    return dest;
}
# endif				/* !HAVE_BCOPY */
#endif /* HAVE_MEMCPY */

/* strchr()
 * Simple and portable version, conforming to Plauger.
 * Would this be more efficient as a macro?
 */
#ifndef HAVE_STRCHR
# ifndef HAVE_INDEX

char *
strchr(const char *s, int c)
{
    do {
	if (*s == (char) c)
	    return s;
    } while (*s++ != (char) 0);

    return NULL;
}
# endif				/* !HAVE_INDEX */
#endif /* HAVE_STRCHR */


/* memset ()
 *
 * Since we want to use memset, we have to map a possibly nonzero fill byte
 * to the bzero function. The following defined might seem a bit odd, but I
 * think this is the only possible way.
 */

#ifndef HAVE_MEMSET
# ifdef HAVE_BZERO
#  define memset(s, b, l) \
do {                      \
  assert((b)==0);         \
  bzero((s), (l));        \
} while(0)
#  else
#  define memset NO_MEMSET_OR_BZERO
# endif /* HAVE_BZERO */
#endif /* HAVE_MEMSET */


/* strerror() */
#ifndef HAVE_STRERROR

char *
strerror(int no)
{
    static char res_str[30];

    if (no > sys_nerr) {
	sprintf(res_str, "unknown errno %d", no);
	return res_str;
    } else {
	return sys_errlist[no];
    }
}
#endif /* HAVE_STRERROR */


/* strstr() */
#ifndef HAVE_STRSTR

char *
strstr(const char *cs, const char *ct)
{
    size_t len;

    if (!cs || !ct)
	return NULL;

    if (!*ct)
	return (char *) cs;

    len = strlen(ct);
    while (*cs) {
	if (strncmp(cs, ct, len) == 0)
	    return (char *) cs;
	cs++;
    }

    return NULL;
}
#endif /* HAVE_STRSTR */


/*
 * POSIX functions
 */

#ifndef HAVE_SLEEP
/* The implementation below does not even come close
 * to what is required by POSIX.1, but I suppose
 * it doesn't really matter on these systems. lh
 */


unsigned int
sleep(unsigned int delay)
{
#if defined(MSDOS)
# if !((defined(__EMX__) || defined(DJGPP))
    /* kludge to provide sleep() for msc 5.1 */
    unsigned long time_is_up;

    time_is_up = time(NULL) + (unsigned long) delay;
    while (time(NULL) < time_is_up)
	/* wait */ ;
# endif
#elif defined(WIN32)
    Sleep((DWORD) delay * 1000);
#endif /* MSDOS ... */

    return 0;
}

#endif /* HAVE_SLEEP */


/*
 * Other common functions
 */

/*****************************************************************
    portable implementation of strnicmp (hopefully)
*****************************************************************/
#ifndef HAVE_STRCASECMP
# ifndef HAVE_STRICMP

/* return (see MSVC documentation and strcasecmp()):
 *  -1  if str1 < str2
 *   0  if str1 == str2
 *   1  if str1 > str2
 */
int
gp_stricmp(const char *s1, const char *s2)
{
    unsigned char c1, c2;

    do {
	c1 = *s1++;
	if (islower(c1))
	    c1 = toupper(c1);
	c2 = *s2++;
	if (islower(c2))
	    c2 = toupper(c2);
    } while (c1 == c2 && c1 && c2);

    if (c1 == c2)
	return 0;
    if (c1 == '\0' || c1 > c2)
	return 1;
    return -1;
}
# endif				/* !HAVE_STRCASECMP */
#endif /* !HAVE_STRNICMP */

/*****************************************************************
    portable implementation of strnicmp (hopefully)
*****************************************************************/

#ifndef HAVE_STRNCASECMP
# ifndef HAVE_STRNICMP

int
gp_strnicmp(const char *s1, const char *s2, size_t n)
{
    unsigned char c1, c2;

    if (n == 0)
	return 0;

    do {
	c1 = *s1++;
	if (islower(c1))
	    c1 = toupper(c1);
	c2 = *s2++;
	if (islower(c2))
	    c2 = toupper(c2);
    } while (c1 == c2 && c1 && c2 && --n > 0);

    if (n == 0 || c1 == c2)
	return 0;
    if (c1 == '\0' || c1 > c2)
	return 1;
    return -1;
}
# endif				/* !HAVE_STRNCASECMP */
#endif /* !HAVE_STRNICMP */


#ifndef HAVE_STRNLEN    
size_t 
strnlen(const char *str, size_t n)
{
    const char * stop = (char *)memchr(str, '\0', n);
    return stop ? stop - str : n;
}
#endif


#ifndef HAVE_STRNDUP
char * 
strndup(const char * str, size_t n)
{
    char * ret = NULL;
    size_t len = strnlen(str, n);
    ret = (char *) malloc(len + 1);
    if (ret == NULL) return NULL;
    ret[len] = '\0';
    return (char *)memcpy(ret, str, len);
}
#endif


/* Safe, '\0'-terminated version of strncpy()
 * safe_strncpy(dest, src, n), where n = sizeof(dest)
 * This is basically the old fit.c(copy_max) function
 */
char *
safe_strncpy(char *d, const char *s, size_t n)
{
    char *ret;

    ret = strncpy(d, s, n);
    if (strlen(s) >= n)
	d[n > 0 ? n - 1 : 0] = NUL;

    return ret;
}


#ifndef HAVE_STRCSPN
/*
 * our own substitute for strcspn()
 * return the length of the inital segment of str1
 * consisting of characters not in str2
 * returns strlen(str1) if none of the characters
 * from str2 are in str1
 * based in misc.c(instring) */
size_t
gp_strcspn(const char *str1, const char *str2)
{
    char *s;
    size_t pos;

    if (!str1 || !str2)
	return 0;
    pos = strlen(str1);
    while (*str2++)
	if (s = strchr(str1, *str2))
	    if ((s - str1) < pos)
		pos = s - str1;
    return (pos);
}
#endif /* !HAVE_STRCSPN */


/* Standard compliant replacement functions for MSVC */
#if defined(_MSC_VER)
int
ms_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if ((size != 0) && (str != NULL))
	count = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
    if (count == -1)
	count = _vscprintf(format, ap);
    return count;
}


int
ms_snprintf(char *str, size_t size, const char * format, ...)
{
    int result;
    va_list ap;

    va_start(ap, format);
    result = ms_vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}
#endif


/* Implement portable generation of a NaN value. */
/* NB: Supposedly DJGPP V2.04 can use atof("NaN"), but... */

double
not_a_number(void)
{
#if defined (__MSC__) || defined (DJGPP) || defined(__DJGPP__) || defined(__MINGW32__)
	unsigned long lnan[2]={0xffffffff, 0x7fffffff};
    return *( double* )lnan;
#else
	return atof("NaN");
#endif
}


#ifdef NEED_CEXP
double _Complex cexp(double _Complex z)
{
	double x = creal(z);
	double y = cimag(z);
	return exp(x) * (cos(y) + I*sin(y));
}
#endif


/* Version of basename, which does take two possible
   separators into account and does not modify its
   argument.
*/
char * gp_basename(char *path)
{
    char * basename = strrchr(path, DIRSEP1);
    if (basename) {
        basename++;
        return basename;
    }
#if DIRSEP2 != NUL
    basename = strrchr(path, DIRSEP2);
    if (basename) {
        basename++;
        return basename;
    }
#endif
    /* no path separator found */
    return path;
}

#ifdef HAVE_ATEXIT
# define GP_ATEXIT(x) atexit((x))
#elif defined(HAVE_ON_EXIT)
# define GP_ATEXIT(x) on_exit((x),0)
#else
# define GP_ATEXIT(x) /* you lose */
#endif

struct EXIT_HANDLER {
    void (*function)(void);
    struct EXIT_HANDLER* next;
};

static struct EXIT_HANDLER* exit_handlers = NULL;

/* Calls the cleanup functions registered using gp_atexit().
 * Normally gnuplot should be exited using gp_exit(). In some cases, this is not
 * possible (notably when returning from main(), where some compilers get
 * confused because they expect a return statement at the very end. In that
 * case, gp_exit_cleanup() should be called before the return statement.
 */
void gp_exit_cleanup(void)
{
    /* Call exit handlers registered using gp_atexit(). This is used instead of
     * normal atexit-handlers, because some libraries (notably Qt) seem to have
     * problems with the destruction order when some objects are only destructed
     * on atexit(3). Circumvent this problem by calling the gnuplot
     * atexit-handlers, before any global destructors are run.
     */
    while (exit_handlers) {
        struct EXIT_HANDLER* handler = exit_handlers;
        (*handler->function)();
        /* note: assumes that function above has not called gp_atexit() */
        exit_handlers = handler->next;
        free(handler);
    }
}

/* Called from exit(3). Verifies that all exit handlers have already been
 * called.
 */
static void debug_exit_handler(void)
{
    if (exit_handlers) {
        fprintf(stderr, "Gnuplot not exited using gp_exit(). Exit handlers may"
                " not work correctly!\n");
        gp_exit_cleanup();
    }
}

/* Gnuplot replacement for atexit(3) */
void gp_atexit(void (*function)(void))
{
    /* Register new handler */
    static bool debug_exit_handler_registered = false;
    struct EXIT_HANDLER* new_handler = (struct EXIT_HANDLER*) malloc(sizeof(struct EXIT_HANDLER));
    new_handler->function = function;
    new_handler->next = exit_handlers;
    exit_handlers = new_handler;

    if (!debug_exit_handler_registered) {
        GP_ATEXIT(debug_exit_handler);
        debug_exit_handler_registered = true;
    }
}

/* Gnuplot replacement for exit(3). Calls the functions registered using
 * gp_atexit(). Always use this function instead of exit(3)!
 */
void gp_exit(int status)
{
    gp_exit_cleanup();
    exit(status);
}

#if !defined(HAVE_DIRENT_H) && defined(WIN32)  && (!defined(__WATCOMC__))
/* BM: OpenWatcom has dirent functions in direct.h!*/
/*

    Implementation of POSIX directory browsing functions and types for Win32.

    Author:  Kevlin Henney (kevlin@acm.org, kevlin@curbralan.com)
    History: Created March 1997. Updated June 2003.
    Rights:  See end of section.

*/

struct DIR
{
    long                handle; /* -1 for failed rewind */
    struct _finddata_t  info;
    struct dirent       result; /* d_name null iff first time */
    char                *name;  /* null-terminated char string */
};

DIR *opendir(const char *name)
{
    DIR *dir = 0;

    if (name && name[0]) {
        size_t base_length = strlen(name);
         /* search pattern must end with suitable wildcard */
        const char *all = strchr("/\\", name[base_length - 1]) ? "*" : "/*";

        if ((dir = (DIR *) malloc(sizeof *dir)) != 0 &&
           (dir->name = (char *) malloc(base_length + strlen(all) + 1)) != 0) {
            strcat(strcpy(dir->name, name), all);

            if ((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1) {
                dir->result.d_name = 0;
            } else { /* rollback */
                free(dir->name);
                free(dir);
                dir = 0;
            }
        } else { /* rollback */
            free(dir);
            dir   = 0;
            errno = ENOMEM;
        }
    } else {
        errno = EINVAL;
    }

    return dir;
}

int closedir(DIR *dir)
{
    int result = -1;

    if (dir) {
        if(dir->handle != -1) {
            result = _findclose(dir->handle);
        }
        free(dir->name);
        free(dir);
    }

    if (result == -1) { /* map all errors to EBADF */
        errno = EBADF;
    }

    return result;
}

struct dirent *readdir(DIR *dir)
{
    struct dirent *result = 0;

    if (dir && dir->handle != -1) {
        if (!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1) {
            result         = &dir->result;
            result->d_name = dir->info.name;
        }
    } else {
        errno = EBADF;
    }

    return result;
}

void rewinddir(DIR *dir)
{
    if (dir && dir->handle != -1) {
        _findclose(dir->handle);
        dir->handle = (long) _findfirst(dir->name, &dir->info);
        dir->result.d_name = 0;
    }
    else {
        errno = EBADF;
    }
}

/*

    Copyright Kevlin Henney, 1997, 2003. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.
    
    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.

*/
#endif /* !HAVE_DIRENT_H && WIN32 */
