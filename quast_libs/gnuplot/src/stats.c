#ifndef lint
static char *RCSid() { return RCSid("$Id: stats.c,v 1.14.2.12 2016/09/28 05:38:15 sfeam Exp $"); }
#endif

/* GNUPLOT - stats.c */

/*
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
 */

#include "gp_types.h"
#ifdef USE_STATS  /* Only compile this if configured with --enable-stats */

#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "datafile.h"
#include "gadgets.h"  /* For polar and parametric flags */
#include "matrix.h"   /* For vector allocation */
#include "scanner.h"  /* To check for legal prefixes */
#include "variable.h" /* For locale handling */

#include "stats.h"

#define INITIAL_DATA_SIZE (4096)   /* initial size of data arrays */

static int comparator __PROTO(( const void *a, const void *b ));
static struct file_stats analyze_file __PROTO(( long n, int outofrange, int invalid, int blank, int dblblank, int headers ));
static struct sgl_column_stats analyze_sgl_column __PROTO(( double *data, long n, long nr ));
static struct two_column_stats analyze_two_columns __PROTO(( double *x, double *y,
							     struct sgl_column_stats res_x,
							     struct sgl_column_stats res_y,
							     long n ));

static void ensure_output __PROTO((void));
static char* fmt __PROTO(( char *buf, double val ));
static void sgl_column_output_nonformat __PROTO(( struct sgl_column_stats s, char *x ));
static void file_output __PROTO(( struct file_stats s ));
static void sgl_column_output __PROTO(( struct sgl_column_stats s, long n ));
static void two_column_output __PROTO(( struct sgl_column_stats x,
					struct sgl_column_stats y,
					struct two_column_stats xy, long n));

static void create_and_set_var __PROTO(( double val, char *prefix,
					 char *base, char *suffix ));

static void sgl_column_variables __PROTO(( struct sgl_column_stats res,
					   char *prefix, char *postfix ));

static TBOOLEAN validate_data __PROTO((double v, AXIS_INDEX ax));

/* =================================================================
   Data Structures
   ================================================================= */

/* Keeps info on a value and its index in the file */
struct pair {
    double val;
    long index;
};

/* Collect results from analysis */
struct file_stats {
    long records;
    long blanks;
    long invalid;
    long outofrange;
    long blocks;  /* blocks are separated by double blank lines */
    long columnheaders;
};

struct sgl_column_stats {
    /* Matrix dimensions */
    int sx;
    int sy;

    double mean;
    double adev;
    double stddev;
    double ssd;		/* sample standard deviation */
    double skewness;
    double kurtosis;

    double mean_err;
    double stddev_err;
    double skewness_err;
    double kurtosis_err;

    double sum;          /* sum x    */
    double sum_sq;       /* sum x**2 */

    struct pair min;
    struct pair max;

    double median;
    double lower_quartile;
    double upper_quartile;

    double cog_x;   /* centre of gravity */
    double cog_y;

    /* info on data points out of bounds? */
};

struct two_column_stats {
    double sum_xy;

    double slope;        /* linear regression */
    double intercept;

    double slope_err;
    double intercept_err;

    double correlation;

    double pos_min_y;	/* x coordinate of min y */
    double pos_max_y;	/* x coordinate of max y */
};

/* =================================================================
   Analysis and Output
   ================================================================= */

/* Needed by qsort() when we sort the data points to find the median.   */
/* FIXME: I am dubious that keeping the original index gains anything. */
/* It makes no sense at all for quartiles,  and even the min/max are not  */
/* guaranteed to be unique.                                               */
static int
comparator( const void *a, const void *b )
{
    struct pair *x = (struct pair *)a;
    struct pair *y = (struct pair *)b;

    if ( x->val < y->val ) return -1;
    if ( x->val > y->val ) return 1;
    return 0;
}

static struct file_stats
analyze_file( long n, int outofrange, int invalid, int blank, int dblblank, int headers )
{
    struct file_stats res;

    res.records = n;
    res.invalid = invalid;
    res.blanks  = blank;
    res.blocks  = dblblank + 1;  /* blocks are separated by dbl blank lines */
    res.outofrange = outofrange;
    res.columnheaders = headers;

    return res;
}

