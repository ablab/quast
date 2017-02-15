#ifndef lint
static char *RCSid() { return RCSid("$Id: fit.c,v 1.145.2.16 2016/05/13 17:28:17 markisch Exp $"); }
#endif

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
 *    Nonlinear least squares fit according to the
 *      Marquardt-Levenberg-algorithm
 *
 *      added as Patch to Gnuplot (v3.2 and higher)
 *      by Carsten Grammes
 *
 * Michele Marziani (marziani@ferrara.infn.it), 930726: Recoding of the
 * Unix-like raw console I/O routines.
 *
 * drd: start unitialised variables at 1 rather than NEARLY_ZERO
 *  (fit is more likely to converge if started from 1 than 1e-30 ?)
 *
 * HBB (broeker@physik.rwth-aachen.de) : fit didn't calculate the errors
 * in the 'physically correct' (:-) way, if a third data column containing
 * the errors (or 'uncertainties') of the input data was given. I think
 * I've fixed that, but I'm not sure I really understood the M-L-algo well
 * enough to get it right. I deduced my change from the final steps of the
 * equivalent algorithm for the linear case, which is much easier to
 * understand. (I also made some minor, mostly cosmetic changes)
 *
 * HBB (again): added error checking for negative covar[i][i] values and
 * for too many parameters being specified.
 *
 * drd: allow 3d fitting. Data value is now called fit_z internally,
 * ie a 2d fit is z vs x, and a 3d fit is z vs x and y.
 *
 * Lars Hecking : review update command, for VMS in particular, where
 * it is not necessary to rename the old file.
 *
 * HBB, 971023: lifted fixed limit on number of datapoints, and number
 * of parameters.
 *
 * HBB/H.Harders, 20020927: log file name now changeable from inside
 * gnuplot, not only by setting an environment variable.
 *
 * Jim Van Zandt, 090201: allow fitting functions with up to five
 * independent variables.
 *
 * Carl Michal, 120311: optionally prescale all the parameters that
 * the L-M routine sees by their initial values, so that parameters
 * that differ by many orders of magnitude do not cause problems.
 * With decent initial guesses, fits often take fewer iterations. If
 * any variables were 0, then don't do it for those variables, since
 * it may do more harm than good.
 *
 * Thomas Mattison, 130421: brief one-line reports, based on patchset #230.
 * Bastian Maerkisch, 130421: different output verbosity levels
 *
 * Bastian Maerkisch, 130427: remember parameters etc. of last fit and use
 * this data in a subsequent update command if the parameter file does not
 * exist yet.
 *
 * Thomas Mattison, 130508: New convergence criterion which is absolute
 * reduction in chisquare for an iteration of less than epsilon*chisquare
 * plus epsilon_abs (new setting).  The default convergence criterion is
 * always relative no matter what the chisquare is, but users now have the
 * flexibility of adding an absolute convergence criterion through
 * `set fit limit_abs`. Patchset #230.
 *
 * Ethan A Merritt, June 2013: Remove limit of 5 independent parameters.
 * The limit is now the smaller of MAXDATACOLS-2 and MAX_NUM_VAR.
 * Dissociate parameters other than x/y from "normal" plot axes.
 * To refine more than 2 parameters, name them with `set dummy`.
 * x and y still refer to axis_array[] in order to allow time/date formats.
 *
 * Bastian Maerkisch, Feb 2014: New syntax to specify errors. The new
 * parameter 'errors' accepts a comma separated list of (dummy) variables
 * to specify which (in-)dependent variable has associated errors. 'z'
 * always denotes the indep. variable. 'unitweights' tells fit to use equal
 * (1) weights for the fit. The new syntax removes the ambiguity between
 * x:y:z:(1) and x:z:s. The old syntax is still accepted but deprecated.
 *
 * Alexander Taeschner, Feb 2014: Optionally take errors of independent
 * variables into account.
 *
 * Bastian Maerkisch, Feb 2014: Split regress() into several functions
 * in order to facilitate the inclusion of alternative fitting codes.
 *
 * Karl Ratzsch, May 2014: Add a result variable reporting the number of
 * iterations
 *
 */

#include "fit.h"
#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "datafile.h"
#include "eval.h"
#include "gp_time.h"
#include "matrix.h"
#include "misc.h"
#include "plot.h"
#include "setshow.h"
#include "scanner.h"  /* For legal_identifier() */
#include "specfun.h"
#include "util.h"
#include "variable.h" /* For locale handling */
#include <signal.h>

/* Just temporary */
#if defined(VA_START) && defined(STDC_HEADERS)
static void Dblfn __PROTO((const char *fmt, ...));
#else
static void Dblfn __PROTO(());
#endif
#define Dblf  Dblfn
#define Dblf2 Dblfn
#define Dblf3 Dblfn
#define Dblf5 Dblfn
#define Dblf6 Dblfn

#if defined(MSDOS) 	/* non-blocking IO stuff */
# include <io.h>
# include <conio.h>
# include <dos.h>
#elif !defined(VMS)
#  include <fcntl.h>
#endif
#ifdef WIN32
# include "win/winmain.h"
#endif

/* constants */

#ifdef INFINITY
# undef INFINITY
#endif

#define INFINITY    1e30
#define NEARLY_ZERO 1e-30

/* create new variables with this value (was NEARLY_ZERO) */
#define INITIAL_VALUE 1.0

/* Relative change for derivatives */
#define DELTA       0.001

#define MAX_DATA    2048
#define MAX_PARAMS  32
#define MAX_LAMBDA  1e20
#define MIN_LAMBDA  1e-20
#define LAMBDA_UP_FACTOR 10
#define LAMBDA_DOWN_FACTOR 10

#if defined(MSDOS) || defined(OS2)
# define PLUSMINUS   "\xF1"	/* plusminus sign */
#else
# define PLUSMINUS   "+/-"
#endif

#define LASTFITCMDLENGTH 511

/* compatible with gnuplot philosophy */
#define STANDARD stderr

/* Suffix of a backup file */
#define BACKUP_SUFFIX ".old"

#define SQR(x) ((x) * (x))

/* type definitions */
enum marq_res {
    OK, ML_ERROR, BETTER, WORSE
};
typedef enum marq_res marq_res_t;


/* externally visible variables: */

/* fit control */
char *fitlogfile = NULL;
TBOOLEAN fit_suppress_log = FALSE;
TBOOLEAN fit_errorvariables = TRUE;
TBOOLEAN fit_covarvariables = FALSE;
verbosity_level fit_verbosity = BRIEF;
TBOOLEAN fit_errorscaling = TRUE;
TBOOLEAN fit_prescale = TRUE;
char *fit_script = NULL;
int fit_wrap = 0;
TBOOLEAN fit_v4compatible = FALSE;

/* names of user control variables */
const char * FITLIMIT = "FIT_LIMIT";
const char * FITSTARTLAMBDA = "FIT_START_LAMBDA";
const char * FITLAMBDAFACTOR = "FIT_LAMBDA_FACTOR";
const char * FITMAXITER = "FIT_MAXITER";

/* private variables: */

static double epsilon = DEF_FIT_LIMIT;	/* relative convergence limit */
double epsilon_abs = 0.0;               /* default to zero non-relative limit */
int maxiter = 0;
static double startup_lambda = 0;
static double lambda_down_factor = LAMBDA_DOWN_FACTOR;
static double lambda_up_factor = LAMBDA_UP_FACTOR;

static const char fitlogfile_default[] = "fit.log";
static const char GNUFITLOG[] = "FIT_LOG";
static FILE *log_f = NULL;
static TBOOLEAN fit_show_lambda = TRUE;
static const char *GP_FIXED = "# FIXED";
static const char *FITSCRIPT = "FIT_SCRIPT";
static const char *DEFAULT_CMD = "replot";	/* if no fitscript spec. */

static int num_data;
static int num_params;
static int num_indep;	 /* # independent variables in fit function */
static int num_errors;	 /* # error columns */
static TBOOLEAN err_cols[MAX_NUM_VAR+1];    /* TRUE if variable has an associated error */
static int columns;	 /* # values read from data file for each point */
static double *fit_x = 0;	/* all independent variable values,
				   e.g. value of the ith variable from
				   the jth data point is in
				   fit_x[j*num_indep+i] */
static double *fit_z = 0;	/* dependent data values */
static double *err_data = 0;	/* standard deviations of indep. and dependent data */
static double *a = 0;		/* array of fitting parameters */
static double **regress_C = 0;	/* global copy of C matrix in regress */
static void (* regress_cleanup)(void) = NULL;	/* memory cleanup function callback */
static TBOOLEAN user_stop = FALSE;
static double *scale_params = 0; /* scaling values for parameters */
static struct udft_entry func;
static fixstr *par_name;

static fixstr *last_par_name = NULL;
static int last_num_params = 0;
static char *last_dummy_var[MAX_NUM_VAR];
static char *last_fit_command = NULL;

/* Mar 2014 - the single hottest call path in fit was looking up the
 * dummy parameters by name (4 billion times in fit.dem).
 * A total waste, since they don't change.  Look up once and store here.
 */
static udvt_entry *fit_dummy_udvs[MAX_NUM_VAR];

/*****************************************************************
			 internal Prototypes
*****************************************************************/

#if !defined(WIN32) || defined(WGP_CONSOLE)
static RETSIGTYPE ctrlc_handle __PROTO((int an_int));
#endif
static void ctrlc_setup __PROTO((void));
static marq_res_t marquardt __PROTO((double a[], double **alpha, double *chisq,
				     double *lambda));
static void analyze(double a[], double **alpha, double beta[],
				 double *chisq, double **deriv);
static void calculate __PROTO((double *zfunc, double **dzda, double a[]));
static void calc_derivatives(const double *par, double *data, double **deriv);
static TBOOLEAN fit_interrupt __PROTO((void));
static TBOOLEAN regress __PROTO((double a[]));
static void regress_init(void);
static void regress_finalize(int iter, double chisq, double last_chisq, double lambda, double **covar);
static void fit_show(int i, double chisq, double last_chisq, double *a,
                          double lambda, FILE * device);
static void fit_show_brief(int iter, double chisq, double last_chisq, double *parms,
                          double lambda, FILE * device);
static void show_results __PROTO((double chisq, double last_chisq, double* a, double* dpar, double** corel));
static void log_axis_restriction __PROTO((FILE *log_f, int param,
			    double min, double max, int autoscale, char *name));
static void print_function_definitions(struct at_type *at, FILE * device);
static TBOOLEAN is_empty __PROTO((char *s));
static int getivar __PROTO((const char *varname));
static double getdvar __PROTO((const char *varname));
static double createdvar __PROTO((char *varname, double value));
static void setvar __PROTO((char *varname, double value));
static void setvarerr __PROTO((char *varname, double value));
static void setvarcovar(char *varname1, char *varname2, double value);
static char *get_next_word __PROTO((char **s, char *subst));
static void backup_file __PROTO((char *, const char *));


/*****************************************************************
    Small function to write the last fit command into a file
    Arg: Pointer to the file; if NULL, nothing is written,
         but only the size of the string is returned.
*****************************************************************/

size_t
wri_to_fil_last_fit_cmd(FILE *fp)
{
    if (last_fit_command == NULL)
	return 0;
    if (fp == NULL)
	return strlen(last_fit_command);
    else
	return (size_t) fputs(last_fit_command, fp);
}


