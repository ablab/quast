#ifndef lint
static char *RCSid() { return RCSid("$Id: eval.c,v 1.119.2.4 2016/08/18 17:23:10 sfeam Exp $"); }
#endif

/* GNUPLOT - eval.c */

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

/* HBB 20010724: I moved several variables and functions from parse.c
 * to here, because they're involved with *evaluating* functions, not
 * with parsing them: evaluate_at(), fpe(), and fpe_env */

#include "eval.h"

#include "syscfg.h"
#include "alloc.h"
#include "datafile.h"
#include "datablock.h"
#include "external.h"	/* for f_calle */
#include "internal.h"
#include "libcerf.h"
#include "specfun.h"
#include "standard.h"
#include "util.h"
#include "version.h"
#include "term_api.h"

#include <signal.h>
#include <setjmp.h>

/* Internal prototypes */
static RETSIGTYPE fpe __PROTO((int an_int));

/* Global variables exported by this module */
struct udvt_entry udv_pi = { NULL, "pi", FALSE, {INTGR, {0} } };
struct udvt_entry *udv_NaN;
/* first in linked list */
struct udvt_entry *first_udv = &udv_pi;
struct udft_entry *first_udf = NULL;
/* pointer to first udv users can delete */
struct udvt_entry **udv_user_head;

TBOOLEAN undefined;

/* The stack this operates on */
static struct value stack[STACK_DEPTH];
static int s_p = -1;		/* stack pointer */
#define top_of_stack stack[s_p]

static int jump_offset;		/* to be modified by 'jump' operators */

