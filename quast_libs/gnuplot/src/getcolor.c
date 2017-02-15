#ifndef lint
static char *RCSid() { return RCSid("$Id: getcolor.c,v 1.36 2013/10/25 15:37:21 sfeam Exp $"); }
#endif

/* GNUPLOT - getcolor.c */

/*[
 *
 * Petr Mikulik, December 1998 -- June 1999
 * Copyright: open source as much as possible
 *
]*/

#include "syscfg.h"

#include "stdfn.h"

#include "util.h"
#include "getcolor.h"

/*
 * This file implements all the gray to color transforms except one:
 * calculate_color_from_formulae() which can be found in color.c.
 * calculate_color_from_formulae() belongs logicaly into this (getcolor.c)
 * file, but it can't be here due to linkage problems:  gnuplot_x11
 * needs a lot of code from getcolor.c but does not contain any function
 * evaluation code [and does not call calculate_color_from_formulae()].
 * This file is compiled twice:  Once as part of gnuplot and the second
 * time with -DGPLT_X11_MODE resulting in getcolor_x11.o which is linked
 * into gnuplot_x11.  With GPLT_X11_MODE defined this file does not
 * contain code for calculating colors from gray by user defined functions.
 */
#ifndef GPLT_X11_MODE
static int calculate_color_from_formulae __PROTO((double, rgb_color *));
#endif

static void color_components_from_gray __PROTO((double gray, rgb_color *color));
static int interpolate_color_from_gray __PROTO((double, rgb_color *));
static double get_max_dev __PROTO((rgb_color *colors, int j, double limit));
static int is_extremum __PROTO((rgb_color left,rgb_color mid,rgb_color right));
static void CMY_2_RGB __PROTO((rgb_color *color));
static void CIEXYZ_2_RGB __PROTO((rgb_color *color));
static void YIQ_2_RGB __PROTO((rgb_color *color));
static void HSV_2_RGB __PROTO((rgb_color *color));


/* check if two palettes p1 and p2 differ significantly */
int
palettes_differ(t_sm_palette *p1, t_sm_palette *p2)
{
    if (p1->colorMode != p2->colorMode)
	return 1;
    if (p1->positive != p2->positive)
	return 1;
    if (p1->cmodel != p2->cmodel)
	return 1;
    if (p1->use_maxcolors != p2->use_maxcolors)
	return 1;

    switch(p1->colorMode) {
    case SMPAL_COLOR_MODE_NONE:
	return 0;  /* ?? */
    case SMPAL_COLOR_MODE_GRAY:
	if (fabs(p1->gamma - p2->gamma) > 1e-3)
	    return 1;
	break;
    case SMPAL_COLOR_MODE_RGB:
	if (p1->colorFormulae != p2->colorFormulae)
	    return 1;
	if (p1->formulaR != p2->formulaR)
	    return 1;
	if (p1->formulaG != p2->formulaG)
	    return 1;
	if (p1->formulaB != p2->formulaB)
	    return 1;
	/* if (p1->ps_allcF != p2->ps_allcF)
	   return 1; */
	break;
    case SMPAL_COLOR_MODE_FUNCTIONS:
	/* coarse check based on typed fnct definitions */
	if (strcmp(p1->Afunc.definition, p2->Afunc.definition))
	    return 1;
	if (strcmp(p1->Bfunc.definition, p2->Bfunc.definition))
	    return 1;
	if (strcmp(p1->Cfunc.definition, p2->Cfunc.definition))
	    return 1;
	break;
    case SMPAL_COLOR_MODE_GRADIENT: {
	int i=0;

	if (p1->gradient_num != p2->gradient_num)
	    return 1;
	for(i=0; i<p1->gradient_num; ++i) {
	    if (p1->gradient[i].pos != p2->gradient[i].pos)
		return 1;
	    if (p1->gradient[i].col.r != p2->gradient[i].col.r)
		return 1;
	    if (p1->gradient[i].col.g != p2->gradient[i].col.g)
		return 1;
	    if (p1->gradient[i].col.b != p2->gradient[i].col.b)
		return 1;
	}
	break;
    case SMPAL_COLOR_MODE_CUBEHELIX:
	return 1;
	break;
    } /* case GRADIENT */
    } /* switch() */

    return 0;  /* no real difference found */
}

#define CONSTRAIN(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