/*****************************************************************
    This is called when a SIGINT occurs during fit
*****************************************************************/

#if !defined(WIN32) || defined(WGP_CONSOLE)
static RETSIGTYPE
ctrlc_handle(int an_int)
{
    (void) an_int;		/* avoid -Wunused warning */
    /* reinstall signal handler (necessary on SysV) */
    (void) signal(SIGINT, (sigfunc) ctrlc_handle);
    ctrlc_flag = TRUE;
}
#endif


/*****************************************************************
    setup the ctrl_c signal handler
*****************************************************************/
static void
ctrlc_setup()
{
/*
 *  MSDOS defines signal(SIGINT) but doesn't handle it through
 *  real interrupts. So there remain cases in which a ctrl-c may
 *  be uncaught by signal. We must use kbhit() instead that really
 *  serves the keyboard interrupt (or write an own interrupt func
 *  which also generates #ifdefs)
 *
 *  I hope that other OSes do it better, if not... add #ifdefs :-(
 */
#if (defined(__EMX__) || !defined(MSDOS)) && (!defined(WIN32) || defined(WGP_CONSOLE))
    (void) signal(SIGINT, (sigfunc) ctrlc_handle);
#endif
}


/*****************************************************************
    getch that handles also function keys etc.
*****************************************************************/
#if defined(MSDOS)

/* HBB 980317: added a prototype... */
int getchx __PROTO((void));

int
getchx()
{
    int c = getch();
    if (!c || c == 0xE0) {
	c <<= 8;
	c |= getch();
    }
    return c;
}
#endif


/*****************************************************************
    in case of fatal errors
*****************************************************************/
void
error_ex(int t_num, const char *str, ...)
{
    char buf[128];
    va_list args;

    va_start(args, str);
    vsnprintf(buf, sizeof(buf), str, args);
    va_end(args);

    /* cleanup - free memory */
    if (log_f) {
	fprintf(log_f, "BREAK: %s", buf);
	fclose(log_f);
	log_f = NULL;
    }
    free(fit_x);
    free(fit_z);
    free(err_data);
    free(a);
    a = fit_x = fit_z = err_data = NULL;
    if (func.at) {
	free_at(func.at);		/* release perm. action table */
	func.at = (struct at_type *) NULL;
    }

    if (regress_cleanup != NULL)
	(*regress_cleanup)();

    /* the datafile may still be open */
    df_close();

    /* restore original SIGINT function */
    interrupt_setup();

    /* FIXME: It would be nice to exit the "fit" command non-fatally, */
    /* so that the script who called it can recover and continue.     */
    /* int_error() makes that impossible.  But if we use int_warn()   */
    /* instead the program tries to continue _inside_ the fit, which  */
    /* generally then dies on some more serious error.                */

    /* exit via int_error() so that it can clean up state variables */
    int_error(t_num, buf);
}


/*****************************************************************
    Marquardt's nonlinear least squares fit
*****************************************************************/
static marq_res_t
marquardt(double a[], double **C, double *chisq, double *lambda)
{
    int i, j;
    static double *da = 0,	/* delta-step of the parameter */
    *temp_a = 0,		/* temptative new params set   */
    *d = 0, *tmp_d = 0, **tmp_C = 0, *residues = 0, **deriv = 0;
    double tmp_chisq;

    /* Initialization when lambda == -1 */
    if (*lambda == -1) {	/* Get first chi-square check */
	temp_a = vec(num_params);
	d = vec(num_data + num_params);
	tmp_d = vec(num_data + num_params);
	da = vec(num_params);
	residues = vec(num_data + num_params);
	tmp_C = matr(num_data + num_params, num_params);
	deriv = NULL;
	if (num_errors > 1)
	    deriv = matr(num_errors - 1, num_data);

	analyze(a, C, d, chisq, deriv);

	/* Calculate a useful startup value for lambda, as given by Schwarz */
	if (startup_lambda != 0)
	    *lambda = startup_lambda;
	else {
	    *lambda = 0;
	    for (i = 0; i < num_data; i++)
		for (j = 0; j < num_params; j++)
		    *lambda += C[i][j] * C[i][j];
	    *lambda = sqrt(*lambda / num_data / num_params);
	}

	/* Fill in the lower square part of C (the diagonal is filled in on
	   each iteration, see below) */
	for (i = 0; i < num_params; i++)
	    for (j = 0; j < i; j++)
		C[num_data + i][j] = 0, C[num_data + j][i] = 0;
	return OK;
    }

    /* once converged, free allocated vars */
    if (*lambda == -2) {
	free(d);
	free(tmp_d);
	free(da);
	free(temp_a);
	free(residues);
	free_matr(tmp_C);
	free_matr(deriv);
	/* may be called more than once */
	d = tmp_d = da = temp_a = residues = (double *) NULL;
	tmp_C = deriv = (double **) NULL;
	return OK;
    }

    /* Givens calculates in-place, so make working copies of C and d */
    for (j = 0; j < num_data + num_params; j++)
	memcpy(tmp_C[j], C[j], num_params * sizeof(double));
    memcpy(tmp_d, d, num_data * sizeof(double));

    /* fill in additional parts of tmp_C, tmp_d */
    for (i = 0; i < num_params; i++) {
	/* fill in low diag. of tmp_C ... */
	tmp_C[num_data + i][i] = *lambda;
	/* ... and low part of tmp_d */
	tmp_d[num_data + i] = 0;
    }

    Givens(tmp_C, tmp_d, da, num_params + num_data, num_params);

    /* check if trial did ameliorate sum of squares */
    for (j = 0; j < num_params; j++)
	temp_a[j] = a[j] + da[j];

    analyze(temp_a, tmp_C, tmp_d, &tmp_chisq, deriv);

    /* tsm patchset 230: Changed < to <= in next line */
    /* so finding exact minimum stops iteration instead of just increasing lambda. */
    /* Disadvantage is that if lambda is large enough so that chisq doesn't change */
    /* is taken as success. */
    if (tmp_chisq <= *chisq) {	/* Success, accept new solution */
	if (*lambda > MIN_LAMBDA) {
	    if (fit_verbosity == VERBOSE)
		putc('/', stderr);
	    *lambda /= lambda_down_factor;
	}
	/* update chisq, C, d, a */
	*chisq = tmp_chisq;
	for (j = 0; j < num_data; j++) {
	    memcpy(C[j], tmp_C[j], num_params * sizeof(double));
	    d[j] = tmp_d[j];
	}
	for (j = 0; j < num_params; j++)
	    a[j] = temp_a[j];
	return BETTER;
    } else {			/* failure, increase lambda and return */
	*lambda *= lambda_up_factor;
	if (fit_verbosity == VERBOSE)
	    (void) putc('*', stderr);
	else if (fit_verbosity == BRIEF)  /* one-line report even if chisq increases */
	    fit_show_brief(-1, tmp_chisq, *chisq, temp_a, *lambda, STANDARD);

	return WORSE;
    }
}


/*****************************************************************
    compute the (effective) error
*****************************************************************/
static double
effective_error(double **deriv, int i)
{
    double tot_err;
    int j, k;

    if (num_errors <= 1) /* z-errors or equal weights */
	tot_err = err_data[i];
    else {
	/* "Effective variance" according to 
	 *  Jay Orear, Am. J. Phys., Vol. 50, No. 10, October 1982 
	 */
	tot_err = SQR(err_data[i * num_errors + (num_errors - 1)]);
	for (j = 0, k = 0; j < num_indep; j++) {
	    if (err_cols[j]) {
		tot_err += SQR(deriv[k][i] * err_data[i * num_errors + k]);
		k++;
	    }
	}
	tot_err = sqrt(tot_err);
    }

    return tot_err;
}


/*****************************************************************
    compute chi-square and numeric derivations
*****************************************************************/
/* Used by marquardt to evaluate the linearized fitting matrix C and
 * vector d. Fills in only the top part of C and d. I don't use a
 * temporary array zfunc[] any more. Just use d[] instead.  */
static void
analyze(double a[], double **C, double d[], double *chisq, double ** deriv)
{
    int i, j;

    calculate(d, C, a);

    /* derivatives in indep. variables are required for 
       effective variance method */
    if (num_errors > 1)
	calc_derivatives(a, d, deriv);

    for (i = 0; i < num_data; i++) {
	double err = effective_error(deriv, i);
	/* note: order reversed, as used by Schwarz */
	d[i] = (d[i] - fit_z[i]) / err;
	for (j = 0; j < num_params; j++)
	    C[i][j] /= err;
    }
    *chisq = sumsq_vec(num_data, d);
}


/*****************************************************************
    compute function values and partial derivatives of chi-square
*****************************************************************/
/* To use the more exact, but slower two-side formula, activate the
   following line: */
/*#define TWO_SIDE_DIFFERENTIATION */
static void
calculate(double *zfunc, double **dzda, double a[])
{
    int k, p;
    double tmp_a;
    double *tmp_high, *tmp_pars;
#ifdef TWO_SIDE_DIFFERENTIATION
    double *tmp_low;
#endif

    tmp_high = vec(num_data);	/* numeric derivations */
#ifdef TWO_SIDE_DIFFERENTIATION
    tmp_low = vec(num_data);
#endif
    tmp_pars = vec(num_params);

    /* first function values */
    call_gnuplot(a, zfunc);

    /* then derivatives in parameters */
    for (p = 0; p < num_params; p++)
	tmp_pars[p] = a[p];
    for (p = 0; p < num_params; p++) {
	tmp_a = fabs(a[p]) < NEARLY_ZERO ? NEARLY_ZERO : a[p];
	tmp_pars[p] = tmp_a * (1 + DELTA);
	call_gnuplot(tmp_pars, tmp_high);
#ifdef TWO_SIDE_DIFFERENTIATION
	tmp_pars[p] = tmp_a * (1 - DELTA);
	call_gnuplot(tmp_pars, tmp_low);
#endif
	for (k = 0; k < num_data; k++)
#ifdef TWO_SIDE_DIFFERENTIATION
	    dzda[k][p] = (tmp_high[k] - tmp_low[k]) / (2 * tmp_a * DELTA);
#else
	    dzda[k][p] = (tmp_high[k] - zfunc[k]) / (tmp_a * DELTA);
#endif
	tmp_pars[p] = a[p];
    }

#ifdef TWO_SIDE_DIFFERENTIATION
    free(tmp_low);
#endif
    free(tmp_high);
    free(tmp_pars);
}


