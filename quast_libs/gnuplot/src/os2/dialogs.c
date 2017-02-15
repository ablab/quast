#ifdef INCRCSDATA
static char RCSid[]="$Id: dialogs.c,v 1.7 2005/01/04 13:01:34 mikulik Exp $" ;
#endif

/****************************************************************************

    PROGRAM: gnupmdrv

        Outboard PM driver for GNUPLOT 3.x

    MODULE:  dialogs.c  Dialog procedures for gnupmdrv

****************************************************************************/

/* PM driver for GNUPLOT */

/*[
 * Copyright 1992, 1993, 1998, 2004   Roger Fearick
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

#define INCL_PM
#define INCL_WIN
#define INCL_DEV
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSYS
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINTRACKRECT
#define INCL_WINENTRYFIELDS
#define INCL_WINWINDOWMGR
#include <os2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gnupmdrv.h"

/* struct for printer capabilities */

static struct {
    long    lTech ;     // printer technology
    long    lVer ;      // driver version
    long    lWidth ;    // page width in pels
    long    lHeight ;   // page height in pels
    long    lWChars ;   // page width in chars
    long    lHChars ;   // page height in chars
    long    lHorRes ;   // horizontal resolution pels / metre
    long    lVertRes ;  // vertical resolution pels / metre
    } prCaps ;

ULONG GetPrinters( PPRQINFO3* pprq, ULONG *pcTot  ) ;