/* The table of built-in functions */
/* These must strictly parallel enum operators in eval.h */
const struct ft_entry GPFAR ft[] =
{
    /* internal functions: */
    {"push",  f_push},
    {"pushc",  f_pushc},
    {"pushd1",  f_pushd1},
    {"pushd2",  f_pushd2},
    {"pushd",  f_pushd},
    {"pop",  f_pop},
    {"call",  f_call},
    {"calln",  f_calln},
    {"sum", f_sum},
    {"lnot",  f_lnot},
    {"bnot",  f_bnot},
    {"uminus",  f_uminus},
    {"lor",  f_lor},
    {"land",  f_land},
    {"bor",  f_bor},
    {"xor",  f_xor},
    {"band",  f_band},
    {"eq",  f_eq},
    {"ne",  f_ne},
    {"gt",  f_gt},
    {"lt",  f_lt},
    {"ge",  f_ge},
    {"le",  f_le},
    {"leftshift", f_leftshift},
    {"rightshift", f_rightshift},
    {"plus",  f_plus},
    {"minus",  f_minus},
    {"mult",  f_mult},
    {"div",  f_div},
    {"mod",  f_mod},
    {"power",  f_power},
    {"factorial",  f_factorial},
    {"bool",  f_bool},
    {"dollars",  f_dollars},	/* for usespec */
    {"concatenate",  f_concatenate},	/* for string variables only */
    {"eqs",  f_eqs},			/* for string variables only */
    {"nes",  f_nes},			/* for string variables only */
    {"[]",  f_range},			/* for string variables only */
    {"assign", f_assign},		/* assignment operator '=' */
    {"jump",  f_jump},
    {"jumpz",  f_jumpz},
    {"jumpnz",  f_jumpnz},
    {"jtern",  f_jtern},

/* Placeholder for SF_START */
    {"", NULL},

#ifdef HAVE_EXTERNAL_FUNCTIONS
    {"", f_calle},
#endif

/* legal in using spec only */
    {"column",  f_column},
    {"stringcolumn",  f_stringcolumn},	/* for using specs */
    {"strcol",  f_stringcolumn},	/* shorthand form */
    {"columnhead",  f_columnhead},
    {"valid",  f_valid},
    {"timecolumn",  f_timecolumn},

/* standard functions: */
    {"real",  f_real},
    {"imag",  f_imag},
    {"arg",  f_arg},
    {"conjg",  f_conjg},
    {"sin",  f_sin},
    {"cos",  f_cos},
    {"tan",  f_tan},
    {"asin",  f_asin},
    {"acos",  f_acos},
    {"atan",  f_atan},
    {"atan2",  f_atan2},
    {"sinh",  f_sinh},
    {"cosh",  f_cosh},
    {"tanh",  f_tanh},
    {"EllipticK",  f_ellip_first},
    {"EllipticE",  f_ellip_second},
    {"EllipticPi", f_ellip_third},
    {"int",  f_int},
    {"abs",  f_abs},
    {"sgn",  f_sgn},
    {"sqrt",  f_sqrt},
    {"exp",  f_exp},
    {"log10",  f_log10},
    {"log",  f_log},
    {"besj0",  f_besj0},
    {"besj1",  f_besj1},
    {"besy0",  f_besy0},
    {"besy1",  f_besy1},
    {"erf",  f_erf},
    {"erfc",  f_erfc},
    {"gamma",  f_gamma},
    {"lgamma",  f_lgamma},
    {"ibeta",  f_ibeta},
    {"voigt",  f_voigt},
    {"igamma",  f_igamma},
    {"rand",  f_rand},
    {"floor",  f_floor},
    {"ceil",  f_ceil},
    {"defined", f_exists},	/* DEPRECATED syntax defined(foo) */

    {"norm",  f_normal},	/* XXX-JG */
    {"inverf",  f_inverse_erf},	/* XXX-JG */
    {"invnorm",  f_inverse_normal},	/* XXX-JG */
    {"asinh",  f_asinh},
    {"acosh",  f_acosh},
    {"atanh",  f_atanh},
    {"lambertw",  f_lambertw}, /* HBB, from G.Kuhnle 20001107 */
    {"airy",  f_airy},         /* janert, 20090905 */
    {"expint",  f_expint},     /* Jim Van Zandt, 20101010 */

#ifdef HAVE_LIBCERF
    {"cerf", f_cerf},		/* complex error function */
    {"cdawson", f_cdawson},	/* complex Dawson's integral */
    {"erfi", f_erfi},		/* imaginary error function */
    {"VP", f_voigtp},		/* Voigt profile */
    {"faddeeva", f_faddeeva},	/* Faddeeva rescaled complex error function "w_of_z" */
#endif

    {"tm_sec",  f_tmsec},	/* for timeseries */
    {"tm_min",  f_tmmin},	/* for timeseries */
    {"tm_hour",  f_tmhour},	/* for timeseries */
    {"tm_mday",  f_tmmday},	/* for timeseries */
    {"tm_mon",  f_tmmon},	/* for timeseries */
    {"tm_year",  f_tmyear},	/* for timeseries */
    {"tm_wday",  f_tmwday},	/* for timeseries */
    {"tm_yday",  f_tmyday},	/* for timeseries */

    {"sprintf",  f_sprintf},	/* for string variables only */
    {"gprintf",  f_gprintf},	/* for string variables only */
    {"strlen",  f_strlen},	/* for string variables only */
    {"strstrt",  f_strstrt},	/* for string variables only */
    {"substr",  f_range},	/* for string variables only */
    {"word",  f_word},		/* for string variables only */
    {"words", f_words},		/* implemented as word(s,-1) */
    {"strftime",  f_strftime},  /* time to string */
    {"strptime",  f_strptime},  /* string to time */
    {"time", f_time},		/* get current time */
    {"system", f_system},       /* "dynamic backtics" */
    {"exist", f_exists},	/* exists("foo") replaces defined(foo) */
    {"exists", f_exists},	/* exists("foo") replaces defined(foo) */
    {"value", f_value},		/* retrieve value of variable known by name */

    {"hsv2rgb", f_hsv2rgb},	/* color conversion */

    {NULL, NULL}
};

/* Module-local variables: */

static JMP_BUF fpe_env;

/* Internal helper functions: */

static RETSIGTYPE
fpe(int an_int)
{
#if defined(MSDOS) && !defined(__EMX__) && !defined(DJGPP) && !defined(_Windows)
    /* thanks to lotto@wjh12.UUCP for telling us about this  */
    _fpreset();
#endif

    (void) an_int;		/* avoid -Wunused warning */
    (void) signal(SIGFPE, (sigfunc) fpe);
    undefined = TRUE;
    LONGJMP(fpe_env, TRUE);
}

/* Exported functions */

/* First, some functions that help other modules use 'struct value' ---
 * these might justify a separate module, but I'll stick with this,
 * for now */