/*****************************************************************
    call internal gnuplot functions
*****************************************************************/
void
call_gnuplot(const double *par, double *data)
{
    int i, j;
    struct value v;

    /* set parameters first */
    for (i = 0; i < num_params; i++)
	setvar(par_name[i], par[i] * scale_params[i]);

    for (i = 0; i < num_data; i++) {
	/* calculate fit-function value */
	/* initialize extra dummy variables from the corresponding
	 actual variables, if any. */
	for (j = 0; j < MAX_NUM_VAR; j++) {
	    struct udvt_entry *udv = fit_dummy_udvs[j];
	    if (!udv)
		int_error(NO_CARET, "Internal error: lost a dummy parameter!");
	    Gcomplex(&func.dummy_values[j],
	             udv->udv_undef ? 0 : real(&(udv->udv_value)),
	             0.0);
	}
	/* set actual dummy variables from file data */
	for (j = 0; j < num_indep; j++)
	    Gcomplex(&func.dummy_values[j],
	             fit_x[i * num_indep + j], 0.0);
	evaluate_at(func.at, &v);

	if (undefined || isnan(real(&v))) {
	    /* Print useful info on undefined-function error. */
	    Dblf("\nCurrent data point\n");
	    Dblf("=========================\n");
	    Dblf3("%-15s = %i out of %i\n", "#", i + 1, num_data);
	    for (j = 0; j < num_indep; j++)
		Dblf3("%-15.15s = %-15g\n", c_dummy_var[j], par[j] * scale_params[j]);
	    Dblf3("%-15.15s = %-15g\n", "z", fit_z[i]);
	    Dblf("\nCurrent set of parameters\n");
	    Dblf("=========================\n");
	    for (j = 0; j < num_params; j++)
		Dblf3("%-15.15s = %-15g\n", par_name[j], par[j] * scale_params[j]);
	    Dblf("\n");
	    if (undefined) {
		Eex("Undefined value during function evaluation");
	    } else {
		Eex("Function evaluation yields NaN (\"not a number\")");
	    }
	}

	data[i] = real(&v);
    }
}


/*****************************************************************
    calculate derivatives wrt the parameters
*****************************************************************/
/* Used to calculate the effective variance in effective_error() */
static void
calc_derivatives(const double *par, double *data, double **deriv)
{
    int i, j, k, m;
    struct value v;
    double h;

    /* set parameters first */
    for (i = 0; i < num_params; i++)
	setvar(par_name[i], par[i] * scale_params[i]);

    for (i = 0; i < num_data; i++) { /* loop over data points */
	for (j = 0, m = 0; j < num_indep; j++) { /* loop over indep. variables */
	    double tmp_high;
	    double tmp_x;
#ifdef TWO_SIDE_DIFFERENTIATION
	    double tmp_low;
#endif
	    /* only calculate derivatives if necessary */
	    if (!err_cols[j])
		continue;

	    /* set actual dummy variables from file data */
	    for (k = 0; k < num_indep; k++) {
		if (j != k)
		    Gcomplex(&func.dummy_values[k],
		             fit_x[i * num_indep + k], 0.0);
	    }
	    tmp_x = fit_x[i * num_indep + j];
	    /* optimal step size */
	    h = GPMAX(DELTA * fabs(tmp_x), 8*1e-8*(fabs(tmp_x) + 1e-8));
	    Gcomplex(&func.dummy_values[j], tmp_x + h, 0.0);
	    evaluate_at(func.at, &v);
	    tmp_high = real(&v);
#ifdef TWO_SIDE_DIFFERENTIATION
	    Gcomplex(&func.dummy_values[j], tmp_x - h, 0.0);
	    evaluate_at(func.at, &v);
	    tmp_low = real(&v);
	    deriv[m][i] = (tmp_high - tmp_low) / (2 * h);
#else
	    deriv[m][i] = (tmp_high - data[i]) / h;
#endif
	    m++;
	}
    }
}


/*****************************************************************
    handle user interrupts during fit
*****************************************************************/
static TBOOLEAN
fit_interrupt()
{
    while (TRUE) {
	fputs("\n\n(S)top fit, (C)ontinue, (E)xecute FIT_SCRIPT:  ", STANDARD);
#ifdef WIN32
	WinRaiseConsole();
#endif
	switch (getchar()) {

	case EOF:
	case 's':
	case 'S':
	    fputs("Stop.\n", STANDARD);
	    user_stop = TRUE;
	    return FALSE;

	case 'c':
	case 'C':
	    fputs("Continue.\n", STANDARD);
	    return TRUE;

	case 'e':
	case 'E':{
		int i;
		char *tmp;

		tmp = getfitscript();
		fprintf(STANDARD, "executing: %s\n", tmp);
		/* FIXME: Shouldn't we also set FIT_STDFIT etc? */
		/* set parameters visible to gnuplot */
		for (i = 0; i < num_params; i++)
		    setvar(par_name[i], a[i] * scale_params[i]);
		do_string(tmp);
		free(tmp);
	    }
	}
    }
    return TRUE;
}


/*****************************************************************
    determine current setting of FIT_SCRIPT
*****************************************************************/
char *
getfitscript(void)
{
    char *tmp;

    if (fit_script != NULL)
	return gp_strdup(fit_script);
    if ((tmp = getenv(FITSCRIPT)) != NULL)
	return gp_strdup(tmp);
    else
	return gp_strdup(DEFAULT_CMD);
}


/*****************************************************************
    initial setup for regress()
*****************************************************************/
static void
regress_init(void)
{
    struct udvt_entry *v;	/* For exporting results to the user */

    /* Reset flag describing fit result status */
    v = add_udv_by_name("FIT_CONVERGED");
    v->udv_undef = FALSE;
    Ginteger(&v->udv_value, 0);

    /* Ctrl-C now serves as Hotkey */
    ctrlc_setup();

    /* HBB 981118: initialize new variable 'user_break' */
    user_stop = FALSE;
}


/*****************************************************************
    finalize regression: print results and set user variables
*****************************************************************/
static void
regress_finalize(int iter, double chisq, double last_chisq, double lambda, double **covar)
{
    int i, j;
    struct udvt_entry *v;	/* For exporting results to the user */
    int ndf;
    int niter;
    double stdfit;
    double pvalue;
    double *dpar;
    double **corel = NULL;
    TBOOLEAN covar_invalid = FALSE;

    /* restore original SIGINT function */
    interrupt_setup();

    /* tsm patchset 230: final progress report labels to console */
    if (fit_verbosity == BRIEF)
	fit_show_brief(-2, chisq, chisq, a, lambda, STANDARD);

    /* tsm patchset 230: final progress report to log file */
    if (!fit_suppress_log) {
	if (fit_verbosity == VERBOSE)
	    fit_show(iter, chisq, last_chisq, a, lambda, log_f);
	else
	    fit_show_brief(iter, chisq, last_chisq, a, lambda, log_f);
    }

    /* test covariance matrix */
    if (covar != NULL) {
	for (i = 0; i < num_params; i++) {
	    /* diagonal elements must be larger than zero */
	    if (covar[i][i] <= 0.0) {
		/* Not a fatal error, but prevent floating point exception later on */
		Dblf2("Calculation error: non-positive diagonal element in covar. matrix of parameter '%s'.\n", par_name[i]);
		covar_invalid = TRUE;
	    }
	}
    }

    /* HBB 970304: the maxiter patch: */
    if ((maxiter > 0) && (iter > maxiter)) {
	Dblf2("\nMaximum iteration count (%d) reached. Fit stopped.\n", maxiter);
    } else if (user_stop) {
	Dblf2("\nThe fit was stopped by the user after %d iterations.\n", iter);
    } else if (lambda >= MAX_LAMBDA) {
	Dblf2("\nThe maximum lambda = %e was exceeded. Fit stopped.\n", MAX_LAMBDA);
    } else if (covar_invalid) {
	Dblf2("\nThe covariance matrix is invalid. Fit did not converge properly.\n");
    } else {
	Dblf2("\nAfter %d iterations the fit converged.\n", iter);
	v = add_udv_by_name("FIT_CONVERGED");
	v->udv_undef = FALSE;
	Ginteger(&v->udv_value, 1);
    }

    /* fit results */
    ndf    = num_data - num_params;
    stdfit = sqrt(chisq / ndf);
    pvalue = 1. - chisq_cdf(ndf, chisq);
    niter = iter;

    /* Export these to user-accessible variables */
    v = add_udv_by_name("FIT_NDF");
    v->udv_undef = FALSE;
    Ginteger(&v->udv_value, ndf);
    v = add_udv_by_name("FIT_STDFIT");
    v->udv_undef = FALSE;
    Gcomplex(&v->udv_value, stdfit, 0);
    v = add_udv_by_name("FIT_WSSR");
    v->udv_undef = FALSE;
    Gcomplex(&v->udv_value, chisq, 0);
    v = add_udv_by_name("FIT_P");
    v->udv_undef = FALSE;
    Gcomplex(&v->udv_value, pvalue, 0);
    v = add_udv_by_name("FIT_NITER");
    v->udv_undef = FALSE;
    Ginteger(&v->udv_value, niter);    

    /* Save final parameters. Depending on the backend and
       its internal state, the last call_gnuplot may not have been
       at the minimum */
    for (i = 0; i < num_params; i++)
	setvar(par_name[i], a[i] * scale_params[i]);

    /* Set error and covariance variables to zero, 
       thus making sure they are created. */
    if (fit_errorvariables) {
	for (i = 0; i < num_params; i++)
	    setvarerr(par_name[i], 0.0);
    }
    if (fit_covarvariables) {
	/* first, remove all previous covariance variables */
	del_udv_by_name("FIT_COV_*", TRUE);
	for (i = 0; i < num_params; i++) {
	    for (j = 0; j < i; j++) {
		setvarcovar(par_name[i], par_name[j], 0.0);
		setvarcovar(par_name[j], par_name[i], 0.0);
	    }
	    setvarcovar(par_name[i], par_name[i], 0.0);
	}
    }

    /* calculate unscaled parameter errors in dpar[]: */
    dpar = vec(num_params);
    if ((covar != NULL) && !covar_invalid) {
	/* calculate unscaled parameter errors in dpar[]: */
	for (i = 0; i < num_params; i++) {
	    dpar[i] = sqrt(covar[i][i]);
	}

	/* transform covariances into correlations */
	corel = matr(num_params, num_params);
	for (i = 0; i < num_params; i++) {
	    /* only lower triangle needs to be handled */
	    for (j = 0; j < i; j++)
		corel[i][j] = covar[i][j] / (dpar[i] * dpar[j]);
	    corel[i][i] = 1.;
	}
    } else {
	/* set all errors to zero if covariance matrix invalid or unavailable */
	for (i = 0; i < num_params; i++)
	    dpar[i] = 0.0;
    }

    if (fit_errorscaling || (num_errors == 0)) {
	/* scale parameter errors based on chisq */
	double temp = sqrt(chisq / (num_data - num_params));
	for (i = 0; i < num_params; i++)
	    dpar[i] *= temp;
    }

    /* Save user error variables. */
    if (fit_errorvariables) {
	for (i = 0; i < num_params; i++)
	    setvarerr(par_name[i], dpar[i] * scale_params[i]);
    }

    /* fill covariance variables if needed */
    if (fit_covarvariables && (covar != NULL) && !covar_invalid) {
	double scale =
	    (fit_errorscaling || (num_errors == 0)) ?
	    (chisq / (num_data - num_params)) : 1.0;
	for (i = 0; i < num_params; i++) {
	    /* only lower triangle needs to be handled */
	    for (j = 0; j <= i; j++) {
		double temp = scale * scale_params[i] * scale_params[j];
		setvarcovar(par_name[i], par_name[j], covar[i][j] * temp);
		setvarcovar(par_name[j], par_name[i], covar[i][j] * temp);
	    }
	}
    }

    show_results(chisq, last_chisq, a, dpar, corel);

    free(dpar);
    free_matr(corel);
}


