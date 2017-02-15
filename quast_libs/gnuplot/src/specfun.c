#ifndef lint
static char *RCSid() { return RCSid("$Id: specfun.c,v 1.53 2013/10/09 02:41:22 sfeam Exp $"); }
#endif

/* GNUPLOT - specfun.c */

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
 *   Original Software:
 *   Jos van der Woude, jvdwoude@hut.nl
 *
 */

/* FIXME:
 * plain comparisons of floating point numbers!
 */

#include "specfun.h"
#include "stdfn.h"
#include "util.h"

#define ITMAX   200

#ifdef FLT_EPSILON
# define MACHEPS FLT_EPSILON	/* 1.0E-08 */
#else
# define MACHEPS 1.0E-08
#endif

#ifndef E_MINEXP
/* AS239 value, e^-88 = 2^-127 */
#define E_MINEXP  (-88.0)
#endif
#ifndef E_MAXEXP
#define E_MAXEXP (-E_MINEXP)
#endif

#ifdef FLT_MAX
# define OFLOW   FLT_MAX		/* 1.0E+37 */
#else
# define OFLOW   1.0E+37
#endif

/* AS239 value for igamma(a,x>=XBIG) = 1.0 */
#define XBIG    1.0E+08

/*
 * Mathematical constants
 */
#define LNPI 1.14472988584940016
#define LNSQRT2PI 0.9189385332046727
#ifdef PI
# undef PI
#endif
#define PI 3.14159265358979323846
#define PNT68 0.6796875
#define SQRT_TWO 1.41421356237309504880168872420969809	/* JG */

/* Prefer lgamma */
#ifndef GAMMA
# ifdef HAVE_LGAMMA
#  define GAMMA(x) lgamma (x)
# elif defined(HAVE_GAMMA)
#  define GAMMA(x) gamma (x)
# else
#  undef GAMMA
# endif
#endif

#if defined(GAMMA) && !HAVE_DECL_SIGNGAM
extern int signgam;		/* this is not always declared in math.h */
#endif

/* Local function declarations, not visible outside this file */
static int mtherr __PROTO((char *, int));
static double polevl __PROTO((double x, const double coef[], int N));
static double p1evl __PROTO((double x, const double coef[], int N));
static double confrac __PROTO((double a, double b, double x));
static double ibeta __PROTO((double a, double b, double x));
static double igamma __PROTO((double a, double x));
static double ranf __PROTO((struct value * init));
static double inverse_error_func __PROTO((double p));
static double inverse_normal_func __PROTO((double p));
static double lambertw __PROTO((double x));
#if (0)	/* Only used by low-precision Airy version */
static double airy_neg __PROTO(( double x ));
static double airy_pos __PROTO((double x));
#endif
#ifndef HAVE_LIBCERF
static double humlik __PROTO((double x, double y));
#endif
static double expint __PROTO((double n, double x));
#ifndef GAMMA
static int ISNAN __PROTO((double x));
static int ISFINITE __PROTO((double x));
static double lngamma __PROTO((double z));
#endif
#ifndef HAVE_ERF
static double erf __PROTO((double a));
#endif
#ifndef HAVE_ERFC
static double erfc __PROTO((double a));
#endif

/* Macros to configure routines taken from CEPHES: */

/* Unknown arithmetic, invokes coefficients given in
 * normal decimal format.  Beware of range boundary
 * problems (MACHEP, MAXLOG, etc. in const.c) and
 * roundoff problems in pow.c:
 * (Sun SPARCstation)
 */
#define UNK 1
#define MACHEP DBL_EPSILON
#define MAXNUM DBL_MAX

/* Define to support tiny denormal numbers, else undefine. */
#define DENORMAL 1

/* Define to ask for infinity support, else undefine. */
#define INFINITIES 1

/* Define to ask for support of numbers that are Not-a-Number,
   else undefine.  This may automatically define INFINITIES in some files. */
#define NANS 1

/* Define to distinguish between -0.0 and +0.0.  */
#define MINUSZERO 1

/*
Cephes Math Library Release 2.0:  April, 1987
Copyright 1984, 1987 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

static int merror = 0;

/* Notice: the order of appearance of the following messages cannot be bound
 * to error codes defined in mconf.h or math.h or similar, as these files are
 * not available on every platform. Thus, enumerate them explicitly.
 */
#define MTHERR_DOMAIN	 1
#define MTHERR_SING	 2
#define MTHERR_OVERFLOW  3
#define MTHERR_UNDERFLOW 4
#define MTHERR_TLPREC	 5
#define MTHERR_PLPREC	 6

static int
mtherr(char *name, int code)
{
    static const char *ermsg[7] = {
	"unknown",                  /* error code 0 */
	"domain",                   /* error code 1 */
	"singularity",              /* et seq.      */
	"overflow",
	"underflow",
	"total loss of precision",
	"partial loss of precision"
    };

    /* Display string passed by calling program,
     * which is supposed to be the name of the
     * function in which the error occurred:
     */
    printf("\n%s ", name);

    /* Set global error message word */
    merror = code;

    /* Display error message defined by the code argument.  */
    if ((code <= 0) || (code >= 7))
        code = 0;
    printf("%s error\n", ermsg[code]);

    /* Return to calling program */
    return (0);
}

/*                                                      polevl.c
 *                                                      p1evl.c
 *
 *      Evaluate polynomial
 *
 *
 *
 * SYNOPSIS:
 *
 * int N;
 * double x, y, coef[N+1], polevl[];
 *
 * y = polevl( x, coef, N );
 *
 *
 *
 * DESCRIPTION:
 *
 * Evaluates polynomial of degree N:
 *
 *                     2          N
 * y  =  C  + C x + C x  +...+ C x
 *        0    1     2          N
 *
 * Coefficients are stored in reverse order:
 *
 * coef[0] = C  , ..., coef[N] = C  .
 *            N                   0
 *
 *  The function p1evl() assumes that coef[N] = 1.0 and is
 * omitted from the array.  Its calling arguments are
 * otherwise the same as polevl().
 *
 *
 * SPEED:
 *
 * In the interest of speed, there are no checks for out
 * of bounds arithmetic.  This routine is used by most of
 * the functions in the library.  Depending on available
 * equipment features, the user may wish to rewrite the
 * program in microcode or assembly language.
 *
 */

/*
Cephes Math Library Release 2.1:  December, 1988
Copyright 1984, 1987, 1988 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/
static double
polevl(double x, const double coef[], int N)
{
    double          ans;
    int             i;
    const double    *p;

    p = coef;
    ans = *p++;
    i = N;

    do
        ans = ans * x + *p++;
    while (--i);

    return (ans);
}

/*                                          N
 * Evaluate polynomial when coefficient of x  is 1.0.
 * Otherwise same as polevl.
 */
static double
p1evl(double x, const double coef[], int N)
{
    double		ans;
    const double	*p;
    int		 	i;

    p = coef;
    ans = x + *p++;
    i = N - 1;

    do
        ans = ans * x + *p++;
    while (--i);

    return (ans);
}

#ifndef GAMMA

/* Provide GAMMA function for those who do not already have one */

int             sgngam;

static int
ISNAN(double x)
{
    volatile double a = x;

    if (a != a)
        return 1;
    return 0;
}

static int
ISFINITE(double x)
{
    volatile double a = x;

    if (a < DBL_MAX)
        return 1;
    return 0;
}

double
lngamma(double x)
{
    /* A[]: Stirling's formula expansion of log gamma
     * B[], C[]: log gamma function between 2 and 3
     */
#ifdef UNK
    static const double   A[] = {
	8.11614167470508450300E-4,
	-5.95061904284301438324E-4,
	7.93650340457716943945E-4,
	-2.77777777730099687205E-3,
	8.33333333333331927722E-2
    };
    static const double   B[] = {
	-1.37825152569120859100E3,
	-3.88016315134637840924E4,
	-3.31612992738871184744E5,
	-1.16237097492762307383E6,
	-1.72173700820839662146E6,
	-8.53555664245765465627E5
    };
    static const double   C[] = {
	/* 1.00000000000000000000E0, */
	-3.51815701436523470549E2,
	-1.70642106651881159223E4,
	-2.20528590553854454839E5,
	-1.13933444367982507207E6,
	-2.53252307177582951285E6,
	-2.01889141433532773231E6
    };
    /* log( sqrt( 2*pi ) ) */
    static const double   LS2PI = 0.91893853320467274178;
#define MAXLGM 2.556348e305
#endif /* UNK */

#ifdef DEC
    static const unsigned short A[] = {
	0035524, 0141201, 0034633, 0031405,
	0135433, 0176755, 0126007, 0045030,
	0035520, 0006371, 0003342, 0172730,
	0136066, 0005540, 0132605, 0026407,
	0037252, 0125252, 0125252, 0125132
    };
    static const unsigned short B[] = {
	0142654, 0044014, 0077633, 0035410,
	0144027, 0110641, 0125335, 0144760,
	0144641, 0165637, 0142204, 0047447,
	0145215, 0162027, 0146246, 0155211,
	0145322, 0026110, 0010317, 0110130,
	0145120, 0061472, 0120300, 0025363
    };
    static const unsigned short C[] = {
	/*0040200,0000000,0000000,0000000*/
	0142257, 0164150, 0163630, 0112622,
	0143605, 0050153, 0156116, 0135272,
	0144527, 0056045, 0145642, 0062332,
	0145213, 0012063, 0106250, 0001025,
	0145432, 0111254, 0044577, 0115142,
	0145366, 0071133, 0050217, 0005122
    };
    /* log( sqrt( 2*pi ) ) */
    static const unsigned short LS2P[] = {040153, 037616, 041445, 0172645,};
#define LS2PI *(double *)LS2P
#define MAXLGM 2.035093e36
#endif /* DEC */

#ifdef IBMPC
    static const unsigned short A[] = {
	0x6661, 0x2733, 0x9850, 0x3f4a,
	0xe943, 0xb580, 0x7fbd, 0xbf43,
	0x5ebb, 0x20dc, 0x019f, 0x3f4a,
	0xa5a1, 0x16b0, 0xc16c, 0xbf66,
	0x554b, 0x5555, 0x5555, 0x3fb5
    };
    static const unsigned short B[] = {
	0x6761, 0x8ff3, 0x8901, 0xc095,
	0xb93e, 0x355b, 0xf234, 0xc0e2,
	0x89e5, 0xf890, 0x3d73, 0xc114,
	0xdb51, 0xf994, 0xbc82, 0xc131,
	0xf20b, 0x0219, 0x4589, 0xc13a,
	0x055e, 0x5418, 0x0c67, 0xc12a
    };
    static const unsigned short C[] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0x12b2, 0x1cf3, 0xfd0d, 0xc075,
	0xd757, 0x7b89, 0xaa0d, 0xc0d0,
	0x4c9b, 0xb974, 0xeb84, 0xc10a,
	0x0043, 0x7195, 0x6286, 0xc131,
	0xf34c, 0x892f, 0x5255, 0xc143,
	0xe14a, 0x6a11, 0xce4b, 0xc13e
    };
    /* log( sqrt( 2*pi ) ) */
    static const unsigned short LS2P[] = {
	0xbeb5, 0xc864, 0x67f1, 0x3fed
    };
#define LS2PI *(double *)LS2P
#define MAXLGM 2.556348e305
#endif /* IBMPC */

#ifdef MIEEE
    static const unsigned short A[] = {
	0x3f4a, 0x9850, 0x2733, 0x6661,
	0xbf43, 0x7fbd, 0xb580, 0xe943,
	0x3f4a, 0x019f, 0x20dc, 0x5ebb,
	0xbf66, 0xc16c, 0x16b0, 0xa5a1,
	0x3fb5, 0x5555, 0x5555, 0x554b
    };
    static const unsigned short B[] = {
	0xc095, 0x8901, 0x8ff3, 0x6761,
	0xc0e2, 0xf234, 0x355b, 0xb93e,
	0xc114, 0x3d73, 0xf890, 0x89e5,
	0xc131, 0xbc82, 0xf994, 0xdb51,
	0xc13a, 0x4589, 0x0219, 0xf20b,
	0xc12a, 0x0c67, 0x5418, 0x055e
    };
    static const unsigned short C[] = {
	0xc075, 0xfd0d, 0x1cf3, 0x12b2,
	0xc0d0, 0xaa0d, 0x7b89, 0xd757,
	0xc10a, 0xeb84, 0xb974, 0x4c9b,
	0xc131, 0x6286, 0x7195, 0x0043,
	0xc143, 0x5255, 0x892f, 0xf34c,
	0xc13e, 0xce4b, 0x6a11, 0xe14a
    };
    /* log( sqrt( 2*pi ) ) */
    static const unsigned short LS2P[] = {
	0x3fed, 0x67f1, 0xc864, 0xbeb5
    };
#define LS2PI *(double *)LS2P
#define MAXLGM 2.556348e305
#endif /* MIEEE */

    static const double LOGPI = 1.1447298858494001741434273513530587116472948129153;

    double          p, q, u, w, z;
    int             i;

    sgngam = 1;
#ifdef NANS
    if (ISNAN(x))
        return (x);
#endif

#ifdef INFINITIES
    if (!ISFINITE((x)))
        return (DBL_MAX * DBL_MAX);
