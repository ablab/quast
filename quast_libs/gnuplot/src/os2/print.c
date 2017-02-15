#ifdef INCRCSDATA
static char RCSid[]="$Id: print.c,v 1.4 2005/01/04 13:01:38 mikulik Exp $" ;
#endif

/****************************************************************************

    PROGRAM: gnupmdrv

    Outboard PM driver for GNUPLOT 3.3

    MODULE:  print.c -- support for printing graphics under OS/2

****************************************************************************/

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

#define INCL_SPLDOSPRINT
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DEV
#define INCL_SPL
#define INCL_PM
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "gnupmdrv.h"

#define   GNUPAGE   4096        /* size of gnuplot page in pixels (driver dependent) */


typedef struct {            /* for print thread parameters */
    HWND  hwnd ;
    HDC   hdc ;                 /* printer device context */
    HPS   hps ;                 /* screen PS to be printed */
    char  szPrintFile[256] ;    /* file for printer output if not to printer */
    PQPRINT pqp ;       /* print queue info */
    } PRINTPARAMS ;

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

//static PDRIVDATA    pdriv = NULL ;
static DRIVDATA     driv = {sizeof( DRIVDATA) } ;
static char         szPrintFile[CCHMAXPATHCOMP] = {0} ;
static DEVOPENSTRUC devop ;

ULONG GetPrinters( PPRQINFO3 *, ULONG *  ) ;
int FindPrinter( char *, PPRQINFO3  ) ;
HMF     CopyToMetaFile( HPS ) ;
static void    ThreadPrintPage( ) ;