/*****************************************************************
    test for user request to stop the fit
*****************************************************************/
TBOOLEAN
regress_check_stop(int iter, double chisq, double last_chisq, double lambda)
{
/*
 *  MSDOS defines signal(SIGINT) but doesn't handle it through
 *  real interrupts. So there remain cases in which a ctrl-c may
 *  be uncaught by signal. We must use kbhit() instead that really
 *  serves the keyboard interrupt (or write an own interrupt func
 *  which also generates #ifdefs)
 *
 *  I hope that other OSes do it better, if not... add #ifdefs :-(
 *  EMX does not have kbhit.
 *
 *  HBB: I think this can be enabled for DJGPP V2. SIGINT is actually
 *  handled there, AFAIK.
 */
#if (defined(MSDOS) && !defined(__EMX__))
    if (kbhit()) {
	do {
	    getchx();
	} while (kbhit());
	ctrlc_flag = TRUE;
    }
#endif
#ifdef WIN32
    /* This call makes the Windows GUI functional during fits.
       Pressing Ctrl-Break now finally has an effect. */
    WinMessageLoop();
#endif

    if (ctrlc_flag) {
	/* Always report on current status. */
	if (fit_verbosity == VERBOSE)
	    fit_show(iter, chisq, last_chisq, a, lambda, STANDARD);
	else
	    fit_show_brief(iter, chisq, last_chisq, a, lambda, STANDARD);

	ctrlc_flag = FALSE;
	if (!fit_interrupt())	/* handle keys */
	    return FALSE;
    }
    return TRUE;
}


/*****************************************************************
    free memory allocated by gnuplot's internal fitting code
*****************************************************************/
static void
internal_cleanup(void)
{
    double lambda;

    free_matr(regress_C);
    regress_C = NULL;
    lambda = -2;		/* flag value, meaning 'destruct!' */
    marquardt(NULL, NULL, NULL, &lambda);
}


/*****************************************************************
    frame routine for the marquardt-fit
*****************************************************************/
static TBOOLEAN
regress(double a[])
{
    double **covar, **C, chisq, last_chisq, lambda;
    int iter;
    marq_res_t res;

    regress_cleanup = &internal_cleanup;

    chisq = last_chisq = INFINITY;
    /* the global copy to is accessible to error_ex, too */
    regress_C = C = matr(num_data + num_params, num_params);
    lambda = -1;		/* use sign as flag */
    iter = 0;			/* iteration counter  */

    /* Initialize internal variables and 1st chi-square check */
    if ((res = marquardt(a, C, &chisq, &lambda)) == ML_ERROR)
	Eex("FIT: error occurred during fit");
    res = BETTER;

    fit_show_lambda = TRUE;
    fit_progress(iter, chisq, chisq, a, lambda, STANDARD);
    if (!fit_suppress_log)
	fit_progress(iter, chisq, chisq, a, lambda, log_f);

    regress_init();

    /* MAIN FIT LOOP: do the regression iteration */

    do {
	if (!regress_check_stop(iter, chisq, last_chisq, lambda))
	    break;
	if (res == BETTER) {
	    iter++;
	    last_chisq = chisq;
	}
	if ((res = marquardt(a, C, &chisq, &lambda)) == BETTER)
	    fit_progress(iter, chisq, last_chisq, a, lambda, STANDARD);
    } while ((res != ML_ERROR)
	     && (lambda < MAX_LAMBDA)
	     && ((maxiter == 0) || (iter <= maxiter))
	     && (chisq != 0)
	     && (res == WORSE ||
	    /* tsm patchset 230: change to new convergence criterion */
	         ((last_chisq - chisq) > (epsilon * chisq + epsilon_abs))));

    /* fit done */

    if (res == ML_ERROR)
	Eex("FIT: error occurred during fit");

    /* compute errors in the parameters */

    /* get covariance-, correlation- and curvature-matrix */
    /* and errors in the parameters                     */

    /* compute covar[][] directly from C */
    Givens(C, 0, 0, num_data, num_params);

    /* Use lower square of C for covar */
    covar = C + num_data;
    Invert_RtR(C, covar, num_params);

    regress_finalize(iter, chisq, last_chisq, lambda, covar);

    /* call destructor for allocated vars */
    internal_cleanup();
    regress_cleanup = NULL;
    return TRUE;
}


/*****************************************************************
    display results of the fit
*****************************************************************/
static void
show_results(double chisq, double last_chisq, double *a, double *dpar, double **corel)
{
    int i, j, k;
    TBOOLEAN have_errors = TRUE;

    Dblf2("final sum of squares of residuals : %g\n", chisq);
    if (chisq > NEARLY_ZERO) {
	Dblf2("rel. change during last iteration : %g\n\n", (chisq - last_chisq) / chisq);
    } else {
	Dblf2("abs. change during last iteration : %g\n\n", (chisq - last_chisq));
    }

    if ((num_data == num_params) && ((num_errors == 0) || fit_errorscaling)) {
	Dblf("\nExactly as many data points as there are parameters.\n");
	Dblf("In this degenerate case, all errors are zero by definition.\n\n");
	have_errors = FALSE;
    } else if ((chisq < NEARLY_ZERO) && ((num_errors == 0) || fit_errorscaling)) {
	Dblf("\nHmmmm.... Sum of squared residuals is zero. Can't compute errors.\n\n");
	have_errors = FALSE;
    } else if (corel == NULL) {
	Dblf("\nCovariance matric unavailable. Can't compute errors.\n\n");
	have_errors = FALSE;
    }

    if (!have_errors) {
	Dblf("Final set of parameters \n");
	Dblf("======================= \n\n");
	for (k = 0; k < num_params; k++)
	    Dblf3("%-15.15s = %-15g\n", par_name[k], a[k] * scale_params[k]);
    } else {
	int ndf          = num_data - num_params;
	double stdfit    = sqrt(chisq/ndf);
	double pvalue    = 1. - chisq_cdf(ndf, chisq);

	Dblf2("degrees of freedom    (FIT_NDF)                        : %d\n", ndf);
	Dblf2("rms of residuals      (FIT_STDFIT) = sqrt(WSSR/ndf)    : %g\n", stdfit);
	Dblf2("variance of residuals (reduced chisquare) = WSSR/ndf   : %g\n", chisq / ndf);
	/* We cannot know if the errors supplied by the user are weighting factors
	or real errors, so we print the p-value in any case, although it does not
	make much sense in the first case.  This means that we print this for x:y:z:(1)
	fits without errors using the old syntax since this requires 4 columns. */
	if (num_errors > 0)
	    Dblf2("p-value of the Chisq distribution (FIT_P)              : %g\n", pvalue);
	Dblf("\n");

	if (fit_errorscaling || (num_errors == 0))
	    Dblf("Final set of parameters            Asymptotic Standard Error\n");
	else
	    Dblf("Final set of parameters            Standard Deviation\n");
	Dblf("=======================            ==========================\n");

	for (i = 0; i < num_params; i++) {
	    double temp = (fabs(a[i]) < NEARLY_ZERO)
		? 0.0
		: fabs(100.0 * dpar[i] / a[i]);

	    Dblf6("%-15.15s = %-15g  %-3.3s %-12.4g (%.4g%%)\n",
		  par_name[i], a[i] * scale_params[i], PLUSMINUS, dpar[i] * scale_params[i], temp);
	}

	/* Print correlation matrix only if there is more than one parameter. */
	if ((num_params > 1) && (corel != NULL)) {
	    Dblf("\ncorrelation matrix of the fit parameters:\n");

	    Dblf("                ");
	    for (j = 0; j < num_params; j++)
		Dblf2("%-6.6s ", par_name[j]);
	    Dblf("\n");

	    for (i = 0; i < num_params; i++) {
		Dblf2("%-15.15s", par_name[i]);
		for (j = 0; j <= i; j++) {
		    /* Only print lower triangle of symmetric matrix */
		    Dblf2("%6.3f ", corel[i][j]);
		}
		Dblf("\n");
	    }
	}
    }
}


/*****************************************************************
    display actual state of the fit
*****************************************************************/
void
fit_progress(int i, double chisq, double last_chisq, double* a, double lambda, FILE *device)
{
    if (fit_verbosity == VERBOSE)
	fit_show(i, chisq, last_chisq, a, lambda, device);
    else if (fit_verbosity == BRIEF)
	fit_show_brief(i, chisq, last_chisq, a, lambda, device);
}


static void
fit_show(int i, double chisq, double last_chisq, double* a, double lambda, FILE *device)
{
    int k;

    fprintf(device, "\n\n\
 Iteration %d\n\
 WSSR        : %-15g   delta(WSSR)/WSSR   : %g\n\
 delta(WSSR) : %-15g   limit for stopping : %g\n",
	    i, chisq, chisq > NEARLY_ZERO ? (chisq - last_chisq) / chisq : 0.0,
	    chisq - last_chisq, epsilon);
    if (fit_show_lambda)
	fprintf(device, "\
 lambda	     : %g\n", lambda);
	fprintf(device, "\n\
%s parameter values\n\n",
	    (i > 0 ? "resultant" : "initial set of free"));
    for (k = 0; k < num_params; k++)
	fprintf(device, "%-15.15s = %g\n", par_name[k], a[k] * scale_params[k]);
}


/* If the exponent of a floating point number in scientific format (%e) has three
digits and the highest digit is zero, it will get removed by this routine. */
static char *
pack_float(char *num)
{
    static int needs_packing = -1;
    if (needs_packing < 0) {
	/* perform the test only once */
	char buf[12];
	snprintf(buf, sizeof(buf), "%.2e", 1.00); /* "1.00e+000" or "1.00e+00" */
	needs_packing = (strlen(buf) == 9);
    }
    if (needs_packing) {
	char *p = strchr(num, 'e');
	if (p == NULL)
	    p = strchr(num, 'E');
	if (p != NULL) {
	    p += 2;  /* also skip sign of exponent */
	    if (*p == '0') {
		do {
		    *p = *(p + 1);
		} while (*++p != NUL);
	    }
	}
    }
    return num;
}


/* tsm patchset 230: new one-line version of progress report */
static void
fit_show_brief(int iter, double chisq, double last_chisq, double* parms, double lambda, FILE *device)
{
    int k, len;
    double delta, lim;
    char buf[256];
    char *p;
    const int indent = 4;

    /* on iteration 0 or -2, print labels */
    if (iter == 0 || iter == -2) {
	strcpy(buf, "iter      chisq       delta/lim ");
	          /* 9999 1.1234567890e+00 -1.12e+00 */
	if (fit_show_lambda)
	    strcat(buf, " lambda  ");
		      /* 1.00e+00 */
	fputs(buf, device);
	len = strlen(buf);
	for (k = 0; k < num_params; k++) {
	    snprintf(buf, sizeof(buf), " %-13.13s", par_name[k]);
	    len += strlen(buf);
	    if ((fit_wrap > 0) && (len >= fit_wrap)) {
		fprintf(device, "\n%*c", indent, ' ');
		len = indent;
	    }
	    fputs(buf, device);
	}
	fputs("\n", device);
    }

    /* on iteration -2, don't print anything else */
    if (iter == -2) return;

    /* new convergence test quantities */
    delta = chisq - last_chisq;
    lim = epsilon * chisq + epsilon_abs;

    /* print values */
    if (iter >= 0)
	snprintf(buf, sizeof(buf), "%4i", iter);
    else /* -1 indicates that chisquare increased */
	snprintf(buf, sizeof(buf), "%4c", '*');
    snprintf(buf + 4, sizeof(buf) - 4, " %-17.10e %- 10.2e", chisq, delta / lim);
    if (fit_show_lambda)
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %-9.2e", lambda);
    for (k = 0, p = buf + 4; (k < 3) && (p != NULL); k++) {
	p++;
	pack_float(p);
	p = strchr(p, 'e');
    }
    fputs(buf, device);
    len = strlen(buf);
    for (k = 0; k < num_params; k++) {
	snprintf(buf, sizeof(buf), " % 14.6e", parms[k] * scale_params[k]);
	pack_float(buf);
	len += strlen(buf);
	if ((fit_wrap > 0) && (len >= fit_wrap)) {
	    fprintf(device, "\n%*c", indent, ' ');
	    len = indent;
	}
	fputs(buf, device);
    }
    fputs("\n", device);
}