MRESULT EXPENTRY QPrintDlgProc ( HWND hwnd, ULONG usMsg, MPARAM mp1, MPARAM mp2 )
/*
**  Query print area and printer setup
*/
    {
    static PQPRINT pqp = NULL ;
    static SWP     swp ;
    static RECTL   rectlDef ;
    TRACKINFO   ti ;
    RECTL       rectlBox ;
    HDC         hdc ;
    ULONG       ulStyle ;
    char        *psz ;

    switch ( usMsg ) {

        case WM_INITDLG :

            pqp = (PQPRINT) PVOIDFROMMP( mp2 ) ;
            if( pqp->caps & QP_CAPS_FILE ) {
                ulStyle = WinQueryWindowULong( WinWindowFromID( hwnd, IDD_PRINTQNAME ),
                                               QWL_STYLE ) ;
                WinSetWindowULong( WinWindowFromID( hwnd, IDD_PRINTQNAME ),
                                   QWL_STYLE, ulStyle ^ DT_HALFTONE  ) ;
                }
            else {
                WinSendMsg( WinWindowFromID( hwnd, IDD_QPRNAME ),
                            EM_SETREADONLY, (MPARAM)TRUE, 0L ) ;
                }
            WinQueryWindowRect( WinWindowFromID( hwnd, IDD_QPRBOX ),
                                &rectlDef ) ;

        case WM_USER_SET_DATA :

            psz = *pqp->piPrinter->pszComment ? pqp->piPrinter->pszComment :
                                        pqp->piPrinter->pszName ;
            WinSetDlgItemText( hwnd, IDD_PRINTNAME, psz ) ;
            WinSetDlgItemFloatF( hwnd, IDD_QPRXSIZE, 1, pqp->xsize ) ;
            WinSetDlgItemFloatF( hwnd, IDD_QPRYSIZE, 1, pqp->ysize ) ;
            WinSetDlgItemFloat( hwnd, IDD_QPRXFRAC, pqp->xfrac ) ;
            WinSetDlgItemFloat( hwnd, IDD_QPRYFRAC, pqp->yfrac ) ;
            WinQueryWindowPos( WinWindowFromID( hwnd, IDD_QPRBOX ),
                               &swp ) ;
            {
            int y = rectlDef.yTop ;
            rectlBox = rectlDef ;
            if( pqp->xsize < pqp->ysize )
                rectlBox.xRight = rectlDef.yTop * pqp->xsize/pqp->ysize ;
            else {
                int x = rectlDef.yTop * pqp->ysize/pqp->xsize ;
                rectlBox.yTop = x ;
                rectlBox.xRight = y ;
                }
            }
            WinSetWindowPos( WinWindowFromID( hwnd, IDD_QPRBOX ),
                             NULLHANDLE, 0, 0, (short) rectlBox.xRight, (short)rectlBox.yTop, SWP_SIZE ) ;
            {
            double ratio = 1.560 ;
            double xs = rectlBox.xRight - rectlBox.xLeft ;
            double ys = rectlBox.yTop - rectlBox.yBottom ;
            if( ys > xs/ratio ) { /* reduce ys to fit */
                 rectlBox.yTop = rectlBox.yBottom + (int)(xs/ratio) ;
                 }
            else if( ys < xs/ratio ) { /* reduce xs to fit */
                 rectlBox.xRight = rectlBox.xLeft + (int)(ys*ratio) ;
                 }
            }
            rectlBox.xRight *= pqp->xfrac ;
            rectlBox.yTop *= pqp->yfrac ;
            WinSetWindowPos( WinWindowFromID( hwnd, IDD_QPRFRAME ),
                             NULLHANDLE,
                             swp.x, swp.y, //+(short)(swp.cy*(1.0-pqp->yfrac)),
                             (short)rectlBox.xRight, (short)rectlBox.yTop, SWP_SIZE|SWP_MOVE ) ;
            break ;

        case WM_COMMAND :

            switch ( SHORT1FROMMP(mp1) ) {

                case DID_OK:
                    WinQueryDlgItemFloat( hwnd, IDD_QPRXFRAC, &pqp->xfrac ) ;
                    WinQueryDlgItemFloat( hwnd, IDD_QPRYFRAC, &pqp->yfrac ) ;

                    if( pqp->caps & QP_CAPS_FILE ) {
                        WinQueryDlgItemText( hwnd, IDD_QPRNAME, 32, pqp->szFilename ) ;
                        }
                    break ;

                case IDD_QPRSETPR:   /* printer setup */
                    if( SetPrinterMode( hwnd, pqp ) == 1 ) {
                        if( (hdc = OpenPrinterDC( WinQueryAnchorBlock( hwnd ),
                                                  pqp,
                                                  OD_INFO,
                                                  NULL )) != DEV_ERROR ) {
                            DevQueryCaps( hdc, CAPS_TECHNOLOGY, (long)sizeof(prCaps)/sizeof(long), (PLONG)&prCaps ) ;
                            DevCloseDC( hdc ) ;
                            pqp->xsize = (float)100.0* (float) prCaps.lWidth / (float) prCaps.lHorRes ; // in cm
                            pqp->ysize = (float)100.0* (float) prCaps.lHeight / (float) prCaps.lVertRes ; // in cm
                            WinSendMsg( hwnd, WM_USER_SET_DATA, 0L, 0L ) ;
                            }
                        }
                    return 0L ;

                case IDD_QPRTRACK :     /* track plot area */
                    WinQueryWindowRect( WinWindowFromID( hwnd, IDD_QPRBOX ),
                                        &rectlBox ) ;
                    {
                    double ratio = 1.560 ;
                    double xs = rectlBox.xRight - rectlBox.xLeft ;
                    double ys = rectlBox.yTop - rectlBox.yBottom ;
                    if( ys > xs/ratio ) { /* reduce ys to fit */
                        rectlBox.yTop = rectlBox.yBottom + (int)(xs/ratio) ;
                        }
                    else if( ys < xs/ratio ) { /* reduce xs to fit */
                        rectlBox.xRight = rectlBox.xLeft + (int)(ys*ratio) ;
                        }
                    }
                    ti.cxBorder = ti.cyBorder = 2 ;
                    ti.cxGrid = ti.cyGrid = 0 ;
                    ti.cxKeyboard = ti.cyKeyboard = 2 ;
                    ti.ptlMinTrackSize.x = ti.ptlMinTrackSize.y = 2 ;
                    ti.rclBoundary = rectlBox ;
                    ti.ptlMaxTrackSize.x = rectlBox.xRight ;
                    ti.ptlMaxTrackSize.y = rectlBox.yTop ;
                    ti.rclTrack.xRight = pqp->xfrac * rectlBox.xRight ;
                    ti.rclTrack.yTop = pqp->yfrac*rectlBox.yTop ;
                    ti.rclTrack.xLeft = 0 ;
                    ti.rclTrack.yBottom = 0 ;//(1.0-pqp->yfrac) * rectlBox.yTop ;
                    ti.fs = TF_RIGHT|TF_TOP|TF_STANDARD|TF_SETPOINTERPOS|TF_ALLINBOUNDARY ;
                    WinSetPointer( HWND_DESKTOP,
                                   WinQuerySysPointer( HWND_DESKTOP, SPTR_SIZENWSE, FALSE ) ) ;
                    WinTrackRect( WinWindowFromID( hwnd, IDD_QPRBOX ),
                                  NULLHANDLE,
                                  &ti ) ;
                    pqp->xfrac = (float)ti.rclTrack.xRight / (float)rectlBox.xRight ;
                    pqp->yfrac = ((float)(ti.rclTrack.yTop-ti.rclTrack.yBottom) / (float)rectlBox.yTop) ;
                    rectlBox.yTop = ti.rclTrack.yTop - ti.rclTrack.yBottom ;
                    rectlBox.xRight = ti.rclTrack.xRight ;
                    {
                    double ratio = 1.560 ;
                    double xs = rectlBox.xRight - rectlBox.xLeft ;
                    double ys = rectlBox.yTop - rectlBox.yBottom ;
                    if( ys > xs/ratio ) { /* reduce ys to fit */
                        rectlBox.yTop = rectlBox.yBottom + (int)(xs/ratio) ;
                        }
                    else if( ys < xs/ratio ) { /* reduce xs to fit */
                        rectlBox.xRight = rectlBox.xLeft + (int)(ys*ratio) ;
                        }
                    }
                    WinSetWindowPos( WinWindowFromID( hwnd, IDD_QPRFRAME ),
                                     NULLHANDLE,
                                     swp.x, swp.y,//+(short)(swp.cy*(1.0-pqp->yfrac)),
                                     (short)rectlBox.xRight, (short)rectlBox.yTop, SWP_SIZE|SWP_MOVE ) ;
                    WinSetDlgItemFloat( hwnd, IDD_QPRXFRAC, pqp->xfrac ) ;
                    WinSetDlgItemFloat( hwnd, IDD_QPRYFRAC, pqp->yfrac ) ;
                    return 0L ;

                default:
                    break ;
                }

         default:
            break ;
            }
        /* fall through to the default control processing */

    return WinDefDlgProc ( hwnd , usMsg , mp1 , mp2 ) ;
    }