/*  This one takes the gradient defined in sm_palette.gradient and
 *  returns an interpolated color for the given gray value.  It
 *  does not matter wether RGB or HSV or whatever values are stored
 *  in sm_palette.gradient[i].col, they will simply be interpolated.
 *  Return 0 on sucess, 1 if gray outside the range covered by the
 *  gradient.  No gamma correction is done.  The user can implement
 *  gamma correction specifying more points.
 *  sm_palette.gradient[] should be sorted acording to .pos.
 *  Returns 1 on failure and fills color with "nearest" color.
 */
static int
interpolate_color_from_gray(double gray, rgb_color *color)
{
    int idx, maxidx;
    rgb_color *col1;
    rgb_color *col2;

    if (gray < 0)  {
        color->r = sm_palette.gradient[0].col.r;
	color->g = sm_palette.gradient[0].col.g;
	color->b = sm_palette.gradient[0].col.b;
	return 1;
    }

    maxidx = sm_palette.gradient_num;
    if (gray > 1) {
        color->r = sm_palette.gradient[maxidx-1].col.r;
	color->g = sm_palette.gradient[maxidx-1].col.g;
	color->b = sm_palette.gradient[maxidx-1].col.b;
        return 1;
    }

    /* find index by bisecting */
    idx = 0;
    if (maxidx > 1) {
        int topidx = maxidx - 1;
        /* treat idx as though it is bottom index */
        while (idx != topidx) {
            int tmpidx = (idx + topidx) / 2;
            if (sm_palette.gradient[tmpidx].pos < gray)
                idx = tmpidx + 1;  /* round up */
            else
                topidx = tmpidx;
        }
    }

    col2 = & sm_palette.gradient[idx].col;
    if (gray == sm_palette.gradient[idx].pos) {
        /* exact hit */
        color->r = col2->r;
	color->g = col2->g;
	color->b = col2->b;
    } else {
        /* linear interpolation of two colors */
        double dx = sm_palette.gradient[idx].pos
	    - sm_palette.gradient[idx - 1].pos;
	double f = (gray - sm_palette.gradient[idx-1].pos) / dx;
	
	col1 = & sm_palette.gradient[idx - 1].col;
	color->r = (col1->r + f * (col2->r - col1->r));
	color->g = (col1->g + f * (col2->g - col1->g));
	color->b = (col1->b + f * (col2->b - col1->b));
    }
    return 0;
}


#ifndef GPLT_X11_MODE
/*  Fills color with the values calculated from sm_palette.[ABC]func
 *  The color values are clipped to [0,1] without further notice.
 *  Returns 0 or does an int_error() when function evaluatin failed.
 *  The result is not in RGB color space jet.
 */
static int
calculate_color_from_formulae(double gray, rgb_color *color)
{
    struct value v;
    double a, b, c;

#define NO_CARET (-1)

    (void) Gcomplex(&sm_palette.Afunc.dummy_values[0], gray, 0.0);
    evaluate_at(sm_palette.Afunc.at, &v);
    if (undefined)
	int_error(NO_CARET,
		  "Undefined value first color during function evaluation");
    a = real(&v);
    a = CONSTRAIN(a);

    (void) Gcomplex(&sm_palette.Bfunc.dummy_values[0], gray, 0.0);
    evaluate_at(sm_palette.Bfunc.at, &v);
    if (undefined)
	int_error(NO_CARET,
		  "Undefined value second color during function evaluation");
    b = real(&v);
    b = CONSTRAIN(b);

    (void) Gcomplex(&sm_palette.Cfunc.dummy_values[0], gray, 0.0);
    evaluate_at(sm_palette.Cfunc.at, &v);
    if (undefined)
	int_error(NO_CARET,
		  "Undefined value third color during function evaluation");
    c = real(&v);
    c = CONSTRAIN(c);

    color->r = a;
    color->g = b;
    color->b = c;
#undef NO_CARET
    return 0;
}
#endif  /* !GPLT_X11_MODE */