/*****************************************************************
    is_empty: check for valid string entries
*****************************************************************/
static TBOOLEAN
is_empty(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
	s++;
    return (TBOOLEAN) (*s == '#' || *s == '\0');
}


/*****************************************************************
    get next word of a multi-word string, advance pointer
*****************************************************************/
static char *
get_next_word(char **s, char *subst)
{
    char *tmp = *s;

    while (*tmp == ' ' || *tmp == '\t' || *tmp == '=')
	tmp++;
    if (*tmp == '\n' || *tmp == '\r' || *tmp == '\0')	/* not found */
	return NULL;
    if ((*s = strpbrk(tmp, " =\t\n\r")) == NULL)
	*s = tmp + strlen(tmp);
    *subst = **s;
    *(*s)++ = '\0';
    return tmp;
}


/*****************************************************************
    first time settings
*****************************************************************/
void
init_fit()
{
    func.at = (struct at_type *) NULL;	/* need to parse 1 time */
}


/*****************************************************************
    Set a GNUPLOT user-defined variable
******************************************************************/

static void
setvar(char *varname, double data)
{
    /* Despite its name it is actually usable for any variable. */
    fill_gpval_float(varname, data);
}


/*****************************************************************
    Set a user-defined variable from an error variable: 
    Take the parameter name, turn it  into an error parameter
    name (e.g. a to a_err) and then set it.
******************************************************************/
static void
setvarerr(char *varname, double value)
{
    /* Create the variable name by appending _err */
    char * pErrValName = (char *) gp_alloc(strlen(varname) + 6, "setvarerr");
    sprintf(pErrValName, "%s_err", varname);
    setvar(pErrValName, value);
    free(pErrValName);
}


/*****************************************************************
    Set a user-defined covariance variable:
    Take the two parameter names, turn them into an covariance
    parameter name (e.g. a and b to FIT_COV_a_b) and then set it.
******************************************************************/
static void
setvarcovar(char *varname1, char *varname2, double value)
{
    /* The name of the (new) covariance variable */
    char * pCovValName = (char *) gp_alloc(strlen(varname1) + strlen(varname2) + 10, "setvarcovar");
    sprintf(pCovValName, "FIT_COV_%s_%s", varname1, varname2);
    setvar(pCovValName, value);
    free(pCovValName);
}


/*****************************************************************
    Get integer variable value
*****************************************************************/
static int
getivar(const char *varname)
{
    struct udvt_entry * v = get_udv_by_name((char *)varname);
    if ((v != NULL) && (!v->udv_undef))
	return real_int(&(v->udv_value));
    else
	return 0;
}


/*****************************************************************
    Get double variable value
*****************************************************************/
static double
getdvar(const char *varname)
{
    struct udvt_entry * v = get_udv_by_name((char *)varname);
    if ((v != NULL) && (!v->udv_undef))
	return real(&(v->udv_value));
    else
	return 0;
}


/*****************************************************************
   like getdvar, but
   - create it and set to `value` if not found or undefined
   - convert it from integer to real if necessary
*****************************************************************/
static double
createdvar(char *varname, double value)
{
    struct udvt_entry *udv_ptr = add_udv_by_name((char *)varname);
    if (udv_ptr->udv_undef) { /* new variable */
	udv_ptr->udv_undef = FALSE;
	Gcomplex(&udv_ptr->udv_value, value, 0.0);
    } else if (udv_ptr->udv_value.type == INTGR) { /* convert to CMPLX */
	Gcomplex(&udv_ptr->udv_value, (double) udv_ptr->udv_value.v.int_val, 0.0);
    }
    return real(&(udv_ptr->udv_value));
}


/* argument: char *fn */
#define VALID_FILENAME(fn) ((fn) != NULL && (*fn) != '\0')

/*****************************************************************
    write the actual parameters to start parameter file
*****************************************************************/
void
update(char *pfile, char *npfile)
{
    char ifilename[PATH_MAX];
    char *ofilename;
    TBOOLEAN createfile = FALSE;

    if (existfile(pfile)) {
	/* update pfile npfile:
	   if npfile is a valid file name, take pfile as input file and
	   npfile as output file
	*/
	if (VALID_FILENAME(npfile)) {
	    safe_strncpy(ifilename, pfile, sizeof(ifilename));
	    ofilename = npfile;
	} else {
#ifdef BACKUP_FILESYSTEM
	    /* filesystem will keep original as previous version */
	    safe_strncpy(ifilename, pfile, sizeof(ifilename));
#else
	    backup_file(ifilename, pfile);	/* will Eex if it fails */
#endif
	    ofilename = pfile;
	}
    } else {
	/* input file does not exists; will create new file */
	createfile = TRUE;
	if (VALID_FILENAME(npfile))
	    ofilename = npfile;
	else
	    ofilename = pfile;
    }

    if (createfile) {
	/* The input file does not exists and--strictly speaking--there is
	   nothing to 'update'.  Instead of bailing out we guess the intended use:
	   We output all INTGR/CMPLX user variables and mark them as '# FIXED' if
	   they were not used during the last fit command. */
	struct udvt_entry *udv = first_udv;
	FILE *nf;

	if ((last_fit_command == NULL) || (strlen(last_fit_command) == 0)) {
	    /* Technically, a prior fit command isn't really required.  But since
	    all variables in the parameter file would be marked '# FIXED' in that
	    case, it cannot be directly used in a subsequent fit command. */
#if 1
	    Eex2("'update' requires a prior 'fit' since the parameter file %s does not exist yet.", ofilename);
#else
	    fprintf(stderr, "'update' without a prior 'fit' and without a previous parameter file:\n");
	    fprintf(stderr, " all variables will be marked '# FIXED'!\n");
#endif
	}

	if (!(nf = fopen(ofilename, "w")))
	    Eex2("new parameter file %s could not be created", ofilename);

	fputs("# Parameter file created by 'update' from current variables\n", nf);
	if ((last_fit_command != NULL) && (strlen(last_fit_command) > 0))
	    fprintf(nf, "## %s\n", last_fit_command);

	while (udv) {
	    if ((strncmp(udv->udv_name, "GPVAL_", 6) == 0) ||
	        (strncmp(udv->udv_name, "MOUSE_", 6) == 0) ||
	        (strncmp(udv->udv_name, "FIT_", 4) == 0) ||
	        (strcmp(udv->udv_name, "NaN") == 0) ||
	        (strcmp(udv->udv_name, "pi") == 0)) {
		/* skip GPVAL_, MOUSE_, FIT_ and builtin variables */
		udv = udv->next_udv;
		continue;
	    }
	    if (!udv->udv_undef &&
	        ((udv->udv_value.type == INTGR) || (udv->udv_value.type == CMPLX))) {
		int k;

		/* ignore indep. variables */
		for (k = 0; k < MAX_NUM_VAR; k++) {
		    if (last_dummy_var[k] && strcmp(last_dummy_var[k], udv->udv_name) == 0)
			break;
		}
		if (k != MAX_NUM_VAR) {
		    udv = udv->next_udv;
		    continue;
		}

		if (udv->udv_value.type == INTGR)
		    fprintf(nf, "%-15s = %-22i", udv->udv_name, udv->udv_value.v.int_val);
		else /* CMPLX */
		    fprintf(nf, "%-15s = %-22s", udv->udv_name, num_to_str(udv->udv_value.v.cmplx_val.real));
		/* mark variables not used for the last fit as fixed */
		for (k = 0; k < last_num_params; k++) {
		    if (strcmp(last_par_name[k], udv->udv_name) == 0)
			break;
		}
		if (k == last_num_params)
		    fprintf(nf, "   %s", GP_FIXED);
		putc('\n', nf);
	    }
	    udv = udv->next_udv;
	}

	if (fclose(nf))
	    Eex("I/O error during update");

    } else { /* !createfile */

	/* input file exists - this is the originally intended case of
	   the update command: update an existing parameter file */
	char sstr[256];
	char *s = sstr;
	char * fnam;
	FILE *of, *nf;

	if (!(of = loadpath_fopen(ifilename, "r")))
	    Eex2("parameter file %s could not be read", ifilename);

	if (!(nf = fopen(ofilename, "w")))
	    Eex2("new parameter file %s could not be created", ofilename);

	fnam = gp_basename(ifilename); /* strip off the path */
	if (fnam == NULL)
	    fnam = ifilename;

	while (fgets(s = sstr, sizeof(sstr), of) != NULL) {
	    char pname[64]; /* name of parameter */
	    double pval;    /* parameter value */
	    char tail[127]; /* trailing characters */
	    char * tmp;
	    char c;

	    if (is_empty(s)) {
		fputs(s, nf);	/* preserve comments */
		continue;
	    }
	    if ((tmp = strchr(s, '#')) != NULL) {
		safe_strncpy(tail, tmp, sizeof(tail));
		*tmp = NUL;
	    } else
		strcpy(tail, "\n");

	    tmp = get_next_word(&s, &c);
	    if (!legal_identifier(tmp) || strlen(tmp) > MAX_ID_LEN) {
		fclose(nf);
		fclose(of);
		Eex2("syntax error in parameter file %s", fnam);
	    }
	    safe_strncpy(pname, tmp, sizeof(pname));
	    /* next must be '=' */
	    if (c != '=') {
		tmp = strchr(s, '=');
		if (tmp == NULL) {
		    fclose(nf);
		    fclose(of);
		    Eex2("syntax error in parameter file %s", fnam);
		}
		s = tmp + 1;
	    }
	    tmp = get_next_word(&s, &c);
	    if (!sscanf(tmp, "%lf", &pval)) {
		fclose(nf);
		fclose(of);
		Eex2("syntax error in parameter file %s", fnam);
	    }
	    if ((tmp = get_next_word(&s, &c)) != NULL) {
		fclose(nf);
		fclose(of);
		Eex2("syntax error in parameter file %s", fnam);
	    }

	    /* now modify */
	    pval = getdvar(pname);
	    fprintf(nf, "%-15s = %-22s   %s", pname, num_to_str(pval), tail);
	}

	if (fclose(nf) || fclose(of))
	    Eex("I/O error during update");
    }
}


/*****************************************************************
    Backup a file by renaming it to something useful. Return
    the new name in tofile
*****************************************************************/

/* tofile must point to a char array[] or allocated data. See update() */