#endif

    if (x < -34.0) {
        q = -x;
        w = lngamma(q);            /* note this modifies sgngam! */
        p = floor(q);
        if (p == q) {
	lgsing:
#ifdef INFINITIES
            mtherr("lngamma", MTHERR_SING);
            return (DBL_MAX * DBL_MAX);
#else
            goto loverf;
#endif
        }
        i = p;
        if ((i & 1) == 0)
            sgngam = -1;
        else
            sgngam = 1;
        z = q - p;
        if (z > 0.5) {
            p += 1.0;
            z = p - q;
        }
        z = q * sin(PI * z);
        if (z == 0.0)
            goto lgsing;
	/*      z = log(PI) - log( z ) - w;*/
        z = LOGPI - log(z) - w;
        return (z);
    }
    if (x < 13.0) {
        z = 1.0;
        p = 0.0;
        u = x;
        while (u >= 3.0) {
            p -= 1.0;
            u = x + p;
            z *= u;
        }
        while (u < 2.0) {
            if (u == 0.0)
                goto lgsing;
            z /= u;
            p += 1.0;
            u = x + p;
        }
        if (z < 0.0) {
            sgngam = -1;
            z = -z;
        } else
            sgngam = 1;
        if (u == 2.0)
            return (log(z));
        p -= 2.0;
        x = x + p;
        p = x * polevl(x, B, 5) / p1evl(x, C, 6);
        return (log(z) + p);
    }
    if (x > MAXLGM) {
#ifdef INFINITIES
        return (sgngam * (DBL_MAX * DBL_MAX));
#else
    loverf:
        mtherr("lngamma", MTHERR_OVERFLOW);
        return (sgngam * MAXNUM);
#endif
    }
    q = (x - 0.5) * log(x) - x + LS2PI;
    if (x > 1.0e8)
        return (q);

    p = 1.0 / (x * x);
    if (x >= 1000.0)
        q += ((7.9365079365079365079365e-4 * p
               - 2.7777777777777777777778e-3) * p
              + 0.0833333333333333333333) / x;
    else
        q += polevl(p, A, 4) / x;
    return (q);
}

#define GAMMA(x) lngamma ((x))
/* HBB 20030816: must override name of sgngam so f_gamma() uses it */
#define signgam sgngam

#endif /* !GAMMA */

/*
 * Make all the following internal routines f_whatever() perform
 * autoconversion from string to numeric value.
 */
#define pop(x) pop_or_convert_from_string(x)

void
f_erf(union argument *arg)
{
    struct value a;
    double x;

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));
    x = erf(x);
    push(Gcomplex(&a, x, 0.0));
}

void
f_erfc(union argument *arg)
{
    struct value a;
    double x;

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));
    x = erfc(x);
    push(Gcomplex(&a, x, 0.0));
}

void
f_ibeta(union argument *arg)
{
    struct value a;
    double x;
    double arg1;
    double arg2;

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));
    arg2 = real(pop(&a));
    arg1 = real(pop(&a));

    x = ibeta(arg1, arg2, x);
    if (x == -1.0) {
	undefined = TRUE;
	push(Ginteger(&a, 0));
    } else
	push(Gcomplex(&a, x, 0.0));
}

void f_igamma(union argument *arg)
{
    struct value a;
    double x;
    double arg1;

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));
    arg1 = real(pop(&a));

    x = igamma(arg1, x);
    if (x == -1.0) {
	undefined = TRUE;
	push(Ginteger(&a, 0));
    } else
	push(Gcomplex(&a, x, 0.0));
}

void f_gamma(union argument *arg)
{
    double y;
    struct value a;

    (void) arg;				/* avoid -Wunused warning */
    y = GAMMA(real(pop(&a)));
    if (y > E_MAXEXP) {
	undefined = TRUE;
	push(Ginteger(&a, 0));
    } else
	push(Gcomplex(&a, signgam * gp_exp(y), 0.0));
}

void f_lgamma(union argument *arg)
{
    struct value a;

    (void) arg;				/* avoid -Wunused warning */
    push(Gcomplex(&a, GAMMA(real(pop(&a))), 0.0));
}

#ifndef BADRAND

void f_rand(union argument *arg)
{
    struct value a;

    (void) arg;				/* avoid -Wunused warning */
    push(Gcomplex(&a, ranf(pop(&a)), 0.0));
}

#else /* BADRAND */

/* Use only to observe the effect of a "bad" random number generator. */
void f_rand(union argument *arg)
{
    struct value a;

    (void) arg;				/* avoid -Wunused warning */
    static unsigned int y = 0;
    unsigned int maxran = 1000;

    (void) real(pop(&a));
    y = (781 * y + 387) % maxran;

    push(Gcomplex(&a, (double) y / maxran, 0.0));
}

#endif /* BADRAND */

/*
 * Fallback implementation of the Faddeeva/Voigt function 
 *	w(z) = exp(*-z^2) * erfc(-i*z)
 * if not available from libcerf or some other library
 */
#ifndef HAVE_LIBCERF
void
f_voigt(union argument *arg)
{
    struct value a;
    double x,y;
    (void) arg;				/* avoid -Wunused warning */
    y = real(pop(&a));
    x = real(pop(&a));
    push(Gcomplex(&a, humlik(x, y), 0.0));
}

/*
 * Calculate the Voigt/Faddeeva function with relative error less than 10^(-4).
 *     (see http://www.atm.ox.ac.uk/user/wells/voigt.html)
 *
 * K(x,y) = \frac{y}{\pi} \int{\frac{e^{-t^2}}{(x-t)^2+y^2}}dt
 *
 *  arguments:
 *	x, y - real and imaginary components of complex argument
 *  return value
 *	real value K(x,y)
 *
 * Algorithm: Josef Humlíček JQSRT 27 (1982) pp 437
 * Fortran program by J.R. Wells  JQSRT 62 (1999) pp 29-48.
 * Translated to C++ with f2c program and modified by Marcin Wojdyr
 * Minor adaptations from C++ to C by E. Stambulchik
 * Adapted for gnuplot by Tommaso Vinci
 */
static double humlik(double x, double y)
{

    const double c[6] = { 1.0117281,     -0.75197147,      0.012557727,
                         0.010022008,   -2.4206814e-4,    5.0084806e-7 };
    const double s[6] = { 1.393237,       0.23115241,     -0.15535147,
                         0.0062183662,   9.1908299e-5,   -6.2752596e-7 };
    const double t[6] = { 0.31424038,     0.94778839,      1.5976826,
                                2.2795071,      3.020637,        3.8897249 };

    const double rrtpi = 0.56418958; /* 1/SQRT(pi) */

    double a0, d0, d2, e0, e2, e4, h0, h2, h4, h6,
                 p0, p2, p4, p6, p8, z0, z2, z4, z6, z8;
    double mf[6], pf[6], mq[6], pq[6], xm[6], ym[6], xp[6], yp[6];
    bool rg1, rg2, rg3;
    double xlim0, xlim1, xlim2, xlim3, xlim4;
    double yq, yrrtpi;
    double abx, xq;
    double k;

    yq = y * y;
    yrrtpi = y * rrtpi;
    rg1 = true, rg2 = true, rg3 = true;
    abx = fabs(x);
    xq = abx * abx;

    if (y >= 70.55)
        return yrrtpi / (xq + yq);

    xlim0 = sqrt(y * (40. - y * 3.6) + 15100.);
    xlim1 = (y >= 8.425 ?  0. : sqrt(164. - y * (y * 1.8 + 4.3)));
    xlim2 = 6.8 - y;
    xlim3 = y * 2.4;
    xlim4 = y * 18.1 + 1.65;
    if (y <= 1e-6)
	xlim2 = xlim1 = xlim0;

    if (abx >= xlim0)                   /* Region 0 algorithm */
        return yrrtpi / (xq + yq);

    else if (abx >= xlim1) {            /* Humlicek W4 Region 1 */
        if (rg1) {                      /* First point in Region 1 */
            rg1 = false;
            a0 = yq + 0.5;              /* Region 1 y-dependents */
            d0 = a0 * a0;
            d2 = yq + yq - 1.;
        }
        return rrtpi / (d0 + xq * (d2 + xq)) * y * (a0 + xq);
    }

    else if (abx > xlim2) {             /* Humlicek W4 Region 2 */
        if (rg2) {                      /* First point in Region 2 */
            rg2 = false;
            /* Region 2 y-dependents */
            h0 = yq * (yq * (yq * (yq + 6.) + 10.5) + 4.5) + 0.5625;
            h2 = yq * (yq * (yq * 4. + 6.) + 9.) - 4.5;
            h4 = 10.5 - yq * (6. - yq * 6.);
            h6 = yq * 4. - 6.;
            e0 = yq * (yq * (yq + 5.5) + 8.25) + 1.875;
            e2 = yq * (yq * 3. + 1.) + 5.25;
            e4 = h6 * 0.75;
        }
        return rrtpi / (h0 + xq * (h2 + xq * (h4 + xq * (h6 + xq))))
                 * y * (e0 + xq * (e2 + xq * (e4 + xq)));
    }

    else if (abx < xlim3) {             /* Humlicek W4 Region 3 */
        if (rg3) {                      /* First point in Region 3 */
            rg3 = false;
            /* Region 3 y-dependents */
            z0 = y * (y * (y * (y * (y * (y * (y * (y * (y * (y
                    + 13.3988) + 88.26741) + 369.1989) + 1074.409)
                    + 2256.981) + 3447.629) + 3764.966) + 2802.87)
                    + 1280.829) + 272.1014;
            z2 = y * (y * (y * (y * (y * (y * (y * (y * 5.  + 53.59518)
                    + 266.2987) + 793.4273) + 1549.675) + 2037.31)
                    + 1758.336) + 902.3066) + 211.678;
            z4 = y * (y * (y * (y * (y * (y * 10. + 80.39278) + 269.2916)
                    + 479.2576) + 497.3014) + 308.1852) + 78.86585;
            z6 = y * (y * (y * (y * 10. + 53.59518) + 92.75679)
                    + 55.02933) + 22.03523;
            z8 = y * (y * 5. + 13.3988) + 1.49646;
            p0 = y * (y * (y * (y * (y * (y * (y * (y * (y * 0.3183291
                    + 4.264678) + 27.93941) + 115.3772) + 328.2151) +
                    662.8097) + 946.897) + 919.4955) + 549.3954)
                    + 153.5168;
            p2 = y * (y * (y * (y * (y * (y * (y * 1.2733163 + 12.79458)
                    + 56.81652) + 139.4665) + 189.773) + 124.5975)
                    - 1.322256) - 34.16955;
            p4 = y * (y * (y * (y * (y * 1.9099744 + 12.79568)
                    + 29.81482) + 24.01655) + 10.46332) + 2.584042;
            p6 = y * (y * (y * 1.273316 + 4.266322) + 0.9377051)
                    - 0.07272979;
            p8 = y * .3183291 + 5.480304e-4;
        }
        return 1.7724538 / (z0 + xq * (z2 + xq * (z4 + xq * (z6 +
                xq * (z8 + xq)))))
                  * (p0 + xq * (p2 + xq * (p4 + xq * (p6 + xq * p8))));
    }

    else {                              /* Humlicek CPF12 algorithm */
        double ypy0 = y + 1.5;
        double ypy0q = ypy0 * ypy0;
        int j;
        for (j = 0; j <= 5; ++j) {
            double d = x - t[j];
            mq[j] = d * d;
            mf[j] = 1. / (mq[j] + ypy0q);
            xm[j] = mf[j] * d;
            ym[j] = mf[j] * ypy0;
            d = x + t[j];
            pq[j] = d * d;
            pf[j] = 1. / (pq[j] + ypy0q);
            xp[j] = pf[j] * d;
            yp[j] = pf[j] * ypy0;
        }
        k = 0.;
        if (abx <= xlim4)               /* Humlicek CPF12 Region I */
            for (j = 0; j <= 5; ++j)
                k += c[j] * (ym[j]+yp[j]) - s[j] * (xm[j]-xp[j]);
        else {                          /* Humlicek CPF12 Region II */
            double yf = y + 3.;
            for (j = 0; j <= 5; ++j)
                k += (c[j] * (mq[j] * mf[j] - ym[j] * 1.5)
                         + s[j] * yf * xm[j]) / (mq[j] + 2.25)
                        + (c[j] * (pq[j] * pf[j] - yp[j] * 1.5)
                           - s[j] * yf * xp[j]) / (pq[j] + 2.25);
            k = y * k + exp(-xq);
        }
        return k;
    }
}
#endif /* libcerf not available */

/* ** ibeta.c
 *
 *   DESCRIBE  Approximate the incomplete beta function Ix(a, b).
 *
 *                           _
 *                          |(a + b)     /x  (a-1)         (b-1)
 *             Ix(a, b) = -_-------_--- * |  t     * (1 - t)     dt (a,b > 0)
 *                        |(a) * |(b)   /0
 *
 *
 *
 *   CALL      p = ibeta(a, b, x)
 *
 *             double    a    > 0
 *             double    b    > 0
 *             double    x    [0, 1]
 *
 *   WARNING   none
 *
 *   RETURN    double    p    [0, 1]
 *                            -1.0 on error condition
 *
 *   XREF      lngamma()
 *
 *   BUGS      This approximation is only accurate on the domain
 *             x < (a-1)/(a+b-2)
 *
 *   REFERENCE The continued fraction expansion as given by
 *             Abramowitz and Stegun (1964) is used.
 *
 * Copyright (c) 1992 Jos van der Woude, jvdwoude@hut.nl
 *
 * Note: this function was translated from the Public Domain Fortran
 *       version available from http://lib.stat.cmu.edu/apstat/xxx
 *
 */

static double
ibeta(double a, double b, double x)
{
    /* Test for admissibility of arguments */
    if (a <= 0.0 || b <= 0.0)
	return -1.0;
    if (x < 0.0 || x > 1.0)
	return -1.0;;

    /* If x equals 0 or 1, return x as prob */
    if (x == 0.0 || x == 1.0)
	return x;

    /* Swap a, b if necessary for more efficient evaluation */
    if (a < x * (a + b)) {
	double temp = confrac(b, a, 1.0 - x);
	return (temp < 0.0) ? temp : 1.0 - temp;
    } else {
	return confrac(a, b, x);
    }
}

static double
confrac(double a, double b, double x)
{
    double Alo = 0.0;
    double Ahi;
    double Aev;
    double Aod;
    double Blo = 1.0;
    double Bhi = 1.0;
    double Bod = 1.0;
    double Bev = 1.0;
    double f;
    double fold;
    double Apb = a + b;
    double d;
    int i;
    int j;

    /* Set up continued fraction expansion evaluation. */
    Ahi = gp_exp(GAMMA(Apb) + a * log(x) + b * log(1.0 - x) -
		 GAMMA(a + 1.0) - GAMMA(b));

    /*
     * Continued fraction loop begins here. Evaluation continues until
     * maximum iterations are exceeded, or convergence achieved.
     */
    for (i = 0, j = 1, f = Ahi; i <= ITMAX; i++, j++) {
	d = a + j + i;
	Aev = -(a + i) * (Apb + i) * x / d / (d - 1.0);
	Aod = j * (b - j) * x / d / (d + 1.0);
	Alo = Bev * Ahi + Aev * Alo;
	Blo = Bev * Bhi + Aev * Blo;
	Ahi = Bod * Alo + Aod * Ahi;
	Bhi = Bod * Blo + Aod * Bhi;

	if (fabs(Bhi) < MACHEPS)
	    Bhi = 0.0;

	if (Bhi != 0.0) {
	    fold = f;
	    f = Ahi / Bhi;
	    if (fabs(f - fold) < fabs(f) * MACHEPS)
		return f;
	}
    }

    return -1.0;
}