/* Map gray in [0,1] to color components according to colorMode */
static void
color_components_from_gray(double gray, rgb_color *color)
{
    if (gray < 0)
    	gray = 0;
    else if (gray > 1.0)
	gray = 1.0;

    switch(sm_palette.colorMode) {
    case SMPAL_COLOR_MODE_GRAY:
	color->r = color->g = color->b = pow(gray, 1.0/sm_palette.gamma);
	return;  /* all done, no color space transformation needed  */
    case SMPAL_COLOR_MODE_RGB:
	color->r = GetColorValueFromFormula(sm_palette.formulaR, gray);
	color->g = GetColorValueFromFormula(sm_palette.formulaG, gray);
	color->b = GetColorValueFromFormula(sm_palette.formulaB, gray);
	break;
    case SMPAL_COLOR_MODE_GRADIENT:
	interpolate_color_from_gray(gray, color);
	break;
#ifndef GPLT_X11_MODE
    case SMPAL_COLOR_MODE_FUNCTIONS:
	calculate_color_from_formulae(gray, color);
	break;
#endif /* !GPLT_X11_MODE */
    case SMPAL_COLOR_MODE_CUBEHELIX: {
	double phi, a;
	phi = 2. * M_PI * (sm_palette.cubehelix_start/3.
			+  gray * sm_palette.cubehelix_cycles);
	if (sm_palette.gamma != 1.0)
	    gray = pow(gray, 1./sm_palette.gamma);
	a = sm_palette.cubehelix_saturation * gray * (1.-gray) / 2.;
	color->r = gray + a * (-0.14861 * cos(phi) + 1.78277 * sin(phi));
	color->g = gray + a * (-0.29227 * cos(phi) - 0.90649 * sin(phi));
	color->b = gray + a * ( 1.97294 * cos(phi));
	if (color->r > 1.0) color->r = 1.0; if (color->r < 0.0) color->r = 0.0;
	if (color->g > 1.0) color->g = 1.0; if (color->g < 0.0) color->g = 0.0;
	if (color->b > 1.0) color->b = 1.0; if (color->b < 0.0) color->b = 0.0;
	}	
	break;
    default:
	fprintf(stderr, "%s:%d ooops: Unknown colorMode '%c'.\n",
		__FILE__, __LINE__, (char)(sm_palette.colorMode));
    }
}

/*
 *  Map a gray value in [0,1] to the corresponding RGB values in [0,1],
 *  according to the current colorMode and color space.
 *
 *  Note -- November 2003: this routine has been renamed from color_from_gray()
 *  to rgb1_from_gray() in order to more clearly distinguish structures
 *  rgb_color and rgb255_color.
 */
void
rgb1_from_gray(double gray, rgb_color *color)
{
    /* get the color */
    color_components_from_gray(gray, color);
    if (sm_palette.colorMode == SMPAL_COLOR_MODE_GRAY)
	return;

    /* transform to RGB if necessary */
    switch(sm_palette.cmodel) {
    case C_MODEL_RGB:
	break;
    case C_MODEL_HSV:
	HSV_2_RGB(color);
	break;
    case C_MODEL_CMY:
	CMY_2_RGB(color);
	break;
    case C_MODEL_YIQ:
	YIQ_2_RGB(color);
	break;
    case C_MODEL_XYZ:
	CIEXYZ_2_RGB(color);
	break;
    default:
	fprintf(stderr, "%s:%d ooops: Unknown color model '%c'\n",
		__FILE__, __LINE__, (char)(sm_palette.cmodel));
    }
}


/*
 *  Convenience function to map R, G and B float values [0,1] to uchars [0,255].
 */
void
rgb255_from_rgb1(rgb_color rgb1, rgb255_color *rgb255)
{
    rgb255->r = (unsigned char)(255 * rgb1.r + 0.5);
    rgb255->g = (unsigned char)(255 * rgb1.g + 0.5);
    rgb255->b = (unsigned char)(255 * rgb1.b + 0.5);
}


/*
 *  Convenience function to map gray values to R, G and B values in [0,1],
 *  limiting the resolution of sampling a continuous palette to use_maxcolors.
 *
 *  EAM Sep 2010 - Guarantee that for palettes with multiple segments
 *  (created by 'set palette defined ...') the mapped gray value is always
 *  in the appropriate segment.  This has the effect of overriding 
 *  use_maxcolors if the defined palette has more segments than the nominal
 *  limit.
 */
void
rgb1maxcolors_from_gray(double gray, rgb_color *color)
{
    if (sm_palette.use_maxcolors != 0)
	gray = quantize_gray(gray);

    rgb1_from_gray(gray, color);
}

