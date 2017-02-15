#ifndef lint
static char *RCSid() { return RCSid("$Id: internal.c,v 1.79.2.3 2016/06/14 21:36:00 sfeam Exp $"); }
#endif

/* GNUPLOT - internal.c */

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


#include "internal.h"

#include "stdfn.h"
#include "alloc.h"
#include "util.h"	/* for int_error() */
#include "gp_time.h"	/* for str(p|f)time */
#include "command.h"	/* for do_system_func */
#include "variable.h"	/* for locale handling */
#include "parse.h"	/* for string_result_only */

#include <math.h>

#ifndef _WIN64
/*
 * FIXME: This is almost certainly out of date on linux, since the matherr
 * mechanism has been replaced by math_error() and supposedly is only 
 * enabled via an explicit declaration #define _SVID_SOURCE.
 */
/*
 * Excerpt from the Solaris man page for matherr():
 *
 *   The The System V Interface Definition, Third Edition (SVID3)
 *   specifies  that  certain  libm functions call matherr() when
 *   exceptions are detected. Users may define their own  mechan-
 *   isms  for handling exceptions, by including a function named
 *   matherr() in their programs.
 */

int
GP_MATHERR( STRUCT_EXCEPTION_P_X )
{
    return (undefined = TRUE);	/* don't print error message */
}
#endif

static enum DATA_TYPES sprintf_specifier __PROTO((const char *format));

#define BAD_DEFAULT default: int_error(NO_CARET, "internal error : type neither INT or CMPLX"); return;
#define BADINT_DEFAULT default: int_error(NO_CARET, "error: bit shift applied to non-INT"); return;

static int recursion_depth = 0;
void
eval_reset_after_error()
{
    recursion_depth = 0;
}

void
f_push(union argument *x)
{
    struct udvt_entry *udv;

    udv = x->udv_arg;
    if (udv->udv_undef) {
	if (string_result_only)
	/* We're only here to check whether this is a string. It isn't. */
	    udv = udv_NaN;
	else
	    int_error(NO_CARET, "undefined variable: %s", udv->udv_name);
    }
    push(&(udv->udv_value));
}

void
f_pushc(union argument *x)
{
    push(&(x->v_arg));
}


void
f_pushd1(union argument *x)
{
    push(&(x->udf_arg->dummy_values[0]));
}

void
f_pop(union argument *x)
{
    struct value dummy;
    pop(&dummy);
    gpfree_string(&dummy);
}

void
f_pushd2(union argument *x)
{
    push(&(x->udf_arg->dummy_values[1]));
}


void
f_pushd(union argument *x)
{
    struct value param;
    (void) pop(&param);
    push(&(x->udf_arg->dummy_values[param.v.int_val]));
}


/* execute a udf */
void
f_call(union argument *x)
{
    struct udft_entry *udf;
    struct value save_dummy;

    udf = x->udf_arg;
    if (!udf->at) {
	if (string_result_only) {
	    /* We're only here to check whether this is a string. It isn't. */
	    f_pop(x);
	    push(&(udv_NaN->udv_value));
	    return;
	}
	int_error(NO_CARET, "undefined function: %s", udf->udf_name);
    }

    save_dummy = udf->dummy_values[0];
    (void) pop(&(udf->dummy_values[0]));

    if (udf->dummy_num != 1)
	int_error(NO_CARET, "function %s requires %d variables", udf->udf_name, udf->dummy_num);

    if (recursion_depth++ > STACK_DEPTH)
	int_error(NO_CARET, "recursion depth limit exceeded");

    execute_at(udf->at);
    gpfree_string(&udf->dummy_values[0]);
    udf->dummy_values[0] = save_dummy;

    recursion_depth--;
}


/* execute a udf of n variables */
void
f_calln(union argument *x)
{
    struct udft_entry *udf;
    struct value save_dummy[MAX_NUM_VAR];

    int i;
    int num_pop;
    struct value num_params;

    udf = x->udf_arg;
    if (!udf->at)		/* undefined */
	int_error(NO_CARET, "undefined function: %s", udf->udf_name);
    for (i = 0; i < MAX_NUM_VAR; i++)
	save_dummy[i] = udf->dummy_values[i];

    (void) pop(&num_params);

    if (num_params.v.int_val != udf->dummy_num)
	int_error(NO_CARET, "function %s requires %d variable%c", 
	    udf->udf_name, udf->dummy_num, (udf->dummy_num == 1)?'\0':'s');

    /* if there are more parameters than the function is expecting */
    /* simply ignore the excess */
    if (num_params.v.int_val > MAX_NUM_VAR) {
	/* pop and discard the dummies that there is no room for */
	num_pop = num_params.v.int_val - MAX_NUM_VAR;
	for (i = 0; i < num_pop; i++)
	    (void) pop(&(udf->dummy_values[0]));

	num_pop = MAX_NUM_VAR;
    } else {
	num_pop = num_params.v.int_val;
    }

    /* pop parameters we can use */
    for (i = num_pop - 1; i >= 0; i--)
	(void) pop(&(udf->dummy_values[i]));

    if (recursion_depth++ > STACK_DEPTH)
	int_error(NO_CARET, "recursion depth limit exceeded");

    execute_at(udf->at);

    recursion_depth--;

    for (i = 0; i < MAX_NUM_VAR; i++) {
	gpfree_string(&udf->dummy_values[i]);
	udf->dummy_values[i] = save_dummy[i];
    }
}