/* ** igamma.c
 *
 *   DESCRIBE  Approximate the incomplete gamma function P(a, x).
 *
 *                         1     /x  -t   (a-1)
 *             P(a, x) = -_--- * |  e  * t     dt      (a > 0)
 *                       |(a)   /0
 *
 *   CALL      p = igamma(a, x)
 *
 *             double    a    >  0
 *             double    x    >= 0
 *
 *   WARNING   none
 *
 *   RETURN    double    p    [0, 1]
 *                            -1.0 on error condition
 *
 *   XREF      lngamma()
 *
 *   BUGS      Values 0 <= x <= 1 may lead to inaccurate results.
 *
 *   REFERENCE ALGORITHM AS239  APPL. STATIST. (1988) VOL. 37, NO. 3
 *
 * Copyright (c) 1992 Jos van der Woude, jvdwoude@hut.nl
 *
 * Note: this function was translated from the Public Domain Fortran
 *       version available from http://lib.stat.cmu.edu/apstat/239
 *
 */

double
igamma(double a, double x)
{
    double arg;
    double aa;
    double an;
    double b;
    int i;

    /* Check that we have valid values for a and x */
    if (x < 0.0 || a <= 0.0)
	return -1.0;

    /* Deal with special cases */
    if (x == 0.0)
	return 0.0;
    if (x > XBIG)
	return 1.0;

    /* Check value of factor arg */
    arg = a * log(x) - x - GAMMA(a + 1.0);
    /* HBB 20031006: removed a spurious check here */
    arg = gp_exp(arg);

    /* Choose infinite series or continued fraction. */

    if ((x > 1.0) && (x >= a + 2.0)) {
	/* Use a continued fraction expansion */
	double pn1, pn2, pn3, pn4, pn5, pn6;
	double rn;
	double rnold;

	aa = 1.0 - a;
	b = aa + x + 1.0;
	pn1 = 1.0;
	pn2 = x;
	pn3 = x + 1.0;
	pn4 = x * b;
	rnold = pn3 / pn4;

	for (i = 1; i <= ITMAX; i++) {

	    aa++;
	    b += 2.0;
	    an = aa * (double) i;

	    pn5 = b * pn3 - an * pn1;
	    pn6 = b * pn4 - an * pn2;

	    if (pn6 != 0.0) {

		rn = pn5 / pn6;
		if (fabs(rnold - rn) <= GPMIN(MACHEPS, MACHEPS * rn))
		    return 1.0 - arg * rn * a;

		rnold = rn;
	    }
	    pn1 = pn3;
	    pn2 = pn4;
	    pn3 = pn5;
	    pn4 = pn6;

	    /* Re-scale terms in continued fraction if terms are large */
	    if (fabs(pn5) >= OFLOW) {

		pn1 /= OFLOW;
		pn2 /= OFLOW;
		pn3 /= OFLOW;
		pn4 /= OFLOW;
	    }
	}
    } else {
	/* Use Pearson's series expansion. */

	for (i = 0, aa = a, an = b = 1.0; i <= ITMAX; i++) {

	    aa++;
	    an *= x / aa;
	    b += an;
	    if (an < b * MACHEPS)
		return arg * b;
	}
    }
    return -1.0;
}


/* ----------------------------------------------------------------
    Cummulative distribution function of the ChiSquare distribution
   ---------------------------------------------------------------- */
double
chisq_cdf(int dof, double chisqr)
{
    if (dof <= 0)
	return not_a_number();
    if (chisqr <= 0.)
	return 0;
    return igamma(0.5 * dof, 0.5 * chisqr);
}


/***********************************************************************
     double ranf(double init)
                Random number generator as a Function
     Returns a random floating point number from a uniform distribution
     over 0 - 1 (endpoints of this interval are not returned) using a
     large integer generator.
     This is a transcription from Pascal to Fortran of routine
     Uniform_01 from the paper
     L'Ecuyer, P. and Cote, S. "Implementing a Random Number Package
     with Splitting Facilities." ACM Transactions on Mathematical
     Software, 17:98-111 (1991)

               Generate Large Integer
     Returns a random integer following a uniform distribution over
     (1, 2147483562) using the generator.
     This is a transcription from Pascal to Fortran of routine
     Random from the paper
     L'Ecuyer, P. and Cote, S. "Implementing a Random Number Package
     with Splitting Facilities." ACM Transactions on Mathematical
     Software, 17:98-111 (1991)
***********************************************************************/
static double
ranf(struct value *init)
{
    long k, z;
    static int firsttime = 1;
    static long seed1, seed2;
    static const long Xm1 = 2147483563L;
    static const long Xm2 = 2147483399L;
    static const long Xa1 = 40014L;
    static const long Xa2 = 40692L;

    /* Seed values must be integer, but check for both values equal zero
       before casting for speed */
    if (real(init) != 0.0 || imag(init) != 0.0) {

	/* Construct new seed values from input parameter */
	long seed1cvrt = real(init);
	long seed2cvrt = imag(init);
	if ( real(init) != (double)seed1cvrt ||
	     imag(init) != (double)seed2cvrt ||
	     seed1cvrt > 017777777777L ||
	     seed2cvrt > 017777777777L ||
	     (seed1cvrt <= 0 && seed2cvrt != 0) ||
	     seed2cvrt < 0 )
	    int_error(NO_CARET,"Illegal seed value");
	else if (seed1cvrt < 0)
	    firsttime = 1;
	else {
	    seed1 = seed1cvrt;
	    seed2 = (seed2cvrt) ? seed2cvrt : seed1cvrt;
	    firsttime = 0;
	}
    }

    /* (Re)-Initialize seeds if necessary */
    if (firsttime) {
	firsttime = 0;
	seed1 = 1234567890L;
	seed2 = 1234567890L;
    }

    FPRINTF((stderr,"ranf: seed = %lo %lo        %ld %ld\n", seed1, seed2));

    /* Generate pseudo random integers, which always end up positive */
    k = seed1 / 53668L;
    seed1 = Xa1 * (seed1 - k * 53668L) - k * 12211;
    if (seed1 < 0)
	seed1 += Xm1;
    k = seed2 / 52774L;
    seed2 = Xa2 * (seed2 - k * 52774L) - k * 3791;
    if (seed2 < 0)
	seed2 += Xm2;
    z = seed1 - seed2;
    if (z < 1)
	z += (Xm1 - 1);

    /*
     * 4.656613057E-10 is 1/Xm1.  Xm1 is set at the top of this file and is
     * currently 2147483563. If Xm1 changes, change this also.
     */
    return (double) 4.656613057E-10 *z;
}

/* ----------------------------------------------------------------
   Following to specfun.c made by John Grosh (jgrosh@arl.mil)
   on 28 OCT 1992.
   ---------------------------------------------------------------- */

void
f_normal(union argument *arg)
{				/* Normal or Gaussian Probability Function */
    struct value a;
    double x;

    /* ref. Abramowitz and Stegun 1964, "Handbook of Mathematical
       Functions", Applied Mathematics Series, vol 55,
       Chapter 26, page 934, Eqn. 26.2.29 and Jos van der Woude
       code found above */

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));

    x = 0.5 * SQRT_TWO * x;
    x = 0.5 * erfc(-x);		/* by using erfc instead of erf, we
				   can get accurate values for -38 <
				   arg < -8 */
    push(Gcomplex(&a, x, 0.0));
}

void
f_inverse_normal(union argument *arg)
{				/* Inverse normal distribution function */
    struct value a;
    double x;

    (void) arg;			/* avoid -Wunused warning */
    x = real(pop(&a));

    if (x <= 0.0 || x >= 1.0) {
	undefined = TRUE;
	push(Gcomplex(&a, 0.0, 0.0));
    } else {
	push(Gcomplex(&a, inverse_normal_func(x), 0.0));
    }
}


void
f_inverse_erf(union argument *arg)
{				/* Inverse error function */
    struct value a;
    double x;

    (void) arg;				/* avoid -Wunused warning */
    x = real(pop(&a));

    if (fabs(x) >= 1.0) {
	undefined = TRUE;
	push(Gcomplex(&a, 0.0, 0.0));
    } else {
	push(Gcomplex(&a, inverse_error_func(x), 0.0));
    }
}

/*                                                      ndtri.c
 *
 *      Inverse of Normal distribution function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, ndtri();
 *
 * x = ndtri( y );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the argument, x, for which the area under the
 * Gaussian probability density function (integrated from
 * minus infinity to x) is equal to y.
 *
 *
 * For small arguments 0 < y < exp(-2), the program computes
 * z = sqrt( -2.0 * log(y) );  then the approximation is
 * x = z - log(z)/z  - (1/z) P(1/z) / Q(1/z).
 * There are two rational functions P/Q, one for 0 < y < exp(-32)
 * and the other for y up to exp(-2).  For larger arguments,
 * w = y - 0.5, and  x/sqrt(2pi) = w + w**3 R(w**2)/S(w**2)).
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain        # trials      peak         rms
 *    DEC      0.125, 1         5500       9.5e-17     2.1e-17
 *    DEC      6e-39, 0.135     3500       5.7e-17     1.3e-17
 *    IEEE     0.125, 1        20000       7.2e-16     1.3e-16
 *    IEEE     3e-308, 0.135   50000       4.6e-16     9.8e-17
 *
 *
 * ERROR MESSAGES:
 *
 *   message         condition    value returned
 * ndtri domain       x <= 0        -DBL_MAX
 * ndtri domain       x >= 1         DBL_MAX
 *
 */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier
*/

#ifdef UNK
/* sqrt(2pi) */
static double   s2pi = 2.50662827463100050242E0;
#endif

#ifdef DEC
static unsigned short s2p[] = {0040440, 0066230, 0177661, 0034055};
#define s2pi *(double *)s2p
#endif

#ifdef IBMPC
static unsigned short s2p[] = {0x2706, 0x1ff6, 0x0d93, 0x4004};
#define s2pi *(double *)s2p
#endif

#ifdef MIEEE
static unsigned short s2p[] = {
    0x4004, 0x0d93, 0x1ff6, 0x2706
};
#define s2pi *(double *)s2p
#endif

static double
inverse_normal_func(double y0)
{
    /* approximation for 0 <= |y - 0.5| <= 3/8 */
#ifdef UNK
    static const double   P0[5] = {
	-5.99633501014107895267E1,
	9.80010754185999661536E1,
	-5.66762857469070293439E1,
	1.39312609387279679503E1,
	-1.23916583867381258016E0,
    };
    static const double   Q0[8] = {
	/* 1.00000000000000000000E0,*/
	1.95448858338141759834E0,
	4.67627912898881538453E0,
	8.63602421390890590575E1,
	-2.25462687854119370527E2,
	2.00260212380060660359E2,
	-8.20372256168333339912E1,
	1.59056225126211695515E1,
	-1.18331621121330003142E0,
    };
#endif
#ifdef DEC
    static const unsigned short P0[20] = {
	0141557, 0155170, 0071360, 0120550,
	0041704, 0000214, 0172417, 0067307,
	0141542, 0132204, 0040066, 0156723,
	0041136, 0163161, 0157276, 0007747,
	0140236, 0116374, 0073666, 0051764,
    };
    static const unsigned short Q0[32] = {
	/*0040200,0000000,0000000,0000000,*/
	0040372, 0026256, 0110403, 0123707,
	0040625, 0122024, 0020277, 0026661,
	0041654, 0134161, 0124134, 0007244,
	0142141, 0073162, 0133021, 0131371,
	0042110, 0041235, 0043516, 0057767,
	0141644, 0011417, 0036155, 0137305,
	0041176, 0076556, 0004043, 0125430,
	0140227, 0073347, 0152776, 0067251,
    };
#endif
#ifdef IBMPC
    static const unsigned short P0[20] = {
	0x142d, 0x0e5e, 0xfb4f, 0xc04d,
	0xedd9, 0x9ea1, 0x8011, 0x4058,
	0xdbba, 0x8806, 0x5690, 0xc04c,
	0xc1fd, 0x3bd7, 0xdcce, 0x402b,
	0xca7e, 0x8ef6, 0xd39f, 0xbff3,
    };
    static const unsigned short Q0[36] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0x74f9, 0xd220, 0x4595, 0x3fff,
	0xe5b6, 0x8417, 0xb482, 0x4012,
	0x81d4, 0x350b, 0x970e, 0x4055,
	0x365f, 0x56c2, 0x2ece, 0xc06c,
	0xcbff, 0xa8e9, 0x0853, 0x4069,
	0xb7d9, 0xe78d, 0x8261, 0xc054,
	0x7563, 0xc104, 0xcfad, 0x402f,
	0xcdd5, 0xfabf, 0xeedc, 0xbff2,
    };
#endif
#ifdef MIEEE
    static const unsigned short P0[20] = {
	0xc04d, 0xfb4f, 0x0e5e, 0x142d,
	0x4058, 0x8011, 0x9ea1, 0xedd9,
	0xc04c, 0x5690, 0x8806, 0xdbba,
	0x402b, 0xdcce, 0x3bd7, 0xc1fd,
	0xbff3, 0xd39f, 0x8ef6, 0xca7e,
    };
    static const unsigned short Q0[32] = {
	/*0x3ff0,0x0000,0x0000,0x0000,*/
	0x3fff, 0x4595, 0xd220, 0x74f9,
	0x4012, 0xb482, 0x8417, 0xe5b6,
	0x4055, 0x970e, 0x350b, 0x81d4,
	0xc06c, 0x2ece, 0x56c2, 0x365f,
	0x4069, 0x0853, 0xa8e9, 0xcbff,
	0xc054, 0x8261, 0xe78d, 0xb7d9,
	0x402f, 0xcfad, 0xc104, 0x7563,
	0xbff2, 0xeedc, 0xfabf, 0xcdd5,
    };
