/*
 * $Id: syscfg.h,v 1.56 2014/07/28 22:16:27 sfeam Exp $
 */

/* GNUPLOT - syscfg.h */

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

/* This header file provides system dependent definitions. New features
 * and platforms should be added here.
 */

#ifndef SYSCFG_H
#define SYSCFG_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*
 * Define operating system dependent constants [default value]:
 *
 * OS:       [""] Name of OS; only required if system has no uname(2) call
 * HELPFILE: ["docs/gnuplot.gih"] Location of helpfile - overridden by Makefile
 * HOME:     ["HOME"] Name of environment variable which points to
 *           the directory where gnuplot's config file is found.
 * PLOTRC:   [".gnuplot"] Name of the gnuplot startup file.
 * SHELL:    ["/bin/sh"] Name, and in some cases, full path to the shell
 *           that is used to run external commands.
 * DIRSEP1:  ['/'] Primary character which separates path components.
 * DIRSEP2:  ['\0'] Secondary character which separates path components.
 * PATHSEP:  [':'] Character which separates path names
 *
 */

#if defined(__NeXT__) || defined(NEXT)
# ifndef NEXT
#  define NEXT
# endif
#endif /* NeXT */

#ifdef OS2
# define OS       "OS/2"
# define HELPFILE "gnuplot.gih"
# define HOME     "GNUPLOT"
# define PLOTRC   "gnuplot.ini"
# define SHELL    "c:\\os2\\cmd.exe"
# define DIRSEP1  '\\'
# define PATHSEP  ';'
# define GNUPLOT_HISTORY_FILE "~\\gnuplot_history"
#endif /* OS/2 */

#if defined(vms) || defined(VMS)
# define OS "VMS"
# ifndef VMS
#  define VMS
# endif
# define HOME   "sys$login"
# define PLOTRC "gnuplot.ini"
# ifdef NO_GIH
   /* for show version long */
#  define HELPFILE "GNUPLOT$HELP"
# endif
# if !defined(VAXCRTL) && !defined(DECCRTL)
#  define VAXCRTL VAXCRTL_AND_DECCRTL_UNDEFINED
#  define DECCRTL VAXCRTL_AND_DECCRTL_UNDEFINED
# endif
/* avoid some IMPLICITFUNC warnings */
# ifdef __DECC
#  include <starlet.h>
# endif  /* __DECC */
#endif /* VMS */

#if defined(_WINDOWS) || defined(_Windows) || defined(WIN32) || defined(_WIN32)
# ifndef _Windows
#  define _Windows
# endif
# ifndef WIN32
#  define WIN32
# endif
# ifndef _WIN32
#  define _WIN32
# endif
# ifdef _WIN64
#  define OS "MS-Windows 64 bit"
# else
#  define OS "MS-Windows 32 bit"
# endif
/* introduced by Pedro Mendes, prm@aber.ac.uk */
#  define far
/* Fix for broken compiler headers
 * See stdfn.h
 */
# define S_IFIFO  _S_IFIFO
# define HOME    "GNUPLOT"
# define PLOTRC  "gnuplot.ini"
# define SHELL   "\\command.com"
# define DIRSEP1 '\\'
# define DIRSEP2 '/'
# define PATHSEP ';'
# define GNUPLOT_HISTORY_FILE "~\\gnuplot_history"
/* Flags for windows.h:
   Minimal required platform is Windows XP
 */
#ifndef WINVER
# define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0501
#endif
#ifndef _WIN32_IE
# define _WIN32_IE 0x0501
#endif
#endif /* _WINDOWS */

#if defined(MSDOS) && !defined(_Windows)
/* should this be here ? */
# define OS       "MS-DOS"
# undef HELPFILE
# define HELPFILE "gnuplot.gih"
# define HOME     "GNUPLOT"
# define PLOTRC   "gnuplot.ini"
# define SHELL    "\\command.com"
# define DIRSEP1  '\\'
# define PATHSEP  ';'
# ifdef __DJGPP__
#  define DIRSEP2 '/'
# endif
# define GNUPLOT_HISTORY_FILE "~\\gnuplot.his"
#endif /* MSDOS */

/* End OS dependent constants; fall-through defaults
 * for the constants defined above are following.
 */

#ifndef OS
# define OS "non-recognized OS"
#endif

#ifndef HELPFILE
#ifndef VMS
# define HELPFILE "docs/gnuplot.gih"
#else
# define HELPFILE "sys$login:gnuplot.gih"
#endif
#endif

#ifndef HOME
# define HOME "HOME"
#endif

#ifndef PLOTRC
# define PLOTRC ".gnuplot"
#endif

#ifndef SHELL
# define SHELL "/bin/sh"    /* used if SHELL env variable not set */
#endif

#ifndef DIRSEP1
# define DIRSEP1 '/'
#endif

#ifndef DIRSEP2
# define DIRSEP2 NUL
#endif

#ifndef PATHSEP
# define PATHSEP ':'
#endif

#ifndef FAQ_LOCATION
#define FAQ_LOCATION "http://www.gnuplot.info/faq/"
#endif

#ifndef CONTACT
# define CONTACT "gnuplot-bugs@lists.sourceforge.net"
#endif

#ifndef HELPMAIL
# define HELPMAIL "gnuplot-info@lists.sourceforge.net"
#endif
/* End fall-through defaults */