void
f_sum(union argument *arg)
{
    struct value beg, end, varname; /* [<var> = <start>:<end>] */
    udft_entry *udf;                /* function to evaluate */
    udvt_entry *udv;                /* iteration variable */
    struct value ret;               /* result */
    struct value z;
    int i;

    (void) pop(&end);
    (void) pop(&beg);
    (void) pop(&varname);

    if (beg.type != INTGR || end.type != INTGR)
        int_error(NO_CARET, "range specifiers of sum must have integer values");
    if (varname.type != STRING)
        int_error(NO_CARET, "internal error: f_sum expects argument (varname) of type string.");

    udv = get_udv_by_name(varname.v.string_val);
    if (!udv)
        int_error(NO_CARET, "internal error: f_sum could not access iteration variable.");
    udv->udv_undef = false;

    udf = arg->udf_arg;
    if (!udf)
        int_error(NO_CARET, "internal error: f_sum could not access summation coefficient function");

    Gcomplex(&ret, 0, 0);
    for (i=beg.v.int_val; i<=end.v.int_val; ++i) {
        double x, y;

        /* calculate f_i = f() with user defined variable i */
        Ginteger(&udv->udv_value, i);
        execute_at(udf->at);

        pop(&z);
        x = real(&ret) + real(&z);
        y = imag(&ret) + imag(&z);
        Gcomplex(&ret, x, y);
    }

    gpfree_string(&varname);

    push(Gcomplex(&z, real(&ret), imag(&ret)));
}


void
f_lnot(union argument *arg)
{
    struct value a;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&a));
    push(Ginteger(&a, !a.v.int_val));
}


void
f_bnot(union argument *arg)
{
    struct value a;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&a));
    push(Ginteger(&a, ~a.v.int_val));
}


void
f_lor(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&b));
    int_check(pop(&a));
    push(Ginteger(&a, a.v.int_val || b.v.int_val));
}

void
f_land(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&b));
    int_check(pop(&a));
    push(Ginteger(&a, a.v.int_val && b.v.int_val));
}


void
f_bor(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&b));
    int_check(pop(&a));
    push(Ginteger(&a, a.v.int_val | b.v.int_val));
}


void
f_xor(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&b));
    int_check(pop(&a));
    push(Ginteger(&a, a.v.int_val ^ b.v.int_val));
}


void
f_band(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    int_check(pop(&b));
    int_check(pop(&a));
    push(Ginteger(&a, a.v.int_val & b.v.int_val));
}


/*
 * Make all the following internal routines perform autoconversion
 * from string to numeric value.
 */
#define pop(x) pop_or_convert_from_string(x)

void
f_uminus(union argument *arg)
{
    struct value a;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	a.v.int_val = -a.v.int_val;
	break;
    case CMPLX:
	a.v.cmplx_val.real =
	    -a.v.cmplx_val.real;
	a.v.cmplx_val.imag =
	    -a.v.cmplx_val.imag;
	break;
	BAD_DEFAULT
    }
    push(&a);
}


