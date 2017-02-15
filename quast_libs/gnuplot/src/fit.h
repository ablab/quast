/*
 * $Id: fit.h,v 1.29.2.2 2015/02/26 18:43:46 sfeam Exp $
 */

/* GNUPLOT - fit.h */

/*  NOTICE: Change of Copyright Status
 *
 *  The author of this module, Carsten Grammes, has expressed in
 *  personal email that he has no more interest in this code, and
 *  doesn't claim any copyright. He has agreed to put this module
 *  into the public domain.
 *
 *  Lars Hecking  15-02-1999
 */

/*
 *	Header file: public functions in fit.c
 *
 *
 *	Previous copyright of this module:   Carsten Grammes, 1993
 *      Experimental Physics, University of Saarbruecken, Germany
 *
 *	Internet address: cagr@rz.uni-sb.de
 *
 *	Permission to use, copy, and distribute this software and its
 *	documentation for any purpose with or without fee is hereby granted,
 *	provided that the above copyright notice appear in all copies and
 *	that both that copyright notice and this permission notice appear
 *	in supporting documentation.
 *
 *      This software is provided "as is" without express or implied warranty.
 */


#ifndef GNUPLOT_FIT_H		/* avoid multiple inclusions */
#define GNUPLOT_FIT_H

/* #if... / #include / #define collection: */

#include "syscfg.h"
#include "stdfn.h"
#include "gp_types.h"

/* defaults */
#define DEF_FIT_LIMIT 1e-5

/* error interrupt for fitting routines */
#define Eex(a)       { error_ex(NO_CARET, (a)); }
#define Eex2(a,b)    { error_ex(NO_CARET, (a), (b)); }
#define Eex3(a,b,c)  { error_ex(NO_CARET, (a), (b), (c)); }
#define Eexc(c,a)    { error_ex((c), (a)); }
#define Eexc2(c,a,b) { error_ex((c), (a), (b)); }

/* Type definitions */

typedef enum e_verbosity_level {
    QUIET, RESULTS, BRIEF, VERBOSE
} verbosity_level;

typedef char fixstr[MAX_ID_LEN+1];

/* Exported Variables of fit.c */

extern const char *FITLIMIT;
extern const char *FITSTARTLAMBDA;
extern const char *FITLAMBDAFACTOR;
extern const char *FITMAXITER;

extern char *fitlogfile;
extern TBOOLEAN fit_suppress_log;
extern TBOOLEAN fit_errorvariables;
extern TBOOLEAN fit_covarvariables;
extern verbosity_level fit_verbosity;
extern TBOOLEAN fit_errorscaling;
extern TBOOLEAN fit_prescale;
extern char *fit_script;
extern double epsilon_abs;  /* absolute convergence criterion */
extern int maxiter;
extern int fit_wrap;
extern TBOOLEAN fit_v4compatible;

/* Prototypes of functions exported by fit.c */

#if defined(__GNUC__)
void error_ex(int t_num, const char *str, ...) __attribute__((noreturn));
#elif defined(_MSC_VER)
__declspec(noreturn) void error_ex(int t_num, const char *str, ...);
#else
void error_ex(int t_num, const char *str, ...);
#endif
void init_fit __PROTO((void));
void update __PROTO((char *pfile, char *npfile));
void fit_command __PROTO((void));
size_t wri_to_fil_last_fit_cmd __PROTO((FILE *fp));
char *getfitlogfile __PROTO((void));
char *getfitscript __PROTO((void));

void call_gnuplot(const double *par, double *data);
TBOOLEAN regress_check_stop(int iter, double chisq, double last_chisq, double lambda);
void fit_progress(int i, double chisq, double last_chisq, double* a, double lambda, FILE *device);

#endif /* GNUPLOT_FIT_H */