/* returns the real part of val */
double
real(struct value *val)
{
    switch (val->type) {
    case INTGR:
	return ((double) val->v.int_val);
    case CMPLX:
	return (val->v.cmplx_val.real);
    case STRING:              /* is this ever used? */
	return (atof(val->v.string_val));
    default:
	int_error(NO_CARET, "unknown type in real()");
    }
    /* NOTREACHED */
    return ((double) 0.0);
}


/* returns the real part of val, converted to int if necessary */
int
real_int(struct value *val)
{
    switch (val->type) {
    case INTGR:
	return val->v.int_val;
    case CMPLX:
	return (int) val->v.cmplx_val.real;
    case STRING:
	return atoi(val->v.string_val);
    default:
	int_error(NO_CARET, "unknown type in real_int()");
    }
    /* NOTREACHED */
    return 0;
}


/* returns the imag part of val */
double
imag(struct value *val)
{
    switch (val->type) {
    case INTGR:
	return (0.0);
    case CMPLX:
	return (val->v.cmplx_val.imag);
    case STRING:
	/* This is where we end up if the user tries: */
	/*     x = 2;  plot sprintf(format,x)         */
	int_warn(NO_CARET, "encountered a string when expecting a number");
	int_error(NO_CARET, "Did you try to generate a file name using dummy variable x or y?");
    default:
	int_error(NO_CARET, "unknown type in imag()");
    }
    /* NOTREACHED */
    return ((double) 0.0);
}



/* returns the magnitude of val */
double
magnitude(struct value *val)
{
    switch (val->type) {
    case INTGR:
	return ((double) abs(val->v.int_val));
    case CMPLX:
	{
	    /* The straightforward implementation sqrt(r*r+i*i)
	     * over-/underflows if either r or i is very large or very
	     * small. This implementation avoids over-/underflows from
	     * squaring large/small numbers whenever possible.  It
	     * only over-/underflows if the correct result would, too.
	     * CAVEAT: sqrt(1+x*x) can still have accuracy
	     * problems. */
	    double abs_r = fabs(val->v.cmplx_val.real);
	    double abs_i = fabs(val->v.cmplx_val.imag);
	    double quotient;

	    if (abs_i == 0)
	    	return abs_r;
	    if (abs_r > abs_i) {
		quotient = abs_i / abs_r;
		return abs_r * sqrt(1 + quotient*quotient);
	    } else {
		quotient = abs_r / abs_i;
		return abs_i * sqrt(1 + quotient*quotient);
	    }
	}
    default:
	int_error(NO_CARET, "unknown type in magnitude()");
    }
    /* NOTREACHED */
    return ((double) 0.0);
}



/* returns the angle of val */
double
angle(struct value *val)
{
    switch (val->type) {
    case INTGR:
	return ((val->v.int_val >= 0) ? 0.0 : M_PI);
    case CMPLX:
	if (val->v.cmplx_val.imag == 0.0) {
	    if (val->v.cmplx_val.real >= 0.0)
		return (0.0);
	    else
		return (M_PI);
	}
	return (atan2(val->v.cmplx_val.imag,
		      val->v.cmplx_val.real));
    default:
	int_error(NO_CARET, "unknown type in angle()");
    }
    /* NOTREACHED */
    return ((double) 0.0);
}


struct value *
Gcomplex(struct value *a, double realpart, double imagpart)
{
    a->type = CMPLX;
    a->v.cmplx_val.real = realpart;
    a->v.cmplx_val.imag = imagpart;
    return (a);
}


struct value *
Ginteger(struct value *a, int i)
{
    a->type = INTGR;
    a->v.int_val = i;
    return (a);
}

struct value *
Gstring(struct value *a, char *s)
{
    a->type = STRING;
    a->v.string_val = s;
    return (a);
}

/* It is always safe to call gpfree_string with a->type is INTGR or CMPLX.
 * However it would be fatal to call it with a->type = STRING if a->string_val
 * was not obtained by a previous call to gp_alloc(), or has already been freed.
 * Thus 'a->type' is set to INTGR afterwards to make subsequent calls safe.
 */
struct value *
gpfree_string(struct value *a)
{
    if (a->type == STRING) {
	free(a->v.string_val);
	/* I would have set it to INVALID if such a type existed */
	a->type = INTGR;
    }
    return a;
}