#endif

    /* Approximation for interval z = sqrt(-2 log y ) between 2 and 8
     * i.e., y between exp(-2) = .135 and exp(-32) = 1.27e-14.
     */
#ifdef UNK
    static const double   P1[9] = {
	4.05544892305962419923E0,
	3.15251094599893866154E1,
	5.71628192246421288162E1,
	4.40805073893200834700E1,
	1.46849561928858024014E1,
	2.18663306850790267539E0,
	-1.40256079171354495875E-1,
	-3.50424626827848203418E-2,
	-8.57456785154685413611E-4,
    };
    static const double   Q1[8] = {
	/*  1.00000000000000000000E0,*/
	1.57799883256466749731E1,
	4.53907635128879210584E1,
	4.13172038254672030440E1,
	1.50425385692907503408E1,
	2.50464946208309415979E0,
	-1.42182922854787788574E-1,
	-3.80806407691578277194E-2,
	-9.33259480895457427372E-4,
    };
#endif
#ifdef DEC
    static const unsigned short P1[36] = {
	0040601, 0143074, 0150744, 0073326,
	0041374, 0031554, 0113253, 0146016,
	0041544, 0123272, 0012463, 0176771,
	0041460, 0051160, 0103560, 0156511,
	0041152, 0172624, 0117772, 0030755,
	0040413, 0170713, 0151545, 0176413,
	0137417, 0117512, 0022154, 0131671,
	0137017, 0104257, 0071432, 0007072,
	0135540, 0143363, 0063137, 0036166,
    };
    static const unsigned short Q1[32] = {
	/*0040200,0000000,0000000,0000000,*/
	0041174, 0075325, 0004736, 0120326,
	0041465, 0110044, 0047561, 0045567,
	0041445, 0042321, 0012142, 0030340,
	0041160, 0127074, 0166076, 0141051,
	0040440, 0046055, 0040745, 0150400,
	0137421, 0114146, 0067330, 0010621,
	0137033, 0175162, 0025555, 0114351,
	0135564, 0122773, 0145750, 0030357,
    };
#endif
#ifdef IBMPC
    static const unsigned short P1[36] = {
	0x8edb, 0x9a3c, 0x38c7, 0x4010,
	0x7982, 0x92d5, 0x866d, 0x403f,
	0x7fbf, 0x42a6, 0x94d7, 0x404c,
	0x1ba9, 0x10ee, 0x0a4e, 0x4046,
	0x463e, 0x93ff, 0x5eb2, 0x402d,
	0xbfa1, 0x7a6c, 0x7e39, 0x4001,
	0x9677, 0x448d, 0xf3e9, 0xbfc1,
	0x41c7, 0xee63, 0xf115, 0xbfa1,
	0xe78f, 0x6ccb, 0x18de, 0xbf4c,
    };
    static const unsigned short Q1[32] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0xd41b, 0xa13b, 0x8f5a, 0x402f,
	0x296f, 0x89ee, 0xb204, 0x4046,
	0x461c, 0x228c, 0xa89a, 0x4044,
	0xd845, 0x9d87, 0x15c7, 0x402e,
	0xba20, 0xa83c, 0x0985, 0x4004,
	0x0232, 0xcddb, 0x330c, 0xbfc2,
	0xb31d, 0x456d, 0x7f4e, 0xbfa3,
	0x061e, 0x797d, 0x94bf, 0xbf4e,
    };
#endif
#ifdef MIEEE
    static const unsigned short P1[36] = {
	0x4010, 0x38c7, 0x9a3c, 0x8edb,
	0x403f, 0x866d, 0x92d5, 0x7982,
	0x404c, 0x94d7, 0x42a6, 0x7fbf,
	0x4046, 0x0a4e, 0x10ee, 0x1ba9,
	0x402d, 0x5eb2, 0x93ff, 0x463e,
	0x4001, 0x7e39, 0x7a6c, 0xbfa1,
	0xbfc1, 0xf3e9, 0x448d, 0x9677,
	0xbfa1, 0xf115, 0xee63, 0x41c7,
	0xbf4c, 0x18de, 0x6ccb, 0xe78f,
    };
    static const unsigned short Q1[32] = {
	/*0x3ff0,0x0000,0x0000,0x0000,*/
	0x402f, 0x8f5a, 0xa13b, 0xd41b,
	0x4046, 0xb204, 0x89ee, 0x296f,
	0x4044, 0xa89a, 0x228c, 0x461c,
	0x402e, 0x15c7, 0x9d87, 0xd845,
	0x4004, 0x0985, 0xa83c, 0xba20,
	0xbfc2, 0x330c, 0xcddb, 0x0232,
	0xbfa3, 0x7f4e, 0x456d, 0xb31d,
	0xbf4e, 0x94bf, 0x797d, 0x061e,
    };
#endif

    /* Approximation for interval z = sqrt(-2 log y ) between 8 and 64
     * i.e., y between exp(-32) = 1.27e-14 and exp(-2048) = 3.67e-890.
     */

#ifdef UNK
    static const double   P2[9] = {
	3.23774891776946035970E0,
	6.91522889068984211695E0,
	3.93881025292474443415E0,
	1.33303460815807542389E0,
	2.01485389549179081538E-1,
	1.23716634817820021358E-2,
	3.01581553508235416007E-4,
	2.65806974686737550832E-6,
	6.23974539184983293730E-9,
    };
    static const double   Q2[8] = {
	/*  1.00000000000000000000E0,*/
	6.02427039364742014255E0,
	3.67983563856160859403E0,
	1.37702099489081330271E0,
	2.16236993594496635890E-1,
	1.34204006088543189037E-2,
	3.28014464682127739104E-4,
	2.89247864745380683936E-6,
	6.79019408009981274425E-9,
    };
#endif
#ifdef DEC
    static const unsigned short P2[36] = {
	0040517, 0033507, 0036236, 0125641,
	0040735, 0044616, 0014473, 0140133,
	0040574, 0012567, 0114535, 0102541,
	0040252, 0120340, 0143474, 0150135,
	0037516, 0051057, 0115361, 0031211,
	0036512, 0131204, 0101511, 0125144,
	0035236, 0016627, 0043160, 0140216,
	0033462, 0060512, 0060141, 0010641,
	0031326, 0062541, 0101304, 0077706,
    };
    static const unsigned short Q2[32] = {
	/*0040200,0000000,0000000,0000000,*/
	0040700, 0143322, 0132137, 0040501,
	0040553, 0101155, 0053221, 0140257,
	0040260, 0041071, 0052573, 0010004,
	0037535, 0066472, 0177261, 0162330,
	0036533, 0160475, 0066666, 0036132,
	0035253, 0174533, 0027771, 0044027,
	0033502, 0016147, 0117666, 0063671,
	0031351, 0047455, 0141663, 0054751,
    };
#endif
#ifdef IBMPC
    static const unsigned short P2[36] = {
	0xd574, 0xe793, 0xe6e8, 0x4009,
	0x780b, 0xc327, 0xa931, 0x401b,
	0xb0ac, 0xf32b, 0x82ae, 0x400f,
	0x9a0c, 0x18e7, 0x541c, 0x3ff5,
	0x2651, 0xf35e, 0xca45, 0x3fc9,
	0x354d, 0x9069, 0x5650, 0x3f89,
	0x1812, 0xe8ce, 0xc3b2, 0x3f33,
	0x2234, 0x4c0c, 0x4c29, 0x3ec6,
	0x8ff9, 0x3058, 0xccac, 0x3e3a,
    };
    static const unsigned short Q2[32] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0xe828, 0x568b, 0x18da, 0x4018,
	0x3816, 0xaad2, 0x704d, 0x400d,
	0x6200, 0x2aaf, 0x0847, 0x3ff6,
	0x3c9b, 0x5fd6, 0xada7, 0x3fcb,
	0xc78b, 0xadb6, 0x7c27, 0x3f8b,
	0x2903, 0x65ff, 0x7f2b, 0x3f35,
	0xccf7, 0xf3f6, 0x438c, 0x3ec8,
	0x6b3d, 0xb876, 0x29e5, 0x3e3d,
    };
#endif
#ifdef MIEEE
    static const unsigned short P2[36] = {
	0x4009, 0xe6e8, 0xe793, 0xd574,
	0x401b, 0xa931, 0xc327, 0x780b,
	0x400f, 0x82ae, 0xf32b, 0xb0ac,
	0x3ff5, 0x541c, 0x18e7, 0x9a0c,
	0x3fc9, 0xca45, 0xf35e, 0x2651,
	0x3f89, 0x5650, 0x9069, 0x354d,
	0x3f33, 0xc3b2, 0xe8ce, 0x1812,
	0x3ec6, 0x4c29, 0x4c0c, 0x2234,
	0x3e3a, 0xccac, 0x3058, 0x8ff9,
    };
    static const unsigned short Q2[32] = {
	/*0x3ff0,0x0000,0x0000,0x0000,*/
	0x4018, 0x18da, 0x568b, 0xe828,
	0x400d, 0x704d, 0xaad2, 0x3816,
	0x3ff6, 0x0847, 0x2aaf, 0x6200,
	0x3fcb, 0xada7, 0x5fd6, 0x3c9b,
	0x3f8b, 0x7c27, 0xadb6, 0xc78b,
	0x3f35, 0x7f2b, 0x65ff, 0x2903,
	0x3ec8, 0x438c, 0xf3f6, 0xccf7,
	0x3e3d, 0x29e5, 0xb876, 0x6b3d,
    };
#endif

    double          x, y, z, y2, x0, x1;
    int             code;

    if (y0 <= 0.0) {
        mtherr("inverse_normal_func", MTHERR_DOMAIN);
        return (-DBL_MAX);
    }
    if (y0 >= 1.0) {
        mtherr("inverse_normal_func", MTHERR_DOMAIN);
        return (DBL_MAX);
    }
    code = 1;
    y = y0;
    if (y > (1.0 - 0.13533528323661269189)) {   /* 0.135... = exp(-2) */
        y = 1.0 - y;
        code = 0;
    }
    if (y > 0.13533528323661269189) {
        y = y - 0.5;
        y2 = y * y;
        x = y + y * (y2 * polevl(y2, P0, 4) / p1evl(y2, Q0, 8));
        x = x * s2pi;
        return (x);
    }
    x = sqrt(-2.0 * log(y));
    x0 = x - log(x) / x;

    z = 1.0 / x;
    if (x < 8.0)                /* y > exp(-32) = 1.2664165549e-14 */
        x1 = z * polevl(z, P1, 8) / p1evl(z, Q1, 8);
    else
        x1 = z * polevl(z, P2, 8) / p1evl(z, Q2, 8);
    x = x0 - x1;
    if (code != 0)
        x = -x;
    return (x);
}

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1988, 1992, 2000 by Stephen L. Moshier
*/

#ifndef HAVE_ERFC
/*                                                     erfc.c
 *
 *      Complementary error function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, erfc();
 *
 * y = erfc( x );
 *
 *
 *
 * DESCRIPTION:
 *
 *
 *  1 - erf(x) =
 *
 *                           inf.
 *                             -
 *                  2         | |          2
 *   erfc(x)  =  --------     |    exp( - t  ) dt
 *               sqrt(pi)   | |
 *                           -
 *                            x
 *
 *
 * For small x, erfc(x) = 1 - erf(x); otherwise rational
 * approximations are computed.
 *
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 9.2319   12000       5.1e-16     1.2e-16
 *    IEEE      0,26.6417   30000       5.7e-14     1.5e-14
 *
 *
 * ERROR MESSAGES:
 *
 *   message         condition              value returned
 * erfc underflow    x > 9.231948545 (DEC)       0.0
 *
 *
 */

