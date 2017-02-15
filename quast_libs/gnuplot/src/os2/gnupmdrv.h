/*
** static char RCSid[]="$Id: gnupmdrv.h,v 1.8 2005/07/28 07:46:06 mikulik Exp $" ;
*/

/* PM driver for GNUPLOT */

/*[
 * Copyright 1992, 1993, 1998, 2004 Roger Fearick
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
 * AUTHOR
 *
 *   Gnuplot driver for OS/2:  Roger Fearick
 */

#include "config.h"
/* include resource defines */

#ifndef DISPDEFS_H
/*#include "dispdefs.h"*/
#include "dialogs.h"
#endif

/*==== own window messages  =================================================*/

#define WM_GNUPLOT          (WM_USER+20)
#define WM_PAUSEPLOT        (WM_USER+21)
#define WM_PAUSEEND         (WM_USER+22)
#define WM_GPSTART          (WM_USER+23)
#define WM_USER_SET_DATA    (WM_USER+90)
#define WM_USER_GET_DATA    (WM_USER+91)
#define WM_USER_CHGFONT     (WM_USER+10)
#define WM_USER_PRINT_BEGIN (WM_USER+200)
#define WM_USER_PRINT_OK    (WM_USER+201)
#define WM_USER_PRINT_ERROR (WM_USER+202)
#define WM_USER_DEV_ERROR   (WM_USER+203)
#define WM_USER_PRINT_QBUSY (WM_USER+204)
#define WM_USER_PRINT_CANCEL (WM_USER+205)

/*==== various names ========================================================*/

#define GNUPIPE     "\\pipe\\gnuplot"       /* named pipe to gnuplot */
#define GNUQUEUE    "\\queues\\gnuplot"     /* queue for gnuplot termination */
#define GNUSEM      "\\sem32\\gnuplot.sem"  /* synch gnuplot and gnupmdrv */
#define GNUINI      "GNUPMDRV.INI"          /* ini filename */
#define GNUEXEFILE  "gnuplot.exe"           /* exe file name */
#define GNUHELPFILE "gnuplot.gih"           /* help file name */
#define GNUTERMINIT "GNUTERM=pm"            /* terminal setup string */
#define INITIAL_FONT "14.Helvetica"         /* initial font for plots */
#define APP_NAME     "GnuplotPM"            /* application name */
#define CHILD_NAME   "GnupltChild"          /* child window name */

        /* profile (ini file) names  */
#define INISHELLPOS  "PosShell"
#define INIPAUSEPOS  "PosPause"
#define INIPLOTPOS   "PosPlot"
#define INIFONT      "DefFont"
#define INIFRAC      "PageFrac"
#define INIPRDRIV    "DrivData"
#define INIPRPR      "Printer"
#define INIOPTS      "DefOpts"
#define INICHAR      "Fontdata"
#define INIKEEPRATIO "KeepRatio"	/* PM */
#define INIUSEMOUSE  "UseMouse"	/* PM */
#define INIMOUSECOORD "MouseCoord"	/* PM */


/*==== global data  ==========================================================*/

HAB         hab ;               /* application anchor block handle */
HWND   	    hApp ;          /* application window handle */
HWND        hwndFrame ;         /* frame window handle */

#define   FONTBUF   256         /* buffer for dropped font namesize */
#define     GNUXPAGE  19500     /* width of plot area in 0.01 cm */
#define     GNUYPAGE  12500     /* height of plot area in 0.01 cm */

extern char szIPCName[];       /* name used in IPC with gnuplot */
extern char szIniFile[256];    /* full path of ini file */
extern int  bServer;
extern int  bPersist;
extern int  bWideLines;
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
extern int  bEnhanced;
#endif

/*==== stuff for querying printer capability =================================*/

typedef struct {  /* query data for printer setup */
    int   cbStruct ;       /* size of struct */
    float xsize ;
    float ysize ;
    float xfrac ;
    float yfrac ;
    short caps ;
    char  szFilename[CCHMAXPATHCOMP] ;
    char  szPrinterName[128] ;
    PPRQINFO3 piPrinter ;
    int   cbpdriv ;
    PDRIVDATA pdriv ;
    } QPRINT, *PQPRINT ;

#define QP_CAPS_NORMAL 0
#define QP_CAPS_FILE   1   /* can print to file */

/*==== stuff for pause dialogs =================================*/

typedef struct {  /* pause data for dialog box */
    int   cbStruct ;       /* size of struct */
    char  *pszMessage ;    /* pause message */
    PSWP  pswp ;           /* dialog box position */
    } PAUSEDATA, *PPAUSEDATA ;

/*==== function declarations =================================================*/

short            ScalePS( HPS ) ;
int              SetupPrinter( HWND, PQPRINT ) ;
HDC              OpenPrinterDC( HAB, PQPRINT, LONG, char* ) ;
int              SetPrinterMode( HWND, PQPRINT ) ;
MPARAM           PrintCmdProc( HWND, ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY PrintDlgProc( HWND, ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY PauseMsgDlgProc( HWND, ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY QFontDlgProc( HWND ,ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY QPrintDlgProc (HWND, ULONG, MPARAM, MPARAM) ;
MRESULT EXPENTRY QPrintersDlgProc ( HWND, ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY DisplayClientWndProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY NewFrameWndProc(HWND, ULONG, MPARAM, MPARAM) ;
MRESULT EXPENTRY About(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY CancelPrintDlgProc ( HWND, ULONG, MPARAM, MPARAM ) ;
MRESULT EXPENTRY SendCommandDlgProc( HWND, ULONG, MPARAM, MPARAM ) ;

        /* own window functions... */
void WinSetDlgItemFloat( HWND, USHORT, float ) ;
void WinSetDlgItemFloatF( HWND, USHORT, int, float ) ;
void WinQueryDlgItemFloat( HWND, USHORT, float* ) ;
