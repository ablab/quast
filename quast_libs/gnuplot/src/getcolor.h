/*
 * $Id: getcolor.h,v 1.11 2012/10/30 04:43:42 sfeam Exp $
 */

/* GNUPLOT - getcolor.h */

/* Routines + constants for mapping interval [0,1] into another [0,1] to be
 * used to get RGB triplets from gray (palette of smooth colours).
 *
 * Note: The code in getcolor.h,.c cannot be inside color.h,.c since gplt_x11.c
 * compiles with getcolor.o, so it cannot load the other huge staff.
 *
 */

/*[
 *
 * Petr Mikulik, since December 1998
 * Copyright: open source as much as possible
 *
]*/


#ifndef GETCOLOR_H
#define GETCOLOR_H

#include "syscfg.h"
#include "color.h"

enum color_models_id {
    C_MODEL_RGB = 'r',
    C_MODEL_HSV = 'h',
    C_MODEL_CMY = 'c',
    C_MODEL_YIQ = 'y',
    C_MODEL_XYZ = 'x'
};


/* main gray --> rgb color mapping */
void rgb1_from_gray __PROTO(( double gray, rgb_color *color ));
void rgb255_from_rgb1 __PROTO(( rgb_color rgb1, rgb255_color *rgb255 ));
/* main gray --> rgb color mapping as above, with take care of palette maxcolors */
void rgb1maxcolors_from_gray __PROTO(( double gray, rgb_color *color ));
void rgb255maxcolors_from_gray __PROTO(( double gray, rgb255_color *rgb255 ));
double quantize_gray __PROTO(( double gray ));

/* HSV --> RGB user-visible function hsv2rgb(h,s,v) */
unsigned int hsv2rgb __PROTO(( rgb_color *color ));

/* used to (de-)serialize color/gradient information */
char *gradient_entry_to_str __PROTO(( gradient_struct *gs ));
void str_to_gradient_entry __PROTO(( char *s, gradient_struct *gs ));

/* check if two palettes p1 and p2 differ */
int palettes_differ __PROTO(( t_sm_palette *p1, t_sm_palette *p2 ));

/* construct minimal gradient to approximate palette */
gradient_struct *approximate_palette __PROTO(( t_sm_palette *palette, int maxsamples, double allowed_deviation, int *gradient_num ));

double GetColorValueFromFormula __PROTO((int formula, double x));

extern const char *ps_math_color_formulae[];

#endif /* GETCOLOR_H */

/* eof getcolor.h */