double 
quantize_gray( double gray )
{
    double qgray = floor(gray * sm_palette.use_maxcolors)
		 / (sm_palette.use_maxcolors-1);

    if (sm_palette.colorMode == SMPAL_COLOR_MODE_GRADIENT) {
	int j;
	gradient_struct *g = sm_palette.gradient;
	double small_interval = 1. / sm_palette.use_maxcolors;

	/* Backward compatibility with common case of 1 segment */
	if ((sm_palette.gradient_num <= 2) && (qgray == 0))
	    ;

	/* All palette segments are large compared to the sampling interval.
	 * Simple truncation of gray is good enough.
	 */
	else if (sm_palette.smallest_gradient_interval > small_interval)
	    ;

	/* There is at least one palette segment that is smaller than the sampling
	 * interval. Earlier versions of quantize_gray() handled this case poorly.
	 * This version isn't perfect either.  In particular it fails to handle the
	 * converse problematic case where qgray is inside some small interval but
	 * the true gray value is not.  This causes a color from the narrow segment 
	 *  to be applied incorrectly to neighoring segments as well.
	 */
	else for (j=0; j<sm_palette.gradient_num; j++) {
	    /* Does the true gray value lie in this interval? */
	    if ((gray >= g[j].pos) &&  (gray <  g[j+1].pos)) {
		/* See if it is so small that truncation missed it */
		if ((g[j+1].pos - g[j].pos) < small_interval)
		    qgray = (g[j].pos + g[j+1].pos) / 2.;
		break;
	    }

	}
    }

    return qgray;	
}


/*
 *  Convenience function to map gray values to R, G and B values in [0,255],
 *  taking care of palette maxcolors (i.e., discrete nb of colors).
 */
void
rgb255maxcolors_from_gray(double gray, rgb255_color *rgb255)
{
    rgb_color rgb1;
    rgb1maxcolors_from_gray(gray, &rgb1);
    rgb255_from_rgb1(rgb1, rgb255);
}


/*
 *  Used by approximate_palette
 */
static double
get_max_dev(rgb_color *colors, int j, double limit)
{
    double max_dev = 0.0;
    double rdev, gdev, bdev;
    double r=colors[0].r, g=colors[0].g, b=colors[0].b;
    int i;
    double sr = (colors[j].r - r) / j;
    double sg = (colors[j].g - g) / j;
    double sb = (colors[j].b - b) / j;

    for(i=1; i<j; ++i) {
	double dx = i;

	rdev = fabs(sr*dx + r - colors[i].r);
	gdev = fabs(sg*dx + g - colors[i].g);
	bdev = fabs(sb*dx + b - colors[i].b);
	if(rdev > max_dev)
	    max_dev = rdev;
	if(gdev > max_dev)
	    max_dev = gdev;
	if(bdev > max_dev)
	    max_dev = bdev;
	if(max_dev >= limit)
	    break;
    }
    return max_dev;
}


/*
 *  Used by approximate_palette:  true if one color component in mid
 *  is higher (or lower) than both left and right, flase other wise
 */
static int
is_extremum(rgb_color left, rgb_color mid, rgb_color right)
{
    /* mid is maximum */
    if(left.r < mid.r && mid.r > right.r)
	return 1;
    if(left.g < mid.g && mid.g > right.g)
	return 1;
    if(left.b < mid.b && mid.b > right.b)
	return 1;

    /* mid is minimum */
    if(left.r > mid.r && mid.r < right.r)
	return 1;
    if(left.g > mid.g && mid.g < right.g)
	return 1;
    if(left.b > mid.b && mid.b < right.b)
	return 1;

    return 0;
}


#define GROW_GRADIENT(n) do {						\
    if(cnt == gradient_size) {						\
	gradient_size += (n);						\
	gradient = (gradient_struct*)					\
	    realloc(gradient, gradient_size * sizeof(gradient_struct));	\
    }									\
} while(0)


/*
 *  This function takes a palette and constructs a gradient which can
 *  be used to approximate the palette by linear interpolation.
 *  The palette is sampled at samples+1 points equally spaced in [0,1].
 *  From this huge gradient a much smaler one is constructed by selecting
 *  just those sampling points which still do approximate the full sampled
 *  one well enough.  allowed_deviation determines the maximum deviation
 *  of all color components which is still acceptable for the reduced
 *  gradient.  Use a sufficiently large number of samples (500 to 5000).
 *  Please free() the returned gradient after use.  samples, allowed_deviation
 *  and max_skip may be <=0 and useful defaults will be used.
 *  Most probably it's useless to approximate a gradient- or rgbformulae-
 *  palette.  Use it to build gradients from function palettes.
 */
