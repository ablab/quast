#ifndef lint
static char *RCSid() { return RCSid("$Id: matrix.c,v 1.14 2014/03/03 04:09:30 sfeam Exp $"); }
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
 *      Matrix algebra, part of
 *
 *      Nonlinear least squares fit according to the
 *      Marquardt-Levenberg-algorithm
 *
 *      added as Patch to Gnuplot (v3.2 and higher)
 *      by Carsten Grammes
 *      Experimental Physics, University of Saarbruecken, Germany
 *
 *      Previous copyright of this module:   Carsten Grammes, 1993
 *
 */

#include "matrix.h"

#include "alloc.h"
#include "fit.h"
#include "util.h"

/*****************************************************************/

#define Swap(a,b)   {double _temp = (a); (a) = (b); (b) = _temp;}
/* HBB 20010424: unused: */
/* #define WINZIG	      1e-30 */


/*****************************************************************
    internal prototypes
*****************************************************************/

static GP_INLINE int fsign __PROTO((double x));

/*****************************************************************
    first straightforward vector and matrix allocation functions
*****************************************************************/

/* allocates a double vector with n elements */
double *
vec(int n)
{
    double *dp;

    if (n < 1)
	return NULL;
    dp = gp_alloc(n * sizeof(double), "vec");
    return dp;
}


/* allocates a double matrix */
double **
matr(int rows, int cols)
{
    int i;
    double **m;

    if (rows < 1 || cols < 1)
	return NULL;
    m = gp_alloc(rows * sizeof(m[0]), "matrix row pointers");
    m[0] = gp_alloc(rows * cols * sizeof(m[0][0]), "matrix elements");
    for (i = 1; i < rows; i++)
	m[i] = m[i - 1] + cols;
    return m;
}


void
free_matr(double **m)
{
    if (m != NULL) {
	free(m[0]);
	free(m);
    }
}


double *
redim_vec(double **v, int n)
{
    if (n < 1)
	*v = NULL;
    else
	*v = gp_realloc(*v, n * sizeof((*v)[0]), "vec");
    return *v;
}

/* HBB: TODO: is there a better value for 'epsilon'? how to specify
 * 'inline'?  is 'fsign' really not available elsewhere? use
 * row-oriented version (p. 309) instead?
 */

static GP_INLINE int
fsign(double x)
{
    return (x > 0 ? 1 : (x < 0) ? -1 : 0);
}

/*****************************************************************

     Solve least squares Problem C*x+d = r, |r| = min!, by Given rotations
     (QR-decomposition). Direct implementation of the algorithm
     presented in H.R.Schwarz: Numerische Mathematik, Equation 7.33

     If 'd == NULL', d is not accesed: the routine just computes the QR
     decomposition of C and exits.

*****************************************************************/

void
Givens(
    double **C,
    double *d,
    double *x,
    int N,
    int n)
{
    int i, j, k;
    double w, gamma, sigma, rho, temp;
    double epsilon = DBL_EPSILON;	/* FIXME (?) */

/*
 * First, construct QR decomposition of C, by 'rotating away'
 * all elements of C below the diagonal. The rotations are
 * stored in place as Givens coefficients rho.
 * Vector d is also rotated in this same turn, if it exists
 */
    for (j = 0; j < n; j++) {
	for (i = j + 1; i < N; i++) {
	    if (C[i][j]) {
		if (fabs(C[j][j]) < epsilon * fabs(C[i][j])) {
		    /* find the rotation parameters */
		    w = -C[i][j];
		    gamma = 0;
		    sigma = 1;
		    rho = 1;
		} else {
		    w = fsign(C[j][j]) * sqrt(C[j][j] * C[j][j] + C[i][j] * C[i][j]);
		    if (w == 0)
			Eex3("w = 0 in Givens();  Cjj = %g,  Cij = %g", C[j][j], C[i][j]);
		    gamma = C[j][j] / w;
		    sigma = -C[i][j] / w;
		    rho = (fabs(sigma) < gamma) ? sigma : fsign(sigma) / gamma;
		}
		C[j][j] = w;
		C[i][j] = rho;	/* store rho in place, for later use */
		for (k = j + 1; k < n; k++) {
		    /* rotation on index pair (i,j) */
		    temp = gamma * C[j][k] - sigma * C[i][k];
		    C[i][k] = sigma * C[j][k] + gamma * C[i][k];
		    C[j][k] = temp;

		}
		if (d) {	/* if no d vector given, don't use it */
		    temp = gamma * d[j] - sigma * d[i];		/* rotate d */
		    d[i] = sigma * d[j] + gamma * d[i];
		    d[j] = temp;
		}
	    }
	}
    }

    if (!d)			/* stop here if no d was specified */
	return;

    /* solve R*x+d = 0, by backsubstitution */
    for (i = n - 1; i >= 0; i--) {
	double s = d[i];

	for (k = i + 1; k < n; k++)
	    s += C[i][k] * x[k];
	if (C[i][i] == 0)
	    Eex("Singular matrix in Givens()");
	x[i] = -s / C[i][i];
    }
}


/* Given a triangular Matrix R, compute (R^T * R)^(-1), by forward
 * then back substitution
 *
 * R, I are n x n Matrices, I is for the result. Both must already be
 * allocated.
 *
 * Will only calculate the lower triangle of I, as it is symmetric
 */

