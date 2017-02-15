/*
 * $Id: matrix.h,v 1.13 2014/03/03 04:09:30 sfeam Exp $
 */

/* GNUPLOT - matrix.h */

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
 *	Header file: public functions in matrix.c
 *
 *
 *	Previous copyright of this module:   Carsten Grammes, 1993
 *      Experimental Physics, University of Saarbruecken, Germany
 */


#ifndef MATRIX_H
#define MATRIX_H

#include "syscfg.h"

/******* public functions ******/

double  *vec __PROTO((int n));
int     *ivec __PROTO((int n));
double  **matr __PROTO((int r, int c));
void    free_matr __PROTO((double **m));
double  *redim_vec __PROTO((double **v, int n));
void    solve __PROTO((double **a, int n, double **b, int m));
void    Givens __PROTO((double **C, double *d, double *x, int N, int n));
void    Invert_RtR __PROTO((double **R, double **I, int n));

/* Functions for use by THIN_PLATE_SPLINES_GRID method */
void    lu_decomp __PROTO((double **, int, int *, double *));
void    lu_backsubst __PROTO((double **, int n, int *, double *));

double   enorm_vec(int n, const double *x);
double   sumsq_vec(int n, const double *x);

#endif /* MATRIX_H */