/* some machines have trouble with exp(-x) for large x
 * if E_MINEXP is defined at compile time, use gp_exp(x) instead,
 * which returns 0 for exp(x) with x < E_MINEXP
 * exp(x) will already have been defined as gp_exp(x) in plot.h
 */

double
gp_exp(double x)
{
#ifdef E_MINEXP
    return (x < (E_MINEXP)) ? 0.0 : exp(x);
#else  /* E_MINEXP */
    int old_errno = errno;
    double result = exp(x);

    /* exp(-large) quite uselessly raises ERANGE --- stop that */
    if (result == 0.0)
	errno = old_errno;
    return result;
#endif /* E_MINEXP */
}

void
reset_stack()
{
    s_p = -1;
}


void
check_stack()
{				/* make sure stack's empty */
    if (s_p != -1)
	fprintf(stderr, "\n\
warning:  internal error--stack not empty!\n\
          (function called with too many parameters?)\n");
}

TBOOLEAN
more_on_stack()
{
    return (s_p >= 0);
}

struct value *
pop(struct value *x)
{
    if (s_p < 0)
	int_error(NO_CARET, "stack underflow (function call with missing parameters?)");
    *x = stack[s_p--];
    return (x);
}

/*
 * Allow autoconversion of string variables to floats if they
 * are dereferenced in a numeric context.
 */
struct value *
pop_or_convert_from_string(struct value *v)
{
    (void) pop(v);

    /* DEBUG Dec 2014 - Consolidate sanity check for variable type */
    /* FIXME: Test for INVALID_VALUE? Other corner cases? */
    if (v->type == INVALID_NAME)
	int_error(NO_CARET, "invalid dummy variable name");

    if (v->type == STRING) {
	char *eov;

	if (strspn(v->v.string_val,"0123456789 ") == strlen(v->v.string_val)) {
	    int i = atoi(v->v.string_val);
	    gpfree_string(v);
	    Ginteger(v, i);
	} else {
	    double d = strtod(v->v.string_val,&eov);
	    if (v->v.string_val == eov) {
		gpfree_string(v);
		int_error(NO_CARET,"Non-numeric string found where a numeric expression was expected");
	    }
	    gpfree_string(v);
	    Gcomplex(v, d, 0.);
	    FPRINTF((stderr,"converted string to CMPLX value %g\n",real(v)));
	}
    }
    return(v);
}

void
push(struct value *x)
{
    if (s_p == STACK_DEPTH - 1)
	int_error(NO_CARET, "stack overflow");
    stack[++s_p] = *x;
    /* WARNING - This is a memory leak if the string is not later freed */
    if (x->type == STRING && x->v.string_val)
	stack[s_p].v.string_val = gp_strdup(x->v.string_val);
}


void
int_check(struct value *v)
{
    if (v->type != INTGR)
	int_error(NO_CARET, "non-integer passed to boolean operator");
}



/* Internal operators of the stack-machine, not directly represented
 * by any user-visible operator, or using private status variables
 * directly */

/* converts top-of-stack to boolean */
void
f_bool(union argument *x)
{
    (void) x;			/* avoid -Wunused warning */

    int_check(&top_of_stack);
    top_of_stack.v.int_val = !!top_of_stack.v.int_val;
}


void
f_jump(union argument *x)
{
    (void) x;			/* avoid -Wunused warning */
    jump_offset = x->j_arg;
}


void
f_jumpz(union argument *x)
{
    struct value a;

    (void) x;			/* avoid -Wunused warning */
    int_check(&top_of_stack);
    if (top_of_stack.v.int_val) {	/* non-zero --> no jump*/
	(void) pop(&a);
    } else
	jump_offset = x->j_arg;	/* leave the argument on TOS */
}


void
f_jumpnz(union argument *x)
{
    struct value a;

    (void) x;			/* avoid -Wunused warning */
    int_check(&top_of_stack);
    if (top_of_stack.v.int_val)	/* non-zero */
	jump_offset = x->j_arg;	/* leave the argument on TOS */
    else {
	(void) pop(&a);
    }
}

void
f_jtern(union argument *x)
{
    struct value a;

    (void) x;			/* avoid -Wunused warning */
    int_check(pop(&a));
    if (! a.v.int_val)
	jump_offset = x->j_arg;	/* go jump to FALSE code */
}

