#ifndef lint
static char *RCSid() { return RCSid("$Id: bf_test.c,v 1.12.2.1 2014/08/22 16:10:56 sfeam Exp $"); }
#endif


/*
 * This program creates some binary data files used by the demo
 * binary.dem to exercise gnuplot's binary input routines.
 * This code is not used by gnuplot itself.
 *
 * Copyright (c) 1992 Robert K. Cunningham, MIT Lincoln Laboratory
 *
 */

/*
 * Ethan A Merritt July 2014
 * Remove dependence on any gnuplot source files
 */
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#else
#  ifdef HAVE_MALLOC_H
#    include <malloc.h>
#  endif
#endif

/* replaces __PROTO() */
static float function (int p, double x, double y);

typedef struct {
  float xmin, xmax;
  float ymin, ymax;
} range;

#define NUM_PLOTS 2
static range TheRange[] = {{-3,3,-2,2},
			   {-3,3,-3,3},
			   {-3,3,-3,3}}; /* Sampling rate causes this to go from -3:6*/

static float
function(int p, double x, double y)
{
    float t = 0;			/* HBB 990828: initialize */

    switch (p) {
    case 0:
	t = 1.0 / (x * x + y * y + 1.0);
	break;
    case 1:
	t = sin(x * x + y * y) / (x * x + y * y);
	if (t > 1.0)
	    t = 1.0;
	break;
    case 2:
	t = sin(x * x + y * y) / (x * x + y * y);
	/* sinc modulated sinc */
	t *= sin(4. * (x * x + y * y)) / (4. * (x * x + y * y));
	if (t > 1.0)
	    t = 1.0;
	break;
    default:
	fprintf(stderr, "Unknown function\n");
	break;
    }
    return t;
}


int
fwrite_matrix( FILE *fout, float **m, int xsize, int ysize, float *rt, float *ct)
{
    int j;
    int status;
    float length = ysize;

    if ((status = fwrite((char *) &length, sizeof(float), 1, fout)) != 1) {
	fprintf(stderr, "fwrite 1 returned %d\n", status);
	return (0);
    }
    fwrite((char *) ct, sizeof(float), ysize, fout);
    for (j = 0; j < xsize; j++) {
	fwrite((char *) &rt[j], sizeof(float), 1, fout);
	fwrite((char *) (m[j]), sizeof(float), ysize, fout);
    }

    return (1);
}


#define ISOSAMPLES 5.0

int
main(void)
{
    int plot;
    int i, j;
    int im;
    float x, y;
    float *rt, *ct;
    float **m;
    int xsize, ysize;
    char buf[256];
    FILE *fout;
/*  Create a few standard test interfaces */

    for (plot = 0; plot < NUM_PLOTS; plot++) {
	xsize = (TheRange[plot].xmax - TheRange[plot].xmin) * ISOSAMPLES + 1;
	ysize = (TheRange[plot].ymax - TheRange[plot].ymin) * ISOSAMPLES + 1;

	rt = calloc(xsize, sizeof(float));
	ct = calloc(ysize, sizeof(float));
	m = calloc(xsize, sizeof(m[0]));
	for (im = 0; im < xsize; im++) {
		m[im] = calloc(ysize, sizeof(m[0]));
	}

	for (y = TheRange[plot].ymin, j = 0; j < ysize; j++, y += 1.0 / (double) ISOSAMPLES) {
	    ct[j] = y;
	}

	for (x = TheRange[plot].xmin, i = 0; i < xsize; i++, x += 1.0 / (double) ISOSAMPLES) {
	    rt[i] = x;
	    for (y = TheRange[plot].ymin, j = 0; j < ysize; j++, y += 1.0 / (double) ISOSAMPLES) {
		m[i][j] = function(plot, x, y);
	    }
	}

	sprintf(buf, "binary%d", plot + 1);
	if (!(fout = fopen(buf, "wb"))) {
	    fprintf(stderr, "Could not open output file\n");
	    return -1;
	} else {
	    fwrite_matrix(fout, m, xsize, ysize, rt, ct);
	}
	free(rt);
	free(ct);
	for (im = 0; im < xsize; im++)
	    free(m[im]);
	free(m);
    }

    /* Show that it's ok to vary sampling rate, as long as x1<x2, y1<y2... */
    xsize = (TheRange[plot].xmax - TheRange[plot].xmin) * ISOSAMPLES + 1;
    ysize = (TheRange[plot].ymax - TheRange[plot].ymin) * ISOSAMPLES + 1;

    rt = calloc(xsize, sizeof(float));
    ct = calloc(ysize, sizeof(float));
    m = calloc(xsize, sizeof(m[0]));
    for (im = 0; im < xsize; im++) {
	    m[im] = calloc(ysize, sizeof(m[0]));
    }

    for (y = TheRange[plot].ymin, j = 0; j < ysize; j++, y += 1.0 / (double) ISOSAMPLES) {
	ct[j] = y > 0 ? 2 * y : y;
    }
    for (x = TheRange[plot].xmin, i = 0; i < xsize; i++, x += 1.0 / (double) ISOSAMPLES) {
	rt[i] = x > 0 ? 2 * x : x;
	for (y = TheRange[plot].ymin, j = 0; j < ysize; j++, y += 1.0 / (double) ISOSAMPLES) {
	    m[i][j] = function(plot, x, y);
	}
    }

    sprintf(buf, "binary%d", plot + 1);
    if (!(fout = fopen(buf, "wb"))) {
	fprintf(stderr, "Could not open output file\n");
	return -1;
    } else {
	fwrite_matrix(fout, m, xsize, ysize, rt, ct);
    }
    free(rt);
    free(ct);
    for (im = 0; im < xsize; im++)
	free(m[im]);
    free(m);

    return 0;
}