static struct sgl_column_stats
analyze_sgl_column( double *data, long n, long nc )
{
    struct sgl_column_stats res;

    long i;

    double s  = 0.0;
    double s2 = 0.0;
    double ad = 0.0;
    double d  = 0.0;
    double d2 = 0.0;
    double d3 = 0.0;
    double d4 = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double var;

    struct pair *tmp = (struct pair *)gp_alloc( n*sizeof(struct pair),
					      "analyze_sgl_column" );

    if ( nc > 0 ) {
	res.sx = nc;
	res.sy = n / nc;
    } else {
	res.sx = 0;
	res.sy = n;
    }

    /* Mean and centre of gravity */
    for( i=0; i<n; i++ ) {
	s  += data[i];
	s2 += data[i]*data[i];
	if ( nc > 0 ) {
	    cx += data[i]*(i % res.sx);
	    cy += data[i]*(i / res.sx);
	}
    }
    res.mean = s/(double)n;

    res.sum  = s;
    res.sum_sq = s2;

    /* Standard deviation, mean absolute deviation, skewness, and kurtosis */
    for( i=0; i<n; i++ ) {
	double t = data[i] - res.mean;
	ad += fabs(t);
	d  += t;
	d2 += t*t;
	d3 += t*t*t;
	d4 += t*t*t*t;
    }

    /* population (not sample) variance, stddev, skew, kurtosis */
    var = (d2 - d * d / n) / n;
    res.stddev = sqrt(var);
    res.adev = ad / n;
    if (var != 0.0) {
	res.skewness = d3 / (n * var * res.stddev);
	res.kurtosis = d4 / (n * var * var);
    } else {
	res.skewness = res.kurtosis = not_a_number();
    }

    res.mean_err = res.stddev / sqrt((double) n);
    res.stddev_err = res.stddev / sqrt(2.0 * n);
    res.skewness_err = sqrt(6.0 / n);
    res.kurtosis_err = sqrt(24.0 / n);

    /* sample standard deviation */
    res.ssd = res.stddev * sqrt((double)(n) / (double)(n-1));

    for( i=0; i<n; i++ ) {
	tmp[i].val = data[i];
	tmp[i].index = i;
    }
    qsort( tmp, n, sizeof(struct pair), comparator );

    res.min = tmp[0];
    res.max = tmp[n-1];

    /*
     * This uses the same quartile definitions as the boxplot code in graphics.c
     */
    if ((n & 0x1) == 0)
	res.median = 0.5 * (tmp[n/2 - 1].val + tmp[n/2].val);
    else
	res.median = tmp[(n-1)/2].val;
    if ((n & 0x3) == 0)
	res.lower_quartile = 0.5 * (tmp[n/4 - 1].val + tmp[n/4].val);
    else
	res.lower_quartile = tmp[(n+3)/4 - 1].val;
    if ((n & 0x3) == 0)
	res.upper_quartile = 0.5 * (tmp[n - n/4].val + tmp[n - n/4 - 1].val);
    else
	res.upper_quartile = tmp[n - (n+3)/4].val;

    /* Note: the centre of gravity makes sense for positive value matrices only */
    if ( cx == 0.0 && cy == 0.0) {
	res.cog_x = 0.0;
	res.cog_y = 0.0;
    } else {
	res.cog_x = cx / s;
	res.cog_y = cy / s;
    }

    free(tmp);

    return res;
}

static struct two_column_stats
analyze_two_columns( double *x, double *y,
		     struct sgl_column_stats res_x,
		     struct sgl_column_stats res_y,
		     long n )
{
    struct two_column_stats res;

    long i;
    double s = 0;
    double ssyy, ssxx, ssxy;

    for( i=0; i<n; i++ ) {
	s += x[i] * y[i];
    }
    res.sum_xy = s;

    /* Definitions according to 
       http://mathworld.wolfram.com/LeastSquaresFitting.html 
     */
    ssyy = res_y.sum_sq - res_y.sum * res_y.sum / n;
    ssxx = res_x.sum_sq - res_x.sum * res_x.sum / n;
    ssxy = res.sum_xy   - res_x.sum * res_y.sum / n;

    if (ssxx != 0.0)
	res.slope = ssxy / ssxx;
    else
	res.slope = not_a_number();
    res.intercept = res_y.mean - res.slope * res_x.mean;

    if (res_y.stddev != 0.0)
	res.correlation = res.slope * res_x.stddev / res_y.stddev;
    else
	res.correlation = not_a_number();

    if (n > 2) {
	double ss = (ssyy - res.slope * ssxy) / (n - 2);
	if (ssxx != 0.0) {
	    res.slope_err = sqrt(ss / ssxx);
	    res.intercept_err = sqrt(ss * (1./n + res_x.sum * res_x.sum / (n * n * ssxx)));
	} else {
	    res.slope_err = res.intercept_err = not_a_number();
	}
    } else if (n == 2) {
	fprintf(stderr, "Warning:  Errors of slope and intercept are zero. There are as many data points as there are parameters.\n");
	res.slope_err = res.intercept_err = 0.0;
    } else {
	fprintf(stderr, "Warning:  Can't compute errors of slope and intercept. Not enough data points.\n");
	res.slope_err = res.intercept_err = not_a_number();
    }

    res.pos_min_y = x[res_y.min.index];
    res.pos_max_y = x[res_y.max.index];

    return res;
}

/* =================================================================
   Output
   ================================================================= */

/* Output */
/* Note: print_out is a FILE ptr, set by the "set print" command */