MRESULT EXPENTRY QPrintersDlgProc ( HWND hwnd, ULONG usMsg, MPARAM mp1, MPARAM mp2 )
/*
**  Query printers and allow selection
*/
    {
    static HWND hwndLB ;
    static PPRQINFO3 pprq=NULL ;
    static ULONG cPrinters ;
    static USHORT usItem ;
    static char *szPrinterName ;
    int i, iSelect ;
    char *psz ;

    switch ( usMsg ) {

        case WM_INITDLG :

            szPrinterName = (char*) PVOIDFROMMP( mp2 ) ;
            iSelect = 0 ;
            GetPrinters( &pprq, &cPrinters ) ;
            hwndLB = WinWindowFromID( hwnd, IDD_QPRSLIST ) ;
            for( i=0; i<cPrinters;i++ ) {
                psz = *pprq[i].pszComment ? pprq[i].pszComment :
                                            pprq[i].pszName ;
                WinSendMsg( hwndLB,
                            LM_INSERTITEM,
                            (MPARAM)LIT_END,
                            MPFROMP(psz) ) ;
                if( strcmp( pprq[i].pszName, szPrinterName ) == 0 )
                    iSelect = i ;
                }

            WinSendMsg( hwndLB,
                        LM_SELECTITEM,
                        MPFROMSHORT( iSelect ),
                        (MPARAM)TRUE ) ;

            break ;

        case WM_COMMAND :

            switch ( SHORT1FROMMP(mp1) ) {

                case DID_OK:
                    strcpy( szPrinterName, pprq[usItem].pszName ) ;
                case DID_CANCEL:
                    free( pprq ) ;
                    pprq = NULL;
                    break ;
                }
            break ;

        case WM_CONTROL:

            if( SHORT1FROMMP( mp1 ) == IDD_QPRSLIST ) {
                if( SHORT2FROMMP( mp1 ) == LN_SELECT  ) {
                    usItem = (ULONG)WinSendMsg( hwndLB,
                                LM_QUERYSELECTION,
                                0L,
                                0L ) ;
                    }
                }

        default:
            break ;
        }
        /* fall through to the default control processing */
    return WinDefDlgProc ( hwnd , usMsg , mp1 , mp2 ) ;
    }

ULONG GetPrinters( PPRQINFO3 *pprq, ULONG *pcTot  )
/*
** get a list of printers
*/
    {
    ULONG rc ;
    ULONG cQueues, cbData ;

    rc = SplEnumQueue( NULL, 3, NULL, 0, &cQueues, pcTot, &cbData, NULL ) ;

    if( *pprq != NULL ) {
        free( *pprq ) ;
        *pprq = NULL ;
        }
    if( *pcTot == 0 ) { /* no printers */
        return 0 ;
        }
    *pprq = malloc( cbData ) ;
    rc = SplEnumQueue( NULL,
                       3,
                       *pprq,
                       cbData,
                       &cQueues,
                       pcTot,
                       &cbData,
                       NULL ) ;
    return *pcTot ;
    }