void
f_eq(union argument *arg)
{
    /* note: floating point equality is rare because of roundoff error! */
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);

    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val ==
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val ==
		      b.v.cmplx_val.real &&
		      b.v.cmplx_val.imag == 0.0);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (b.v.int_val == a.v.cmplx_val.real &&
		      a.v.cmplx_val.imag == 0.0);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real ==
		      b.v.cmplx_val.real &&
		      a.v.cmplx_val.imag ==
		      b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_ne(union argument *arg)
{
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val !=
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val !=
		      b.v.cmplx_val.real ||
		      b.v.cmplx_val.imag != 0.0);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (b.v.int_val !=
		      a.v.cmplx_val.real ||
		      a.v.cmplx_val.imag != 0.0);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real !=
		      b.v.cmplx_val.real ||
		      a.v.cmplx_val.imag !=
		      b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_gt(union argument *arg)
{
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val >
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val >
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (a.v.cmplx_val.real >
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real >
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_lt(union argument *arg)
{
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val <
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val <
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (a.v.cmplx_val.real <
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real <
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_ge(union argument *arg)
{
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val >=
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val >=
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (a.v.cmplx_val.real >=
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real >=
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_le(union argument *arg)
{
    struct value a, b;
    int result = 0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    result = (a.v.int_val <=
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.int_val <=
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    result = (a.v.cmplx_val.real <=
		      b.v.int_val);
	    break;
	case CMPLX:
	    result = (a.v.cmplx_val.real <=
		      b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(Ginteger(&a, result));
}


void
f_leftshift(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    (void) Ginteger(&result, (unsigned)(a.v.int_val) << b.v.int_val);
	    break;
	BADINT_DEFAULT
	}
	break;
    BADINT_DEFAULT
    }
    push(&result);
}



void
f_rightshift(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    (void) Ginteger(&result, (unsigned)(a.v.int_val) >> b.v.int_val);
	    break;
	BADINT_DEFAULT
	}
	break;
    BADINT_DEFAULT
    }
    push(&result);
}



void
f_plus(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    (void) Ginteger(&result, a.v.int_val +
			    b.v.int_val);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.int_val +
			    b.v.cmplx_val.real,
			    b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    (void) Gcomplex(&result, b.v.int_val +
			    a.v.cmplx_val.real,
			    a.v.cmplx_val.imag);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.cmplx_val.real +
			    b.v.cmplx_val.real,
			    a.v.cmplx_val.imag +
			    b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(&result);
}


void
f_minus(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);		/* now do a - b */
    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    (void) Ginteger(&result, a.v.int_val -
			    b.v.int_val);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.int_val -
			    b.v.cmplx_val.real,
			    -b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    (void) Gcomplex(&result, a.v.cmplx_val.real -
			    b.v.int_val,
			    a.v.cmplx_val.imag);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.cmplx_val.real -
			    b.v.cmplx_val.real,
			    a.v.cmplx_val.imag -
			    b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(&result);
}


void
f_mult(union argument *arg)
{
    struct value a, b, result;
    double product;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);		/* now do a*b */

    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    product = (double)a.v.int_val * (double)b.v.int_val;
	    if (fabs(product) >= (double)INT_MAX)
		(void) Gcomplex(&result, product, 0.0);
	    else
		(void) Ginteger(&result, a.v.int_val * b.v.int_val);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.int_val *
			    b.v.cmplx_val.real,
			    a.v.int_val *
			    b.v.cmplx_val.imag);
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    (void) Gcomplex(&result, b.v.int_val *
			    a.v.cmplx_val.real,
			    b.v.int_val *
			    a.v.cmplx_val.imag);
	    break;
	case CMPLX:
	    (void) Gcomplex(&result, a.v.cmplx_val.real *
			    b.v.cmplx_val.real -
			    a.v.cmplx_val.imag *
			    b.v.cmplx_val.imag,
			    a.v.cmplx_val.real *
			    b.v.cmplx_val.imag +
			    a.v.cmplx_val.imag *
			    b.v.cmplx_val.real);
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(&result);
}


void
f_div(union argument *arg)
{
    struct value a, b, result;
    double square;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);		/* now do a/b */

    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    if (b.v.int_val)
		(void) Ginteger(&result, a.v.int_val /
				b.v.int_val);
	    else {
		(void) Ginteger(&result, 0);
		undefined = TRUE;
	    }
	    break;
	case CMPLX:
	    square = b.v.cmplx_val.real *
		b.v.cmplx_val.real +
		b.v.cmplx_val.imag *
		b.v.cmplx_val.imag;
	    if (square)
		(void) Gcomplex(&result, a.v.int_val *
				b.v.cmplx_val.real / square,
				-a.v.int_val *
				b.v.cmplx_val.imag / square);
	    else {
		(void) Gcomplex(&result, 0.0, 0.0);
		undefined = TRUE;
	    }
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    if (b.v.int_val)
		(void) Gcomplex(&result, a.v.cmplx_val.real /
				b.v.int_val,
				a.v.cmplx_val.imag /
				b.v.int_val);
	    else {
		(void) Gcomplex(&result, 0.0, 0.0);
		undefined = TRUE;
	    }
	    break;
	case CMPLX:
	    square = b.v.cmplx_val.real *
		b.v.cmplx_val.real +
		b.v.cmplx_val.imag *
		b.v.cmplx_val.imag;
	    if (square)
		(void) Gcomplex(&result, (a.v.cmplx_val.real *
					  b.v.cmplx_val.real +
					  a.v.cmplx_val.imag *
					  b.v.cmplx_val.imag) / square,
				(a.v.cmplx_val.imag *
				 b.v.cmplx_val.real -
				 a.v.cmplx_val.real *
				 b.v.cmplx_val.imag) /
				square);
	    else {
		(void) Gcomplex(&result, 0.0, 0.0);
		undefined = TRUE;
	    }
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(&result);
}


void
f_mod(union argument *arg)
{
    struct value a, b;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);		/* now do a%b */

    if (a.type != INTGR || b.type != INTGR)
	int_error(NO_CARET, "non-integer operand for %%");
    if (b.v.int_val)
	push(Ginteger(&a, a.v.int_val % b.v.int_val));
    else {
	push(Ginteger(&a, 0));
	undefined = TRUE;
    }
}