static void
backup_file(char *tofile, const char *fromfile)
{
#if defined(MSDOS) || defined(VMS)
    char *tmpn;
#endif

/* first attempt, for all o/s other than MSDOS */

#ifndef MSDOS
    strcpy(tofile, fromfile);
#ifdef VMS
    /* replace all dots with _, since we will be adding the only
     * dot allowed in VMS names
     */
    while ((tmpn = strchr(tofile, '.')) != NULL)
	*tmpn = '_';
#endif /*VMS */
    strcat(tofile, BACKUP_SUFFIX);
    if (rename(fromfile, tofile) == 0)
	return;			/* hurrah */
    if (existfile(tofile))
	Eex2("The backup file %s already exists and will not be overwritten.", tofile);
#endif

#ifdef MSDOS
    /* first attempt for msdos. */

    /* Copy only the first 8 characters of the filename, to comply
     * with the restrictions of FAT filesystems. */
    safe_strncpy(tofile, fromfile, 8 + 1);

    while ((tmpn = strchr(tofile, '.')) != NULL)
	*tmpn = '_';

    strcat(tofile, BACKUP_SUFFIX);

    if (rename(fromfile, tofile) == 0)
	return;			/* success */
#endif /* MSDOS */

    /* get here => rename failed. */
    Eex3("Could not rename file %s to %s", fromfile, tofile);
}


/* Modified from save.c:save_range() */
static void
log_axis_restriction(FILE *log_f, int param, double min, double max, int autoscale, char *name)
{
    char s[80];
    /* FIXME: Is it really worth it to format time values? */
    AXIS *axis = (param == 1) ? &Y_AXIS : &X_AXIS;

    fprintf(log_f, "        %s range restricted to [", name);
    if (autoscale & AUTOSCALE_MIN) {
	putc('*', log_f);
    } else if (param < 2 && axis->datatype == DT_TIMEDATE) {
	putc('"', log_f);
	gstrftime(s, 80, timefmt, min);
	fputs(s, log_f);
	putc('"', log_f);
    } else {
	fprintf(log_f, "%#g", min);
    }

    fputs(" : ", log_f);
    if (autoscale & AUTOSCALE_MAX) {
	putc('*', log_f);
    } else if (param < 2 && axis->datatype == DT_TIMEDATE) {
	putc('"', log_f);
	gstrftime(s, 80, timefmt, max);
	fputs(s, log_f);
	putc('"', log_f);
    } else {
	fprintf(log_f, "%#g", max);
    }
    fputs("]\n", log_f);
}


/*****************************************************************
    Recursively print definitions of function referenced.
*****************************************************************/
static int
print_function_definitions_recursion(struct at_type *at, int *count, int maxfun, char *definitions[], int depth, int maxdepth)
{
    int i, k;
    int rc = 0;

    if (at->a_count == 0)
	return 0;
    if (*count == maxfun) /* limit the maximum number of unique function definitions  */
	return 1;
    if (depth >= maxdepth) /* limit the maximum recursion depth */
	return 2;

    for (i = 0; (i < at->a_count) && (*count < maxfun); i++) {
	if (((at->actions[i].index == CALL) || (at->actions[i].index == CALLN)) &&
	    (at->actions[i].arg.udf_arg->definition != NULL)) {
	    for (k = 0; k < maxfun; k++) {
		if (definitions[k] == at->actions[i].arg.udf_arg->definition)
		    break; /* duplicate definition already in list */
		if (definitions[k] == NULL) {
		    *count += 1; /* increment counter */
		    definitions[k] = at->actions[i].arg.udf_arg->definition;
		    break;
		}
	    }
	    rc |= print_function_definitions_recursion(at->actions[i].arg.udf_arg->at, 
	                                               count, maxfun, definitions,
	                                               depth + 1, maxdepth);
	}
    }

    return rc;
}


static void
print_function_definitions(struct at_type *at, FILE * device)
{
    char *definitions[32];
    const int maxfun   = 32;  /* maximum number of unique functions definitions */
    const int maxdepth = 20;  /* maximum recursion depth */
    int count = 0;
    int k, rc;

    memset(definitions, 0, maxfun * sizeof(char *));
    rc = print_function_definitions_recursion(at, &count, maxfun, definitions, 0, maxdepth);
    for (k = 0; k < count; k++)
	fprintf(device, "\t%s\n", definitions[k]);
    if ((rc & 1) != 0)
	fprintf(device, "\t[omitting further function definitions (max=%i)]\n", maxfun);
    if ((rc & 2) != 0)
	fprintf(device, "\t[too many nested (or recursive) function definitions (max=%i)]\n", maxdepth);
}



/*****************************************************************
    Interface to the gnuplot "fit" command
*****************************************************************/