/* This is the heart of the expression evaluation module: the stack
   program execution loop.

  'ft' is a table containing C functions within this program.

   An 'action_table' contains pointers to these functions and
   arguments to be passed to them.

   at_ptr is a pointer to the action table which must be executed
   (evaluated).

   so the iterated line executes the function indexed by the at_ptr
   and passes the address of the argument which is pointed to by the
   arg_ptr

*/

void
execute_at(struct at_type *at_ptr)
{
    int instruction_index, operator, count;
    int saved_jump_offset = jump_offset;

    count = at_ptr->a_count;
    for (instruction_index = 0; instruction_index < count;) {
	operator = (int) at_ptr->actions[instruction_index].index;
	jump_offset = 1;	/* jump operators can modify this */
	(*ft[operator].func) (&(at_ptr->actions[instruction_index].arg));
	assert(is_jump(operator) || (jump_offset == 1));
	instruction_index += jump_offset;
    }

    jump_offset = saved_jump_offset;
}

/* May 2013: Old hackery #ifdef'ed out so that input of Inf/NaN */
/* values through evaluation is treated equivalently to direct  */
/* input of a formated value.  See revised imageNaN demo.       */
void
evaluate_at(struct at_type *at_ptr, struct value *val_ptr)
{
    undefined = FALSE;
    errno = 0;
    reset_stack();

    if (!evaluate_inside_using || !df_nofpe_trap) {
	if (SETJMP(fpe_env, 1))
	    return;
	(void) signal(SIGFPE, (sigfunc) fpe);
    }

    execute_at(at_ptr);

    if (!evaluate_inside_using || !df_nofpe_trap)
	(void) signal(SIGFPE, SIG_DFL);

    if (errno == EDOM || errno == ERANGE)
	undefined = TRUE;
#if (1)	/* New code */
    else if (!undefined) {
	(void) pop(val_ptr);
	check_stack();
    }

#else /* Old hackery */
    else if (!undefined) { /* undefined (but not errno) may have been set by matherr */
	(void) pop(val_ptr);
	check_stack();
	/* At least one machine (ATT 3b1) computes Inf without a SIGFPE */
	if (val_ptr->type != STRING) {
	    double temp = real(val_ptr);
	    if (temp > VERYLARGE || temp < -VERYLARGE)
		undefined = TRUE;
	}
    }
#if defined(NeXT) || defined(ultrix)
    /*
     * linux was able to fit curves which NeXT gave up on -- traced it to
     * silently returning NaN for the undefined cases and plowing ahead
     * I can force that behavior this way.  (0.0/0.0 generates NaN)
     */
    if (undefined && (errno == EDOM || errno == ERANGE)) {	/* corey@cac */
	undefined = FALSE;
	errno = 0;
	Gcomplex(val_ptr, 0.0 / 0.0, 0.0 / 0.0);
    }
#endif /* NeXT || ultrix */
#endif /* old hackery */
}

void
real_free_at(struct at_type *at_ptr)
{
    int i;
    /* All string constants belonging to this action table have to be
     * freed before destruction. */
    if (!at_ptr)
        return;
    for(i=0; i<at_ptr->a_count; i++) {
	struct at_entry *a = &(at_ptr->actions[i]);
	/* if union a->arg is used as a->arg.v_arg free potential string */
	if ( a->index == PUSHC || a->index == DOLLARS )
	    gpfree_string(&(a->arg.v_arg));
	/* a summation contains its own action table wrapped in a private udf */
	if (a->index == SUM) {
	    real_free_at(a->arg.udf_arg->at);
	    free(a->arg.udf_arg);
	}
#ifdef HAVE_EXTERNAL_FUNCTIONS
	/* external function calls contain a parameter list */
	if (a->index == CALLE)
	    free(a->arg.exf_arg);
#endif
    }
    free(at_ptr);
}

/* EAM July 2003 - Return pointer to udv with this name; if the key does not
 * match any existing udv names, create a new one and return a pointer to it.
 */
