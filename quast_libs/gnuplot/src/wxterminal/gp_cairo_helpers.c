/*
 * $Id: gp_cairo_helpers.c,v 1.4 2013/10/25 04:45:23 sfeam Exp $
 */

/* GNUPLOT - gp_cairo_helpers.c */

/*[
 * Copyright 2009   Timothee Lecomte
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above. If you wish to allow
 * use of your version of this file only under the terms of the GPL and not
 * to allow others to use your version of this file under the above gnuplot
 * license, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the GPL. If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the GPL or the gnuplot license.
]*/

#include "gp_cairo_helpers.h"

#include "alloc.h"
/* for rgb functions */
# include "getcolor.h"

unsigned int * gp_cairo_helper_coordval_to_chars(coordval* image, int M, int N, t_imagecolor color_mode)
{
	int m,n;
	unsigned int *image255, *image255copy;
	rgb_color rgb1;
	rgb255_color rgb255;

	/* cairo image buffer, upper bits are alpha, then r, g and b
	 * Depends on endianess */
	image255 = (unsigned int*) malloc(M*N*sizeof(unsigned int));
	if (!image255) { fprintf(stderr,"cairo terminal: out of memory!\n"); gp_exit(EXIT_FAILURE);}
	image255copy = image255;

	/* TrueColor 24-bit plot->color mode */
	if (color_mode == IC_RGB) {
		for (n=0; n<N; n++) {
		for (m=0; m<M; m++) {
			rgb1.r = *image++;
			rgb1.g = *image++;
			rgb1.b = *image++;
			rgb255_from_rgb1( rgb1, &rgb255 );
			*image255copy++ = (0xFF<<24) + (rgb255.r<<16) + (rgb255.g<<8) + rgb255.b;
		}
		}
	} else if (color_mode == IC_RGBA) {
		unsigned char alpha255;
		double alpha1;
		for (n=0; n<N; n++) {
		for (m=0; m<M; m++) {
			alpha255 = *(image+3);
			alpha1 = (float)alpha255 / 255.;
			rgb1.r = alpha1 * (*image++);
			rgb1.g = alpha1 * (*image++);
			rgb1.b = alpha1 * (*image++);
			image++;
			rgb255_from_rgb1( rgb1, &rgb255 );
			*image255copy++ = (alpha255<<24)
					+ (rgb255.r<<16) + (rgb255.g<<8) + rgb255.b;
		}
		}
	/* Palette plot->color lookup from gray value */
	} else {
		for (n=0; n<N; n++) {
		for (m=0; m<M; m++) {
			if (isnan(*image)) {
				image++;
				*image255copy++ = 0x00000000;
			} else {
				rgb255maxcolors_from_gray( *image++, &rgb255 );
				*image255copy++ = (0xFF<<24) + (rgb255.r<<16) + (rgb255.g<<8) + rgb255.b;
			}
		}
		}
	}

	return image255;
}