static double
erfc(double a)
{
#ifdef UNK
    static const double   P[] = {
	2.46196981473530512524E-10,
	5.64189564831068821977E-1,
	7.46321056442269912687E0,
	4.86371970985681366614E1,
	1.96520832956077098242E2,
	5.26445194995477358631E2,
	9.34528527171957607540E2,
	1.02755188689515710272E3,
	5.57535335369399327526E2
    };
    static const double   Q[] = {
	/* 1.00000000000000000000E0,*/
	1.32281951154744992508E1,
	8.67072140885989742329E1,
	3.54937778887819891062E2,
	9.75708501743205489753E2,
	1.82390916687909736289E3,
	2.24633760818710981792E3,
	1.65666309194161350182E3,
	5.57535340817727675546E2
    };
    static const double   R[] = {
	5.64189583547755073984E-1,
	1.27536670759978104416E0,
	5.01905042251180477414E0,
	6.16021097993053585195E0,
	7.40974269950448939160E0,
	2.97886665372100240670E0
    };
    static const double   S[] = {
	/* 1.00000000000000000000E0,*/
	2.26052863220117276590E0,
	9.39603524938001434673E0,
	1.20489539808096656605E1,
	1.70814450747565897222E1,
	9.60896809063285878198E0,
	3.36907645100081516050E0
    };
#endif /* UNK */

#ifdef DEC
    static const unsigned short P[] = {
	0030207, 0054445, 0011173, 0021706,
	0040020, 0067272, 0030661, 0122075,
	0040756, 0151236, 0173053, 0067042,
	0041502, 0106175, 0062555, 0151457,
	0042104, 0102525, 0047401, 0003667,
	0042403, 0116176, 0011446, 0075303,
	0042551, 0120723, 0061641, 0123275,
	0042600, 0070651, 0007264, 0134516,
	0042413, 0061102, 0167507, 0176625
    };
    static const unsigned short Q[] = {
	/*0040200,0000000,0000000,0000000,*/
	0041123, 0123257, 0165741, 0017142,
	0041655, 0065027, 0173413, 0115450,
	0042261, 0074011, 0021573, 0004150,
	0042563, 0166530, 0013662, 0007200,
	0042743, 0176427, 0162443, 0105214,
	0043014, 0062546, 0153727, 0123772,
	0042717, 0012470, 0006227, 0067424,
	0042413, 0061103, 0003042, 0013254
    };
    static const unsigned short R[] = {
	0040020, 0067272, 0101024, 0155421,
	0040243, 0037467, 0056706, 0026462,
	0040640, 0116017, 0120665, 0034315,
	0040705, 0020162, 0143350, 0060137,
	0040755, 0016234, 0134304, 0130157,
	0040476, 0122700, 0051070, 0015473
    };
    static const unsigned short S[] = {
	/*0040200,0000000,0000000,0000000,*/
	0040420, 0126200, 0044276, 0070413,
	0041026, 0053051, 0007302, 0063746,
	0041100, 0144203, 0174051, 0061151,
	0041210, 0123314, 0126343, 0177646,
	0041031, 0137125, 0051431, 0033011,
	0040527, 0117362, 0152661, 0066201
    };
#endif /* DEC */

#ifdef IBMPC
    static const unsigned short P[] = {
	0x6479, 0xa24f, 0xeb24, 0x3df0,
	0x3488, 0x4636, 0x0dd7, 0x3fe2,
	0x6dc4, 0xdec5, 0xda53, 0x401d,
	0xba66, 0xacad, 0x518f, 0x4048,
	0x20f7, 0xa9e0, 0x90aa, 0x4068,
	0xcf58, 0xc264, 0x738f, 0x4080,
	0x34d8, 0x6c74, 0x343a, 0x408d,
	0x972a, 0x21d6, 0x0e35, 0x4090,
	0xffb3, 0x5de8, 0x6c48, 0x4081
    };
    static const unsigned short Q[] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0x23cc, 0xfd7c, 0x74d5, 0x402a,
	0x7365, 0xfee1, 0xad42, 0x4055,
	0x610d, 0x246f, 0x2f01, 0x4076,
	0x41d0, 0x02f6, 0x7dab, 0x408e,
	0x7151, 0xfca4, 0x7fa2, 0x409c,
	0xf4ff, 0xdafa, 0x8cac, 0x40a1,
	0xede2, 0x0192, 0xe2a7, 0x4099,
	0x42d6, 0x60c4, 0x6c48, 0x4081
    };
    static const unsigned short R[] = {
	0x9b62, 0x5042, 0x0dd7, 0x3fe2,
	0xc5a6, 0xebb8, 0x67e6, 0x3ff4,
	0xa71a, 0xf436, 0x1381, 0x4014,
	0x0c0c, 0x58dd, 0xa40e, 0x4018,
	0x960e, 0x9718, 0xa393, 0x401d,
	0x0367, 0x0a47, 0xd4b8, 0x4007
    };
    static const unsigned short S[] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0xce21, 0x0917, 0x1590, 0x4002,
	0x4cfd, 0x21d8, 0xcac5, 0x4022,
	0x2c4d, 0x7f05, 0x1910, 0x4028,
	0x7ff5, 0x959c, 0x14d9, 0x4031,
	0x26c1, 0xaa63, 0x37ca, 0x4023,
	0x2d90, 0x5ab6, 0xf3de, 0x400a
    };
#endif /* IBMPC */

#ifdef MIEEE
    static const unsigned short P[] = {
	0x3df0, 0xeb24, 0xa24f, 0x6479,
	0x3fe2, 0x0dd7, 0x4636, 0x3488,
	0x401d, 0xda53, 0xdec5, 0x6dc4,
	0x4048, 0x518f, 0xacad, 0xba66,
	0x4068, 0x90aa, 0xa9e0, 0x20f7,
	0x4080, 0x738f, 0xc264, 0xcf58,
	0x408d, 0x343a, 0x6c74, 0x34d8,
	0x4090, 0x0e35, 0x21d6, 0x972a,
	0x4081, 0x6c48, 0x5de8, 0xffb3
    };
    static const unsigned short Q[] = {
	0x402a, 0x74d5, 0xfd7c, 0x23cc,
	0x4055, 0xad42, 0xfee1, 0x7365,
	0x4076, 0x2f01, 0x246f, 0x610d,
	0x408e, 0x7dab, 0x02f6, 0x41d0,
	0x409c, 0x7fa2, 0xfca4, 0x7151,
	0x40a1, 0x8cac, 0xdafa, 0xf4ff,
	0x4099, 0xe2a7, 0x0192, 0xede2,
	0x4081, 0x6c48, 0x60c4, 0x42d6
    };
    static const unsigned short R[] = {
	0x3fe2, 0x0dd7, 0x5042, 0x9b62,
	0x3ff4, 0x67e6, 0xebb8, 0xc5a6,
	0x4014, 0x1381, 0xf436, 0xa71a,
	0x4018, 0xa40e, 0x58dd, 0x0c0c,
	0x401d, 0xa393, 0x9718, 0x960e,
	0x4007, 0xd4b8, 0x0a47, 0x0367
    };
    static const unsigned short S[] = {
	0x4002, 0x1590, 0x0917, 0xce21,
	0x4022, 0xcac5, 0x21d8, 0x4cfd,
	0x4028, 0x1910, 0x7f05, 0x2c4d,
	0x4031, 0x14d9, 0x959c, 0x7ff5,
	0x4023, 0x37ca, 0xaa63, 0x26c1,
	0x400a, 0xf3de, 0x5ab6, 0x2d90
    };
#endif /* MIEEE */

    double p, q, x, y, z;

    if (a < 0.0)
        x = -a;
    else
        x = a;

    if (x < 1.0)
        return (1.0 - erf(a));

    z = -a * a;

    if (z < DBL_MIN_10_EXP) {
    under:
        mtherr("erfc", MTHERR_UNDERFLOW);
        if (a < 0)
            return (2.0);
        else
            return (0.0);
    }
    z = exp(z);

    if (x < 8.0) {
        p = polevl(x, P, 8);
        q = p1evl(x, Q, 8);
    } else {
        p = polevl(x, R, 5);
        q = p1evl(x, S, 6);
    }
    y = (z * p) / q;

    if (a < 0)
        y = 2.0 - y;

    if (y == 0.0)
        goto under;

    return (y);
}
#endif /* !HAVE_ERFC */

#ifndef HAVE_ERF
/*                                                     erf.c
 *
 *      Error function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, erf();
 *
 * y = erf( x );
 *
 *
 *
 * DESCRIPTION:
 *
 * The integral is
 *
 *                           x
 *                            -
 *                 2         | |          2
 *   erf(x)  =  --------     |    exp( - t  ) dt.
 *              sqrt(pi)   | |
 *                          -
 *                           0
 *
 * The magnitude of x is limited to 9.231948545 for DEC
 * arithmetic; 1 or -1 is returned outside this range.
 *
 * For 0 <= |x| < 1, erf(x) = x * P4(x**2)/Q5(x**2); otherwise
 * erf(x) = 1 - erfc(x).
 *
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0,1         14000       4.7e-17     1.5e-17
 *    IEEE      0,1         30000       3.7e-16     1.0e-16
 *
 */

static double
erf(double x)
{

# ifdef UNK
    static const double   T[] = {
	9.60497373987051638749E0,
	9.00260197203842689217E1,
	2.23200534594684319226E3,
	7.00332514112805075473E3,
	5.55923013010394962768E4
    };
    static const double   U[] = {
	/* 1.00000000000000000000E0,*/
	3.35617141647503099647E1,
	5.21357949780152679795E2,
	4.59432382970980127987E3,
	2.26290000613890934246E4,
	4.92673942608635921086E4
    };
# endif

# ifdef DEC
    static const unsigned short T[] = {
	0041031, 0126770, 0170672, 0166101,
	0041664, 0006522, 0072360, 0031770,
	0043013, 0100025, 0162641, 0126671,
	0043332, 0155231, 0161627, 0076200,
	0044131, 0024115, 0021020, 0117343
    };
    static const unsigned short U[] = {
	/*0040200,0000000,0000000,0000000,*/
	0041406, 0037461, 0177575, 0032714,
	0042402, 0053350, 0123061, 0153557,
	0043217, 0111227, 0032007, 0164217,
	0043660, 0145000, 0004013, 0160114,
	0044100, 0071544, 0167107, 0125471
    };
# endif

# ifdef IBMPC
    static const unsigned short T[] = {
	0x5d88, 0x1e37, 0x35bf, 0x4023,
	0x067f, 0x4e9e, 0x81aa, 0x4056,
	0x35b7, 0xbcb4, 0x7002, 0x40a1,
	0xef90, 0x3c72, 0x5b53, 0x40bb,
	0x13dc, 0xa442, 0x2509, 0x40eb
    };
    static const unsigned short U[] = {
	/*0x0000,0x0000,0x0000,0x3ff0,*/
	0xa6ba, 0x3fef, 0xc7e6, 0x4040,
	0x3aee, 0x14c6, 0x4add, 0x4080,
	0xfd12, 0xe680, 0xf252, 0x40b1,
	0x7c0a, 0x0101, 0x1940, 0x40d6,
	0xf567, 0x9dc8, 0x0e6c, 0x40e8
    };
# endif

# ifdef MIEEE
    static const unsigned short T[] = {
	0x4023, 0x35bf, 0x1e37, 0x5d88,
	0x4056, 0x81aa, 0x4e9e, 0x067f,
	0x40a1, 0x7002, 0xbcb4, 0x35b7,
	0x40bb, 0x5b53, 0x3c72, 0xef90,
	0x40eb, 0x2509, 0xa442, 0x13dc
    };
    static const unsigned short U[] = {
	0x4040, 0xc7e6, 0x3fef, 0xa6ba,
	0x4080, 0x4add, 0x14c6, 0x3aee,
	0x40b1, 0xf252, 0xe680, 0xfd12,
	0x40d6, 0x1940, 0x0101, 0x7c0a,
	0x40e8, 0x0e6c, 0x9dc8, 0xf567
    };
# endif

    double y, z;

    if (fabs(x) > 1.0)
        return (1.0 - erfc(x));
    z = x * x;
    y = x * polevl(z, T, 4) / p1evl(z, U, 5);
    return (y);
}
#endif /* !HAVE_ERF */

/* ----------------------------------------------------------------
   Following function for the inverse error function is taken from
   NIST on 16. May 2002.
   Use Newton-Raphson correction also for range -1 to -y0 and
   add 3rd cycle to improve convergence -  E A Merritt 21.10.2003
   ----------------------------------------------------------------
 */

static double
inverse_error_func(double y)
{
    double x = 0.0;    /* The output */
    double z = 0.0;    /* Intermadiate variable */
    double y0 = 0.7;   /* Central range variable */

    /* Coefficients in rational approximations. */
    static const double a[4] = {
	0.886226899, -1.645349621, 0.914624893, -0.140543331
    };
    static const double b[4] = {
	-2.118377725, 1.442710462, -0.329097515, 0.012229801
    };
    static const double c[4] = {
	-1.970840454, -1.624906493, 3.429567803, 1.641345311
    };
    static const double d[2] = {
	3.543889200, 1.637067800
    };

    if ((y < -1.0) || (1.0 < y)) {
        printf("inverse_error_func: The value out of the range of the function");
        x = log(-1.0);
	return (x);
    } else if ((y == -1.0) || (1.0 == y)) {
        x = -y * log(0.0);
	return (x);
    } else if ((-1.0 < y) && (y < -y0)) {
        z = sqrt(-log((1.0 + y) / 2.0));
        x = -(((c[3] * z + c[2]) * z + c[1]) * z + c[0]) / ((d[1] * z + d[0]) * z + 1.0);
    } else {
        if ((-y0 <= y) && (y <= y0)) {
            z = y * y;
            x = y * (((a[3] * z + a[2]) * z + a[1]) * z + a[0]) /
                ((((b[3] * z + b[3]) * z + b[1]) * z + b[0]) * z + 1.0);
        } else if ((y0 < y) && (y < 1.0)) {
            z = sqrt(-log((1.0 - y) / 2.0));
            x = (((c[3] * z + c[2]) * z + c[1]) * z + c[0]) / ((d[1] * z + d[0]) * z + 1.0);
        }
    }
    /* Three steps of Newton-Raphson correction to full accuracy. OK - four */
    x = x - (erf(x) - y) / (2.0 / sqrt(PI) * gp_exp(-x * x));
    x = x - (erf(x) - y) / (2.0 / sqrt(PI) * gp_exp(-x * x));
    x = x - (erf(x) - y) / (2.0 / sqrt(PI) * gp_exp(-x * x));
    x = x - (erf(x) - y) / (2.0 / sqrt(PI) * gp_exp(-x * x));

    return (x);
}


/* Implementation of Lamberts W-function which is defined as
 * w(x)*e^(w(x))=x
 * Implementation by Gunter Kuhnle, gk@uni-leipzig.de
 * Algorithm originally developed by
 * KEITH BRIGGS, DEPARTMENT OF PLANT SCIENCES,
 * e-mail:kmb28@cam.ac.uk
 * http://epidem13.plantsci.cam.ac.uk/~kbriggs/W-ology.html */

static double
lambertw(double x)
{
    double p, e, t, w, eps;
    int i;

    eps = MACHEPS;

    if (x < -exp(-1))
	return -1;              /* error, value undefined */

    if (fabs(x) <= eps)
	return x;

    if (x < 1) {
	p = sqrt(2.0 * (exp(1.0) * x + 1.0));
	w = -1.0 + p - p * p / 3.0 + 11.0 / 72.0 * p * p * p;
    } else {
	w = log(x);
    }

    if (x > 3) {
	w = w - log(w);
    }
    for (i = 0; i < 20; i++) {
	e = gp_exp(w);
	t = w * e - x;
	t = t / (e * (w + 1.0) - 0.5 * (w + 2.0) * t / (w + 1.0));
	w = w - t;
	if (fabs(t) < eps * (1.0 + fabs(w)))
	    return w;
    }
    return -1;                 /* error: iteration didn't converge */
}

void
f_lambertw(union argument *arg)
{
    struct value a;
    double x;

    (void) arg;                        /* avoid -Wunused warning */
    x = real(pop(&a));

    x = lambertw(x);
    if (x <= -1)
	/* Error return from lambertw --> flag 'undefined' */
	undefined = TRUE;

    push(Gcomplex(&a, x, 0.0));
}

#if (0)	/* This approximation of the airy function saves 2200 bytes relative
		 * to the one from Cephes, but has low precision (2% relative error)
		 */