void
f_power(union argument *arg)
{
    struct value a, b, result;
    int i, t;
    double mag, ang;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);		/* now find a**b */

    switch (a.type) {
    case INTGR:
	switch (b.type) {
	case INTGR:
	    if (a.v.int_val == 0) {
		if (b.v.int_val < 0)
		    undefined = TRUE;
		(void) Ginteger(&result, b.v.int_val == 0 ? 1 : 0);
		break;
	    }
	    /* EAM Oct 2009 - avoid integer overflow by switching to double */
	    mag = pow((double)a.v.int_val,(double)b.v.int_val);
	    if (mag > (double)INT_MAX  ||  b.v.int_val < 0) {
		(void) Gcomplex(&result, mag, 0.0);
		break;
	    }
	    t = 1;
	    /* this ought to use bit-masks and squares, etc */
	    for (i = 0; i < b.v.int_val; i++)
		t *= a.v.int_val;
	    (void) Ginteger(&result, t);
	    break;
	case CMPLX:
	    if (a.v.int_val == 0) {
		if (b.v.cmplx_val.imag != 0 || b.v.cmplx_val.real < 0) {
		    undefined = TRUE;
		}
		/* return 1.0 for 0**0 */
		Gcomplex(&result, b.v.cmplx_val.real == 0 ? 1.0 : 0.0, 0.0);
	    } else {
		mag =
		    pow(magnitude(&a), fabs(b.v.cmplx_val.real));
		if (b.v.cmplx_val.real < 0.0) {
		    if (mag != 0.0)
			mag = 1.0 / mag;
		    else
			undefined = TRUE;
		}
		mag *= gp_exp(-b.v.cmplx_val.imag * angle(&a));
		ang = b.v.cmplx_val.real * angle(&a) +
		    b.v.cmplx_val.imag * log(magnitude(&a));
		(void) Gcomplex(&result, mag * cos(ang),
				mag * sin(ang));
	    }
	    break;
	    BAD_DEFAULT
	}
	break;
    case CMPLX:
	switch (b.type) {
	case INTGR:
	    if (a.v.cmplx_val.imag == 0.0) {
		mag = pow(a.v.cmplx_val.real, (double) abs(b.v.int_val));
		if (b.v.int_val < 0) {
		    if (mag != 0.0)
			mag = 1.0 / mag;
		    else
			undefined = TRUE;
		}
		(void) Gcomplex(&result, mag, 0.0);
	    } else {
		/* not so good, but...! */
		mag = pow(magnitude(&a), (double) abs(b.v.int_val));
		if (b.v.int_val < 0) {
		    if (mag != 0.0)
			mag = 1.0 / mag;
		    else
			undefined = TRUE;
		}
		ang = angle(&a) * b.v.int_val;
		(void) Gcomplex(&result, mag * cos(ang),
				mag * sin(ang));
	    }
	    break;
	case CMPLX:
	    if (a.v.cmplx_val.real == 0 && a.v.cmplx_val.imag == 0) {
		if (b.v.cmplx_val.imag != 0 || b.v.cmplx_val.real < 0) {
		    undefined = TRUE;
		}
		/* return 1.0 for 0**0 */
		Gcomplex(&result, b.v.cmplx_val.real == 0 ? 1.0 : 0.0, 0.0);
	    } else {
		mag = pow(magnitude(&a), fabs(b.v.cmplx_val.real));
		if (b.v.cmplx_val.real < 0.0) {
		    if (mag != 0.0)
			mag = 1.0 / mag;
		    else
			undefined = TRUE;
		}
		mag *= gp_exp(-b.v.cmplx_val.imag * angle(&a));
		ang = b.v.cmplx_val.real * angle(&a) +
		    b.v.cmplx_val.imag * log(magnitude(&a));
		(void) Gcomplex(&result, mag * cos(ang),
				mag * sin(ang));
	    }
	    break;
	    BAD_DEFAULT
	}
	break;
	BAD_DEFAULT
    }
    push(&result);
}


void
f_factorial(union argument *arg)
{
    struct value a;
    int i;
    double val = 0.0;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&a);		/* find a! (factorial) */

    switch (a.type) {
    case INTGR:
	val = 1.0;
	for (i = a.v.int_val; i > 1; i--)	/*fpe's should catch overflows */
	    val *= i;
	break;
    default:
	int_error(NO_CARET, "factorial (!) argument must be an integer");
	return;			/* avoid gcc -Wall warning about val */
    }

    push(Gcomplex(&a, val, 0.0));

}

/*
 * Terminate the autoconversion from string to numeric values
 */
#undef pop

void
f_concatenate(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);

    if (b.type == INTGR) {
	int i = b.v.int_val;
	b.type = STRING;
	b.v.string_val = (char *)gp_alloc(32,"str_const");
#ifdef HAVE_SNPRINTF
	snprintf(b.v.string_val,32,"%d",i);
#else
	sprintf(b.v.string_val,"%d",i);
#endif
    }

    if (a.type != STRING || b.type != STRING)
	int_error(NO_CARET, "internal error : STRING operator applied to non-STRING type");

    (void) Gstring(&result, gp_stradd(a.v.string_val, b.v.string_val));
    gpfree_string(&a);
    gpfree_string(&b);
    push(&result);
    gpfree_string(&result); /* free string allocated within gp_stradd() */
}