static void
ensure_output()
{
    if (!print_out)
	print_out = stderr;
}

static char*
fmt( char *buf, double val )
{
    if ( isnan(val) ) 
	sprintf( buf, "%11s", "undefined");
    else if ( fabs(val) < 1e-14 )
	sprintf( buf, "%11.4f", 0.0 );
    else if ( fabs(log10(fabs(val))) < 6 )
	sprintf( buf, "%11.4f", val );
    else
	sprintf( buf, "%11.5e", val );
    return buf;
}

static void
file_output( struct file_stats s )
{
    int width = 3;

    /* Assuming that records is the largest number of the four... */
    if ( s.records > 0 )
	width = 1 + (int)( log10((double)s.records) );

    ensure_output();

    /* Non-formatted to disk */
    if ( print_out != stdout && print_out != stderr ) {
	fprintf( print_out, "%s\t%ld\n", "records", s.records );
	fprintf( print_out, "%s\t%ld\n", "invalid", s.invalid );
	fprintf( print_out, "%s\t%ld\n", "blanks", s.blanks );
	fprintf( print_out, "%s\t%ld\n", "blocks", s.blocks );
	fprintf( print_out, "%s\t%ld\n", "columnheaders", s.columnheaders );
	fprintf( print_out, "%s\t%ld\n", "outofrange", s.outofrange );
	return;
    }

    /* Formatted to screen */
    fprintf( print_out, "\n" );
    fprintf( print_out, "* FILE: \n" );
    fprintf( print_out, "  Records:           %*ld\n", width, s.records );
    fprintf( print_out, "  Out of range:      %*ld\n", width, s.outofrange );
    fprintf( print_out, "  Invalid:           %*ld\n", width, s.invalid );
    fprintf( print_out, "  Column headers:    %*ld\n", width, s.columnheaders );
    fprintf( print_out, "  Blank:             %*ld\n", width, s.blanks );
    fprintf( print_out, "  Data Blocks:       %*ld\n", width, s.blocks );
}

static void
sgl_column_output_nonformat( struct sgl_column_stats s, char *x )
{
    fprintf( print_out, "%s%s\t%f\n", "mean",     x, s.mean );
    fprintf( print_out, "%s%s\t%f\n", "stddev",   x, s.stddev );
    fprintf( print_out, "%s%s\t%f\n", "ssd",      x, s.ssd );
    fprintf( print_out, "%s%s\t%f\n", "skewness", x, s.skewness );
    fprintf( print_out, "%s%s\t%f\n", "kurtosis", x, s.kurtosis );
    fprintf( print_out, "%s%s\t%f\n", "adev",     x, s.adev);
    fprintf( print_out, "%s%s\t%f\n", "sum",      x, s.sum );
    fprintf( print_out, "%s%s\t%f\n", "sum_sq",   x, s.sum_sq );

    fprintf( print_out, "%s%s\t%f\n", "mean_err",     x, s.mean_err );
    fprintf( print_out, "%s%s\t%f\n", "stddev_err",   x, s.stddev_err );
    fprintf( print_out, "%s%s\t%f\n", "skewness_err", x, s.skewness_err );
    fprintf( print_out, "%s%s\t%f\n", "kurtosis_err", x, s.kurtosis_err );

    fprintf( print_out, "%s%s\t%f\n", "min",     x, s.min.val );
    if ( s.sx == 0 ) {
	fprintf( print_out, "%s%s\t%f\n", "lo_quartile", x, s.lower_quartile );
	fprintf( print_out, "%s%s\t%f\n", "median",      x, s.median );
	fprintf( print_out, "%s%s\t%f\n", "up_quartile", x, s.upper_quartile );
    }
    fprintf( print_out, "%s%s\t%f\n", "max",     x, s.max.val );

    /* If data set is matrix */
    if ( s.sx > 0 ) {
	fprintf( print_out, "%s%s\t%ld\n","index_min_x",  x, (s.min.index) % s.sx );
	fprintf( print_out, "%s%s\t%ld\n","index_min_y",  x, (s.min.index) / s.sx );
	fprintf( print_out, "%s%s\t%ld\n","index_max_x",  x, (s.max.index) % s.sx );
	fprintf( print_out, "%s%s\t%ld\n","index_max_y",  x, (s.max.index) / s.sx );
	fprintf( print_out, "%s%s\t%f\n","cog_x",  x, s.cog_x );
	fprintf( print_out, "%s%s\t%f\n","cog_y",  x, s.cog_y );
    } else {
	fprintf( print_out, "%s%s\t%ld\n","min_index",  x, s.min.index );
	fprintf( print_out, "%s%s\t%ld\n","max_index",  x, s.max.index );
    }
}