/* ------------------------------------------------------------
   Airy Function Ai(x)

   After:
   "Two-Point Quasi-Fractional Approximations to the Airy Function Ai(x)"
   by Pablo Martin, Ricardo Perez, Antonio L. Guerrero
   Journal of Computational Physics 99, 337-340 (1992)

   Beware of a misprint in equation (5) in this paper: The second term in
   parentheses must be multiplied by "x", as is clear from equation (3)
   and by comparison with equation (6). The implementation in this file
   uses the CORRECT formula (with the "x").

   This is not a very high accuracy approximation, but sufficient for
   plotting and similar applications. Higher accuracy formulas are
   available, but are much more complicated (typically requiring iteration).

   Added: janert (PKJ) 2009-09-05
   ------------------------------------------------------------ */

static double
airy_neg( double x ) {
  double z = sqrt( 0.37 + pow( fabs(x), 3.0 ) );
  double t = (2.0/3.0)*pow( fabs(x), 1.5 );
  double y = 0;

  y += ( -0.043883564 + 0.3989422*z )*cos(t)/pow( z, 7.0/6.0 );
  y += x*( -0.013883003 - 0.3989422*z )*sin(t)/( pow( z, 5.0/6.0 ) * 1.5 * t );

  return y;
}

static double
airy_pos( double x ) {
  double z = sqrt( 0.0425 + pow( fabs(x), 3.0 ) );
  double y = 0;

  y += (-0.002800908 + 0.326662423*z )/pow( z, 7.0/6.0 );
  y += x * ( -0.007232251 - 0.044567423*z )/pow( z, 11.0/6.0 );
  y *= exp(-(2.0/3.0)*z );

  return y;
}

void
f_airy(union argument *arg)
{
    struct value a;
    double x;

    (void) arg;                        /* avoid -Wunused warning */
    x = real(pop(&a));

    if( x < 0 ) {
      x = airy_neg(x);
    } else {
      x = airy_pos(x);
    }

    push(Gcomplex(&a, x, 0.0));
}
#else	/* Airy function from the Cephes library */
/*							airy.c
 *
 *	Airy function
 *
 * SYNOPSIS:
 *
 * double x, ai, aip, bi, bip;
 * int airy();
 *
 * airy( x, _&ai, _&aip, _&bi, _&bip );
 *
 *
 * DESCRIPTION:
 *
 * Solution of the differential equation
 *
 *	y"(x) = xy.
 *
 * The function returns the two independent solutions Ai, Bi
 * and their first derivatives Ai'(x), Bi'(x).
 *
 * Evaluation is by power series summation for small x,
 * by rational minimax approximations for large x.
 *
 *
 * ACCURACY:
 * Error criterion is absolute when function <= 1, relative
 * when function > 1, except * denotes relative error criterion.
 * For large negative x, the absolute error increases as x^1.5.
 * For large positive x, the relative error increases as x^1.5.
 *
 * Arithmetic  domain   function  # trials      peak         rms
 * IEEE        -10, 0     Ai        10000       1.6e-15     2.7e-16
 * IEEE          0, 10    Ai        10000       2.3e-14*    1.8e-15*
 * IEEE        -10, 0     Ai'       10000       4.6e-15     7.6e-16
 * IEEE          0, 10    Ai'       10000       1.8e-14*    1.5e-15*
 * IEEE        -10, 10    Bi        30000       4.2e-15     5.3e-16
 * IEEE        -10, 10    Bi'       30000       4.9e-15     7.3e-16
 * DEC         -10, 0     Ai         5000       1.7e-16     2.8e-17
 * DEC           0, 10    Ai         5000       2.1e-15*    1.7e-16*
 * DEC         -10, 0     Ai'        5000       4.7e-16     7.8e-17
 * DEC           0, 10    Ai'       12000       1.8e-15*    1.5e-16*
 * DEC         -10, 10    Bi        10000       5.5e-16     6.8e-17
 * DEC         -10, 10    Bi'        7000       5.3e-16     8.7e-17
 *
 */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1989, 2000 by Stephen L. Moshier
*/

static double c1 = 0.35502805388781723926;
static double c2 = 0.258819403792806798405;
static double sqrt3 = 1.732050807568877293527;
static double sqpii = 5.64189583547756286948E-1;

#ifdef UNK
#define MAXAIRY 25.77
#endif
#ifdef DEC
#define MAXAIRY 25.77
#endif
#ifdef IBMPC
#define MAXAIRY 103.892
#endif
#ifdef MIEEE
#define MAXAIRY 103.892
#endif


#ifdef UNK
static double AN[8] = {
  3.46538101525629032477E-1,
  1.20075952739645805542E1,
  7.62796053615234516538E1,
  1.68089224934630576269E2,
  1.59756391350164413639E2,
  7.05360906840444183113E1,
  1.40264691163389668864E1,
  9.99999999999999995305E-1,
};
static double AD[8] = {
  5.67594532638770212846E-1,
  1.47562562584847203173E1,
  8.45138970141474626562E1,
  1.77318088145400459522E2,
  1.64234692871529701831E2,
  7.14778400825575695274E1,
  1.40959135607834029598E1,
  1.00000000000000000470E0,
};
#endif
#ifdef DEC
static unsigned short AN[32] = {
0037661,0066561,0024675,0131301,
0041100,0017434,0034324,0101466,
0041630,0107450,0067427,0007430,
0042050,0013327,0071000,0034737,
0042037,0140642,0156417,0167366,
0041615,0011172,0075147,0051165,
0041140,0066152,0160520,0075146,
0040200,0000000,0000000,0000000,
};
static unsigned short AD[32] = {
0040021,0046740,0011422,0064606,
0041154,0014640,0024631,0062450,
0041651,0003435,0101152,0106401,
0042061,0050556,0034605,0136602,
0042044,0036024,0152377,0151414,
0041616,0172247,0072216,0115374,
0041141,0104334,0124154,0166007,
0040200,0000000,0000000,0000000,
};
#endif
#ifdef IBMPC
static unsigned short AN[32] = {
0xb658,0x2537,0x2dae,0x3fd6,
0x9067,0x871a,0x03e3,0x4028,
0xe1e3,0x0de2,0x11e5,0x4053,
0x073c,0xee40,0x02da,0x4065,
0xfddf,0x5ba1,0xf834,0x4063,
0xea4f,0x4f4c,0xa24f,0x4051,
0x0f4d,0x5c2a,0x0d8d,0x402c,
0x0000,0x0000,0x0000,0x3ff0,
};
static unsigned short AD[32] = {
0x4d31,0x0262,0x29bc,0x3fe2,
0x2ca5,0x0533,0x8334,0x402d,
0x51a0,0xb04d,0x20e3,0x4055,
0xb7b0,0xc730,0x2a2d,0x4066,
0xfa61,0x9a9f,0x8782,0x4064,
0xd35f,0xee91,0xde94,0x4051,
0x9d81,0x950d,0x311b,0x402c,
0x0000,0x0000,0x0000,0x3ff0,
};
#endif
#ifdef MIEEE
static unsigned short AN[32] = {
0x3fd6,0x2dae,0x2537,0xb658,
0x4028,0x03e3,0x871a,0x9067,
0x4053,0x11e5,0x0de2,0xe1e3,
0x4065,0x02da,0xee40,0x073c,
0x4063,0xf834,0x5ba1,0xfddf,
0x4051,0xa24f,0x4f4c,0xea4f,
0x402c,0x0d8d,0x5c2a,0x0f4d,
0x3ff0,0x0000,0x0000,0x0000,
};
static unsigned short AD[32] = {
0x3fe2,0x29bc,0x0262,0x4d31,
0x402d,0x8334,0x0533,0x2ca5,
0x4055,0x20e3,0xb04d,0x51a0,
0x4066,0x2a2d,0xc730,0xb7b0,
0x4064,0x8782,0x9a9f,0xfa61,
0x4051,0xde94,0xee91,0xd35f,
0x402c,0x311b,0x950d,0x9d81,
0x3ff0,0x0000,0x0000,0x0000,
};
#endif

#ifdef UNK
static double APN[8] = {
  6.13759184814035759225E-1,
  1.47454670787755323881E1,
  8.20584123476060982430E1,
  1.71184781360976385540E2,
  1.59317847137141783523E2,
  6.99778599330103016170E1,
  1.39470856980481566958E1,
  1.00000000000000000550E0,
};
static double APD[8] = {
  3.34203677749736953049E-1,
  1.11810297306158156705E1,
  7.11727352147859965283E1,
  1.58778084372838313640E2,
  1.53206427475809220834E2,
  6.86752304592780337944E1,
  1.38498634758259442477E1,
  9.99999999999999994502E-1,
};
#endif
#ifdef DEC
static unsigned short APN[32] = {
0040035,0017522,0065145,0054755,
0041153,0166556,0161471,0057174,
0041644,0016750,0034445,0046462,
0042053,0027515,0152316,0046717,
0042037,0050536,0067023,0023264,
0041613,0172252,0007240,0131055,
0041137,0023503,0052472,0002305,
0040200,0000000,0000000,0000000,
};
static unsigned short APD[32] = {
0037653,0016276,0112106,0126625,
0041062,0162577,0067111,0111761,
0041616,0054160,0140004,0137455,
0042036,0143460,0104626,0157206,
0042031,0032330,0067131,0114260,
0041611,0054667,0147207,0134564,
0041135,0114412,0070653,0146015,
0040200,0000000,0000000,0000000,
};
#endif
#ifdef IBMPC
static unsigned short APN[32] = {
0xab3e,0x4d4c,0xa3ea,0x3fe3,
0x2bcf,0xdc67,0x7dad,0x402d,
0xa9a6,0x0724,0x83bd,0x4054,
0xc9ba,0xba99,0x65e9,0x4065,
0x64d7,0xcdc2,0xea2b,0x4063,
0x1646,0x41d4,0x7e95,0x4051,
0x4099,0x6aa7,0xe4e8,0x402b,
0x0000,0x0000,0x0000,0x3ff0,
};
static unsigned short APD[32] = {
0xd5b3,0xd288,0x6397,0x3fd5,
0x327e,0xedc9,0x5caf,0x4026,
0x97e6,0x1800,0xcb0e,0x4051,
0xdbd1,0x1132,0xd8e6,0x4063,
0x3316,0x0dcb,0x269b,0x4063,
0xf72f,0xf9d0,0x2b36,0x4051,
0x7982,0x4e35,0xb321,0x402b,
0x0000,0x0000,0x0000,0x3ff0,
};
#endif
#ifdef MIEEE
static unsigned short APN[32] = {
0x3fe3,0xa3ea,0x4d4c,0xab3e,
0x402d,0x7dad,0xdc67,0x2bcf,
0x4054,0x83bd,0x0724,0xa9a6,
0x4065,0x65e9,0xba99,0xc9ba,
0x4063,0xea2b,0xcdc2,0x64d7,
0x4051,0x7e95,0x41d4,0x1646,
0x402b,0xe4e8,0x6aa7,0x4099,
0x3ff0,0x0000,0x0000,0x0000,
};
static unsigned short APD[32] = {
0x3fd5,0x6397,0xd288,0xd5b3,
0x4026,0x5caf,0xedc9,0x327e,
0x4051,0xcb0e,0x1800,0x97e6,
0x4063,0xd8e6,0x1132,0xdbd1,
0x4063,0x269b,0x0dcb,0x3316,
0x4051,0x2b36,0xf9d0,0xf72f,
0x402b,0xb321,0x4e35,0x7982,
0x3ff0,0x0000,0x0000,0x0000,
};
#endif

#ifdef UNK
static double BN16[5] = {
-2.53240795869364152689E-1,
 5.75285167332467384228E-1,
-3.29907036873225371650E-1,
 6.44404068948199951727E-2,
-3.82519546641336734394E-3,
};
static double BD16[5] = {
/* 1.00000000000000000000E0,*/
-7.15685095054035237902E0,
 1.06039580715664694291E1,
-5.23246636471251500874E0,
 9.57395864378383833152E-1,
-5.50828147163549611107E-2,
};
#endif
#ifdef DEC
static unsigned short BN16[20] = {
0137601,0124307,0010213,0035210,
0040023,0042743,0101621,0016031,
0137650,0164623,0036056,0074511,
0037203,0174525,0000473,0142474,
0136172,0130041,0066726,0064324,
};
static unsigned short BD16[20] = {
/*0040200,0000000,0000000,0000000,*/
0140745,0002354,0044335,0055276,
0041051,0124717,0170130,0104013,
0140647,0070135,0046473,0103501,
0040165,0013745,0033324,0127766,
0137141,0117204,0076164,0033107,
};
#endif
#ifdef IBMPC
static unsigned short BN16[20] = {
0x6751,0xe211,0x3518,0xbfd0,
0x2383,0x7072,0x68bc,0x3fe2,
0xcf29,0x6785,0x1d32,0xbfd5,
0x78a8,0xa027,0x7f2a,0x3fb0,
0xcd1b,0x2dba,0x5604,0xbf6f,
};
static unsigned short BD16[20] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0xab58,0x891b,0xa09d,0xc01c,
0x1101,0xfe0b,0x3539,0x4025,
0x70e8,0xa9a7,0xee0b,0xc014,
0x95ff,0xa6da,0xa2fc,0x3fee,
0x86c9,0x8f8e,0x33d0,0xbfac,
};
#endif
#ifdef MIEEE
static unsigned short BN16[20] = {
0xbfd0,0x3518,0xe211,0x6751,
0x3fe2,0x68bc,0x7072,0x2383,
0xbfd5,0x1d32,0x6785,0xcf29,
0x3fb0,0x7f2a,0xa027,0x78a8,
0xbf6f,0x5604,0x2dba,0xcd1b,
};
static unsigned short BD16[20] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0xc01c,0xa09d,0x891b,0xab58,
0x4025,0x3539,0xfe0b,0x1101,
0xc014,0xee0b,0xa9a7,0x70e8,
0x3fee,0xa2fc,0xa6da,0x95ff,
0xbfac,0x33d0,0x8f8e,0x86c9,
};
#endif