void
f_eqs(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);

    if(a.type != STRING || b.type != STRING)
	int_error(NO_CARET, "internal error : STRING operator applied to non-STRING type");

    (void) Ginteger(&result, !strcmp(a.v.string_val, b.v.string_val));
    gpfree_string(&a);
    gpfree_string(&b);
    push(&result);
}

void
f_nes(union argument *arg)
{
    struct value a, b, result;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&b);
    (void) pop(&a);

    if(a.type != STRING || b.type != STRING)
	int_error(NO_CARET, "internal error : STRING operator applied to non-STRING type");

    (void) Ginteger(&result, (int)(strcmp(a.v.string_val, b.v.string_val)!=0));
    gpfree_string(&a);
    gpfree_string(&b);
    push(&result);
}

void
f_strlen(union argument *arg)
{
    struct value a, result;

    (void) arg;
    (void) pop(&a);

    if (a.type != STRING)
	int_error(NO_CARET, "internal error : strlen of non-STRING argument");

    (void) Ginteger(&result, (int)gp_strlen(a.v.string_val));
    gpfree_string(&a);
    push(&result);
}

void
f_strstrt(union argument *arg)
{
    struct value needle, haystack, result;
    char *start;

    (void) arg;
    (void) pop(&needle);
    (void) pop(&haystack);

    if (needle.type != STRING || haystack.type != STRING)
	int_error(NO_CARET, "internal error : non-STRING argument to strstrt");

    start = strstr(haystack.v.string_val, needle.v.string_val);
    (void) Ginteger(&result, (int)(start ? (start-haystack.v.string_val)+1 : 0));
    gpfree_string(&needle);
    gpfree_string(&haystack);
    push(&result);
}

/*
 * f_range() handles both explicit calls to substr(string, beg, end)
 * and the short form string[beg:end].  The calls to gp_strlen() and
 * gp_strchrn() allow it to handle utf8 strings.
 */
void
f_range(union argument *arg)
{
    struct value beg, end, full;
    struct value substr;
    int ibeg, iend;

    (void) arg;			/* avoid -Wunused warning */
    (void) pop(&end);
    (void) pop(&beg);
    (void) pop(&full);

    if (beg.type == INTGR)
	ibeg = beg.v.int_val;
    else if (beg.type == CMPLX)
	ibeg = floor(beg.v.cmplx_val.real);
    else
	int_error(NO_CARET, "internal error: non-numeric substring range specifier");
    if (end.type == INTGR)
	iend = end.v.int_val;
    else if (end.type == CMPLX)
	iend = floor(end.v.cmplx_val.real);
    else
	int_error(NO_CARET, "internal error: non-numeric substring range specifier");
	
    if (full.type != STRING)
	int_error(NO_CARET, "internal error: substring range operator applied to non-STRING type");

    FPRINTF((stderr,"f_range( \"%s\", %d, %d)\n", full.v.string_val, beg.v.int_val, end.v.int_val));

    if (iend > gp_strlen(full.v.string_val))
	iend = gp_strlen(full.v.string_val);
    if (ibeg < 1)
	ibeg = 1;

    if (ibeg > iend) {
	push(Gstring(&substr, ""));
    } else {
	char *begp = gp_strchrn(full.v.string_val,ibeg-1);
	char *endp = gp_strchrn(full.v.string_val,iend);
	*endp = '\0';
	push(Gstring(&substr, begp));
    }
    gpfree_string(&full);
}

/* Magic number! */
#define RETURN_WORD_COUNT (-17*23*61)

void
f_words(union argument *arg)
{
    struct value a;

    /* "words(s)" is implemented as "word(s,RETURN_WORD_COUNT)" */
    push(Ginteger(&a, RETURN_WORD_COUNT));
    f_word(arg);
}

void
f_word(union argument *arg)
{
    struct value a, b, result;
    int nwords = 0;
    int in_string = 0;
    int ntarget;
    char q;
    char *s;

    (void) arg;
    if (pop(&b)->type != INTGR)
	int_error(NO_CARET, "internal error : non-INTGR argument");
    ntarget = b.v.int_val;

    if (pop(&a)->type != STRING)
	int_error(NO_CARET, "internal error : non-STRING argument");
    s = a.v.string_val;

    Gstring(&result, "");
    while (*s) {
	while (isspace((unsigned char)*s)) s++;
	if (!*s)
	    break;
	nwords++;
	if (*s == '"' || *s == '\'') {
	    q = *s;
	    s++;
	    in_string = 1;
	}
	if (nwords == ntarget) { /* Found the one we wanted */
	    Gstring(&result,s);
	    s = result.v.string_val;
	}
	while (*s && ((!isspace((unsigned char)*s) && !in_string) || (in_string && *s != q))) s++;
	if (nwords == ntarget) { /* Terminate this word cleanly */
	    *s = '\0';
	    break;
	}
	if (in_string && (*s == q)) {
	    in_string = 0;
	    s++;
	}
    }

    /* words(s) = word(s,magic_number) = # of words in string */
    if (ntarget == RETURN_WORD_COUNT)
	Ginteger(&result, nwords);

    push(&result);
    gpfree_string(&a);
}
#undef RETURN_WORD_COUNT