MPARAM PrintCmdProc( HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
/*
**  Handle messages for print commands for 1- and 2-d spectra
** (i.e for the appropriate 1-and 2-d child windows )
*/
    {
    static PRINTPARAMS tp ;
    static char szBusy[] = "Busy - try again later" ;
    static char szStart[] = "Printing started" ;
    static HEV semPrint = 0L ;
    static HWND hwndCancel = NULLHANDLE ;
    char szTemp[32] ;
    unsigned short lErr ;
    TID tid ;
    char *pszMess;

    if( semPrint == 0L ) {
        DosCreateMutexSem( NULL, &semPrint, 0L, 0L ) ;
        }

    switch( msg ) {

        case WM_USER_PRINT_BEGIN:

            if( DosRequestMutexSem( semPrint, SEM_IMMEDIATE_RETURN ) != 0 ) {
                pszMess = szBusy ;
                WinMessageBox( HWND_DESKTOP,
                               hWnd,
                               pszMess,
                               APP_NAME,
                               0,
                               MB_OK | MB_ICONEXCLAMATION ) ;
                }
            else {
                pszMess = szStart ;
                tp.hwnd = hWnd ;
                tp.pqp = (PQPRINT) mp1 ;
                tp.hps = (HPS) mp2 ;
                strcpy( tp.szPrintFile, szPrintFile ) ;
                tid = _beginthread( ThreadPrintPage, NULL, 32768, &tp ) ;
                hwndCancel = WinLoadDlg( HWND_DESKTOP,
                                         hWnd,
                                         (PFNWP)CancelPrintDlgProc,
                                         0L,
                                         ID_PRINTSTOP,
                                         NULL ) ;
                }
            break ;


        case WM_USER_PRINT_OK :

            if( hwndCancel != NULLHANDLE ) {
                WinDismissDlg( hwndCancel, 0 ) ;
                hwndCancel = NULLHANDLE ;
                }
             DosReleaseMutexSem( semPrint ) ;
             break ;

        case WM_USER_DEV_ERROR :

            if( hwndCancel != NULLHANDLE ) {
                WinDismissDlg( hwndCancel, 0 ) ;
                hwndCancel = NULLHANDLE ;
                }
            lErr = ERRORIDERROR( (ERRORID) mp1 ) ;
            sprintf( szTemp, "Dev error: %d %x", lErr, lErr ) ;
            WinMessageBox( HWND_DESKTOP,
                           hWnd,
                           szTemp,
                           APP_NAME,
                           0,
                           MB_OK | MB_ICONEXCLAMATION ) ;
             DosReleaseMutexSem( semPrint ) ;
             break ;

        case WM_USER_PRINT_ERROR :

            if( hwndCancel != NULLHANDLE ) {
                WinDismissDlg( hwndCancel, 0 ) ;
                hwndCancel = NULLHANDLE ;
                }
            lErr = ERRORIDERROR( (ERRORID) mp1 ) ;
            sprintf( szTemp, "Print error: %d %x", lErr, lErr ) ;
            WinMessageBox( HWND_DESKTOP,
                           hWnd,
                           szTemp,
                           APP_NAME,
                           0,
                           MB_OK | MB_ICONEXCLAMATION ) ;
             DosReleaseMutexSem( semPrint ) ;
             break ;

        case WM_USER_PRINT_CANCEL :

             DevEscape( tp.hdc, DEVESC_ABORTDOC, 0L, NULL, NULL, NULL ) ;
             break ;


        case WM_USER_PRINT_QBUSY :

            return( (MPARAM)DosRequestMutexSem( semPrint, SEM_IMMEDIATE_RETURN ) ) ;

        default : break ;
        }

    return 0L ;
    }

int SetupPrinter( HWND hwnd, PQPRINT pqp )
/*
**  Set up the printer
**
*/
    {
    HDC hdc ;
    float flXFrac, flYFrac;
          /* check that printer is still around .. */
    if( FindPrinter( pqp->szPrinterName, pqp->piPrinter ) != 0 ) return 0 ;
          /* get printer capabilities */
    if( (hdc = OpenPrinterDC( WinQueryAnchorBlock( hwnd ), pqp, OD_INFO, NULL )) != DEV_ERROR ) {
        DevQueryCaps( hdc, CAPS_TECHNOLOGY, (long)sizeof(prCaps)/sizeof(long), (PLONG)&prCaps ) ;
        DevCloseDC( hdc ) ;
        pqp->xsize = (float)100.0* (float) prCaps.lWidth / (float) prCaps.lHorRes ; // in cm
        pqp->ysize = (float)100.0* (float) prCaps.lHeight / (float) prCaps.lVertRes ; // in cm
        flXFrac = pqp->xfrac ;
        flYFrac = pqp->yfrac ;
        pqp->szFilename[0] = 0 ;
        szPrintFile[0] = 0 ;
        pqp->caps  = prCaps.lTech & (CAPS_TECH_VECTOR_PLOTTER|CAPS_TECH_POSTSCRIPT) ?
                   QP_CAPS_FILE : QP_CAPS_NORMAL ;
        if( WinDlgBox( HWND_DESKTOP,
                      hwnd,
                    (PFNWP)QPrintDlgProc,
                    0L,
                    ID_QPRINT,
                    pqp ) == DID_OK ) {
          if( pqp->caps & QP_CAPS_FILE ) {
              if( pqp->szFilename[0] != 0 ) strcpy( szPrintFile, pqp->szFilename ) ;
              }
          return 1 ;
          }
        pqp->xfrac = flXFrac ;
        pqp->yfrac = flYFrac ;
        }

    return 0 ;
    }

int SetPrinterMode( HWND hwnd, PQPRINT pqp )
/*
**  call up printer driver's own setup dialog box
**
**  returns :  -1 if error
**              0 if no settable modes
**              1 if OK
*/
    {
    HAB hab ;
    LONG lBytes ;
    PPRQINFO3 pinfo = pqp->piPrinter ;

    hab = WinQueryAnchorBlock( hwnd ) ;
    driv.szDeviceName[0]='\0' ;
    lBytes = DevPostDeviceModes( hab,
                                 NULL,
                                 devop.pszDriverName,
                                 pinfo->pDriverData->szDeviceName,
                                 //driv.szDeviceName,
                                 NULL,
                                 DPDM_POSTJOBPROP ) ;
    if( lBytes > 0L ) {
            /* if we have old pdriv data, and if it's for the same printer,
               keep it to retain user's current settings, else get new */
        if( pqp->pdriv != NULL
        && strcmp( pqp->pdriv->szDeviceName, pinfo->pDriverData->szDeviceName ) != 0 ) {
            free( pqp->pdriv ) ;
            pqp->pdriv = NULL ;
            }
        if( pqp->pdriv == NULL ) {
            if( lBytes < pinfo->pDriverData->cb ) lBytes = pinfo->pDriverData->cb ;
            pqp->pdriv = malloc( lBytes ) ;
            pqp->cbpdriv = lBytes ;
            memcpy( pqp->pdriv, pinfo->pDriverData, lBytes ) ;
            }
        strcpy( driv.szDeviceName, pqp->pdriv->szDeviceName ) ;
//        pqp->pdriv->szDeviceName[0] = '\0' ;  /* to check if 'cancel' selected */
        lBytes = DevPostDeviceModes( hab,
                                     pqp->pdriv,
                                     devop.pszDriverName,
                                     driv.szDeviceName,
                                     NULL,
                                     DPDM_POSTJOBPROP ) ;
        if( lBytes != 1 /*pqp->pdriv->szDeviceName[0] == '\0'*/ ) {  /* could be: 'cancel' selected */
            pqp->cbpdriv = lBytes = 0 ;
            free(pqp->pdriv ) ;   /* is this right ???? */
            pqp->pdriv = NULL ;
            }
        }
    return ( (int) lBytes ) ;
    }

static void ThreadPrintPage( PRINTPARAMS *ptp )
/*
**  thread to set up printer DC and print page
**
**  Input: THREADPARAMS *ptp -- pointer to thread data passed by beginthread
**
*/
    {
    HAB         hab ;       // thread anchor block nandle
    HDC         hdc ;       // printer device context handle
    HPS         hps ;       // presentation space handle
    SHORT       msgRet ;    // message posted prior to return (end of thread)
    SIZEL       sizPage ;   // size of page for creation of presentation space
    LONG        alPage[2] ; // actual size of printer page in pixels
    RECTL       rectPage ;  // viewport on page into which we draw
    LONG        lColors ;
    char        *szPrintFile ;
    HMF         hmf ;
    LONG        alOpt[9] ;
    HPS         hpsSc ;
    hab = WinInitialize( 0 ) ;

    szPrintFile = ptp->szPrintFile[0] == '\0' ? NULL : ptp->szPrintFile ;

    if( (hdc = OpenPrinterDC( hab, ptp->pqp, 0L, szPrintFile )) != DEV_ERROR ) {

            // create presentation space for printer

        ptp->hdc = hdc ;
        hmf = CopyToMetaFile( ptp->hps ) ;
        hpsSc = ptp->hps ;

        sizPage.cx = GNUXPAGE;
        sizPage.cy = GNUYPAGE;
        hps = GpiCreatePS( hab,
                           hdc,
                           &sizPage,
                           PU_HIMETRIC | GPIF_DEFAULT | GPIT_NORMAL | GPIA_ASSOC ) ;

        DevQueryCaps( hdc, CAPS_WIDTH, 2L, alPage ) ;
        DevQueryCaps( hdc, CAPS_PHYS_COLORS, 1L, &lColors ) ;
        rectPage.xLeft = 0L ;
        rectPage.xRight = alPage[0] ;
        rectPage.yTop = alPage[1] ;//alPage[1]*(1.0-flYFrac) ;
        rectPage.yBottom = 0L ; //  = alPage[1] ;

        {
        double ratio = 1.560 ;
        double xs = rectPage.xRight - rectPage.xLeft ;
        double ys = rectPage.yTop - rectPage.yBottom ;
        if( ys > xs/ratio ) { /* reduce ys to fit */
            rectPage.yTop = rectPage.yBottom + (int)(xs/ratio) ;
            }
        else if( ys < xs/ratio ) { /* reduce xs to fit */
            rectPage.xRight = rectPage.xLeft + (int)(ys*ratio) ;
            }
        }

        rectPage.xRight = rectPage.xRight*ptp->pqp->xfrac ;
        rectPage.yTop = rectPage.yTop*ptp->pqp->yfrac ;//alPage[1]*(1.0-flYFrac) ;

        {
        double ratio = 1.560 ;
        double xs = rectPage.xRight - rectPage.xLeft ;
        double ys = rectPage.yTop - rectPage.yBottom ;
        if( ys > xs/ratio ) { /* reduce ys to fit */
            rectPage.yTop = rectPage.yBottom + (int)(xs/ratio) ;
            }
        else if( ys < xs/ratio ) { /* reduce xs to fit */
            rectPage.xRight = rectPage.xLeft + (int)(ys*ratio) ;
            }
        }


            // start printing

        if( DevEscape( hdc,
                       DEVESC_STARTDOC,
                       7L,
                       APP_NAME,
                       NULL,
                       NULL ) != DEVESC_ERROR ) {
            char buff[256] ;
            int rc;

            rc = GpiSetPageViewport( hps, &rectPage ) ;

            alOpt[0] = 0L ;
            alOpt[1] = LT_ORIGINALVIEW ;
            alOpt[2] = 0L ;
            alOpt[3] = LC_LOADDISC ;
            alOpt[4] = RES_DEFAULT ;
            alOpt[5] = SUP_DEFAULT ;
            alOpt[6] = CTAB_DEFAULT ;
            alOpt[7] = CREA_DEFAULT ;
            alOpt[8] = DDEF_DEFAULT ;
            if (rc) rc=GpiPlayMetaFile( hps, hmf, 9L, alOpt, NULL, 255, buff ) ;

            if (rc) {
              DevEscape( hdc, DEVESC_ENDDOC, 0L, NULL, NULL, NULL ) ;
              msgRet = WM_USER_PRINT_OK ;
              }
            else
              msgRet = WM_USER_PRINT_ERROR;

            }
        else
            msgRet = WM_USER_PRINT_ERROR ;

        GpiDestroyPS( hps ) ;
        DevCloseDC( hdc ) ;
        }
    else
        msgRet = WM_USER_DEV_ERROR ;

    DosEnterCritSec() ;
    WinPostMsg( ptp->hwnd, msgRet, (MPARAM)WinGetLastError(hab), 0L ) ;
    WinTerminate( hab ) ;
    }

HDC OpenPrinterDC( HAB hab, PQPRINT pqp, LONG lMode, char *szPrintFile )
/*
** get printer info from os2.ini and set up DC
**
** Input:  HAB hab  -- handle of anchor block of printing thread
**         PQPRINT-- pointer to data of current selected printer
**         LONG lMode -- mode in which device context is opened = OD_QUEUED, OD_DIRECT, OD_INFO
**         char *szPrintFile -- name of file for printer output, NULL
**                  if to printer (only used for devices that support file
**                  output eg plotter, postscript)
**
** Return: HDC      -- handle of printer device context
**                   = DEV_ERROR (=0) if error
*/
    {
    LONG   lType ;
    static CHAR   achPrinterData[256] ;

    if( pqp->piPrinter == NULL ) return DEV_ERROR ;

    strcpy( achPrinterData, pqp->piPrinter->pszDriverName ) ;
    achPrinterData[ strcspn(achPrinterData,".") ] = '\0' ;

    devop.pszDriverName = achPrinterData ;
    devop.pszLogAddress = pqp->piPrinter->pszName ;

    if( pqp->pdriv != NULL
        && strcmp( pqp->pdriv->szDeviceName, pqp->piPrinter->pDriverData->szDeviceName ) == 0 ) {
        devop.pdriv = pqp->pdriv ;
        }
    else devop.pdriv = pqp->piPrinter->pDriverData ;

    if( szPrintFile != NULL )  devop.pszLogAddress = szPrintFile ;

            // set data type to RAW

    devop.pszDataType = "PM_Q_RAW" ;

            // open device context
    if( lMode != 0L )
        lType = lMode ;
    else
        lType = (szPrintFile == NULL) ? OD_QUEUED: OD_DIRECT ;

    return DevOpenDC( hab, //  WinQueryAnchorBlock( hwnd ),
                      lType,
                      "*",
                      4L,
                      (PDEVOPENDATA) &devop,
                      NULLHANDLE ) ;
    }

int FindPrinter( char *szName, PPRQINFO3 piPrinter )
/*
**  Find a valid printer
*/
    {
    PPRQINFO3 pprq = NULL ;
    PDRIVDATA pdriv = NULL ;
    LONG np ;

    if( *szName && (strcmp( szName, piPrinter->pszName ) == 0) ) return 0 ;
    if( GetPrinters( &pprq , &np ) == 0 ) return 1 ;
    for( --np; np>=0; np-- ) {
        if( strcmp( szName, pprq[np].pszName ) == 0 ) {
            if( piPrinter->pDriverData != NULL ) free( piPrinter->pDriverData ) ;
            pdriv = malloc( pprq[np].pDriverData->cb ) ;
            memcpy( piPrinter, &pprq[np], sizeof( PRQINFO3 ) ) ;
            piPrinter->pDriverData = pdriv ;
            memcpy( pdriv, pprq[np].pDriverData, pprq[np].pDriverData->cb ) ;
            free( pprq ) ;
            return 0 ;
            }
        }
    memcpy( piPrinter, &pprq[0], sizeof( PRQINFO3 ) ) ;
    free( pprq ) ;
    return 0 ;
    }

MRESULT EXPENTRY CancelPrintDlgProc ( HWND hwnd, ULONG usMsg, MPARAM mp1, MPARAM mp2 )
/*
**  Cancel printing dialog box proc
*/
    {
    switch ( usMsg ) {

        case WM_COMMAND :
            switch ( SHORT1FROMMP(mp1) ) {
                case DID_CANCEL:
                    WinSendMsg( WinQueryWindow( hwnd, QW_OWNER ),
                                    WM_USER_PRINT_CANCEL,
                                    0L,
                                    0L ) ;
                    WinDismissDlg( hwnd, 0 ) ;
                    break ;
                default:
                    break ;
                }
        default:
            break ;
        }
        /* fall through to the default control processing */
    return WinDefDlgProc ( hwnd , usMsg , mp1 , mp2 ) ;
    }