#ifdef UNK
static double BPPN[5] = {
 4.65461162774651610328E-1,
-1.08992173800493920734E0,
 6.38800117371827987759E-1,
-1.26844349553102907034E-1,
 7.62487844342109852105E-3,
};
static double BPPD[5] = {
/* 1.00000000000000000000E0,*/
-8.70622787633159124240E0,
 1.38993162704553213172E1,
-7.14116144616431159572E0,
 1.34008595960680518666E0,
-7.84273211323341930448E-2,
};
#endif
#ifdef DEC
static unsigned short BPPN[20] = {
0037756,0050354,0167531,0135731,
0140213,0101216,0032767,0020375,
0040043,0104147,0106312,0177632,
0137401,0161574,0032015,0043714,
0036371,0155035,0143165,0142262,
};
static unsigned short BPPD[20] = {
/*0040200,0000000,0000000,0000000,*/
0141013,0046265,0115005,0161053,
0041136,0061631,0072445,0156131,
0140744,0102145,0001127,0065304,
0040253,0103757,0146453,0102513,
0137240,0117200,0155402,0113500,
};
#endif
#ifdef IBMPC
static unsigned short BPPN[20] = {
0x377b,0x9deb,0xca1d,0x3fdd,
0xe420,0xc6be,0x7051,0xbff1,
0x5ff3,0xf199,0x710c,0x3fe4,
0xa8fa,0x8681,0x3c6f,0xbfc0,
0xb896,0xb8ce,0x3b43,0x3f7f,
};
static unsigned short BPPD[20] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0xbc45,0xb340,0x6996,0xc021,
0xbb8b,0x2ea4,0xcc73,0x402b,
0xed59,0xa04a,0x908c,0xc01c,
0x70a9,0xf9a5,0x70fd,0x3ff5,
0x52e8,0x1b60,0x13d0,0xbfb4,
};
#endif
#ifdef MIEEE
static unsigned short BPPN[20] = {
0x3fdd,0xca1d,0x9deb,0x377b,
0xbff1,0x7051,0xc6be,0xe420,
0x3fe4,0x710c,0xf199,0x5ff3,
0xbfc0,0x3c6f,0x8681,0xa8fa,
0x3f7f,0x3b43,0xb8ce,0xb896,
};
static unsigned short BPPD[20] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0xc021,0x6996,0xb340,0xbc45,
0x402b,0xcc73,0x2ea4,0xbb8b,
0xc01c,0x908c,0xa04a,0xed59,
0x3ff5,0x70fd,0xf9a5,0x70a9,
0xbfb4,0x13d0,0x1b60,0x52e8,
};
#endif

#ifdef UNK
static double AFN[9] = {
-1.31696323418331795333E-1,
-6.26456544431912369773E-1,
-6.93158036036933542233E-1,
-2.79779981545119124951E-1,
-4.91900132609500318020E-2,
-4.06265923594885404393E-3,
-1.59276496239262096340E-4,
-2.77649108155232920844E-6,
-1.67787698489114633780E-8,
};
static double AFD[9] = {
/* 1.00000000000000000000E0,*/
 1.33560420706553243746E1,
 3.26825032795224613948E1,
 2.67367040941499554804E1,
 9.18707402907259625840E0,
 1.47529146771666414581E0,
 1.15687173795188044134E-1,
 4.40291641615211203805E-3,
 7.54720348287414296618E-5,
 4.51850092970580378464E-7,
};
#endif
#ifdef DEC
static unsigned short AFN[36] = {
0137406,0155546,0124127,0033732,
0140040,0057564,0141263,0041222,
0140061,0071316,0013674,0175754,
0137617,0037522,0056637,0120130,
0137111,0075567,0121755,0166122,
0136205,0020016,0043317,0002201,
0135047,0001565,0075130,0002334,
0133472,0051700,0165021,0131551,
0131620,0020347,0132165,0013215,
};
static unsigned short AFD[36] = {
/*0040200,0000000,0000000,0000000,*/
0041125,0131131,0025627,0067623,
0041402,0135342,0021703,0154315,
0041325,0162305,0016671,0120175,
0041022,0177101,0053114,0141632,
0040274,0153131,0147364,0114306,
0037354,0166545,0120042,0150530,
0036220,0043127,0000727,0130273,
0034636,0043275,0075667,0034733,
0032762,0112715,0146250,0142474,
};
#endif
#ifdef IBMPC
static unsigned short AFN[36] = {
0xe6fb,0xd50a,0xdb6c,0xbfc0,
0x6852,0x9856,0x0bee,0xbfe4,
0x9f7d,0xc2f7,0x2e59,0xbfe6,
0xf40b,0x4bb3,0xe7ea,0xbfd1,
0xbd8a,0xf47d,0x2f6e,0xbfa9,
0xe090,0xc8d9,0xa401,0xbf70,
0x009c,0xaf4b,0xe06e,0xbf24,
0x366d,0x1d42,0x4a78,0xbec7,
0xa2d2,0xf68e,0x041c,0xbe52,
};
static unsigned short AFD[36] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0xedf2,0x2572,0xb64b,0x402a,
0x7b1a,0x4478,0x575c,0x4040,
0x3410,0xa3b7,0xbc98,0x403a,
0x9873,0x2ac9,0x5fc8,0x4022,
0x9319,0x39de,0x9acb,0x3ff7,
0x5a2b,0xb404,0x9dac,0x3fbd,
0xf617,0xe03a,0x08ca,0x3f72,
0xe73b,0xaf76,0xc8d7,0x3f13,
0x18a7,0xb995,0x52b9,0x3e9e,
};
#endif
#ifdef MIEEE
static unsigned short AFN[36] = {
0xbfc0,0xdb6c,0xd50a,0xe6fb,
0xbfe4,0x0bee,0x9856,0x6852,
0xbfe6,0x2e59,0xc2f7,0x9f7d,
0xbfd1,0xe7ea,0x4bb3,0xf40b,
0xbfa9,0x2f6e,0xf47d,0xbd8a,
0xbf70,0xa401,0xc8d9,0xe090,
0xbf24,0xe06e,0xaf4b,0x009c,
0xbec7,0x4a78,0x1d42,0x366d,
0xbe52,0x041c,0xf68e,0xa2d2,
};
static unsigned short AFD[36] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0x402a,0xb64b,0x2572,0xedf2,
0x4040,0x575c,0x4478,0x7b1a,
0x403a,0xbc98,0xa3b7,0x3410,
0x4022,0x5fc8,0x2ac9,0x9873,
0x3ff7,0x9acb,0x39de,0x9319,
0x3fbd,0x9dac,0xb404,0x5a2b,
0x3f72,0x08ca,0xe03a,0xf617,
0x3f13,0xc8d7,0xaf76,0xe73b,
0x3e9e,0x52b9,0xb995,0x18a7,
};
#endif

#ifdef UNK
static double AGN[11] = {
  1.97339932091685679179E-2,
  3.91103029615688277255E-1,
  1.06579897599595591108E0,
  9.39169229816650230044E-1,
  3.51465656105547619242E-1,
  6.33888919628925490927E-2,
  5.85804113048388458567E-3,
  2.82851600836737019778E-4,
  6.98793669997260967291E-6,
  8.11789239554389293311E-8,
  3.41551784765923618484E-10,
};
static double AGD[10] = {
/*  1.00000000000000000000E0,*/
  9.30892908077441974853E0,
  1.98352928718312140417E1,
  1.55646628932864612953E1,
  5.47686069422975497931E0,
  9.54293611618961883998E-1,
  8.64580826352392193095E-2,
  4.12656523824222607191E-3,
  1.01259085116509135510E-4,
  1.17166733214413521882E-6,
  4.91834570062930015649E-9,
};
#endif
#ifdef DEC
static unsigned short AGN[44] = {
0036641,0124456,0167175,0157354,
0037710,0037250,0001441,0136671,
0040210,0066031,0150401,0123532,
0040160,0066545,0003570,0153133,
0037663,0171516,0072507,0170345,
0037201,0151011,0007510,0045702,
0036277,0172317,0104572,0101030,
0035224,0045663,0000160,0136422,
0033752,0074753,0047702,0135160,
0032256,0052225,0156550,0107103,
0030273,0142443,0166277,0071720,
};
static unsigned short AGD[40] = {
/*0040200,0000000,0000000,0000000,*/
0041024,0170537,0117253,0055003,
0041236,0127256,0003570,0143240,
0041171,0004333,0172476,0160645,
0040657,0041161,0055716,0157161,
0040164,0046226,0006257,0063431,
0037261,0010357,0065445,0047563,
0036207,0034043,0057434,0116732,
0034724,0055416,0130035,0026377,
0033235,0041056,0154071,0023502,
0031250,0177071,0167254,0047242,
};
#endif
#ifdef IBMPC
static unsigned short AGN[44] = {
0xbbde,0xddcf,0x3525,0x3f94,
0x37b7,0x0064,0x07d5,0x3fd9,
0x34eb,0x3a20,0x0d83,0x3ff1,
0x1acb,0xa0ef,0x0dac,0x3fee,
0xfe1d,0xcea8,0x7e69,0x3fd6,
0x0978,0x21e9,0x3a41,0x3fb0,
0x5043,0xf12f,0xfe99,0x3f77,
0x17a2,0x600e,0x8976,0x3f32,
0x574e,0x69f8,0x4f3d,0x3edd,
0x11c8,0xbbad,0xca92,0x3e75,
0xee7a,0x7d97,0x78a4,0x3df7,
};
static unsigned short AGD[40] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0x6b40,0xf3d5,0x9e2b,0x4022,
0x18d4,0xc0ef,0xd5d5,0x4033,
0xdc35,0x7ea7,0x211b,0x402f,
0xdbce,0x2b79,0xe84e,0x4015,
0xece3,0xc195,0x8992,0x3fee,
0xa9ee,0xed64,0x221d,0x3fb6,
0x93bb,0x6be3,0xe704,0x3f70,
0xa5a0,0xd603,0x8b61,0x3f1a,
0x24e8,0xdb07,0xa845,0x3eb3,
0x89d4,0x3dd5,0x1fc7,0x3e35,
};
#endif
#ifdef MIEEE
static unsigned short AGN[44] = {
0x3f94,0x3525,0xddcf,0xbbde,
0x3fd9,0x07d5,0x0064,0x37b7,
0x3ff1,0x0d83,0x3a20,0x34eb,
0x3fee,0x0dac,0xa0ef,0x1acb,
0x3fd6,0x7e69,0xcea8,0xfe1d,
0x3fb0,0x3a41,0x21e9,0x0978,
0x3f77,0xfe99,0xf12f,0x5043,
0x3f32,0x8976,0x600e,0x17a2,
0x3edd,0x4f3d,0x69f8,0x574e,
0x3e75,0xca92,0xbbad,0x11c8,
0x3df7,0x78a4,0x7d97,0xee7a,
};
static unsigned short AGD[40] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0x4022,0x9e2b,0xf3d5,0x6b40,
0x4033,0xd5d5,0xc0ef,0x18d4,
0x402f,0x211b,0x7ea7,0xdc35,
0x4015,0xe84e,0x2b79,0xdbce,
0x3fee,0x8992,0xc195,0xece3,
0x3fb6,0x221d,0xed64,0xa9ee,
0x3f70,0xe704,0x6be3,0x93bb,
0x3f1a,0x8b61,0xd603,0xa5a0,
0x3eb3,0xa845,0xdb07,0x24e8,
0x3e35,0x1fc7,0x3dd5,0x89d4,
};
#endif

#ifdef UNK
static double APFN[9] = {
  1.85365624022535566142E-1,
  8.86712188052584095637E-1,
  9.87391981747398547272E-1,
  4.01241082318003734092E-1,
  7.10304926289631174579E-2,
  5.90618657995661810071E-3,
  2.33051409401776799569E-4,
  4.08718778289035454598E-6,
  2.48379932900442457853E-8,
};
static double APFD[9] = {
/*  1.00000000000000000000E0,*/
  1.47345854687502542552E1,
  3.75423933435489594466E1,
  3.14657751203046424330E1,
  1.09969125207298778536E1,
  1.78885054766999417817E0,
  1.41733275753662636873E-1,
  5.44066067017226003627E-3,
  9.39421290654511171663E-5,
  5.65978713036027009243E-7,
};
#endif
#ifdef DEC
static unsigned short APFN[36] = {
0037475,0150174,0071752,0166651,
0040142,0177621,0164246,0101757,
0040174,0142670,0106760,0006573,
0037715,0067570,0116274,0022404,
0037221,0074157,0053341,0117207,
0036301,0104257,0015075,0004777,
0035164,0057502,0164034,0001313,
0033611,0022254,0176000,0112565,
0031725,0055523,0025153,0166057,
};
static unsigned short APFD[36] = {
/*0040200,0000000,0000000,0000000,*/
0041153,0140334,0130506,0061402,
0041426,0025551,0024440,0070611,
0041373,0134750,0047147,0176702,
0041057,0171532,0105430,0017674,
0040344,0174416,0001726,0047754,
0037421,0021207,0020167,0136264,
0036262,0043621,0151321,0124324,
0034705,0001313,0163733,0016407,
0033027,0166702,0150440,0170561,
};
#endif
#ifdef IBMPC
static unsigned short APFN[36] = {
0x5db5,0x8e7d,0xba0f,0x3fc7,
0xd07e,0x3d14,0x5ff2,0x3fec,
0x01af,0x11be,0x98b7,0x3fef,
0x84a1,0x1397,0xadef,0x3fd9,
0x33d1,0xeadc,0x2f0d,0x3fb2,
0xa140,0xe347,0x3115,0x3f78,
0x8059,0x5d03,0x8be8,0x3f2e,
0x12af,0x9f80,0x2495,0x3ed1,
0x7d86,0x654d,0xab6a,0x3e5a,
};
static unsigned short APFD[36] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0xcc60,0x9628,0x781b,0x402d,
0x0e31,0x2524,0xc56d,0x4042,
0xffb8,0x09cc,0x773d,0x403f,
0x03f7,0x5163,0xfe6b,0x4025,
0xc9fd,0xc07a,0x9f21,0x3ffc,
0xf796,0xe40e,0x2450,0x3fc2,
0x351a,0x3a5a,0x48f2,0x3f76,
0x63a1,0x7cfb,0xa059,0x3f18,
0x1e2e,0x5a24,0xfdb8,0x3ea2,
};
#endif
#ifdef MIEEE
static unsigned short APFN[36] = {
0x3fc7,0xba0f,0x8e7d,0x5db5,
0x3fec,0x5ff2,0x3d14,0xd07e,
0x3fef,0x98b7,0x11be,0x01af,
0x3fd9,0xadef,0x1397,0x84a1,
0x3fb2,0x2f0d,0xeadc,0x33d1,
0x3f78,0x3115,0xe347,0xa140,
0x3f2e,0x8be8,0x5d03,0x8059,
0x3ed1,0x2495,0x9f80,0x12af,
0x3e5a,0xab6a,0x654d,0x7d86,
};
static unsigned short APFD[36] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0x402d,0x781b,0x9628,0xcc60,
0x4042,0xc56d,0x2524,0x0e31,
0x403f,0x773d,0x09cc,0xffb8,
0x4025,0xfe6b,0x5163,0x03f7,
0x3ffc,0x9f21,0xc07a,0xc9fd,
0x3fc2,0x2450,0xe40e,0xf796,
0x3f76,0x48f2,0x3a5a,0x351a,
0x3f18,0xa059,0x7cfb,0x63a1,
0x3ea2,0xfdb8,0x5a24,0x1e2e,
};
#endif

