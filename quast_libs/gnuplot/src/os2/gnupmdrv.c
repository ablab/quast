#ifdef INCRCSDATA
static char RCSid[]="$Id: gnupmdrv.c,v 1.6 2005/07/28 07:46:06 mikulik Exp $" ;
#endif

/****************************************************************************

    PROGRAM: gnupmdrv

    Outboard PM driver for GNUPLOT 3.x

    MODULE:  gnupmdrv.c

    This file contains the startup procedures for gnupmdrv

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
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "gnupmdrv.h"

/*==== g l o b a l    d a t a ================================================*/

char szIPCName[256] ;
char szIniFile[256] ;
#define IPCDEFAULT "gnuplot"
int  bServer=0 ;
int  bPersist=0 ;
int  bWideLines=0 ;
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
int  bEnhanced=0 ;
#endif

/*==== l o c a l    d a t a ==================================================*/

            /* class names for window registration */

static char szTitle[256] = "Gnuplot" ;

/*==== f u n c t i o n s =====================================================*/

BOOL             QueryIni( HAB ) ;
int              main( int, char** ) ;
static HWND      InitHelp( HAB, HWND ) ;

/*==== c o d e ===============================================================*/

int main ( int argc, char **argv )
/*
** args:  argv[1] : name to be used for IPC (pipes/semaphores) with gnuplot
**
** Standard PM initialisation:
** -- set up message processing loop
** -- register all window classes
** -- start up main window
** -- subclass main window for help and dde message trapping to frame window
** -- init help system
** -- check command line and open any filename found there
**
*/
    {
    static ULONG flFrameFlags = (FCF_ACCELTABLE|FCF_STANDARD);//&(~FCF_TASKLIST) ;
    static ULONG flClientFlags = WS_VISIBLE ;
    HMQ          hmq ;
    QMSG         qmsg ;
    PFNWP        pfnOldFrameWndProc ;
    HWND         hwndHelp ;
    BOOL         bPos ;

    /* (am, 19981001)
     * A subtile problem is fixed here:
     * upon the first initialization of this driver (i.e. we're here in main())
     * it may inherit handles of files opened (temporarily) by gnuplot itself!
     * We close them here.
     */
    _fcloseall();

    if( argc <= 1 ) strcpy( szIPCName, IPCDEFAULT ) ;
    else {
        int i ;
        strcpy( szIPCName, argv[1] ) ;
        for ( i=2; i<argc; i++ ) {
                    while( *argv[i] != '\0' ) {
                if( *argv[i] == '-' ) {
                    ++argv[i] ;
                    switch( *argv[i] ) {
                        case 's' :
                            bServer = 1 ;
                            break ;
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
                        case 'e' :
                            bEnhanced = 1 ;
                            break ;
#endif
                        case 'p' :
                            bPersist = 1 ;
                            break ;
                        case 'w' :
                            bWideLines = 1 ;
                            break ;
                        }
                    }
                else if ( *argv[i] == '"' ) {
                    char *p = szTitle ;
                    argv[i]++ ;
                    while( *argv[i] != '"' ) {
                        *p++ = *argv[i]++ ;
                        }
                    *p = '\0' ;
                    }
                argv[i]++ ;
                }
            }
        }
    {
    char *p ;
        /* get path from argv[0] to track down program files */
    strcpy( szIniFile, argv[0] ) ;
    while( (p=strchr(szIniFile,'/'))!=NULL ) *p = '\\' ;
    p = strrchr(szIniFile,'\\') ;
    if(p==NULL) p = strrchr(szIniFile,':') ;
    if(p==NULL) p = szIniFile ;
    else ++p ;
    strcpy(p,GNUINI);
    }

    hab = WinInitialize( 0 ) ;
    hmq = WinCreateMsgQueue( hab, 50 ) ;

                // get defaults from gnupmdrv.ini

    bPos = QueryIni( hab ) ;

                // register window and child window classes

    if( ! WinRegisterClass( hab,        /* Exit if can't register */
                            APP_NAME,
                            (PFNWP)DisplayClientWndProc,
                            CS_SIZEREDRAW,
                            0 )
                            ) return 0L ;

                // create main window

    hwndFrame = WinCreateStdWindow (
                    HWND_DESKTOP,
                    0,//WS_VISIBLE,
                    &flFrameFlags,
                    APP_NAME,
                    NULL,
                    flClientFlags,
                    0L,
                    1,
                    &hApp) ;

    if ( ! hwndFrame ) return 0 ;

                // subclass window for help & DDE trapping

    pfnOldFrameWndProc = WinSubclassWindow( hwndFrame, (PFNWP)NewFrameWndProc ) ;
    WinSetWindowULong( hwndFrame, QWL_USER, (ULONG) pfnOldFrameWndProc ) ;

                // init the help manager

    hwndHelp = InitHelp( hab, hwndFrame ) ;

                // set window title and make it active

    {
    char text[256] = APP_NAME;
    strcat( text, " [" ) ;
    strcat( text, szTitle ) ;
    strcat( text, "]" ) ;
    WinSetWindowText( hwndFrame, text ) ;
    }
                // process window messages

    while (WinGetMsg (hab, &qmsg, NULLHANDLE, 0, 0))
         WinDispatchMsg (hab, &qmsg) ;

                // shut down

    WinDestroyHelpInstance( hwndHelp ) ;
    WinDestroyWindow (hwndFrame) ;
    WinDestroyMsgQueue (hmq) ;
    WinTerminate (hab) ;

    return 0 ;
    }

static HWND InitHelp( HAB hab, HWND hwnd )
/*
**  initialise the help system
*/
    {
    static HELPINIT helpinit = { sizeof(HELPINIT),
                                 0L,
                                 NULL,
                                 (PHELPTABLE)MAKELONG(1, 0xFFFF),
                                 0L,
                                 0L,
                                 0,
                                 0,
                                 "GnuplotPM Help",
                                 CMIC_HIDE_PANEL_ID,
                                 "gnupmdrv.hlp" } ;
    HWND hwndHelp ;
    /* should be bigger or dynamic */
    static char helppath[256] ;
    char *p;
    _execname(helppath, sizeof(helppath));
    _fnslashify(helppath);
    p=strrchr(helppath, '/');
    if (p)
       *p='\0';
    strcat( helppath, "/" ) ;
    strcat( helppath, helpinit.pszHelpLibraryName ) ;
    helpinit.pszHelpLibraryName = helppath ;

    hwndHelp = WinCreateHelpInstance( hab, &helpinit ) ;
    WinAssociateHelpInstance( hwndHelp, hwnd ) ;
    return hwndHelp ;
    }

MRESULT EXPENTRY NewFrameWndProc (HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
/*
**  Subclasses top-level frame window to trap help & dde messages
*/
    {
    PFNWP       pfnOldFrameWndProc ;

    pfnOldFrameWndProc = (PFNWP) WinQueryWindowULong( hwnd, QWL_USER ) ;
    switch( msg ) {
        default:
            break ;

        case HM_QUERY_KEYS_HELP:
            return (MRESULT) IDH_KEYS ;
        }
    return (*pfnOldFrameWndProc)(hwnd, msg, mp1, mp2) ;
    }


MRESULT EXPENTRY About( HWND hDlg, ULONG message, MPARAM mp1, MPARAM mp2)
/*
** 'About' box dialog function
*/
    {
    return WinDefDlgProc( hDlg, message, mp1, mp2 ) ;
    }