/* EAM July 2004  (revised to dynamic buffer July 2005)
 * There are probably an infinite number of things that can
 * go wrong if the user mis-matches arguments and format strings
 * in the call to sprintf, but I hope none will do worse than
 * result in a garbage output string.
 */
void
f_sprintf(union argument *arg)
{
    struct value a[10], *args;
    struct value num_params;
    struct value result;
    char *buffer;
    int bufsize;
    char *next_start, *outpos, tempchar;
    int next_length;
    char *prev_start;
    int prev_pos;
    int i, remaining;
    int nargs = 0;
    enum DATA_TYPES spec_type;

    /* Retrieve number of parameters from top of stack */
    (void) arg;
    pop(&num_params);
    nargs = num_params.v.int_val;
    if (nargs > 10) {	/* Fall back to slow but sure allocation */
	args = gp_alloc(sizeof(struct value)*nargs, "sprintf args");
    } else
	args = a;

    for (i=0; i<nargs; i++)
	pop(&args[i]);  /* pop next argument */

    /* Make sure we got a format string of some sort */
    if (args[nargs-1].type != STRING)
	int_error(NO_CARET,"First parameter to sprintf must be a format string");

    /* Allocate space for the output string. If this isn't */
    /* long enough we can reallocate a larger space later. */
    bufsize = 80 + strlen(args[nargs-1].v.string_val);
    buffer = gp_alloc(bufsize, "f_sprintf");

    /* Copy leading fragment of format into output buffer */
    outpos = buffer;
    next_start  = args[nargs-1].v.string_val;
    next_length = strcspn(next_start,"%");
    strncpy(outpos, next_start, next_length);

    next_start += next_length;
    outpos += next_length;

    /* Format the remaining sprintf() parameters one by one */
    prev_start = next_start;
    prev_pos = next_length;
    remaining = nargs - 1;

    /* If the user has set an explicit LC_NUMERIC locale, apply it */
    /* to sprintf calls during expression evaluation.              */
    set_numeric_locale();

    /* Each time we start this loop we are pointing to a % character */
    while (remaining-->0 && next_start[0] && next_start[1]) {
	struct value *next_param = &args[remaining];

	/* Check for %%; print as literal and don't consume a parameter */
	if (!strncmp(next_start,"%%",2)) {
	    next_start++;
	    do {
		*outpos++ = *next_start++;
	    } while(*next_start && *next_start != '%');
	    remaining++;
	    continue;
	}

	next_length = strcspn(next_start+1,"%") + 1;
	tempchar = next_start[next_length];
	next_start[next_length] = '\0';

	spec_type = sprintf_specifier(next_start);

	/* string value <-> numerical value check */
	if ( spec_type == STRING && next_param->type != STRING )
	    int_error(NO_CARET,"f_sprintf: attempt to print numeric value with string format");
	if ( spec_type != STRING && next_param->type == STRING )
	    int_error(NO_CARET,"f_sprintf: attempt to print string value with numeric format");

#ifdef HAVE_SNPRINTF
	/* Use the format to print next arg */
	switch(spec_type) {
	case INTGR:
	    snprintf(outpos,bufsize-(outpos-buffer),
		     next_start, (int)real(next_param));
	    break;
	case CMPLX:
	    snprintf(outpos,bufsize-(outpos-buffer),
		     next_start, real(next_param));
	    break;
	case STRING:
	    snprintf(outpos,bufsize-(outpos-buffer),
		next_start, next_param->v.string_val);
	    break;
	default:
	    int_error(NO_CARET,"internal error: invalid spec_type");
	}
#else
	/* FIXME - this is bad; we should dummy up an snprintf equivalent */
	switch(spec_type) {
	case INTGR:
	    sprintf(outpos, next_start, (int)real(next_param));
	    break;
	case CMPLX:
	    sprintf(outpos, next_start, real(next_param));
	    break;
	case STRING:
	    sprintf(outpos, next_start, next_param->v.string_val);
	    break;
	default:
	    int_error(NO_CARET,"internal error: invalid spec_type");
	}
#endif

	next_start[next_length] = tempchar;
	next_start += next_length;
	outpos = &buffer[strlen(buffer)];

	/* Check whether previous parameter output hit the end of the buffer */
	/* If so, reallocate a larger buffer, go back and try it again.      */
	if (strlen(buffer) >= bufsize-2) {
	    bufsize *= 2;
	    buffer = gp_realloc(buffer, bufsize, "f_sprintf");
	    next_start = prev_start;
	    outpos = buffer + prev_pos;
	    remaining++;
	    continue;
	} else {
	    prev_start = next_start;
	    prev_pos = outpos - buffer;
	}

    }

    /* Copy the trailing portion of the format, if any */
    /* We could just call snprintf(), but it doesn't check for */
    /* whether there really are more variables to handle.      */
    i = bufsize - (outpos-buffer);
    while (*next_start && --i > 0) {
	if (*next_start == '%' && *(next_start+1) == '%')
	    next_start++;
	*outpos++ = *next_start++;
    }
    *outpos = '\0';

    FPRINTF((stderr," snprintf result = \"%s\"\n",buffer));
    push(Gstring(&result, buffer));
    free(buffer);

    /* Free any strings from parameters we have now used */
    for (i=0; i<nargs; i++)
	gpfree_string(&args[i]);

    if (args != a)
	free(args);

    /* Return to C locale for internal use */
    reset_numeric_locale();

}