#ifdef UNK
static double APGN[11] = {
-3.55615429033082288335E-2,
-6.37311518129435504426E-1,
-1.70856738884312371053E0,
-1.50221872117316635393E0,
-5.63606665822102676611E-1,
-1.02101031120216891789E-1,
-9.48396695961445269093E-3,
-4.60325307486780994357E-4,
-1.14300836484517375919E-5,
-1.33415518685547420648E-7,
-5.63803833958893494476E-10,
};
static double APGD[11] = {
/*  1.00000000000000000000E0,*/
  9.85865801696130355144E0,
  2.16401867356585941885E1,
  1.73130776389749389525E1,
  6.17872175280828766327E0,
  1.08848694396321495475E0,
  9.95005543440888479402E-2,
  4.78468199683886610842E-3,
  1.18159633322838625562E-4,
  1.37480673554219441465E-6,
  5.79912514929147598821E-9,
};
#endif
#ifdef DEC
static unsigned short APGN[44] = {
0137021,0124372,0176075,0075331,
0140043,0023330,0177672,0161655,
0140332,0131126,0010413,0171112,
0140300,0044263,0175560,0054070,
0140020,0044206,0142603,0073324,
0137321,0015130,0066144,0144033,
0136433,0061243,0175542,0103373,
0135361,0053721,0020441,0053203,
0134077,0141725,0160277,0130612,
0132417,0040372,0100363,0060200,
0130432,0175052,0171064,0034147,
};
static unsigned short APGD[40] = {
/*0040200,0000000,0000000,0000000,*/
0041035,0136420,0030124,0140220,
0041255,0017432,0034447,0162256,
0041212,0100456,0154544,0006321,
0040705,0134026,0127154,0123414,
0040213,0051612,0044470,0172607,
0037313,0143362,0053273,0157051,
0036234,0144322,0054536,0007264,
0034767,0146170,0054265,0170342,
0033270,0102777,0167362,0073631,
0031307,0040644,0167103,0021763,
};
#endif
#ifdef IBMPC
static unsigned short APGN[44] = {
0xaf5b,0x5f87,0x351f,0xbfa2,
0x5c76,0x1ff7,0x64db,0xbfe4,
0x7e49,0xc221,0x564a,0xbffb,
0x0b07,0x7f6e,0x0916,0xbff8,
0x6edb,0xd8b0,0x0910,0xbfe2,
0x9903,0x0d8c,0x234b,0xbfba,
0x50df,0x7f6c,0x6c54,0xbf83,
0x2ad0,0x2424,0x2afa,0xbf3e,
0xf631,0xbc17,0xf87a,0xbee7,
0x6c10,0x501e,0xe81f,0xbe81,
0x870d,0x5e46,0x5f45,0xbe03,
};
static unsigned short APGD[40] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0x9812,0x060a,0xb7a2,0x4023,
0xfc96,0x4724,0xa3e3,0x4035,
0x819a,0xdb2c,0x5025,0x4031,
0x94e2,0xd5cd,0xb702,0x4018,
0x1eb1,0x4927,0x6a71,0x3ff1,
0x7bc5,0x4ad7,0x78de,0x3fb9,
0xc1d7,0x4b2b,0x991a,0x3f73,
0xbe1c,0x0b16,0xf98f,0x3f1e,
0x4ef3,0xfdde,0x10bf,0x3eb7,
0x647e,0x9dc8,0xe834,0x3e38,
};
#endif
#ifdef MIEEE
static unsigned short APGN[44] = {
0xbfa2,0x351f,0x5f87,0xaf5b,
0xbfe4,0x64db,0x1ff7,0x5c76,
0xbffb,0x564a,0xc221,0x7e49,
0xbff8,0x0916,0x7f6e,0x0b07,
0xbfe2,0x0910,0xd8b0,0x6edb,
0xbfba,0x234b,0x0d8c,0x9903,
0xbf83,0x6c54,0x7f6c,0x50df,
0xbf3e,0x2afa,0x2424,0x2ad0,
0xbee7,0xf87a,0xbc17,0xf631,
0xbe81,0xe81f,0x501e,0x6c10,
0xbe03,0x5f45,0x5e46,0x870d,
};
static unsigned short APGD[40] = {
/*0x3ff0,0x0000,0x0000,0x0000,*/
0x4023,0xb7a2,0x060a,0x9812,
0x4035,0xa3e3,0x4724,0xfc96,
0x4031,0x5025,0xdb2c,0x819a,
0x4018,0xb702,0xd5cd,0x94e2,
0x3ff1,0x6a71,0x4927,0x1eb1,
0x3fb9,0x78de,0x4ad7,0x7bc5,
0x3f73,0x991a,0x4b2b,0xc1d7,
0x3f1e,0xf98f,0x0b16,0xbe1c,
0x3eb7,0x10bf,0xfdde,0x4ef3,
0x3e38,0xe834,0x9dc8,0x647e,
};
#endif

#ifdef ANSIPROT
extern double fabs ( double );
extern double exp ( double );
extern double sqrt ( double );
extern double polevl ( double, void *, int );
extern double p1evl ( double, void *, int );
extern double sin ( double );
extern double cos ( double );
#else
double fabs(), exp(), sqrt();
double polevl(), p1evl(), sin(), cos();
#endif

int 
airy( x, ai, aip, bi, bip )
double x, *ai, *aip, *bi, *bip;
{
    double z, zz, t, f, g, uf, ug, k, zeta, theta;
    int domflg;

    domflg = 0;
    if( x > MAXAIRY )
	{
	*ai = 0;
	*aip = 0;
	*bi = MAXNUM;
	*bip = MAXNUM;
	return(-1);
	}

    if( x < -2.09 )
	{
	domflg = 15;
	t = sqrt(-x);
	zeta = -2.0 * x * t / 3.0;
	t = sqrt(t);
	k = sqpii / t;
	z = 1.0/zeta;
	zz = z * z;
	uf = 1.0 + zz * polevl( zz, AFN, 8 ) / p1evl( zz, AFD, 9 );
	ug = z * polevl( zz, AGN, 10 ) / p1evl( zz, AGD, 10 );
	theta = zeta + 0.25 * PI;
	f = sin( theta );
	g = cos( theta );
	*ai = k * (f * uf - g * ug);
	*bi = k * (g * uf + f * ug);
	uf = 1.0 + zz * polevl( zz, APFN, 8 ) / p1evl( zz, APFD, 9 );
	ug = z * polevl( zz, APGN, 10 ) / p1evl( zz, APGD, 10 );
	k = sqpii * t;
	*aip = -k * (g * uf + f * ug);
	*bip = k * (f * uf - g * ug);
	return(0);
	}

    if( x >= 2.09 )	/* cbrt(9) */
	{
	domflg = 5;
	t = sqrt(x);
	zeta = 2.0 * x * t / 3.0;
	g = exp( zeta );
	t = sqrt(t);
	k = 2.0 * t * g;
	z = 1.0/zeta;
	f = polevl( z, AN, 7 ) / polevl( z, AD, 7 );
	*ai = sqpii * f / k;
	k = -0.5 * sqpii * t / g;
	f = polevl( z, APN, 7 ) / polevl( z, APD, 7 );
	*aip = f * k;

	if( x > 8.3203353 )	/* zeta > 16 */
		{
		f = z * polevl( z, BN16, 4 ) / p1evl( z, BD16, 5 );
		k = sqpii * g;
		*bi = k * (1.0 + f) / t;
		f = z * polevl( z, BPPN, 4 ) / p1evl( z, BPPD, 5 );
		*bip = k * t * (1.0 + f);
		return(0);
		}
	}

    f = 1.0;
    g = x;
    t = 1.0;
    uf = 1.0;
    ug = x;
    k = 1.0;
    z = x * x * x;
    while( t > MACHEP )
	{
	uf *= z;
	k += 1.0;
	uf /=k;
	ug *= z;
	k += 1.0;
	ug /=k;
	uf /=k;
	f += uf;
	k += 1.0;
	ug /=k;
	g += ug;
	t = fabs(uf/f);
	}
    uf = c1 * f;
    ug = c2 * g;
    if( (domflg & 1) == 0 )
	*ai = uf - ug;
    if( (domflg & 2) == 0 )
	*bi = sqrt3 * (uf + ug);

    /* the deriviative of ai */
    k = 4.0;
    uf = x * x/2.0;
    ug = z/3.0;
    f = uf;
    g = 1.0 + ug;
    uf /= 3.0;
    t = 1.0;

    while( t > MACHEP )
	{
	uf *= z;
	ug /=k;
	k += 1.0;
	ug *= z;
	uf /=k;
	f += uf;
	k += 1.0;
	ug /=k;
	uf /=k;
	g += ug;
	k += 1.0;
	t = fabs(ug/g);
	}

    uf = c1 * f;
    ug = c2 * g;
    if( (domflg & 4) == 0 )
	*aip = uf - ug;
    if( (domflg & 8) == 0 )
	*bip = sqrt3 * (uf + ug);
    return(0);
}

void
f_airy(union argument *arg)
{
    struct value a;
    double x;
    double ai, aip, bi, bip;

    (void) arg;                        /* avoid -Wunused warning */
    x = real(pop(&a));

    airy(x, &ai, &aip, &bi, &bip);

    push(Gcomplex(&a, ai, 0.0));
}

#endif	/* End choice of low- or high-precision airy function */

/* ** expint.c
 *
 *   DESCRIBE  Approximate the exponential integral function
 *
 *
 *                       /inf   -n    -zt
 *             E_n(z) =  |     t   * e    dt (n = 0, 1, 2, ...)
 *                       /1
 *
 *
 *   CALL      p = expint(n, z)
 *
 *             double    n    >= 0
 *             double    z    >= 0
 *               also: n must be an integer
 *                     either z > 0 or n > 1
 *
 *   WARNING   none
 *
 *   RETURN    double    p    > 0
 *                            -1.0 on error condition
 *
 *   REFERENCE Abramowitz and Stegun (1964)
 *
 * Copyright (c) 2010 James R. Van Zandt, jrvz@comcast.net
 */

static double
expint(double n, double z)
{
    double y; /* the answer */

    {
      /* Test for admissibility of arguments */
      double junk;
      if (n < 0 || z < 0 || modf(n,&junk))
	return -1.0;
      if (z == 0 && n < 2)
	return -1.0;
    }

    /* special cases */
    if (n == 0) return exp(-z)/z;
    if (z == 0) return 1/(n-1);

    /* for z=3, CF requires 36 terms and series requires 29 */

    if (z > 3)
      {	/* For large z, use continued fraction (Abramowitz & Stegun 5.1.22):
	   E_n(z) = exp(-z)(1/(z+n/(1+1/(z+(n+1)/1+2/(z+...)))))
	   The CF is valid and stable for z>0, and efficient for z>1 or so.  */
	double n0, n1, n2, n3, d0, d1, d2, d3, y_prev=1;
	int i;

	n0 = 0; n1 = 1;
	d0 = 1; d1 = z;

	for (i=0; i<333; i++)
	  { /* evaluate the CF "top down" using the recurrence
	       relations for the numerators and denominators of
	       successive convergents */
	    n2 = n0*(n+i) + n1;
	    d2 = d0*(n+i) + d1;
	    n3 = n1*(1+i) + n2*z;
	    d3 = d1*(1+i) + d2*z;
	    y = n3/d3;
	    if (y == y_prev) break;
	    y_prev = y;
	    n0 = n2; n1 = n3;
	    d0 = d2; d1 = d3;

	    /* Re-scale terms in continued fraction if terms are large */
	    if (d3 >= OFLOW) {

		n0 /= OFLOW;
		n1 /= OFLOW;
		d0 /= OFLOW;
		d1 /= OFLOW;
	    }
	  }
	y = exp(-z)*y;
      }

    else
      {	/* For small z, use series (Abramowitz & Stegun 5.1.12):
	   E_1(z) = -\gamma + \ln z +
	            \sum_{m=1}^\infty { (-z)^m \over (m) m! }
	   The series is valid for z>0, and efficient for z<4 or so.  */

	/* from Abramowitz & Stegun, Table 1.1 */
	double euler_constant = .577215664901532860606512;

	double y_prev = 0;
	double t, m;

	y = -euler_constant - log(z);
	t = 1;
	for (m = 1; m<333; m++)
	  {
	    t = -t*z/m;
	    y = y - t/m;
	    if (y == y_prev) break;
	    y_prev = y;
	  }

	/* For n > 1, use recurrence relation (Abramowitz & Stegun 5.1.14):
	   n E_{n+1}(z) + z E_n(z) = e^{-z}, n >= 1
	   The recurrence is unstable for increasing n and z>4 or so,
	   but okay for z<3.  */

	for (m=1; m<n; m++)
	  y=(exp(-z) - z*y)/m;
      }
    return y;
}

void
f_expint(union argument *arg)
{
    struct value a;
    double n, x;

    (void) arg;                        /* avoid -Wunused warning */
    x = real(pop(&a));
    n = real(pop(&a));

    x = expint(n, x);
    if (x <= -1)
	/* Error return from expint --> flag 'undefined' */
	undefined = TRUE;

    push(Gcomplex(&a, x, 0.0));
}

