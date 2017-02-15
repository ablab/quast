/* GNUPLOT - gplt_x11.h */

/*[
 * Copyright 2000   Thomas Williams, Colin Kelley
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

#ifndef GNUPLOT_GPLT_X11_H
#define GNUPLOT_GPLT_X11_H

#if defined(HAVE_SYS_SELECT_H) && !defined(VMS)
# include <sys/select.h>
#endif

#ifndef FD_SET
# define FD_SET(n, p)    ((p)->fds_bits[0] |= (1 << ((n) % 32)))
# define FD_CLR(n, p)    ((p)->fds_bits[0] &= ~(1 << ((n) % 32)))
# define FD_ISSET(n, p)  ((p)->fds_bits[0] & (1 << ((n) % 32)))
# define FD_ZERO(p)      memset((char *)(p), '\0', sizeof(*(p)))
#endif /* not FD_SET */

#define X11_COMMAND_BUFFER_LENGTH 1024

/* The endian value sent across the pipe can't have '\n' or '\0' within. */
#define ENDIAN_VALUE 0x50515253
#define X11_GR_CHECK_ENDIANESS  'B'

#define X11_GR_MAKE_PALETTE     'p'
#define X11_GR_RELEASE_PALETTE  'e'
#define X11_GR_SET_COLOR        'k'
#define X11_GR_BINARY_COLOR     'c'
#define X11_GR_SET_RGBCOLOR     'g'
#define X11_GR_SET_LINECOLOR    'l'
#define X11_GR_BINARY_POLYGON   'f'
#define X11_GR_IMAGE		'i' /* indicates an image chunk */
#define X11_GR_IMAGE_END	'j' /* indicates that we just sent the last chunk in an image */

#ifdef EXTERNAL_X11_WINDOW
#define X11_GR_SET_WINDOW_ID    'w'
#endif

/* One character for function code, and perhaps one or two for the core
 * routine to do something strange with end of buffer.  So shorten by a
 * few by trial and error.
 */
#define BINARY_MAX_CHAR_PER_TRANSFER (X11_COMMAND_BUFFER_LENGTH-3)

/* Encoding character for removing '\n' and '\0' from data stream.
 * The strategy is as follows, we pick a character half way between
 * '\0' and '\n'.  Then the encoded characters occupy a range
 * of 10.  Next, by observing statistics of the occurrances
 * of the encoded characters in the binary data, the translation
 * character is chosen so that these encoded characters are moved
 * to locations having the least occurances of bytes.
 */
#define SET_COLOR_CODE_CHAR      5
#define FILLED_POLYGON_CODE_CHAR 5
#define IMAGE_CODE_CHAR          1

/* Translation value for making '\0', '\n' and the CODE_CHAR
 * less prevalent in the encoded data.
 */
#define SET_COLOR_TRANSLATION_CHAR         3
#define FILLED_POLYGON_TRANSLATION_CHAR  -13
#define IMAGE_TRANSLATION_CHAR            45

/* Maximum image plane value of data transfer.  */
#define IMAGE_PALETTE_VALUE_MAX USHRT_MAX

#endif /* GNUPLOT_GPLT_X11_H */