gradient_struct *
approximate_palette(t_sm_palette *palette, int samples,
		    double allowed_deviation,
		    int *gradient_num)
{
    int i=0, j=0;
    double gray=0;
    int gradient_size=50;
    gradient_struct *gradient;
    int colors_size=100;
    rgb_color *colors;
    int cnt=0;
    rgb_color color;
    double max_dev=0.0;
    /* int maximum_j=0, extrema=0; */

    /* useful defaults */
    if (samples <= 0)
	samples = 2000;
    if (allowed_deviation <= 0)
	allowed_deviation = 0.003;

    gradient = (gradient_struct*)
	malloc(gradient_size * sizeof(gradient_struct));
    colors = (rgb_color*) malloc(colors_size * sizeof(rgb_color));

    /* start (gray=0.0) is needed */
    cnt = 0;
    color_components_from_gray(0.0, colors + 0);
    gradient[0].pos = 0.0;
    gradient[0].col = colors[0];
    ++cnt;
    color_components_from_gray(1.0 / samples, colors + 1);

    for(i = 0; i < samples; ++i) {
	for(j = 2; i + j <= samples; ++j) {
	    gray = ((double) (i + j)) / samples;
	    if (j == colors_size) {
	        colors_size += 50;
		colors = (rgb_color *)
		    realloc(colors, colors_size*sizeof(gradient_struct));
	    }
	    color_components_from_gray(gray, colors + j);

	    /* test for extremum */
	    if(is_extremum(colors[j - 2], colors[j - 1], colors[j])) {
	        /* fprintf(stderr,"Extremum at %g\n", gray); */
	        /* ++extrema; */
		break;
	    }

	    /* to big deviation */
	    max_dev = get_max_dev(colors, j, allowed_deviation);
	    if(max_dev > allowed_deviation) {
	        /* fprintf(stderr,"Control Point at %.3g\n",gray); */
		break;
	    }
	}
	GROW_GRADIENT(25);

	gradient[cnt].pos = gray;
	gradient[cnt].col = colors[j - 1];
	++cnt;

	/* if(j-1 > maximum_j) maximum_j = j-1; */

	colors[0] = colors[j - 1];
	colors[1] = colors[j];
	i += j - 1;
    }

    color_components_from_gray(1.0, &color);
    GROW_GRADIENT(1);
    gradient[cnt].pos = 1.0;
    gradient[cnt].col = color;
    ++cnt;

    /***********
    fprintf(stderr,
	     "PS interpolation table: %d samples, allowed deviation %.2f%%:\n",
	     samples, 100*allowed_deviation);

    fprintf(stderr, "  --> new size %d, %d extrema, max skip %d\n",
	     cnt, extrema, maximum_j);
    ************/
    free(colors);
    *gradient_num = cnt;
    return gradient;  /* don't forget to free() it once you'r done with it */
}

#undef GROW_GRADIENT


/*
 *  Original fixed color transformations
 */