/* EAM July 2004 - Gnuplot's own string formatting conventions.
 * Currently this routine assumes base 10 representation, because
 * it is not clear where it could be specified to be anything else.
 */
void
f_gprintf(union argument *arg)
{
    struct value fmt, val, result;
    char *buffer;
    int length;
    double base = 10.;
 
    /* Retrieve parameters from top of stack */
    (void) arg;
    pop(&val);
    pop(&fmt);

#ifdef DEBUG
    fprintf(stderr,"----------\nGot gprintf parameters\nfmt: ");
	disp_value(stderr, &fmt, TRUE);
    fprintf(stderr,"\nval: ");
	disp_value(stderr, &val, TRUE);
    fprintf(stderr,"\n----------\n");
#endif

    /* Make sure parameters are of the correct type */
    if (fmt.type != STRING)
	int_error(NO_CARET,"First parameter to gprintf must be a format string");

    /* Make sure we have at least as much space in the output as the format itself */
    length = 80 + strlen(fmt.v.string_val);
    buffer = gp_alloc(length, "f_gprintf");

    /* Call the old internal routine */
    gprintf(buffer, length, fmt.v.string_val, base, real(&val));

    FPRINTF((stderr," gprintf result = \"%s\"\n",buffer));
    push(Gstring(&result, buffer));

    gpfree_string(&fmt);
    free(buffer);
}


/* Output time given in seconds from year 2000 into string */
void
f_strftime(union argument *arg)
{
    struct value fmt, val;
    char *fmtstr, *buffer;
    int fmtlen, buflen, length;

    (void) arg; /* Avoid compiler warnings */

    /* Retrieve parameters from top of stack */
    pop(&val);
    pop(&fmt);
    if ( fmt.type != STRING )
	int_error(NO_CARET,
		  "First parameter to strftime must be a format string");

    /* Prepare format string.
     * Make sure the resulting string not empty by adding a space.
     * Otherwise, the return value of gstrftime doesn't give enough
     * information.
     */
    fmtlen = strlen(fmt.v.string_val) + 1;
    fmtstr = gp_alloc(fmtlen + 1, "f_strftime: fmt");
    strncpy(fmtstr, fmt.v.string_val, fmtlen);
    strncat(fmtstr, " ", fmtlen);
    buflen = 80 + 2*fmtlen;
    buffer = gp_alloc(buflen, "f_strftime: buffer");

    /* Get time_str */
    length = gstrftime(buffer, buflen, fmtstr, real(&val));
    if (length == 0 || length >= buflen)
	int_error(NO_CARET, "Resulting string is too long");

    /* Remove trailing space */
    assert(buffer[length-1] == ' ');
    buffer[length-1] = NUL;

    gpfree_string(&val);
    gpfree_string(&fmt);
    free(fmtstr);

    push(Gstring(&val, buffer));
    free(buffer);
}

/* Convert string into seconds from year 2000 */
void
f_strptime(union argument *arg)
{
    struct value fmt, val;
    struct tm time_tm;
    double usec = 0.0;
    double result;

    (void) arg; /* Avoid compiler warnings */

    pop(&val);
    pop(&fmt);

    if ( fmt.type != STRING || val.type != STRING )
	int_error(NO_CARET,
		  "Both parameters to strptime must be strings");
    if ( !fmt.v.string_val || !val.v.string_val )
	int_error(NO_CARET, "Internal error: string not allocated");


    /* string -> time_tm  plus extra fractional second */
    gstrptime(val.v.string_val, fmt.v.string_val, &time_tm, &usec);

    /* time_tm -> result */
    result = gtimegm(&time_tm);
    FPRINTF((stderr," strptime result = %g seconds \n", result));

    /* Add back any extra fractional second */
    result += usec;

    gpfree_string(&val);
    gpfree_string(&fmt);
    push(Gcomplex(&val, result, 0.0));
}

/* Get current system time in seconds since 2000 
 * The type of the value popped from the stack 
 * determines what is returned.
 * If integer, the result is also an integer.
 * If real (complex), the result is also real, 
 * with microsecond precision (if available).
 * If string, it is assumed to be a format string, 
 * and it is passed to strftime to get a formatted time string.
 */