static void
sgl_column_output( struct sgl_column_stats s, long n )
{
    int width = 1;
    char buf[32];
    char buf2[32];

    if ( n > 0 )
	width = 1 + (int)( log10( (double)n ) );

    ensure_output();

    /* Non-formatted to disk */
    if ( print_out != stdout && print_out != stderr ) {
	sgl_column_output_nonformat( s, "_y" );
	return;
    }

    /* Formatted to screen */
    fprintf( print_out, "\n" );

    /* First, we check whether the data file was a matrix */
     if ( s.sx > 0)
	 fprintf( print_out, "* MATRIX: [%d X %d] \n", s.sx, s.sy );
     else
	 fprintf( print_out, "* COLUMN: \n" );

    fprintf( print_out, "  Mean:          %s\n", fmt( buf, s.mean ) );
    fprintf( print_out, "  Std Dev:       %s\n", fmt( buf, s.stddev ) );
    fprintf( print_out, "  Sample StdDev: %s\n", fmt( buf, s.ssd ) );
    fprintf( print_out, "  Skewness:      %s\n", fmt( buf, s.skewness ) );
    fprintf( print_out, "  Kurtosis:      %s\n", fmt( buf, s.kurtosis ) );
    fprintf( print_out, "  Avg Dev:       %s\n", fmt( buf, s.adev ) );
    fprintf( print_out, "  Sum:           %s\n", fmt( buf, s.sum ) );
    fprintf( print_out, "  Sum Sq.:       %s\n", fmt( buf, s.sum_sq ) );
    fprintf( print_out, "\n" );

    fprintf( print_out, "  Mean Err.:     %s\n", fmt( buf, s.mean_err ) );
    fprintf( print_out, "  Std Dev Err.:  %s\n", fmt( buf, s.stddev_err ) );
    fprintf( print_out, "  Skewness Err.: %s\n", fmt( buf, s.skewness_err ) );
    fprintf( print_out, "  Kurtosis Err.: %s\n", fmt( buf, s.kurtosis_err ) );
    fprintf( print_out, "\n" );

    /* For matrices, the quartiles and the median do not make too much sense */
    if ( s.sx > 0 ) {
	fprintf( print_out, "  Minimum:       %s [%*ld %ld ]\n", fmt(buf, s.min.val), width,
	     (s.min.index) % s.sx, (s.min.index) / s.sx);
	fprintf( print_out, "  Maximum:       %s [%*ld %ld ]\n", fmt(buf, s.max.val), width,
	     (s.max.index) % s.sx, (s.max.index) / s.sx);
	fprintf( print_out, "  COG:           %s %s\n", fmt(buf, s.cog_x), fmt(buf2, s.cog_y) );
    } else {
	/* FIXME:  The "position" are randomly selected from a non-unique set. Bad! */
	fprintf( print_out, "  Minimum:       %s [%*ld]\n", fmt(buf, s.min.val), width, s.min.index );
	fprintf( print_out, "  Maximum:       %s [%*ld]\n", fmt(buf, s.max.val), width, s.max.index );
	fprintf( print_out, "  Quartile:      %s \n", fmt(buf, s.lower_quartile) );
	fprintf( print_out, "  Median:        %s \n", fmt(buf, s.median) );
	fprintf( print_out, "  Quartile:      %s \n", fmt(buf, s.upper_quartile) );
	fprintf( print_out, "\n" );
    }
}