struct udvt_entry *
add_udv_by_name(char *key)
{
    struct udvt_entry **udv_ptr = &first_udv;

    /* check if it's already in the table... */

    while (*udv_ptr) {
	if (!strcmp(key, (*udv_ptr)->udv_name))
	    return (*udv_ptr);
	udv_ptr = &((*udv_ptr)->next_udv);
    }

    *udv_ptr = (struct udvt_entry *)
	gp_alloc(sizeof(struct udvt_entry), "value");
    (*udv_ptr)->next_udv = NULL;
    (*udv_ptr)->udv_name = gp_strdup(key);
    (*udv_ptr)->udv_undef = TRUE;
    (*udv_ptr)->udv_value.type = 0;
    return (*udv_ptr);
}

struct udvt_entry *
get_udv_by_name(char *key)
{
    struct udvt_entry *udv = first_udv;

    while (udv) {
        if (!strcmp(key, udv->udv_name))
            return udv;

        udv = udv->next_udv;
    }

    return NULL;
}

/* This doesn't really delete, it just marks the udv as undefined */
void
del_udv_by_name(char *key, TBOOLEAN wildcard)
{
    struct udvt_entry *udv_ptr = *udv_user_head;

    while (udv_ptr) {
	/* Forbidden to delete GPVAL_* */
	if (!strncmp(udv_ptr->udv_name,"GPVAL",5))
	    ;
	else if (!strncmp(udv_ptr->udv_name,"GNUTERM",7))
	    ;

 	/* exact match */
	else if (!wildcard && !strcmp(key, udv_ptr->udv_name)) {
	    udv_ptr->udv_undef = TRUE;
	    gpfree_string(&(udv_ptr->udv_value));
	    gpfree_datablock(&(udv_ptr->udv_value));
	    break;
	}

	/* wildcard match: prefix matches */
	else if ( wildcard && !strncmp(key, udv_ptr->udv_name, strlen(key)) ) {
	    udv_ptr->udv_undef = TRUE;
	    gpfree_string(&(udv_ptr->udv_value));
	    gpfree_datablock(&(udv_ptr->udv_value));
	    /* no break - keep looking! */
	}

	udv_ptr = udv_ptr->next_udv;
    }
}

/* Clear (delete) all user defined functions */
void
clear_udf_list()
{
    struct udft_entry *udf_ptr = first_udf;
    struct udft_entry *udf_next;

    while (udf_ptr) {
	free(udf_ptr->udf_name);
	free(udf_ptr->definition);
	free_at(udf_ptr->at);
	udf_next = udf_ptr->next_udf;
	free(udf_ptr);
	udf_ptr = udf_next;
    }
    first_udf = NULL;
}

static void update_plot_bounds __PROTO((void));
static void fill_gpval_axis __PROTO((AXIS_INDEX axis));
static void set_gpval_axis_sth_double __PROTO((const char *prefix, AXIS_INDEX axis, const char *suffix, double value, int is_int));

static void
set_gpval_axis_sth_double(const char *prefix, AXIS_INDEX axis, const char *suffix, double value, int is_int)
{
    struct udvt_entry *v;
    char *cc, s[24];
    sprintf(s, "%s_%s_%s", prefix, axis_name(axis), suffix);
    for (cc=s; *cc; cc++)
	*cc = toupper((unsigned char)*cc); /* make the name uppercase */
    v = add_udv_by_name(s);
    if (!v) 
	return; /* should not happen */
    v->udv_undef = FALSE;
    if (is_int)
	Ginteger(&v->udv_value, (int)(value+0.5));
    else
	Gcomplex(&v->udv_value, value, 0);
}

static void
fill_gpval_axis(AXIS_INDEX axis)
{
    const char *prefix = "GPVAL";
    AXIS *ap = &axis_array[axis];
    double a = AXIS_DE_LOG_VALUE(axis, ap->min);
    double b = AXIS_DE_LOG_VALUE(axis, ap->max);
    set_gpval_axis_sth_double(prefix, axis, "MIN", a, 0);
    set_gpval_axis_sth_double(prefix, axis, "MAX", b, 0);
    set_gpval_axis_sth_double(prefix, axis, "LOG", ap->base, 0);

    if (axis < POLAR_AXIS) {
	set_gpval_axis_sth_double("GPVAL_DATA", axis, "MIN", AXIS_DE_LOG_VALUE(axis, ap->data_min), 0);
	set_gpval_axis_sth_double("GPVAL_DATA", axis, "MAX", AXIS_DE_LOG_VALUE(axis, ap->data_max), 0);
    }
}