void
fit_command()
{
/* HBB 20000430: revised this completely, to make it more similar to
 * what plot3drequest() does */

    /* Backwards compatibility - these were the default names in 4.4 and 4.6	*/
    static const char *dummy_old_default[5] = {"x","y","t","u","v"};

    /* Keep range info in local storage rather than overwriting axis structure.	*/
    /* The final range is "z" (actually the range of the function value).	*/
    double range_min[MAX_NUM_VAR+1];
    double range_max[MAX_NUM_VAR+1];
    int range_autoscale[MAX_NUM_VAR+1];
    int num_ranges = 0;

    int max_data;
    int max_params;

    int dummy_token[MAX_NUM_VAR+1];    /* Point to variable name for each explicit range */
    int skipped[MAX_NUM_VAR+1];        /* number of points out of range */
    int num_points = 0;                /* number of data points read from file */
    static const int iz = MAX_NUM_VAR;

    int i, j;
    double v[MAXDATACOLS];
    double tmpd;
    time_t timer;
    int token1, token2, token3;
    char *tmp, *file_name;
    TBOOLEAN zero_initial_value;

    c_token++;

    /* FIXME EAM - I don't understand what these are needed for. */
    x_axis = FIRST_X_AXIS;
    y_axis = FIRST_Y_AXIS;
    z_axis = FIRST_Z_AXIS;

    /* First look for a restricted fit range... */
    /* Start with the current range limits on variable 1 ("x"),
     * variable 2 ("y"), and function range ("z").
     * Historically variables 3-5 inherited the current range of t, u, and v
     * but no longer.  NB: THIS IS A CHANGE
     */
    AXIS_INIT3D(x_axis, 0, 0);
    AXIS_INIT3D(y_axis, 0, 0);
    AXIS_INIT3D(z_axis, 0, 1);
    for (i = 0; i < MAX_NUM_VAR+1; i++)
	dummy_token[i] = -1;
    range_min[0] = axis_array[x_axis].min;
    range_max[0] = axis_array[x_axis].max;
    range_autoscale[0] = axis_array[x_axis].autoscale;
    range_min[1] = axis_array[y_axis].min;
    range_max[1] = axis_array[y_axis].max;
    range_autoscale[1] = axis_array[y_axis].autoscale;
    for (i = 2; i < MAX_NUM_VAR; i++) {
	range_min[i] = VERYLARGE;
	range_max[i] = -VERYLARGE;
	range_autoscale[i] = AUTOSCALE_BOTH;
    }
    range_min[iz] = axis_array[z_axis].min;
    range_max[iz] = axis_array[z_axis].max;
    range_autoscale[iz] = axis_array[z_axis].autoscale;

    num_ranges = 0;
    while (equals(c_token, "[")) {
	if (i > MAX_NUM_VAR)
	    Eexc(c_token, "too many range specifiers");
	/* NB: This has nothing really to do with the Z axis, we're */
	/* just using that slot of the axis array as scratch space. */
	AXIS_INIT3D(SECOND_Z_AXIS, 0, 1);
	dummy_token[num_ranges] = parse_range(SECOND_Z_AXIS);
	range_min[num_ranges] = axis_array[SECOND_Z_AXIS].min;
	range_max[num_ranges] = axis_array[SECOND_Z_AXIS].max;
	range_autoscale[num_ranges] = axis_array[SECOND_Z_AXIS].autoscale;
	num_ranges++;
    }

    /* now compile the function */
    token1 = c_token;

    if (func.at) {
	free_at(func.at);
	func.at = NULL;		/* in case perm_at() does int_error */
    }
    dummy_func = &func;

    /* set all possible dummy variable names, even if we're using fewer */
    for (i = 0; i < MAX_NUM_VAR; i++) {
	if (dummy_token[i] > 0)
	    copy_str(c_dummy_var[i], dummy_token[i], MAX_ID_LEN);
	else if (*set_dummy_var[i] != '\0')
	    strcpy(c_dummy_var[i], set_dummy_var[i]);
	else if (i < 5)  /* Fall back to legacy ordering x y t u v */
	    strcpy(c_dummy_var[i], dummy_old_default[i]);
	fit_dummy_udvs[i] = add_udv_by_name(c_dummy_var[i]);
    }

    memset(fit_dummy_var, 0, sizeof(fit_dummy_var));
    func.at = perm_at();	/* parse expression and save action table */
    dummy_func = NULL;

    token2 = c_token;

    /* get filename */
    file_name = string_or_express(NULL);
    if (file_name )
	file_name = gp_strdup(file_name);
    else
	Eexc(token2, "missing filename or datablock");

    /* use datafile module to parse the datafile and qualifiers */
    df_set_plot_mode(MODE_QUERY);  /* Does nothing except for binary datafiles */

    /* Historically we could only handle 7 using specs, hence 5 independent	*/
    /* variables (the last 2 cols are used for z and z_err).			*/
    /* June 2013 - Now the number of using specs can be increased by changing	*/
    /* MAXDATACOLS.  Logically this should be at least as large as MAX_NUM_VAR,	*/
    /* the limit on parameters passed to a user-defined function.		*/
    /* I.e. we expect that MAXDATACOLS >= MAX_NUM_VAR + 2			*/

    columns = df_open(file_name, MAXDATACOLS, NULL);
    if (columns < 0)
	Eexc2(token2, "Can't read data from", file_name);
    free(file_name);
    if (columns == 1)
	Eexc(c_token, "Need more than 1 input data column");

    /* Allow time data only on first two dimensions (x and y) */
    df_axis[0] = x_axis;
    df_axis[1] = y_axis;

    /* BM: New options to distinguish fits with and without errors */
    /* reset error columns */
    memset(err_cols, FALSE, sizeof(err_cols));
    if (almost_equals(c_token, "err$ors")) {
	/* error column specs follow */
	c_token++;
	num_errors = 0;
	do {
	    char * err_spec = NULL;

	    if (!isletter(c_token))
		Eexc(c_token, "Expecting a variable specifier.");
	    m_capture(&err_spec, c_token, c_token);
	    /* check if this is a valid dummy var */
	    for (i = 0; i < MAX_NUM_VAR; i++) {
		if (strcmp(err_spec, c_dummy_var[i]) == 0) {
		    err_cols[i] = TRUE;
		    num_errors++;
		    break;
		}
	    }
	    if (i == MAX_NUM_VAR) { /* variable name not found, yet */
		if (strcmp(err_spec, "z") == 0) {
		    err_cols[iz] = TRUE;
		    num_errors++;
		 } else
		    Eexc(c_token, "Invalid variable specifier.");
	    }
	    FPRINTF((stderr, "error spec \"%s\"\n", err_spec));
	    free(err_spec);
	} while (equals(++c_token, ",") && ++c_token);

	/* z-errors are required. */
	if (!err_cols[iz]) {
	    Eexc(c_token, "z-errors are required.");
	    err_cols[iz] = TRUE;
	    num_errors++;
	}

	/* The dummy variable with the highest index indicates the minimum number
	   of indep. variables required. */
	num_indep = 0;
	for (i = 0; i < MAX_NUM_VAR; i++) {
	    if (err_cols[i])
		num_indep = i + 1;
	}

	/* Check if there are enough columns.
	   Require # of indep. and dependent variables + # of errors */
	if ((columns != 0) && (columns < num_indep + 1 + num_errors))
	    Eexc2(c_token, "Not enough columns in using spec.  At least %i are required for this error spec.",
		num_indep + 1 + num_errors);

	/* Success. */
	if (columns > 0)
	    num_indep = columns - num_errors - 1;
    } else if (almost_equals(c_token, "zerr$ors")) {
	/* convenience alias */
	if (columns == 1)
	    Eexc(c_token, "zerror requires at least 2 columns");
	num_indep = (columns == 0) ? 1 : columns - 2;
	num_errors = 1;
	err_cols[iz] = TRUE;
	c_token++;
    } else if (almost_equals(c_token, "yerr$ors")) {
	/* convenience alias, x:z:sz (or x:y:sy) */
	if ((columns != 0) && (columns != 3))
	    Eexc(c_token, "yerror requires exactly 3 columns");
	num_indep = 1;
	num_errors = 1;
	err_cols[iz] = TRUE;
	c_token++;
    } else if (almost_equals(c_token, "xyerr$ors")) {
	/* convienience alias, x:z:sx:sz (or x:y:sx:sy) */
	if ((columns != 0) && (columns != 4))
	    Eexc(c_token, "xyerror requires exactly 4 columns");
	num_indep = 1;
	num_errors = 2;
	err_cols[0] = TRUE;
	err_cols[iz] = TRUE;
	c_token++;
    } else if (almost_equals(c_token, "uni$tweights")) {
	/* 'unitweights' are the default now. So basically this option is only useful in v4 compatibility mode.*/
	/* no error columns given */
	c_token++;
	num_indep = (columns == 0) ? 1 : columns - 1;
	num_errors = 0;
    } else {
	/* no error keyword found */
	if (fit_v4compatible) {
	    /* using old syntax */
	    num_indep = (columns < 3) ? 1 : columns - 2;
	    num_errors = (columns < 3) ? 0 : 1;
	    if (num_errors > 0)
		err_cols[iz] = TRUE;
	} else if (columns >= 3 && fit_dummy_var[columns-2] == 0) {
	    int_warn(NO_CARET,
		"\n\t> Implied independent variable %s not found in fit function."
		"\n\t> Assuming version 4 syntax with zerror in column %d but no zerror keyword.\n",
		c_dummy_var[columns-2], columns);
		num_indep = columns - 2;
		num_errors = 1;
		err_cols[iz] = TRUE;
	} else {
	    /* default to unitweights */
	    num_indep = (columns == 0) ? 1 : columns - 1;
	    num_errors = 0;
	} 
    }

    FPRINTF((stderr, "cmd=%s\n", gp_input_line));
    FPRINTF((stderr, "cols=%i indep=%i errors=%i\n", columns, num_indep, num_errors));

    /* HBB 980401: if this is a single-variable fit, we shouldn't have
     * allowed a variable name specifier for 'y': */
    /* FIXME EAM - Is this relevant any more? */
    if ((dummy_token[1] > 0) && (num_indep == 1))
	Eexc(dummy_token[1], "Can't re-name 'y' in a one-variable fit");

    /* depending on number of independent variables, the last range
     * spec may be for the Z axis */
    if (num_ranges > num_indep+1)
	Eexc2(dummy_token[num_ranges-1], "Too many range-specs for a %d-variable fit", num_indep);
    if (num_ranges == (num_indep + 1)) {
	/* last range was actually for the dependent variable */
	range_min[iz] = range_min[num_indep];
	range_max[iz] = range_max[num_indep];
	range_autoscale[iz] = range_autoscale[num_indep];
    }

    /* defer actually reading the data until we have parsed the rest
     * of the line */
    token3 = c_token;

    /* open logfile before we use any Dblfn calls */
    if (!fit_suppress_log) {
	char *logfile = getfitlogfile();
	if ((logfile != NULL) && !log_f && !(log_f = fopen(logfile, "a")))
	    Eex2("could not open log-file %s", logfile);
	free(logfile);
    }

    tmpd = getdvar(FITLIMIT);	/* get epsilon if given explicitly */
    if (tmpd < 1.0 && tmpd > 0.0)
	epsilon = tmpd;
    else
	epsilon = DEF_FIT_LIMIT;
    FPRINTF((STANDARD, "epsilon=%e\n", epsilon));

    /* tsm patchset 230: new absolute convergence variable */
    FPRINTF((STANDARD, "epsilon_abs=%e\n", epsilon_abs));

    /* HBB 970304: maxiter patch */
    maxiter = getivar(FITMAXITER);
    if (maxiter < 0)
	maxiter = 0;
    FPRINTF((STANDARD, "maxiter=%i\n", maxiter));

    /* get startup value for lambda, if given */
    tmpd = getdvar(FITSTARTLAMBDA);
    if (tmpd > 0.0) {
	startup_lambda = tmpd;
	Dblf2("lambda start value set: %g\n", startup_lambda);
    } else {
	/* use default value or calculation */
	startup_lambda = 0.0;
    }

    /* get lambda up/down factor, if given */
    tmpd = getdvar(FITLAMBDAFACTOR);
    if (tmpd > 0.0) {
	lambda_up_factor = lambda_down_factor = tmpd;
	Dblf2("lambda scaling factors reset:  %g\n", lambda_up_factor);
    } else {
	lambda_down_factor = LAMBDA_DOWN_FACTOR;
	lambda_up_factor = LAMBDA_UP_FACTOR;
    }

    FPRINTF((STANDARD, "prescale=%i\n", fit_prescale));
    FPRINTF((STANDARD, "errorscaling=%i\n", fit_errorscaling));

    (void) time(&timer);
    if (!fit_suppress_log) {
	char *line = NULL;

	fputs("\n\n*******************************************************************************\n", log_f);
	fprintf(log_f, "%s\n\n", ctime(&timer));

	m_capture(&line, token2, token3 - 1);
	fprintf(log_f, "FIT:    data read from %s\n", line);
	fprintf(log_f, "        format = ");
	free(line);
	for (i = 0; (i < num_indep) && (i < columns - 1); i++)
	    fprintf(log_f, "%s:", c_dummy_var[i]);
	fprintf(log_f, "z");
	if (num_errors > 0) {
	    for (i = 0; (i < num_indep) && (i < columns - 1); i++)
		if (err_cols[i])
		    fprintf(log_f, ":s%s", c_dummy_var[i]);
	    fprintf(log_f, ":s\n");
	} else {
	    fprintf(log_f, "\n");
	}
    }

    /* report all range specs, starting with Z */
    if (!fit_suppress_log) {
	if ((range_autoscale[iz] & AUTOSCALE_BOTH) != AUTOSCALE_BOTH)
	    log_axis_restriction(log_f, iz, range_min[iz], range_max[iz], range_autoscale[iz], "function");
	for (i = 0; i < num_indep; i++) {
	    if ((range_autoscale[i] & AUTOSCALE_BOTH) != AUTOSCALE_BOTH)
		log_axis_restriction(log_f, i, range_min[i], range_max[i], range_autoscale[i], c_dummy_var[i]);
	}
    }

    /* start by allocting memory for MAX_DATA datapoints */
    max_data = MAX_DATA;
    fit_x = vec(max_data * num_indep);
    fit_z = vec(max_data);
    /* allocate error array, last one is always the z-error */
    err_data = vec(max_data * GPMAX(num_errors, 1));
    num_data = 0;

    /* Set skipped[i] = 0 for all i */
    memset(skipped, 0, sizeof(skipped));

    /* first read in experimental data */

    /* If the user has set an explicit locale for numeric input, apply it */
    /* here so that it affects data fields read from the input file.      */
    set_numeric_locale();

    while ((i = df_readline(v, num_indep + num_errors + 1)) != DF_EOF) {
        if (num_data >= max_data) {
	    max_data *= 2;
	    if (!redim_vec(&fit_x, max_data * num_indep) ||
		!redim_vec(&fit_z, max_data) ||
		!redim_vec(&err_data, max_data * GPMAX(num_errors, 1))) {
		/* Some of the reallocations went bad: */
		Eex2("Out of memory in fit: too many datapoints (%d)?", max_data);
	    }
	} /* if (need to extend storage space) */

	/* BM: silently ignore lines with NaN */
	{
	    TBOOLEAN skip_nan = FALSE;
	    int k;

	    for (k = 0; k < i; k++) {
		if (isnan(v[k]))
		    skip_nan = TRUE;
	    }
	    if (skip_nan)
		continue;
	}

	switch (i) {
	case DF_MISSING:
	case DF_UNDEFINED:
	case DF_FIRST_BLANK:
	case DF_SECOND_BLANK:
	    continue;
	case DF_COLUMN_HEADERS:
	    continue;
	case DF_FOUND_KEY_TITLE:
	    continue;
	case 0:
	    Eex2("bad data on line %d of datafile", df_line_number);
	    break;
	case 1:		/* only z provided */
	    v[1] = v[0];
	    v[0] = (double) df_datum;
	    break;
	default:	/* June 2013 - allow more than 7 data columns */
	    if (i<0)
		int_error(NO_CARET, "unexpected value returned by df_readline");
	    break;
	}
	num_points++;

	/* skip this point if it is out of range */
	for (i = 0; i < num_indep; i++) {
	    if (!(range_autoscale[i] & AUTOSCALE_MIN) && (v[i] < range_min[i])) {
		skipped[i]++;
		goto out_of_range;
	    }
	    if (!(range_autoscale[i] & AUTOSCALE_MAX) && (v[i] > range_max[i])) {
		skipped[i]++;
		goto out_of_range;
	    }
	    fit_x[num_data * num_indep + i] = v[i]; /* save independent variable data */
	}
	/* check Z value too */
	if (!(range_autoscale[iz] & AUTOSCALE_MIN) && (v[i] < range_min[iz])) {
	    skipped[iz]++;
	    goto out_of_range;
	}
	if (!(range_autoscale[iz] & AUTOSCALE_MAX) && (v[i] > range_max[iz])) {
	    skipped[iz]++;
	    goto out_of_range;
	}

	fit_z[num_data] = v[i++];	      /* save dependent variable data */

	/* only use error from data file if _explicitly_ asked for by a using spec */

	if (num_errors == 0)
	    err_data[num_data] = 1.0; /* constant weight */
	else if (num_errors == 1)
	    err_data[num_data] = v[i++]; /* z-error */
	else {
	    int k, idx;

	    for (k = 0, idx = 0; k < MAX_NUM_VAR; k++) {
		if (err_cols[k])
		    err_data[num_errors * num_data + idx++] = v[i++];
	    }
	    if (err_cols[iz])
		err_data[num_errors * num_data + idx] = v[i++]; /* z-error */
	    else
		/* This case is not currently allowed. We always require z-errors. */
		Eexc(NO_CARET, "z errors are always required");
	}

	/* Increment index into stored values.
	 * Note that out-of-range or NaN values bypass this operation.
	 */
	num_data++;

    out_of_range:
	;
    }
    df_close();

    /* We are finished reading user input; return to C locale for internal use */
    reset_numeric_locale();

    if (num_data <= 1) {
	/* no data! Try to explain why. */
	printf("         Read %d points\n", num_points);
	for (i = 0; i < num_indep; i++) {
	    if (skipped[i]) {
		printf("         Skipped %d points outside range [%s=",
		    skipped[i], c_dummy_var[i]);
		if (range_autoscale[i] & AUTOSCALE_MIN)
		    printf("*:");
		else
		    printf("%g:", range_min[i]);
		if (range_autoscale[i] & AUTOSCALE_MAX)
		    printf("*]\n");
		else
		    printf("%g]\n", range_max[i]);
	    }
	}
	if (skipped[iz]) {
	    printf("         Skipped %d points outside range [%s=",
		skipped[iz], "z");
	if (axis_array[FIRST_Z_AXIS].autoscale & AUTOSCALE_MIN)
	    printf("*:");
	else
	    printf("%g:", axis_array[FIRST_Z_AXIS].min);
	if (axis_array[FIRST_Z_AXIS].autoscale & AUTOSCALE_MAX)
	    printf("*]\n");
	else
	    printf("%g]\n", axis_array[FIRST_Z_AXIS].max);
	}
	Eex("No data to fit");
    }

    /* tsm patchset 230: check for zero error values */
    if (num_errors > 0) {
	for (i = 0; i < num_data; i++) {
	    if (err_data[i * num_errors + (num_errors - 1)] != 0.0)
		continue;
	    Dblf("\nCurrent data point\n");
	    Dblf("=========================\n");
	    Dblf3("%-15s = %i out of %i\n", "#", i + 1, num_data);
	    for (j = 0; j < num_indep; j++)
		Dblf3("%-15.15s = %-15g\n", c_dummy_var[j], fit_x[i * num_indep + j]);
	    Dblf3("%-15.15s = %-15g\n", "z", fit_z[i]);
	    Dblf3("%-15.15s = %-15g\n", "s", err_data[i * num_errors + (num_errors - 1)]);
	    Dblf("\n");
	    Eex("Zero error value in data file");
	}
    }

    /* now resize fields to actual length: */
    redim_vec(&fit_x, num_data * num_indep);
    redim_vec(&fit_z, num_data);
    redim_vec(&err_data, num_data * GPMAX(num_errors, 1));


    if (!fit_suppress_log) {
	char *line = NULL;
	fprintf(log_f, "        #datapoints = %d\n", num_data);
	if (num_errors == 0)
	    fputs("        residuals are weighted equally (unit weight)\n\n", log_f);
	m_capture(&line, token1, token2 - 1);
	fprintf(log_f, "function used for fitting: %s\n", line);
	print_function_definitions(func.at, log_f);
	free(line);
    }

    /* read in parameters */
    max_params = MAX_PARAMS;	/* HBB 971023: make this resizeable */

    if (!equals(c_token++, "via"))
	Eexc(c_token, "Need via and either parameter list or file");

    /* allocate arrays for parameter values, names */
    a = vec(max_params);
    par_name = (fixstr *) gp_alloc((max_params + 1) * sizeof(fixstr),
				   "fit param");
    num_params = 0;

    if (isstringvalue(c_token)) {	/* It's a parameter *file* */
	TBOOLEAN fixed;
	double tmp_par;
	char c, *s;
	char sstr[MAX_LINE_LEN + 1];
	FILE *f;

	static char *viafile = NULL;
	free(viafile);			/* Free previous name, if any */
	viafile = try_to_get_string();
	if (!viafile || !(f = loadpath_fopen(viafile, "r")))
	    Eex2("could not read parameter-file \"%s\"", viafile);
	if (!fit_suppress_log)
	    fprintf(log_f, "fitted parameters and initial values from file: %s\n\n", viafile);

	/* get parameters and values out of file and ignore fixed ones */

	while (TRUE) {
	    if (!fgets(s = sstr, sizeof(sstr), f))	/* EOF found */
		break;
	    if ((tmp = strstr(s, GP_FIXED)) != NULL) {	/* ignore fixed params */
		*tmp = NUL;
		if (!fit_suppress_log)
		    fprintf(log_f, "FIXED:  %s\n", s);
		fixed = TRUE;
	    } else
		fixed = FALSE;
	    if ((tmp = strchr(s, '#')) != NULL)
		*tmp = NUL;
	    if (is_empty(s))
		continue;
	    tmp = get_next_word(&s, &c);
	    if (!legal_identifier(tmp) || strlen(tmp) > MAX_ID_LEN) {
		(void) fclose(f);
		Eex("syntax error in parameter file");
	    }
	    safe_strncpy(par_name[num_params], tmp, sizeof(fixstr));
	    /* next must be '=' */
	    if (c != '=') {
		tmp = strchr(s, '=');
		if (tmp == NULL) {
		    (void) fclose(f);
		    Eex("syntax error in parameter file");
		}
		s = tmp + 1;
	    }
	    tmp = get_next_word(&s, &c);
	    if (sscanf(tmp, "%lf", &tmp_par) != 1) {
		(void) fclose(f);
		Eex("syntax error in parameter file");
	    }
	    /* make fixed params visible to GNUPLOT */
	    if (fixed) {
		/* use parname as temp */
		setvar(par_name[num_params], tmp_par);
	    } else {
		if (num_params >= max_params) {
		    max_params = (max_params * 3) / 2;
		    if (0
			|| !redim_vec(&a, max_params)
			|| !(par_name = (fixstr *) gp_realloc(par_name, (max_params + 1) * sizeof(fixstr), "fit param resize"))
			) {
			(void) fclose(f);
			Eex("Out of memory in fit: too many parameters?");
		    }
		}
		a[num_params++] = tmp_par;
	    }

	    if ((tmp = get_next_word(&s, &c)) != NULL) {
		(void) fclose(f);
		Eex2("syntax error in parameter file %s", viafile);
	    }
	}
	(void) fclose(f);

    } else {
	/* not a string after via: it's a variable listing */

	if (!fit_suppress_log)
	    fputs("fitted parameters initialized with current variable values\n\n", log_f);
	do {
	    if (!isletter(c_token))
		Eex("no parameter specified");
	    capture(par_name[num_params], c_token, c_token, (int) sizeof(par_name[0]));
	    if (num_params >= max_params) {
		max_params = (max_params * 3) / 2;
		if (0
		    || !redim_vec(&a, max_params)
		    || !(par_name = (fixstr *) gp_realloc(par_name, (max_params + 1) * sizeof(fixstr), "fit param resize"))
		    ) {
		    Eex("Out of memory in fit: too many parameters?");
		}
	    }
	    /* create variable if it doesn't exist */
	    a[num_params] = createdvar(par_name[num_params], INITIAL_VALUE);
	    ++num_params;
	} while (equals(++c_token, ",") && ++c_token);
    }

    redim_vec(&a, num_params);
    par_name = (fixstr *) gp_realloc(par_name, (num_params + 1) * sizeof(fixstr), "fit param");

    if (num_data < num_params)
	Eex("Number of data points smaller than number of parameters");

    /* initialize scaling parameters */
    if (!redim_vec(&scale_params, num_params))
	Eex2("Out of memory in fit: too many datapoints (%d)?", max_data);

    zero_initial_value = FALSE;
    for (i = 0; i < num_params; i++) {
	/* avoid parameters being equal to zero */
	if (a[i] == 0.0) {
	    Dblf2("Warning: Initial value of parameter '%s' is zero.\n", par_name[i]);
	    a[i] = NEARLY_ZERO;
	    scale_params[i] = 1.0;
	    zero_initial_value = TRUE;
	} else if (fit_prescale) {
	    /* scale parameters, but preserve sign */
	    double a_sign = (a[i] > 0) - (a[i] < 0);
	    scale_params[i] = a_sign * a[i];
	    a[i] = a_sign;
	} else {
	    scale_params[i] = 1.0;
	}
    }
    if (zero_initial_value) {  /* print this message only once */
	/* tsm patchset 230: explain what good initial parameter values are */
	fprintf(STANDARD, "  Please provide non-zero initial values for the parameters, at least of\n");
	fprintf(STANDARD, "  the right order of magnitude. If the expected value is zero, then use\n");
	fprintf(STANDARD, "  the magnitude of the expected error. If all else fails, try 1.0\n\n");
    }

    if (num_params == 0)
	int_warn(NO_CARET, "No fittable parameters!\n");
    else
	regress(a);	/* fit */

    if (log_f)
	fclose(log_f);
    log_f = NULL;
    free(fit_x);
    free(fit_z);
    free(err_data);
    free(a);
    a = fit_x = fit_z = err_data = NULL;
    if (func.at) {
	free_at(func.at);		/* release perm. action table */
	func.at = (struct at_type *) NULL;
    }
    /* remember parameter names for 'update' */
    last_num_params = num_params;
    free(last_par_name);
    last_par_name = par_name;
    /* remember names of indep. variables for 'update' */
    for (i = 0; i < MAX_NUM_VAR; i++) {
	free(last_dummy_var[i]);
	last_dummy_var[i] = gp_strdup(c_dummy_var[i]);
    }
    /* remember last fit command for 'save' */
    free(last_fit_command);
    last_fit_command = strdup(gp_input_line);
    for (i = 0; i < num_tokens; i++) {
	if (equals(i,";")) {
	    last_fit_command[token[i].start_index] = '\0';
	    break;
	}
    }
    /* save fit command to user variable */
    fill_gpval_string("GPVAL_LAST_FIT", last_fit_command);
}

