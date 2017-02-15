/* GNUPLOT - qt_term.h */

/*[
 * Copyright 2009   Jérôme Lodewyck
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

/* ------------------------------------------------------
 * Declarations of the C++ functions
 * used in qt.trm and defined in qt_gui.cpp
 * ------------------------------------------------------*/

#ifndef GNUPLOT_QT_TERM_H
#define GNUPLOT_QT_TERM_H

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

void qt_options(void);
void qt_init(void);
void qt_reset(void);
void qt_text_wrapper(void);
void qt_graphics(void);
void qt_move(unsigned int x, unsigned int y);
void qt_vector(unsigned int x, unsigned int y);
void qt_linetype(int linetype);
void qt_dashtype (int type, t_dashtype *custom_dash_type);
void qt_put_text(unsigned int x, unsigned int y, const char *str);
int  qt_text_angle(int ang);
int  qt_justify_text(enum JUSTIFY mode);
void qt_point(unsigned int x, unsigned int y, int pointstyle);
int  qt_set_font(const char *font);
void qt_pointsize(double ptsize);
void qt_text(void);
void qt_fillbox(int style, unsigned int x1, unsigned int y1, unsigned int width, unsigned int height);
void qt_linewidth(double linewidth);
# ifdef USE_MOUSE
int  qt_waitforinput(int);
void qt_put_tmptext(int, const char str[]);
void qt_set_ruler(int x, int y);
void qt_set_cursor(int, int, int);
void qt_set_clipboard(const char s[]);
# endif /* USE_MOUSE */
int  qt_make_palette(t_sm_palette *palette);
void qt_set_color(t_colorspec *colorspec);
void qt_filled_polygon(int n, gpiPoint * corners);
void qt_image(unsigned int M, unsigned int N, coordval * image, gpiPoint * corner, t_imagecolor color_mode);
void qt_enhanced_open(char* fontname, double fontsize, double base, TBOOLEAN widthflag, TBOOLEAN showflag, int overprint);
void qt_enhanced_flush();
void qt_enhanced_writec(int c);
void qt_layer(t_termlayer layer);
void qt_hypertext(int type, const char *text);
#ifdef EAM_BOXED_TEXT
void qt_boxed_text(unsigned int x, unsigned int y, int option);
#endif
void qt_modify_plots(unsigned int ops, int plotno);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* GNUPLOT_QT_TERM_H */