void
Invert_RtR(double **R, double **I, int n)
{
    int i, j, k;

    /* fill in the I matrix, and check R for regularity : */

    for (i = 0; i < n; i++) {
	for (j = 0; j < i; j++)	/* upper triangle isn't needed */
	    I[i][j] = 0;
	I[i][i] = 1;
	if (!R[i][i])
	    Eex("Singular matrix in Invert_RtR");
    }

    /* Forward substitution: Solve R^T * B = I, store B in place of I */

    for (k = 0; k < n; k++) {
	for (i = k; i < n; i++) {	/* upper half needn't be computed */
	    double s = I[i][k];
	    for (j = k; j < i; j++)	/* for j<k, I[j][k] always stays zero! */
		s -= R[j][i] * I[j][k];
	    I[i][k] = s / R[i][i];
	}
    }
    /* Backward substitution: Solve R * A = B, store A in place of B */

    for (k = 0; k < n; k++) {
	for (i = n - 1; i >= k; i--) {	/* don't compute upper triangle of A */
	    double s = I[i][k];
	    for (j = i + 1; j < n; j++)
		s -= R[i][j] * I[j][k];
	    I[i][k] = s / R[i][i];
	}
    }
}

/* HBB 20010424: Functions that used to be here in matrix.c, but were
 * replaced by others and deleted, later.  But the
 * THIN_PLATE_SPLINES_GRID needed them, later, so they appeared in
 * plot3d.c, where they don't belong --> moved them back here. */

void
lu_decomp(double **a, int n, int *indx, double *d)
{
    int i, imax = -1, j, k;	/* HBB: added initial value, to shut up gcc -Wall */
    double large, dummy, temp, **ar, **lim, *limc, *ac, *dp, *vscal;

    dp = vscal = vec(n);
    *d = 1.0;
    for (ar = a, lim = &(a[n]); ar < lim; ar++) {
	large = 0.0;
	for (ac = *ar, limc = &(ac[n]); ac < limc;)
	    if ((temp = fabs(*ac++)) > large)
		large = temp;
	if (large == 0.0)
	    int_error(NO_CARET, "Singular matrix in LU-DECOMP");
	*dp++ = 1 / large;
    }
    ar = a;
    for (j = 0; j < n; j++, ar++) {
	for (i = 0; i < j; i++) {
	    ac = &(a[i][j]);
	    for (k = 0; k < i; k++)
		*ac -= a[i][k] * a[k][j];
	}
	large = 0.0;
	dp = &(vscal[j]);
	for (i = j; i < n; i++) {
	    ac = &(a[i][j]);
	    for (k = 0; k < j; k++)
		*ac -= a[i][k] * a[k][j];
	    if ((dummy = *dp++ * fabs(*ac)) >= large) {
		large = dummy;
		imax = i;
	    }
	}
	if (j != imax) {
	    ac = a[imax];
	    dp = *ar;
	    for (k = 0; k < n; k++, ac++, dp++)
		Swap(*ac, *dp);
	    *d = -(*d);
	    vscal[imax] = vscal[j];
	}
	indx[j] = imax;
	if (*(dp = &(*ar)[j]) == 0)
	    *dp = 1e-30;

	if (j != n - 1) {
	    dummy = 1 / (*ar)[j];
	    for (i = j + 1; i < n; i++)
		a[i][j] *= dummy;
	}
    }
    free(vscal);
}

void
lu_backsubst(double **a, int n, int *indx, double *b)
{
    int i, memi = -1, ip, j;

    double sum, *bp, *bip, **ar, *ac;

    ar = a;

    for (i = 0; i < n; i++, ar++) {
	ip = indx[i];
	sum = b[ip];
	b[ip] = b[i];
	if (memi >= 0) {
	    ac = &((*ar)[memi]);
	    bp = &(b[memi]);
	    for (j = memi; j <= i - 1; j++)
		sum -= *ac++ * *bp++;
	} else if (sum)
	    memi = i;
	b[i] = sum;
    }
    ar--;
    for (i = n - 1; i >= 0; i--) {
	ac = &(*ar)[i + 1];
	bp = &(b[i + 1]);
	bip = &(b[i]);
	for (j = i + 1; j < n; j++)
	    *bip -= *ac++ * *bp++;
	*bip /= (*ar--)[i];
    }
}


/*****************************************************************

    Sum up squared components of a vector
    In order to reduce rounding errors in summing up the entries
    of a vector, we employ the Neumaier variant of the Kahan and
    Babuska algorithm:
    A. Neumaier, Rundungsfehleranalyse einiger Verfahren zur
    Summation endlicher Summen, Z. angew. Math. Mechanik, 54:39-51, 1974

*****************************************************************/
double
sumsq_vec(int n, const double *x)
{
    int i;
    double s;
    double c = 0.0;

    if ((x == NULL) || (n == 0))
	return 0;

    s =  x[0] * x[0];
    for (i = 1; i < n; i++) {
	double xi = x[i] * x[i];
	double t  = s + xi;
	if (fabs(s) >= fabs(xi))
	    c += ((s - t) + xi);
	else
	    c += ((xi - t) + s);
	s = t;
    };
    s += c;
    return s;
}


/*****************************************************************

    Euclidean norm of a vector

*****************************************************************/
double
enorm_vec(int n, const double *x)
{
    return sqrt(sumsq_vec(n, x));
}