/*
 * Print message to stderr and log file
 */
#if defined(VA_START) && defined(STDC_HEADERS)
static void
Dblfn(const char *fmt, ...)
#else
static void
Dblfn(const char *fmt, va_dcl)
#endif
{
#ifdef VA_START
    va_list args;

    VA_START(args, fmt);
# if defined(HAVE_VFPRINTF) || _LIBC
    if (fit_verbosity != QUIET)
	vfprintf(STANDARD, fmt, args);
    va_end(args);
    if (!fit_suppress_log) {
	VA_START(args, fmt);
	vfprintf(log_f, fmt, args);
    }
# else
    if (fit_verbosity != QUIET)
	_doprnt(fmt, args, STANDARD);
    if (!fit_suppress_log) {
	_doprnt(fmt, args, log_f);
    }
# endif
    va_end(args);
#else
    if (fit_verbosity != QUIET)
	fprintf(STANDARD, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
    if (!fit_suppress_log)
	fprintf(log_f, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* VA_START */
}


/*****************************************************************
    Get name of current log-file
*****************************************************************/
char *
getfitlogfile()
{
    char *logfile = NULL;

    if (fitlogfile == NULL) {
	char *tmp = getenv(GNUFITLOG);	/* open logfile */

	if (tmp != NULL && *tmp != '\0') {
	    char *tmp2 = tmp + (strlen(tmp) - 1);

	    /* if given log file name ends in path separator, treat it
	     * as a directory to store the default "fit.log" in */
	    if (*tmp2 == '/' || *tmp2 == '\\') {
		logfile = (char *) gp_alloc(strlen(tmp)
				   + strlen(fitlogfile_default) + 1,
				   "logfile");
		strcpy(logfile, tmp);
		strcat(logfile, fitlogfile_default);
	    } else {
		logfile = gp_strdup(tmp);
	    }
	} else {
	    logfile = gp_strdup(fitlogfile_default);
	}
    } else {
	logfile = gp_strdup(fitlogfile);
    }
    return logfile;
}