double
GetColorValueFromFormula(int formula, double x)
{
    /* the input gray x is supposed to be in interval [0,1] */
    if (formula < 0) {		/* negate the value for negative formula */
	x = 1 - x;
	formula = -formula;
    }
    switch (formula) {
    case 0:
	return 0;
    case 1:
	return 0.5;
    case 2:
	return 1;
    case 3:			/* x = x */
	break;
    case 4:
	x = x * x;
	break;
    case 5:
	x = x * x * x;
	break;
    case 6:
	x = x * x * x * x;
	break;
    case 7:
	x = sqrt(x);
	break;
    case 8:
	x = sqrt(sqrt(x));
	break;
    case 9:
	x = sin(90 * x * DEG2RAD);
	break;
    case 10:
	x = cos(90 * x * DEG2RAD);
	break;
    case 11:
	x = fabs(x - 0.5);
	break;
    case 12:
	x = (2 * x - 1) * (2.0 * x - 1);
	break;
    case 13:
	x = sin(180 * x * DEG2RAD);
	break;
    case 14:
	x = fabs(cos(180 * x * DEG2RAD));
	break;
    case 15:
	x = sin(360 * x * DEG2RAD);
	break;
    case 16:
	x = cos(360 * x * DEG2RAD);
	break;
    case 17:
	x = fabs(sin(360 * x * DEG2RAD));
	break;
    case 18:
	x = fabs(cos(360 * x * DEG2RAD));
	break;
    case 19:
	x = fabs(sin(720 * x * DEG2RAD));
	break;
    case 20:
	x = fabs(cos(720 * x * DEG2RAD));
	break;
    case 21:
	x = 3 * x;
	break;
    case 22:
	x = 3 * x - 1;
	break;
    case 23:
	x = 3 * x - 2;
	break;
    case 24:
	x = fabs(3 * x - 1);
	break;
    case 25:
	x = fabs(3 * x - 2);
	break;
    case 26:
	x = (1.5 * x - 0.5);
	break;
    case 27:
	x = (1.5 * x - 1);
	break;
    case 28:
	x = fabs(1.5 * x - 0.5);
	break;
    case 29:
	x = fabs(1.5 * x - 1);
	break;
    case 30:
	if (x <= 0.25)
	    return 0;
	if (x >= 0.57)
	    return 1;
	x = x / 0.32 - 0.78125;
	break;
    case 31:
	if (x <= 0.42)
	    return 0;
	if (x >= 0.92)
	    return 1;
	x = 2 * x - 0.84;
	break;
    case 32:
	if (x <= 0.42)
	    x *= 4;
	else
	    x = (x <= 0.92) ? -2 * x + 1.84 : x / 0.08 - 11.5;
	break;
    case 33:
	x = fabs(2 * x - 0.5);
	break;
    case 34:
	x = 2 * x;
	break;
    case 35:
	x = 2 * x - 0.5;
	break;
    case 36:
	x = 2 * x - 1;
	break;
	/*
	   IMPORTANT: if any new formula is added here, then:
	   (1) its postscript counterpart must be added to the array
	   ps_math_color_formulae[] below.
	   (2) number of colours must be incremented in color.c: variable
	   sm_palette, first item---search for "t_sm_palette sm_palette = "
	 */
    default:
	/* Cannot happen! */
	FPRINTF((stderr, "gnuplot:  invalid palette rgbformula"));
	x = 0;
    }
    if (x <= 0)
	return 0;
    if (x >= 1)
	return 1;
    return x;
}


/* Implementation of pm3dGetColorValue() in the postscript way.
   Notice that the description, i.e. the part after %, is important
   since it is used in `show pm3d' for displaying the analytical formulae.
   The postscript formulae will be expanded into lines like:
	"/cF0 {pop 0} bind def\t% 0",
	"/cF4 {dup mul} bind def\t% x^2",
*/

const char *ps_math_color_formulae[] = {
    /* /cF0  */ "pop 0", "0",
    /* /cF1  */ "pop 0.5", "0.5",
    /* /cF2  */ "pop 1", "1",
    /* /cF3  */ " ", "x",
    /* /cF4  */ "dup mul", "x^2",
    /* /cF5  */ "dup dup mul mul", "x^3",
    /* /cF6  */ "dup mul dup mul", "x^4",
    /* /cF7  */ "sqrt", "sqrt(x)",
    /* /cF8  */ "sqrt sqrt", "sqrt(sqrt(x))",
    /* /cF9  */ "90 mul sin", "sin(90x)",
    /* /cF10 */ "90 mul cos", "cos(90x)",
    /* /cF11 */ "0.5 sub abs", "|x-0.5|",
    /* /cF12 */ "2 mul 1 sub dup mul", "(2x-1)^2",
    /* /cF13 */ "180 mul sin", "sin(180x)",
    /* /cF14 */ "180 mul cos abs", "|cos(180x)|",
    /* /cF15 */ "360 mul sin", "sin(360x)",
    /* /cF16 */ "360 mul cos", "cos(360x)",
    /* /cF17 */ "360 mul sin abs", "|sin(360x)|",
    /* /cF18 */ "360 mul cos abs", "|cos(360x)|",
    /* /cF19 */ "720 mul sin abs", "|sin(720x)|",
    /* /cF20 */ "720 mul cos abs", "|cos(720x)|",
    /* /cF21 */ "3 mul", "3x",
    /* /cF22 */ "3 mul 1 sub", "3x-1",
    /* /cF23 */ "3 mul 2 sub", "3x-2",
    /* /cF24 */ "3 mul 1 sub abs", "|3x-1|",
    /* /cF25 */ "3 mul 2 sub abs", "|3x-2|",
    /* /cF26 */ "1.5 mul .5 sub", "(3x-1)/2",
    /* /cF27 */ "1.5 mul 1 sub", "(3x-2)/2",
    /* /cF28 */ "1.5 mul .5 sub abs", "|(3x-1)/2|",
    /* /cF29 */ "1.5 mul 1 sub abs", "|(3x-2)/2|",
    /* /cF30 */ "0.32 div 0.78125 sub", "x/0.32-0.78125",
    /* /cF31 */ "2 mul 0.84 sub", "2*x-0.84",
    /* /cF32 */ "dup 0.42 le {4 mul} {dup 0.92 le {-2 mul 1.84 add} {0.08 div 11.5 sub} ifelse} ifelse",
    "4x;1;-2x+1.84;x/0.08-11.5",
    /* /cF33 */ "2 mul 0.5 sub abs", "|2*x - 0.5|",
    /* /cF34 */ "2 mul", "2*x",
    /* /cF35 */ "2 mul 0.5 sub", "2*x - 0.5",
    /* /cF36 */ "2 mul 1 sub", "2*x - 1",
    "", ""};