void
f_time(union argument *arg)
{
    struct value val, val2;
    double time_now;
#ifdef HAVE_SYS_TIME_H
    struct timeval tp;

    gettimeofday(&tp, NULL);
    tp.tv_sec -= SEC_OFFS_SYS;
    time_now = tp.tv_sec + (tp.tv_usec/1000000.0);
#else

    time_now = (double) time(NULL);
    time_now -= SEC_OFFS_SYS;
#endif

    (void) arg; /* Avoid compiler warnings */
    pop(&val); 
    
    switch(val.type) {
	case INTGR:
	    push(Ginteger(&val, (int) time_now));
	    break;
	case CMPLX:
	    push(Gcomplex(&val, time_now, 0.0));
	    break;
	case STRING:
	    push(&val); /* format string */
	    push(Gcomplex(&val2, time_now, 0.0));
	    f_strftime(arg);
	    break;
	default:
	    int_error(NO_CARET,"internal error: invalid argument type");
    }
}


/* Return which argument type sprintf will need for this format string:
 *   char*       STRING
 *   int         INTGR
 *   double      CMPLX
 * Should call int_err for any other type.
 * format is expected to start with '%'
 */
static enum DATA_TYPES
sprintf_specifier(const char* format)
{
    const char string_spec[]  = "s";
    const char real_spec[]    = "aAeEfFgG";
    const char int_spec[]     = "cdiouxX";
    /* The following characters are used for use of invalid types */
    const char illegal_spec[] = "hlLqjzZtCSpn";

    int string_pos, real_pos, int_pos, illegal_pos;

    /* check if really format specifier */
    if (format[0] != '%')
	int_error(NO_CARET,
		  "internal error: sprintf_specifier called without '%'\n");

    string_pos  = strcspn(format, string_spec);
    real_pos    = strcspn(format, real_spec);
    int_pos     = strcspn(format, int_spec);
    illegal_pos = strcspn(format, illegal_spec);

    if ( illegal_pos < int_pos && illegal_pos < real_pos
	 && illegal_pos < string_pos )
	int_error(NO_CARET,
		  "sprintf_specifier: used with invalid format specifier\n");
    else if ( string_pos < real_pos && string_pos < int_pos )
	return STRING;
    else if ( real_pos < int_pos )
	return CMPLX;
    else if ( int_pos < strlen(format) )
	return INTGR;
    else
	int_error(NO_CARET,
		  "sprintf_specifier: no format specifier\n");

    return INTGR; /* Can't happen, but the compiler doesn't realize that */
}


/* execute a system call and return stream from STDOUT */
void
f_system(union argument *arg)
{
    struct value val, result;
    char *output;
    int output_len, ierr;

    /* Retrieve parameters from top of stack */
    (void) arg;
    pop(&val);

    /* Make sure parameters are of the correct type */
    if (val.type != STRING)
	int_error(NO_CARET, "non-string argument to system()");

    FPRINTF((stderr," f_system input = \"%s\"\n", val.v.string_val));

    ierr = do_system_func(val.v.string_val, &output);
    fill_gpval_integer("GPVAL_ERRNO", ierr);
    output_len = strlen(output);

    /* chomp result */
    if ( output_len > 0 && output[output_len-1] == '\n' )
	output[output_len-1] = NUL;

    FPRINTF((stderr," f_system result = \"%s\"\n", output));

    push(Gstring(&result, output));

    gpfree_string(&result); /* free output */
    gpfree_string(&val);    /* free command string */
}


/* Variable assignment operator */
void
f_assign(union argument *arg)
{
    struct value a, b;
    (void) arg;
    (void) pop(&b);	/* new value */
    (void) pop(&a);	/* name of variable */
    
    if (a.type == STRING) {
	struct udvt_entry *udv;
	if (!strncmp(a.v.string_val,"GPVAL_",6) || !strncmp(a.v.string_val,"MOUSE_",6))
	    int_error(NO_CARET,"Attempt to assign to a read-only variable");
	udv = add_udv_by_name(a.v.string_val);
	gpfree_string(&a);
	if (!udv->udv_undef)
	    gpfree_string(&(udv->udv_value));
	udv->udv_value = b;
	udv->udv_undef = FALSE;
	push(&b);
    } else {
	int_error(NO_CARET, "attempt to assign to something other than a named variable");
    }
}

/*
 * Retrieve the current value of a user-defined variable whose name is known.
 * B = value("A") has the same result as B = A.
 */

void
f_value(union argument *arg)
{
    struct udvt_entry *p = first_udv;
    struct value a;
    struct value result;

    (void) arg;
    (void) pop(&a);

    if (a.type != STRING) {
	/* int_warn(NO_CARET,"non-string value passed to value()"); */
	push(&a);
	return;
    }

    while (p) {
	if (!strcmp(p->udv_name, a.v.string_val)) {
	    result = p->udv_value;
	    if (p->udv_undef)
		p = NULL;
	    else if (result.type == STRING)
		result.v.string_val = gp_strdup(result.v.string_val);
	    break;
	}
	p = p->next_udv;
    }
    gpfree_string(&a);
    if (!p) {
	/* int_warn(NO_CARET,"undefined variable name passed to value()"); */
	result.type = CMPLX;
	result.v.cmplx_val.real = not_a_number();
    }
    push(&result);
}
