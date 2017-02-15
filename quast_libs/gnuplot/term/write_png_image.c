/*
 * Dump an image block as a png image.
 * This routine is used by several terminal drivers so it gets a file by itself.
 */
#ifdef  TERM_BODY
#ifndef WRITE_PNG_IMAGE
#define WRITE_PNG_IMAGE

#ifdef HAVE_CAIROPDF

#include "cairo-pdf.h"
#include "wxterminal/gp_cairo.h"
#include "wxterminal/gp_cairo_helpers.h"

/* cairo PNG code */
static int 
write_png_image (unsigned m, unsigned n, coordval *image, t_imagecolor color_mode, const char *filename) {
  cairo_surface_t *image_surface;
  cairo_status_t cairo_stat;
  unsigned int *image255;

  image255 = gp_cairo_helper_coordval_to_chars(image, m, n, color_mode);
  image_surface = cairo_image_surface_create_for_data((unsigned char*) image255, CAIRO_FORMAT_ARGB32, m, n, 4*m);
  if ((cairo_stat = cairo_surface_write_to_png(image_surface, filename)) != CAIRO_STATUS_SUCCESS) {
    os_error(NO_CARET, "write_png_image cairo: could not write image file '%s': %s.", filename, cairo_status_to_string(cairo_stat));
  }
  cairo_surface_destroy(image_surface);
  return 0;
}

#else      /* libgd PNG code mainly taken from gd.trm */
#include <gd.h>

static int 
write_png_image (unsigned M, unsigned N, coordval *image, t_imagecolor color_mode, const char *filename) {
  int m, n, pixel;
  unsigned int rgb;
  gdImagePtr im;
  FILE *out;

  im = gdImageCreateTrueColor(M, N);
  if (!im)
    int_error(NO_CARET, "libgd: failed to create image structure");
  /* gdImageColorAllocateAlpha(im, 255, 255, 255, 127); */
  gdImageSaveAlpha(im, 1);
  gdImageAlphaBlending(im, 0);

  if (color_mode == IC_RGBA) {
    /* RGB + Alpha channel */
    for (n=0; n<N; n++) {
      for (m=0; m<M; m++) {
        rgb_color rgb1;
        rgb255_color rgb255;
        int alpha;
        rgb1.r = *image++;
        rgb1.g = *image++;
        rgb1.b = *image++;
        alpha  = *image++;
        alpha  = 127 - (alpha>>1);  /* input is [0:255] but gd wants [127:0] */
        rgb255_from_rgb1( rgb1, &rgb255 );
        pixel = gdImageColorResolveAlpha(im, (int)rgb255.r, (int)rgb255.g, (int)rgb255.b, alpha);
        gdImageSetPixel( im, m, n, pixel );
      }
    }
  } else if (color_mode == IC_RGB) {
    /* TrueColor 24-bit color mode */
    for (n=0; n<N; n++) {
      for (m=0; m<M; m++) {
        rgb_color rgb1;
        rgb255_color rgb255;
        rgb1.r = *image++;
        rgb1.g = *image++;
        rgb1.b = *image++;
        rgb255_from_rgb1( rgb1, &rgb255 );
        pixel = gdImageColorResolve(im, (int)rgb255.r, (int)rgb255.g, (int)rgb255.b );
        gdImageSetPixel( im, m, n, pixel );
      }
    }
  } else if (color_mode == IC_PALETTE) {
    /* Palette color lookup from gray value */
    for (n=0; n<N; n++) {
      for (m=0; m<M; m++) {
        rgb255_color rgb;
        if (isnan(*image)) {
          /* FIXME: tried to take the comment from gd.trm into account but needs a testcase */
          pixel = gdImageColorResolveAlpha(im, 0, 0, 0, 127);
          image++;
        } else {
          rgb255maxcolors_from_gray( *image++, &rgb );
          pixel = gdImageColorResolve( im, (int)rgb.r, (int)rgb.g, (int)rgb.b );
        }
        gdImageSetPixel( im, m, n, pixel );
      }
    }
  }

  out = fopen(filename, "wb");
  if (!out) {
    os_error(NO_CARET, "write_png_image libgd: could not write image file '%s'", filename);
  }
  gdImagePng(im, out);
  fclose(out);
  gdImageDestroy(im);

  return 0;
}
#endif
#endif
#endif