/*
 *  Color Conversion Algorithms
 *  taken from http://www.cs.rit.edu/~ncs/color/t_convert.html
 *  each color model should provide a conversion to RGB.
 *  RGB values are clipped to [0,1] as some colors in some
 *  models have no RGB value.
 */
static void
CMY_2_RGB(rgb_color *col)
{
    double c,m,y;
    c = col->r;
    m = col->g;
    y = col->b;
    col->r = CONSTRAIN(1.0 - c);
    col->g = CONSTRAIN(1.0 - m);
    col->b = CONSTRAIN(1.0 - y);
}

static void
CIEXYZ_2_RGB(rgb_color *col)
{
    double x,y,z;
    
    x = col->r;
    y = col->g;
    z = col->b;
    col->r = CONSTRAIN( 1.9100 * x - 0.5338 * y - 0.2891 * z);
    col->g = CONSTRAIN(-0.9844 * x + 1.9990 * y - 0.0279 * z);
    col->b = CONSTRAIN( 0.0585 * x - 0.1187 * y - 0.9017 * z);
}

static void
YIQ_2_RGB(rgb_color *col)
{
    double y,i,q;
    
    y = col->r;
    i = col->g;
    q = col->b;
    col->r = CONSTRAIN(y - 0.956 * i + 0.621 * q);
    col->g = CONSTRAIN(y - 0.272 * i - 0.647 * q);
    col->b = CONSTRAIN(y - 1.105 * i - 1.702 * q);
}

static void
HSV_2_RGB(rgb_color *col)
{
    double h,s,v, f,p,q,t;
    int i;

    h = col->r;
    s = col->g;
    v = col->b;
    if (s == 0) { /* achromatic (gray) */
        col->r = col->g = col->b = v;
	return;
    }

    h *= 6.;  /* h range in gnuplot is [0,1] and not the usual [0,360] */
    i = floor(h);
    f = h - i;
    p = v * (1.0 - s);
    q = v * (1.0 - s*f);
    t = v * (1.0 - s*(1.0-f));
    switch(i % 6) {
    case 0:
	col->r = v;
	col->g = t;
	col->b = p;
	break;
    case 1:
	col->r = q;
	col->g = v;
	col->b = p;
	break;
    case 2:
	col->r = p;
	col->g = v;
	col->b = t;
	break;
    case 3:
	col->r = p;
	col->g = q;
	col->b = v;
	break;
    case 4:
	col->r = t;
	col->g = p;
	col->b = v;
	break;
    default:
	col->r = v;
	col->g = p;
	col->b = q;
	break;
    }
}
#undef CONSTRAIN

/*
 * Support for user-callable rgb = hsv2rgb(h,s,v)
 */
unsigned int 
hsv2rgb ( rgb_color *color )
{
    HSV_2_RGB(color);

    return    ((unsigned int)(255.*color->r) << 16) 
	    + ((unsigned int)(255.*color->g) << 8)
	    + ((unsigned int)(255.*color->b));
}

/* eof getcolor.c */