MRESULT EXPENTRY PauseMsgDlgProc ( HWND hwnd, ULONG usMsg, MPARAM mp1, MPARAM mp2 )
/*
**  Pause message dialog box proc
*/
    {
    static PPAUSEDATA ppdata = NULL ;
    char *pszText ;
    switch ( usMsg ) {

        case WM_INITDLG :
                /* set the position so user can move out the way, and
                   have it come back there next time */
            ppdata = (PPAUSEDATA) PVOIDFROMMP( mp2 ) ;
            if( ppdata->pswp != NULL )
                WinSetWindowPos( hwnd, HWND_TOP, ppdata->pswp->x, ppdata->pswp->y,
                                 0, 0, SWP_MOVE ) ;
            pszText = ppdata->pszMessage ;
            while(*pszText==' ') ++pszText ;
            WinSetDlgItemText( hwnd, IDD_PAUSETEXT, pszText ) ;
            break ;

        case WM_COMMAND :
            switch ( SHORT1FROMMP(mp1) ) {
                case DID_OK:
                case DID_CANCEL:
                    WinPostMsg( WinQueryWindow( hwnd, QW_OWNER ),
                                WM_PAUSEEND,
                                SHORT1FROMMP(mp1)==DID_OK?(MPARAM)1L:0L,
                                0L ) ;
                    if( ppdata->pswp == NULL ) ppdata->pswp = (PSWP)malloc( sizeof(SWP) ) ;
                    WinQueryWindowPos( hwnd, ppdata->pswp ) ;
                    WinDismissDlg( hwnd, 0 ) ;
                    break ;
                case IDM_PRINT:
                    WinPostMsg( WinQueryWindow( hwnd, QW_OWNER ),
                                WM_COMMAND,
                                MPFROMSHORT(IDM_PRINT),
                                0L ) ;
                    return 0 ;
                default:
                    break ;
                }
        default:
            break ;
        }
        /* fall through to the default control processing */
    return WinDefDlgProc ( hwnd , usMsg , mp1 , mp2 ) ;
    }

void WinSetDlgItemFloatF( HWND hwnd, USHORT usID, int nDec, float flValue )
/*
*/
    {
    char achBuffer [ 34 ]; // default string field size ...
    char fmt[32] ;

    sprintf( fmt, "%%12.%df", nDec ) ;
    sprintf( achBuffer, fmt, flValue ) ;
    WinSetDlgItemText( hwnd, usID, achBuffer ) ;
    }

void WinSetDlgItemFloat( HWND hwnd, USHORT usID, float flValue )
/*
*/
    {
    char achBuffer [ 34 ] ; // default string field size ...
    char fmt[10] ;

    sprintf( fmt, "%%12.%df", 4 ) ;
    sprintf( achBuffer, fmt, flValue ) ;
    WinSetDlgItemText( hwnd, usID, achBuffer ) ;
    WinSetDlgItemText( hwnd, usID, achBuffer ) ;
    }

void WinQueryDlgItemFloat( HWND hwnd, USHORT usID, float *pflValue )
/*
*/
    {
    char achBuffer [ 34 ] ; // default string field size ...

    WinQueryDlgItemText( hwnd, usID, 34, achBuffer ) ;
    *pflValue = (float) atof( achBuffer ) ;
    }

MRESULT EXPENTRY SendCommandDlgProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
/*
**  Dialog for sending a command to gnuplot
**  //PM Petr Mikulik, 17. 8. 1999
*/
{
#define TEXT_LENGTH 80
static char buf[TEXT_LENGTH+1];
extern void gp_execute (char *s);

  switch (msg)
    {
    case WM_INITDLG:
      WinSendDlgItemMsg( hwnd, IDM_DO_SENDCOMMAND, EM_SETTEXTLIMIT,
                         MPFROMSHORT (TEXT_LENGTH), 0L );
      WinSetDlgItemText( hwnd, IDM_DO_SENDCOMMAND, buf );
      WinSendDlgItemMsg( hwnd, IDM_DO_SENDCOMMAND, EM_SETSEL,
                         MPFROM2SHORT (0, strlen(buf)), 0L );
      return (MRESULT)FALSE;

    case WM_COMMAND:
      switch (COMMANDMSG (&msg)->cmd)
        {
        case DID_OK:
          WinQueryDlgItemText( hwnd, IDM_DO_SENDCOMMAND, sizeof(buf), buf );
          WinDismissDlg( hwnd, TRUE );
	  gp_execute(buf);
          break;
        default:
          WinDismissDlg( hwnd, FALSE );
          break;
        }
      return 0;
    }
  return ( WinDefDlgProc( hwnd, msg, mp1, mp2 ) );
}