static void
two_column_output( struct sgl_column_stats x,
		   struct sgl_column_stats y,
		   struct two_column_stats xy,
		   long n )
{
    int width = 1;
    char bfx[32];
    char bfy[32];
    char blank[32];

    if ( n > 0 )
	width = 1 + (int)log10((double)n);

    /* Non-formatted to disk */
    if ( print_out != stdout && print_out != stderr ) {
	sgl_column_output_nonformat( x, "_x" );
	sgl_column_output_nonformat( y, "_y" );

	fprintf( print_out, "%s\t%f\n", "slope", xy.slope );
	if ( n > 2 )
	fprintf( print_out, "%s\t%f\n", "slope_err", xy.slope_err );
	fprintf( print_out, "%s\t%f\n", "intercept", xy.intercept );
	if ( n > 2 )
	fprintf( print_out, "%s\t%f\n", "intercept_err", xy.intercept_err );
	fprintf( print_out, "%s\t%f\n", "correlation", xy.correlation );
	fprintf( print_out, "%s\t%f\n", "sumxy", xy.sum_xy );
	return;
    }

    /* Create a string of blanks of the required length */
    strncpy( blank, "                 ", width+4 );
    blank[width+4] = '\0';

    ensure_output();

    fprintf( print_out, "\n" );
    fprintf( print_out, "* COLUMNS:\n" );
    fprintf( print_out, "  Mean:          %s %s %s\n", fmt(bfx, x.mean),   blank, fmt(bfy, y.mean) );
    fprintf( print_out, "  Std Dev:       %s %s %s\n", fmt(bfx, x.stddev), blank, fmt(bfy, y.stddev ) );
    fprintf( print_out, "  Sample StdDev: %s %s %s\n", fmt(bfx, x.ssd), blank, fmt(bfy, y.ssd ) );
    fprintf( print_out, "  Skewness:      %s %s %s\n", fmt(bfx, x.skewness), blank, fmt(bfy, y.skewness) );
    fprintf( print_out, "  Kurtosis:      %s %s %s\n", fmt(bfx, x.kurtosis), blank, fmt(bfy, y.kurtosis) );
    fprintf( print_out, "  Avg Dev:       %s %s %s\n", fmt(bfx, x.adev), blank, fmt(bfy, y.adev ) );
    fprintf( print_out, "  Sum:           %s %s %s\n", fmt(bfx, x.sum),  blank, fmt(bfy, y.sum) );
    fprintf( print_out, "  Sum Sq.:       %s %s %s\n", fmt(bfx, x.sum_sq), blank, fmt(bfy, y.sum_sq ) );
    fprintf( print_out, "\n" );

    fprintf( print_out, "  Mean Err.:     %s %s %s\n", fmt(bfx, x.mean_err),   blank, fmt(bfy, y.mean_err) );
    fprintf( print_out, "  Std Dev Err.:  %s %s %s\n", fmt(bfx, x.stddev_err), blank, fmt(bfy, y.stddev_err) );
    fprintf( print_out, "  Skewness Err.: %s %s %s\n", fmt(bfx, x.skewness_err), blank, fmt(bfy, y.skewness_err) );
    fprintf( print_out, "  Kurtosis Err.: %s %s %s\n", fmt(bfx, x.kurtosis_err), blank, fmt(bfy, y.kurtosis_err) );
    fprintf( print_out, "\n" );

    /* FIXME:  The "positions" are randomly selected from a non-unique set.  Bad! */
    fprintf( print_out, "  Minimum:       %s [%*ld]   %s [%*ld]\n", fmt(bfx, x.min.val),
    	width, x.min.index, fmt(bfy, y.min.val), width, y.min.index );
    fprintf( print_out, "  Maximum:       %s [%*ld]   %s [%*ld]\n", fmt(bfx, x.max.val),
    	width, x.max.index, fmt(bfy, y.max.val), width, y.max.index );

    fprintf( print_out, "  Quartile:      %s %s %s\n",
	fmt(bfx, x.lower_quartile), blank, fmt(bfy, y.lower_quartile) );
    fprintf( print_out, "  Median:        %s %s %s\n",
	fmt(bfx, x.median), blank, fmt(bfy, y.median) );
    fprintf( print_out, "  Quartile:      %s %s %s\n",
	fmt(bfx, x.upper_quartile), blank, fmt(bfy, y.upper_quartile) );
    fprintf( print_out, "\n" );

    /* Simpler below - don't care about alignment */
    if ( xy.intercept < 0.0 )
	fprintf( print_out, "  Linear Model:       y = %.4g x - %.4g\n", xy.slope, -xy.intercept );
    else
	fprintf( print_out, "  Linear Model:       y = %.4g x + %.4g\n", xy.slope, xy.intercept );

    fprintf( print_out, "  Slope:              %.4g +- %.4g\n", xy.slope, xy.slope_err );
    fprintf( print_out, "  Intercept:          %.4g +- %.4g\n", xy.intercept, xy.intercept_err );

    fprintf( print_out, "  Correlation:        r = %.4g\n", xy.correlation );
    fprintf( print_out, "  Sum xy:             %.4g\n", xy.sum_xy );
    fprintf( print_out, "\n" );
}

/* =================================================================
   Variable Handling
   ================================================================= */

static void
create_and_set_var( double val, char *prefix, char *base, char *suffix )
{
    int len;
    char *varname;
    struct udvt_entry *udv_ptr;

    t_value data;
    Gcomplex( &data, val, 0.0 ); /* data is complex, real=val, imag=0.0 */

    /* In case prefix (or suffix) is NULL - make them empty strings */
    prefix = prefix ? prefix : "";
    suffix = suffix ? suffix : "";

    len = strlen(prefix) + strlen(base) + strlen(suffix) + 1;
    varname = (char *)gp_alloc( len, "create_and_set_var" );
    sprintf( varname, "%s%s%s", prefix, base, suffix );

    /* Note that add_udv_by_name() checks if the name already exists, and
     * returns the existing ptr if found. It also allocates memory for
     * its own copy of the varname.
     */
    udv_ptr = add_udv_by_name(varname);
    udv_ptr->udv_value = data;
    udv_ptr->udv_undef = FALSE;

    free( varname );
}