/* Fill variable "var" visible by "show var" or "show var all" ("GPVAL_*")
 * by the given value (string, integer, float, complex).
 */
void
fill_gpval_string(char *var, const char *stringvalue)
{
    struct udvt_entry *v = add_udv_by_name(var);
    if (!v)
	return;
    if (v->udv_undef == FALSE && !strcmp(v->udv_value.v.string_val, stringvalue))
	return;
    if (v->udv_undef)
	v->udv_undef = FALSE;
    else
	gpfree_string(&v->udv_value);
    Gstring(&v->udv_value, gp_strdup(stringvalue));
}

void
fill_gpval_integer(char *var, int value)
{
    struct udvt_entry *v = add_udv_by_name(var);
    if (!v)
	return;
    v->udv_undef = FALSE;
    Ginteger(&v->udv_value, value);
}

void
fill_gpval_float(char *var, double value)
{
    struct udvt_entry *v = add_udv_by_name(var);
    if (!v)
	return;
    v->udv_undef = FALSE;
    Gcomplex(&v->udv_value, value, 0);
}

void
fill_gpval_complex(char *var, double areal, double aimag)
{
    struct udvt_entry *v = add_udv_by_name(var);
    if (!v)
	return;
    v->udv_undef = FALSE;
    Gcomplex(&v->udv_value, areal, aimag);
}

/*
 * Export axis bounds in terminal coordinates from previous plot.
 * This allows offline mapping of pixel coordinates onto plot coordinates.
 */
static void
update_plot_bounds(void)
{
    fill_gpval_integer("GPVAL_TERM_XMIN", axis_array[FIRST_X_AXIS].term_lower / term->tscale);
    fill_gpval_integer("GPVAL_TERM_XMAX", axis_array[FIRST_X_AXIS].term_upper / term->tscale);
    fill_gpval_integer("GPVAL_TERM_YMIN", axis_array[FIRST_Y_AXIS].term_lower / term->tscale);
    fill_gpval_integer("GPVAL_TERM_YMAX", axis_array[FIRST_Y_AXIS].term_upper / term->tscale);
    fill_gpval_integer("GPVAL_TERM_XSIZE", canvas.xright+1);
    fill_gpval_integer("GPVAL_TERM_YSIZE", canvas.ytop+1);
    fill_gpval_integer("GPVAL_TERM_SCALE", term->tscale);
}

/*
 * Put all the handling for GPVAL_* variables in this one routine.
 * We call it from one of several contexts:
 * 0: following a successful set/unset command
 * 1: following a successful plot/splot
 * 2: following an unsuccessful command (int_error)
 * 3: program entry
 * 4: explicit reset of error status
 * 5: directory changed
 * 6: X11 Window ID changed
 */
