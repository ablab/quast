/*
 * $Id: wcommon.h,v 1.19.2.1 2016/03/22 16:48:33 markisch Exp $
 */

/* GNUPLOT - wcommon.h */

/*[
 * Copyright 1992 - 1993, 1998, 2004 Maurice Castro, Russell Lang
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

/*
 * AUTHORS
 *
 *   Maurice Castro
 *   Russell Lang
 */

#ifndef GNUPLOT_WCOMMON_H
#define GNUPLOT_WCOMMON_H

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY       5
#endif

/* maximum number of plots which can be enabled/disabled via toolbar */
#define MAXPLOTSHIDE 10

#ifdef __cplusplus
extern "C" {
#endif

/* winmain.c */
# define PACKVERSION(major,minor) MAKELONG(minor,major)
extern DWORD GetDllVersion(LPCTSTR lpszDllName);
extern BOOL IsWindowsXPorLater(void);
extern char *appdata_directory(void);
extern FILE *open_printer();
extern void close_printer(FILE *outfile);
extern BOOL cp_changed;
extern UINT cp_input;
extern UINT cp_output;

/* wgnuplib.c */
extern HINSTANCE hdllInstance;
extern LPSTR szParentClass;
extern LPSTR szTextClass;
extern LPSTR szPauseClass;
extern LPSTR szGraphClass;
extern LPSTR szAboutClass;

void NEAR * LocalAllocPtr(UINT flags, UINT size);
void NEAR * LocalReAllocPtr(void NEAR * ptr, UINT flags, UINT size);
void LocalFreePtr(void NEAR *ptr);
LPSTR GetInt(LPSTR str, LPINT pval);

/* wtext.c */
void WriteTextIni(LPTW lptw);
void ReadTextIni(LPTW lptw);
void DragFunc(LPTW lptw, HDROP hdrop);
void TextShow(LPTW lptw);

/* wmenu.c - Menu */
void SendMacro(LPTW lptw, UINT m);
void LoadMacros(LPTW lptw);
void CloseMacros(LPTW lptw);

/* wprinter.c - Printer setup and dump */
BOOL PrintSize(HDC printer, HWND hwnd, LPRECT lprect);
void PrintRegister(GP_LPPRINT lpr);
void PrintUnregister(GP_LPPRINT lpr);
BOOL CALLBACK PrintAbortProc(HDC hdcPrn, int code);
INT_PTR CALLBACK PrintDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

/* wgraph.c */
unsigned luma_from_color(unsigned red, unsigned green, unsigned blue);
void add_tooltip(LPGW lpgw, PRECT rect, LPWSTR text);
void clear_tooltips(LPGW lpgw);
void draw_update_keybox(LPGW lpgw, unsigned plotno, unsigned x, unsigned y);
int draw_enhanced_text(LPGW lpgw, HDC hdc, LPRECT rect, int x, int y, const char * str);
void draw_get_enhanced_text_extend(PRECT extend);
void draw_image(LPGW lpgw, HDC hdc, char *image, POINT corners[4], unsigned int width, unsigned int height, int color_mode);
void SetFont(LPGW lpgw, HDC hdc);
void GraphChangeFont(LPGW lpgw, LPCSTR font, int fontsize, HDC hdc, RECT rect);
LPWSTR UnicodeText(const char *str, enum set_encoding_id encoding);

#ifdef __cplusplus
}
#endif

#endif /* GNUPLOT_WCOMMON_H */