static void
file_variables( struct file_stats s, char *prefix )
{
    /* Suffix does not make sense here! */
    create_and_set_var( s.records, prefix, "records", "" );
    create_and_set_var( s.invalid, prefix, "invalid", "" );
    create_and_set_var( s.columnheaders, prefix, "headers", "" );
    create_and_set_var( s.blanks,  prefix, "blank",   "" );
    create_and_set_var( s.blocks,  prefix, "blocks",  "" );
    create_and_set_var( s.outofrange, prefix, "outofrange", "" );
    create_and_set_var( df_last_col, prefix, "columns", "" );
}

static void
sgl_column_variables( struct sgl_column_stats s, char *prefix, char *suffix )
{
    create_and_set_var( s.mean,     prefix, "mean",     suffix );
    create_and_set_var( s.stddev,   prefix, "stddev",   suffix );
    create_and_set_var( s.ssd,      prefix, "ssd",      suffix );
    create_and_set_var( s.skewness, prefix, "skewness", suffix );
    create_and_set_var( s.kurtosis, prefix, "kurtosis", suffix );
    create_and_set_var( s.adev,     prefix, "adev",     suffix );

    create_and_set_var( s.mean_err,     prefix, "mean_err",     suffix );
    create_and_set_var( s.stddev_err,   prefix, "stddev_err",   suffix );
    create_and_set_var( s.skewness_err, prefix, "skewness_err", suffix );
    create_and_set_var( s.kurtosis_err, prefix, "kurtosis_err", suffix );

    create_and_set_var( s.sum,    prefix, "sum",   suffix );
    create_and_set_var( s.sum_sq, prefix, "sumsq", suffix );

    create_and_set_var( s.min.val, prefix, "min", suffix );
    create_and_set_var( s.max.val, prefix, "max", suffix );

    /* If data set is matrix */
    if ( s.sx > 0 ) {
	create_and_set_var( (s.min.index) % s.sx, prefix, "index_min_x", suffix );
	create_and_set_var( (s.min.index) / s.sx, prefix, "index_min_y", suffix );
	create_and_set_var( (s.max.index) % s.sx, prefix, "index_max_x", suffix );
	create_and_set_var( (s.max.index) / s.sx, prefix, "index_max_y", suffix );
	create_and_set_var( s.sx, prefix, "size_x", suffix );
	create_and_set_var( s.sy, prefix, "size_y", suffix );
    } else {
	create_and_set_var( s.median,         prefix, "median",      suffix );
	create_and_set_var( s.lower_quartile, prefix, "lo_quartile", suffix );
	create_and_set_var( s.upper_quartile, prefix, "up_quartile", suffix );
	create_and_set_var( s.min.index, prefix, "index_min", suffix );
	create_and_set_var( s.max.index, prefix, "index_max", suffix );
    }
}

static void
two_column_variables( struct two_column_stats s, char *prefix, long n )
{
    /* Suffix does not make sense here! */
    create_and_set_var( s.slope,         prefix, "slope",         "" );
    create_and_set_var( s.intercept,     prefix, "intercept",     "" );
    /* The errors can only calculated for n > 2, but we set them (to zero) anyway. */
    create_and_set_var( s.slope_err,     prefix, "slope_err",     "" );
    create_and_set_var( s.intercept_err, prefix, "intercept_err", "" );
    create_and_set_var( s.correlation,   prefix, "correlation",   "" );
    create_and_set_var( s.sum_xy,        prefix, "sumxy",         "" );

    create_and_set_var( s.pos_min_y,     prefix, "pos_min_y",     "" );
    create_and_set_var( s.pos_max_y,     prefix, "pos_max_y",     "" );
}

/* =================================================================
   Range Handling
   ================================================================= */

/* We validate our data here: discard everything that is outside
 * the specified range. However, we have to be a bit careful here,
 * because if no range is specified, we keep everything
 */
static TBOOLEAN validate_data(double v, AXIS_INDEX ax)
{
    /* These are flag bits, not constants!!! */
    if ((axis_array[ax].autoscale & AUTOSCALE_BOTH) == AUTOSCALE_BOTH)
	return TRUE;
    if (((axis_array[ax].autoscale & AUTOSCALE_BOTH) == AUTOSCALE_MIN)
    &&  (v <= axis_array[ax].max))
	return TRUE;
    if (((axis_array[ax].autoscale & AUTOSCALE_BOTH) == AUTOSCALE_MAX)
    &&  (v >= axis_array[ax].min))
	return TRUE;
    if (((axis_array[ax].autoscale & AUTOSCALE_BOTH) == AUTOSCALE_NONE)
	 && ((v <= axis_array[ax].max) && (v >= axis_array[ax].min)))
	return(TRUE);

    return(FALSE);
}

/* =================================================================
   Parse Command Line and Process
   ================================================================= */