void
update_gpval_variables(int context)
{
    /* These values may change during a plot command due to auto range */
    if (context == 1) {
	fill_gpval_axis(FIRST_X_AXIS);
	fill_gpval_axis(FIRST_Y_AXIS);
	fill_gpval_axis(SECOND_X_AXIS);
	fill_gpval_axis(SECOND_Y_AXIS);
	fill_gpval_axis(FIRST_Z_AXIS);
	fill_gpval_axis(COLOR_AXIS);
	fill_gpval_axis(T_AXIS);
	fill_gpval_axis(U_AXIS);
	fill_gpval_axis(V_AXIS);
	fill_gpval_float("GPVAL_R_MIN", R_AXIS.min);
	fill_gpval_float("GPVAL_R_MAX", R_AXIS.max);
	fill_gpval_float("GPVAL_R_LOG", R_AXIS.base);
	update_plot_bounds();
	fill_gpval_integer("GPVAL_PLOT", is_3d_plot ? 0:1);
	fill_gpval_integer("GPVAL_SPLOT", is_3d_plot ? 1:0);
	fill_gpval_integer("GPVAL_VIEW_MAP", splot_map ? 1:0);
	fill_gpval_float("GPVAL_VIEW_ROT_X", surface_rot_x);
	fill_gpval_float("GPVAL_VIEW_ROT_Z", surface_rot_z);
	fill_gpval_float("GPVAL_VIEW_SCALE", surface_scale);
	fill_gpval_float("GPVAL_VIEW_ZSCALE", surface_zscale);
	return;
    }

    /* These are set after every "set" command, which is kind of silly */
    /* because they only change after 'set term' 'set output' ...      */
    if (context == 0 || context == 2 || context == 3) {
	/* This prevents a segfault if term==NULL, which can */
	/* happen if set_terminal() exits via int_error().   */
	if (!term)
	    fill_gpval_string("GPVAL_TERM", "unknown");
	else
	    fill_gpval_string("GPVAL_TERM", (char *)(term->name));

	fill_gpval_string("GPVAL_TERMOPTIONS", term_options);
	fill_gpval_string("GPVAL_OUTPUT", (outstr) ? outstr : "");
	fill_gpval_string("GPVAL_ENCODING", encoding_names[encoding]);
	fill_gpval_string("GPVAL_MINUS_SIGN", minus_sign ? minus_sign : "-");
    }

    /* If we are called from int_error() then set the error state */
    if (context == 2)
	fill_gpval_integer("GPVAL_ERRNO", 1);

    /* These initializations need only be done once, on program entry */
    if (context == 3) {
	struct udvt_entry *v = add_udv_by_name("GPVAL_VERSION");
	char *tmp;
	if (v && v->udv_undef == TRUE) {
	    v->udv_undef = FALSE;
	    Gcomplex(&v->udv_value, atof(gnuplot_version), 0);
	}
	v = add_udv_by_name("GPVAL_PATCHLEVEL");
	if (v && v->udv_undef == TRUE)
	    fill_gpval_string("GPVAL_PATCHLEVEL", gnuplot_patchlevel);
	v = add_udv_by_name("GPVAL_COMPILE_OPTIONS");
	if (v && v->udv_undef == TRUE)
	    fill_gpval_string("GPVAL_COMPILE_OPTIONS", compile_options);

	/* Start-up values */
	fill_gpval_integer("GPVAL_MULTIPLOT", 0);
	fill_gpval_integer("GPVAL_PLOT", 0);
	fill_gpval_integer("GPVAL_SPLOT", 0);

	tmp = get_terminals_names();
	fill_gpval_string("GPVAL_TERMINALS", tmp);
	free(tmp);

	fill_gpval_string("GPVAL_ENCODING", encoding_names[encoding]);

	/* Permanent copy of user-clobberable variables pi and NaN */
	fill_gpval_float("GPVAL_pi", M_PI);
	fill_gpval_float("GPVAL_NaN", not_a_number());
    }

    if (context == 3 || context == 4) {
	fill_gpval_integer("GPVAL_ERRNO", 0);
	fill_gpval_string("GPVAL_ERRMSG","");
    }

    if (context == 3 || context == 5) {
	char *save_file = NULL;
	save_file = (char *) gp_alloc(PATH_MAX, "filling GPVAL_PWD");
	if (save_file) {
	    GP_GETCWD(save_file, PATH_MAX);
	    fill_gpval_string("GPVAL_PWD", save_file);
	    free(save_file);
	}
    }

    if (context == 6) {
	fill_gpval_integer("GPVAL_TERM_WINDOWID", current_x11_windowid);
    }
}

/* Callable wrapper for the words() internal function */
int
gp_words(char *string)
{
    struct value a;

    push(Gstring(&a, string));
    f_words((union argument *)NULL);
    pop(&a);

    return a.v.int_val;
}

/* Callable wrapper for the word() internal function */
char *
gp_word(char *string, int i)
{
    struct value a;

    push(Gstring(&a, string));
    push(Ginteger(&a, i));
    f_word((union argument *)NULL);
    pop(&a);

    return a.v.string_val;
}


/* Evaluate the function linking secondary axis to primary axis */
double
eval_link_function(int axis, double raw_coord)
{
    udft_entry *link_udf = axis_array[axis].link_udf;
    int dummy_var;
    struct value a;

    if (axis == FIRST_Y_AXIS || axis == SECOND_Y_AXIS)
	dummy_var = 1;
    else
	dummy_var = 0;
    link_udf->dummy_values[1-dummy_var].type = INVALID_NAME;

    Gcomplex(&link_udf->dummy_values[dummy_var], raw_coord, 0.0);
    evaluate_at(link_udf->at, &a);

    if (a.type != CMPLX)
	a = udv_NaN->udv_value;

    return a.v.cmplx_val.real;
}