/* Need this before any headers are incldued */
#ifdef PROTOTYPES
# define __PROTO(proto) proto
#else
# define __PROTO(proto) ()
#endif

/* DOS/Windows stuff. Moved here from command.c */
#if defined(MSDOS)
# ifdef DJGPP
#  include <dos.h>
#  include <dir.h>              /* HBB: for setdisk() */
# else
#  include <process.h>
# endif                         /* !DJGPP */
#endif /* MSDOS */

/* Watcom's compiler; this should probably be somewhere
 * in the Windows section
 */
#ifdef __WATCOMC__
# include <direct.h>
# include <dos.h>
# define HAVE_GETCWD 1
# define GP_EXCEPTION_NAME _exception
#endif

#ifdef __MSC__
# include <direct.h> /* for getcwd() */
#endif



#if defined(alliant)
# undef HAVE_LIMITS_H
#endif

#ifdef sequent
# undef HAVE_LIMITS_H
# undef HAVE_STRCHR
#endif

/* HBB 20000416: stuff moved from plot.h to here. It's system-dependent,
 * so it belongs here, IMHO */

/* BM 20110904: remnant of huge memory model support */
#define GPHUGE /* nothing */
#define GPFAR /* nothing */

/* LFS support */
#if !defined(HAVE_FSEEKO) || !defined(HAVE_OFF_T)
# if defined(HAVE_SYS_TYPES_H)
#   include <sys/types.h>
# endif
# if defined(_MSC_VER)
#   define off_t __int64
# elif defined(__MINGW32__)
#   define off_t off64_t
# elif !defined(HAVE_OFF_T)
#   define off_t long
# endif
#endif

typedef double coordval;

/* This is the maximum number of arguments in a user-defined function.
 * Note: This could be increased further, but in this case it would be good to
 * make  c_dummy_var[][] and set_dummy_var[][] into pointer arrays rather than
 * fixed-size storage for long variable name strings that will never be used.
 */
#define MAX_NUM_VAR	12

#ifdef VMS
# define DEFAULT_COMMENTS_CHARS "#!"
# define is_system(c) ((c) == '$')
/* maybe configure could check this? */
# define BACKUP_FILESYSTEM 1
#else /* not VMS */
# define DEFAULT_COMMENTS_CHARS "#"
# define is_system(c) ((c) == '!')
#endif /* not VMS */

#ifndef RETSIGTYPE
/* assume ANSI definition by default */
# define RETSIGTYPE void
#endif

#ifndef SIGFUNC_NO_INT_ARG
typedef RETSIGTYPE (*sigfunc)__PROTO((int));
#else
typedef RETSIGTYPE (*sigfunc)__PROTO((void));
#endif

#ifdef HAVE_SIGSETJMP
# define SETJMP(env, save_signals) sigsetjmp(env, save_signals)
# define LONGJMP(env, retval) siglongjmp(env, retval)
# define JMP_BUF sigjmp_buf
#else
# define SETJMP(env, save_signals) setjmp(env)
# define LONGJMP(env, retval) longjmp(env, retval)
# define JMP_BUF jmp_buf
#endif

/* generic pointer type. For old compilers this has to be changed to char *,
 * but I don't know if there are any CC's that support void and not void *
 */
#define generic void

/* HBB 20010720: removed 'sortfunc' --- it's no longer used */
/* FIXME HBB 20010720: Where is SORTFUNC_ARGS supposed to be defined?  */
#ifndef SORTFUNC_ARGS
#define SORTFUNC_ARGS const generic *
#endif

/* Macros for string concatenation */
#ifdef HAVE_STRINGIZE
/* ANSI version */
# define CONCAT(x,y) x##y
# define CONCAT3(x,y,z) x##y##z
#else
/* K&R version */
# define CONCAT(x,y) x/**/y
# define CONCAT3(x,y,z) x/**/y/**/z
#endif

/* Windows needs to redefine stdin/stdout functions */
#if defined(_Windows) && !defined(WINDOWS_NO_GUI)
# include "win/wtext.h"
#endif

#ifndef GP_EXCEPTION_NAME
# define GP_EXCEPTION_NAME exception
#endif

#ifndef GP_MATHERR
# define GP_MATHERR matherr
#endif

#ifdef HAVE_STRUCT_EXCEPTION_IN_MATH_H
# define STRUCT_EXCEPTION_P_X struct GP_EXCEPTION_NAME *x
#else
# define STRUCT_EXCEPTION_P_X /* nothing */
#endif

/* if GP_INLINE has not yet been defined, set to __inline__ for gcc,
 * nothing. I'd prefer that any other compilers have the defn in
 * the makefile, rather than having a huge list of compilers here.
 * But gcc is sufficiently ubiquitous that I'll allow it here !!!
 */
#ifndef GP_INLINE
# ifdef __GNUC__
#  define GP_INLINE __inline__
# else
#  define GP_INLINE /*nothing*/
# endif
#endif

#if HAVE_STDBOOL_H
# include <stdbool.h>
#else
# if ! HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
typedef unsigned char _Bool;
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

/* May or may not fix a problem reported for Sun Studio compilers */
#if defined(__SUNPRO_CC) && !defined __cplusplus && !defined(bool)
#define bool unsigned char
#endif

#undef TRUE
#define TRUE true
#undef FALSE
#define FALSE false

#define TBOOLEAN bool

#endif /* !SYSCFG_H */