void
statsrequest(void)
{
    int i;
    int columns;
    double v[2];
    static char *file_name = NULL;
    char *temp_name;

    /* Vars to hold data and results */
    long n;                /* number of records retained */
    long max_n;

    static double *data_x = NULL;
    static double *data_y = NULL;   /* values read from file */
    long invalid;          /* number of missing/invalid records */
    long blanks;           /* number of blank lines */
    long doubleblanks;     /* number of repeated blank lines */
    long columnheaders;    /* number of records treated as headers rather than data */
    long out_of_range;     /* number pts rejected, because out of range */

    struct file_stats res_file;
    struct sgl_column_stats res_x = {0}, res_y = {0};
    struct two_column_stats res_xy = {0};

    /* Vars for variable handling */
    static char *prefix = NULL;       /* prefix for user-defined vars names */
    TBOOLEAN prefix_from_columnhead = FALSE;

    /* Vars that control output */
    TBOOLEAN do_output = TRUE;     /* Generate formatted output */

    c_token++;

    /* Parse ranges */
    AXIS_INIT2D(FIRST_X_AXIS, 0);
    AXIS_INIT2D(FIRST_Y_AXIS, 0);
    parse_range(FIRST_X_AXIS);
    parse_range(FIRST_Y_AXIS);

    /* Initialize */
    invalid = 0;          /* number of missing/invalid records */
    blanks = 0;           /* number of blank lines */
    columnheaders = 0;    /* number of records treated as headers rather than data */
    doubleblanks = 0;     /* number of repeated blank lines */
    out_of_range = 0;     /* number pts rejected, because out of range */
    n = 0;                /* number of records retained */
    max_n = INITIAL_DATA_SIZE;

    free(data_x);
    free(data_y);
    data_x = vec(max_n);       /* start with max. value */
    data_y = vec(max_n);

    if ( !data_x || !data_y )
      int_error( NO_CARET, "Internal error: out of memory in stats" );

    n = invalid = blanks = columnheaders = doubleblanks = out_of_range = 0;

    /* Get filename */
    i = c_token;
    temp_name = string_or_express(NULL);
    if (temp_name) {
	free(file_name);
	file_name = gp_strdup(temp_name);
    } else
	int_error(i, "missing filename or datablock");

    /* Jan 2015: We used to handle ascii matrix data as a special case but
     * the code did not work correctly.  Since df_read_matrix() dummies up
     * ascii matrix data to look as if had been presented as a binary blob,
     * we should be able to proceed with no special attention other than
     * to set the effective number of columns to 1.
     */
    if (TRUE) {
	df_set_plot_mode(MODE_PLOT);		/* Used for matrix datafiles */
	columns = df_open(file_name, 2, NULL);	/* up to 2 using specs allowed */

	if (columns < 0) {
	    int_warn(NO_CARET, "Can't read data file");
	    while (!END_OF_COMMAND)
		c_token++;
	    goto stats_cleanup;
	}

	/* For all these below: we could save the state, switch off, then restore */
	if ( axis_array[FIRST_X_AXIS].log || axis_array[FIRST_Y_AXIS].log )
	    int_error( NO_CARET, "Stats command not available with logscale active");

	if (axis_array[FIRST_X_AXIS].datatype == DT_TIMEDATE
	||  axis_array[FIRST_Y_AXIS].datatype == DT_TIMEDATE )
	    int_error( NO_CARET, "Stats command not available in timedata mode");

	if ( polar )
	    int_error( NO_CARET, "Stats command not available in polar mode" );

	if ( parametric )
	    int_error( NO_CARET, "Stats command not available in parametric mode" );

	/* Parse the remainder of the command line */
	while( !(END_OF_COMMAND) ) {
	    if ( almost_equals( c_token, "out$put" ) ) {
		    do_output = TRUE;
		    c_token++;

	    } else if ( almost_equals( c_token, "noout$put" ) ) {
		    do_output = FALSE;
		    c_token++;

	    } else if ( almost_equals(c_token, "pre$fix")
		   ||   equals(c_token, "name")) {
		c_token++;
		free ( prefix );
		if (almost_equals(c_token,"col$umnheader")) {
		    df_set_key_title_columnhead(NULL);
		    prefix_from_columnhead = TRUE;
		    continue;
		}
		prefix = try_to_get_string();
		if (!legal_identifier(prefix) || !strcmp ("GPVAL_", prefix))
		    int_error( --c_token, "illegal prefix" );

	    }  else {
		int_error( c_token, "Unrecognized fit option");
	    }
	}

	/* If the user has set an explicit locale for numeric input, apply it */
	/* here so that it affects data fields read from the input file. */
	set_numeric_locale();

	/* The way readline and friends work is as follows:
	 - df_open will return the number of columns requested in the using spec
	   so that "columns" will be 0, 1, or 2 (no using, using 1, using 1:2)
	 - readline always returns the same number of columns (for us: 1 or 2)
	 - using n:m = return two columns, skipping lines w/ bad data
	 - using n   = return single column (supply zeros (0) for the second col)
	 - no using  = first two columns if both are present on the first line of data
		       else first column only
	 */
	while( (i = df_readline(v, 2)) != DF_EOF ) {

	    if ( n >= max_n ) {
		max_n = (max_n * 3) / 2; /* increase max_n by factor of 1.5 */

		/* Some of the reallocations went bad: */
		if ( !redim_vec(&data_x, max_n) || !redim_vec(&data_y, max_n) ) {
		    df_close();
		    int_error( NO_CARET,
		       "Out of memory in stats: too many datapoints (%d)?", max_n );
		}
	    } /* if (need to extend storage space) */

	    switch (i) {
	    case DF_MISSING:
	    case DF_UNDEFINED:
	      invalid += 1;
	      continue;

	    case DF_FIRST_BLANK:
	      blanks += 1;
	      continue;

	    case DF_SECOND_BLANK:
	      blanks += 1;
	      doubleblanks += 1;
	      continue;

	    case DF_COLUMN_HEADERS:
	      columnheaders += 1;
	      continue;

	    case 0:
	      int_warn( NO_CARET, "bad data on line %d of file %s",
	  		df_line_number, df_filename ? df_filename : "" );
	      break;

	    case 1: /* Read single column successfully  */
	      if ( validate_data(v[0], FIRST_Y_AXIS) )  {
		data_y[n] = v[0];
		n++;
	      } else {
		out_of_range++;
	      }
	      columns = 1;
	      break;

	    case 2: /* Read two columns successfully  */
	      if ( validate_data(v[0], FIRST_X_AXIS) &&
		  validate_data(v[1], FIRST_Y_AXIS) ) {
		data_x[n] = v[0];
		data_y[n] = v[1];
		n++;
	      } else {
		out_of_range++;
	      }
	      columns = 2;
	      break;

	    default: /* Who are these? */
	      FPRINTF((stderr,"unhandled return code %d from df_readline\n", i));
	      break;

	    }
	} /* end-while : done reading file */
	df_close();

	/* now resize fields to actual length: */
	redim_vec(&data_x, n);
	redim_vec(&data_y, n);
    }

    /* Now finished reading user input; return to C locale for internal use*/
    reset_numeric_locale();

    /* No data! Try to explain why. */
    if ( n == 0 ) {
	if ( out_of_range > 0 )
	    int_warn( NO_CARET, "All points out of range" );
	else
	    int_warn( NO_CARET, "No valid data points found in file" );
    }

    /* The analysis variables are named STATS_* unless the user either */
    /* gave a specific name or told us to use a columnheader.          */
    if (!prefix && prefix_from_columnhead && df_key_title && *df_key_title) {
	prefix = gp_strdup(df_key_title);
	squash_spaces(prefix, 0);
	if (!legal_identifier(prefix)) {
	    int_warn(NO_CARET, "columnhead %s is not a valid prefix", prefix ? prefix : "");
	    free(prefix);
	    prefix = NULL;
	}
    }
    if (!prefix)
	prefix = gp_strdup("STATS_");
    i = strlen(prefix);
    if (prefix[i-1] != '_') {
	prefix = (char *) gp_realloc(prefix, i+2, "prefix");
	strcat(prefix,"_");
    }

    /* Do the actual analysis */
    res_file = analyze_file( n, out_of_range, invalid, blanks, doubleblanks, columnheaders );

    /* Jan 2015: Revised detection and handling of matrix data */
    if (df_matrix) {
	int nc = df_bin_record[df_num_bin_records-1].scan_dim[0];
	res_y = analyze_sgl_column( data_y, n, nc );
	columns = 1;

    } else if (columns == 1) {
	res_y = analyze_sgl_column( data_y, n, 0 );

    } else {
	/* If there are two columns, then the data file is not a matrix */
	res_x = analyze_sgl_column( data_x, n, 0 );
	res_y = analyze_sgl_column( data_y, n, 0 );
	res_xy = analyze_two_columns( data_x, data_y, res_x, res_y, n );
    }

    /* Store results in user-accessible variables */
    /* Clear out any previous use of these variables */
    del_udv_by_name( prefix, TRUE );

    file_variables( res_file, prefix );

    if ( columns == 1 ) {
	sgl_column_variables( res_y, prefix, "" );
    }

    if ( columns == 2 ) {
	sgl_column_variables( res_x, prefix, "_x" );
	sgl_column_variables( res_y, prefix, "_y" );
	two_column_variables( res_xy, prefix, n );
    }

    /* Output */
    if ( do_output ) {
	file_output( res_file );
	if ( columns == 1 )
	    sgl_column_output( res_y, res_file.records );
	else
	    two_column_output( res_x, res_y, res_xy, res_file.records );
    }

    /* Cleanup */
    stats_cleanup:

    free(data_x);
    free(data_y);

    data_x = NULL;
    data_y = NULL;

    free( file_name );
    file_name = NULL;

    free( prefix );
    prefix = NULL;
}
#endif /* The whole file is conditional on USE_STATS */
