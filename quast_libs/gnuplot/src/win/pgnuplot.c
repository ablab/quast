#ifndef lint
static char *RCSid() { return RCSid("$Id: pgnuplot.c,v 1.18 2012/11/29 00:12:57 broeker Exp $"); }
#endif

/*
 * pgnuplot.c -- pipe stdin to wgnuplot
 *
 * Version 0.4 -- October 2002
 *
 * This program is based on pgnuplot.c Copyright (C) 1999 Hans-Bernhard Broeker
 * with substantial modifications Copyright (C) 1999 Craig R. Schardt.
 *
 * The code is released to the public domain.
 *
 */

/* Changes by Petr Mikulik, October 2002:
 * Added command line options --version and --help, and consequently dependency
 * on gnuplot's version.h and version.c.
 * Compile pgnuplot by:
 *     gcc -O2 -s -o pgnuplot.exe pgnuplot.c ../version.c -I.. -luser32
 */

/* Comments from original pgnuplot.c */
/*
 * pgnuplot.c -- 'Pipe to gnuplot'  Version 990608
 *
 * A small program, to be compiled as a Win32 console mode application.
 * (NB: will not work on 16bit Windows, not even with Win32s installed).
 *
 * This program will accept commands on STDIN, and pipe them to an
 * active (or newly created) wgnuplot text window. Command line options
 * are passed on to wgnuplot.
 *
 * Effectively, this means `pgnuplot' is an almost complete substitute
 * for `wgnuplot', on the command line, with the added benefit that it
 * does accept commands from redirected stdin. (Being a Windows GUI
 * application, `wgnuplot' itself cannot read stdin at all.)
 *
 * Copyright (C) 1999 by Hans-Bernhard Broeker
 *                       (broeker@physik.rwth-aachen.de)
 * This file is in the public domain. It might become part of a future
 * distribution of gnuplot.
 *
 * based on a program posted to comp.graphics.apps.gnuplot in May 1998 by
 * jl Hamel <jlhamel@cea.fr>
 *
 * Changes relative to that original version:
 * -- doesn't start a new wgnuplot if one already is running.
 * -- doesn't end up in an endless loop if STDIN is not redirected.
 *    (refuses to read from STDIN at all, in that case).
 * -- doesn't stop the wgnuplot session at the end of
 *    stdin, if it didn't start wgnuplot itself.
 * -- avoids the usual buffer overrun problem with gets().
 *
 * For the record, I usually use MinGW32 to compile this, with a
 * command line looking like this:
 *
 *     gcc -o pgnuplot.exe pgnuplot.c -luser32 -s
 *
 * Note that if you're using Cygwin GCC, you'll want to add the option
 * -mno-cygwin to that command line to avoid getting a pgnuplot.exe
 * that depends on their GPL'ed cygwin1.dll.
 */

/*	Modifications by Craig R. Schardt (17 Jun 1999)

	Copyright (C) 1999 by Craig R. Schardt (craig@silica.mse.ufl.edu)

	Major changes: (See the explanation below for more information)
		+ Always starts a new instance of wgnuplot.
		+ If stdin isn't redirected then start wgnuplot and give it focus.
		+ Uses CreateProcess() instead of WinExec() to start wgnuplot when stdin
		  is redirected.

	Other changes:
		+ New technique for building the command line to pass to wgnuplot.exe
		  which is less complicated and seems to work more reliably than the old
		  technique.
		+ Simplified message passing section of the code.
		+ All printf(...) statements are now fprintf(stderr,...) so that errors
		  are sent to the console, even if stdout is redirected.

	The previous version of pgnuplot would fail when more than one program
	tried to access wgnuplot simultaneously or when one program tried to start
	more than one wgnuplot session. Only a single instance of wgnuplot would be
	started and all input would be sent to that instance. When two or more programs
	tried to pipe input to wgnuplot, the two seperate input streams would be sent
	to one wgnuplot window resulting in one very confused copy of wgnuplot. The only
	way to avoid this problem was to change pgnuplot so that it would start a
	new instance of wgnuplot every time.

	Just starting a new instance of wgnuplot isn't enough. pgnuplot must also
	make sure that the data on each stdin pipe is sent to the proper wgnuplot
	instance. This is achieved by using CreateProcess() which returns a handle
	to the newly created process. Once the process has initialized, it can be
	searched for the text window and then data can be routed correctly. The search
	is carried out by the EnumThreadWindows() call and the data passing is carried
	out by a rewritten version of the original code. With these changes, pgnuplot
	now behaves in a manner consistent with the behavior of gnuplot on UNIX
	computers.

	This program has been compiled using Microsoft Visual C++ 4.0 with the
	following command line:

		cl /O2 pgnuplot.c /link user32.lib

	The resulting program has been tested on WinNT and Win98 both by calling
	it directly from the command line with and without redirected input. The
	program also works on WinNT with a modified version of Gnuplot.py (a script
	for interactive control of Gnuplot from Python).

	22 JUN 1999:
	+ Fixed command line code to behave properly when the first
	  item is quoted in the original command line.

	29 JUN 1999:
	+ Added some code to print the command line. This is for testing
	  only and should be removed before the general release. To enable,
	  compile with SHOWCMDLINE defined.

	30 JUN 1999:
	+ New function FindUnquotedSpace() which replaces the earlier technique for
	  finding the command line arguments to send on to wgnuplot. Prior to this
	  the arguments were assumed to start after argv[0], however argv[0] is not
	  set the same by all combinitaions of compiler, command processor, and OS.
	  The new method ignores argv completely and manually search the command line
	  string for the first space which isn't enclosed in double-quotes.

  */

#include <io.h>
#include <conio.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "version.h"

#ifndef _O_BINARY
# define _O_BINARY O_BINARY
#endif
#if (__BORLANDC__ >= 0x450) /* about BCBuilder 1.0 */
# define _setmode setmode
#endif
#ifdef __WATCOMC__
# define _setmode setmode
#endif

/* Customize this path if needed */
#define PROGNAME "wgnuplot.exe"
/* CRS: The value given above will work correctly as long as pgnuplot.exe
 * is in the same directory as wgnuplot.exe or the directory containing
 * wgnuplot.exe is included in the path. I would recommend placing the
 * pgnuplot.exe executable in the same directory as wgnuplot.exe and
 * leaving this definition alone.
 */

#define WINDOWNAME "gnuplot"
#define PARENTCLASS "wgnuplot_parent"
#define TEXTCLASS "wgnuplot_text"
#define GRAPHWINDOW "gnuplot graph"
#define GRAPHCLASS "wgnuplot_graph"
#define BUFFER_SIZE 80

/* GLOBAL Variables */
HWND hwndParent = NULL;
HWND hwndText = NULL;

PROCESS_INFORMATION piProcInfo;
STARTUPINFO         siStartInfo;

/* CRS: Callback for the EnumThreadWindows function */
BOOL CALLBACK
cbGetTextWindow(HWND  hwnd, LPARAM  lParam)
{
    /* save the value of the parent window */
    hwndParent = hwnd;
    /* check to see if it has a child text window */
    hwndText = FindWindowEx(hwnd, NULL, TEXTCLASS, NULL);

    /* if the text window was found, stop looking */
    return (hwndText == NULL);
}

/* sends a string to the specified window */
/* CRS: made this into a function call */
void
PostString(HWND hwnd, char *pc)
{
    while(*pc) {
	PostMessage(hwnd, WM_CHAR, (unsigned char) *pc, 1L);
	/* CRS: should add a check of return code on PostMessage. If 0, the
	   message que was full and the message wasn't posted. */
	pc++;
    }
}

/* FindUnquotedSpace(): Search a string for the first space not enclosed in quotes.
 *   Returns a pointer to the space, or the empty string if no space is found.
 *   -CRS 30061999
 */
char*
FindUnquotedSpace(char *pc)
{
    while ((*pc) && (*pc != ' ') && (*pc != '\t')) {
	if (*pc == '"') {
	    do {
		pc++;
	    } while (pc[1] && (*pc != '"'));
	}
	pc++;
    }
    return pc;
}

BOOL
ProcessAlive(HANDLE hProcess)
{
    DWORD code = 0;
    if (GetExitCodeProcess(hProcess, &code))
	return (code == STILL_ACTIVE);
    return FALSE;
}

int
main (int argc, char *argv[])
{
    char    psBuffer[BUFFER_SIZE];
    char    psGnuplotCommandLine[MAX_PATH] = PROGNAME;
    LPTSTR  psCmdLine;
    BOOL    bSuccess;
    BOOL    bPersist = FALSE;
    int	i;

#if !defined(_O_BINARY) && defined(O_BINARY)
# define _O_BINARY O_BINARY
# define _setmode setmode /* this is for BC4.5 ... */
#endif
    _setmode(fileno(stdout), _O_BINARY);

    for (i = 1; i < argc; i++) {
	if (!argv[i])
	    continue;
	if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
	    printf("gnuplot %s patchlevel %s\n",
		   gnuplot_version, gnuplot_patchlevel);
	    return 0;
	} else if ((!stricmp(argv[i], "-noend")) || (!stricmp(argv[i], "/noend")) || 
		   (!stricmp(argv[i], "-persist"))) {
	    bPersist = TRUE;
	} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
	    printf("Usage: gnuplot [OPTION] [FILE] [-]\n"
		    "  -V, --version       show gnuplot version\n"
		    "  -h, --help          show this help\n"
		    "  -e \"cmd; cmd; ...\"  prepand additional commands\n"
		    "  -persist            don't close the plot after executing FILE\n"
		    "  -noend, /noend      like -persist (non-portable Windows-only options)\n"
		    "  -                   allow work in interactive mode after executing FILE\n"
		    "Only on Windows, -persist and - have the same effect.\n"
		    "This is gnuplot %s patchlevel %s\n"
		    "Report bugs to <info-gnuplot-beta@lists.sourceforge.net>\n",
		    gnuplot_version, gnuplot_patchlevel);
	    return 0;
	}
    } /* for(argc) */

    /* CRS: create the new command line, passing all of the command
     * line options to wgnuplot so that it can process them:
     * first, get the command line,
     * then move past the name of the program (e.g., 'pgnuplot'),
     * finally, add what's left of the line onto the gnuplot command line. */
    psCmdLine = GetCommandLine();

#ifdef SHOWCMDLINE
    fprintf(stderr,"CmdLine: %s\n", psCmdLine);
    fprintf(stderr,"argv[0]: %s\n",argv[0]);
#endif

    /* CRS 30061999: Search for the first unquoted space. This should
       separate the program name from the arguments. */
    psCmdLine = FindUnquotedSpace(psCmdLine);

    strncat(psGnuplotCommandLine, psCmdLine, sizeof(psGnuplotCommandLine) - strlen(psGnuplotCommandLine)-1);

#ifdef SHOWCMDLINE
    fprintf(stderr,"Arguments: %s\n", psCmdLine);
    fprintf(stderr,"GnuplotCommandLine: %s\n",psGnuplotCommandLine);
#endif

    /* CRS: if stdin isn't redirected then just launch wgnuplot normally
     * and exit. */
    if (isatty(fileno(stdin))) {
	if (WinExec(psGnuplotCommandLine, SW_SHOWDEFAULT) > 31) {
	    exit(EXIT_SUCCESS);
	}
	fprintf(stderr,"ERROR %u: Couldn't execute: \"%s\"\n",
		GetLastError(), psGnuplotCommandLine);
	exit(EXIT_FAILURE);
    }

    /* CRS: initialize the STARTUPINFO and call CreateProcess(). */
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.lpReserved = NULL;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpDesktop = NULL;
    siStartInfo.dwFlags = STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_SHOWMINIMIZED;

    bSuccess = CreateProcess(
			     NULL,                   /* pointer to name of executable module   */
			     psGnuplotCommandLine,   /* pointer to command line string         */
			     NULL,                   /* pointer to process security attributes */
			     NULL,                   /* pointer to thread security attributes  */
			     FALSE,                  /* handle inheritance flag                */
			     0,                      /* creation flags                         */
			     NULL,                   /* pointer to new environment block       */
			     NULL,                   /* pointer to current directory name      */
			     &siStartInfo,           /* pointer to STARTUPINFO                 */
			     &piProcInfo             /* pointer to PROCESS_INFORMATION         */
			     );

    /* if CreateProcess() failed, print a warning and exit. */
    if (! bSuccess) {
	fprintf(stderr,"ERROR %u: Couldn't execute: \"%s\"\n",
		GetLastError(), psGnuplotCommandLine);
	exit(EXIT_FAILURE);
    }

    /* CRS: give gnuplot enough time to start (1 sec.) */
    if (WaitForInputIdle(piProcInfo.hProcess, 1000)) {
	fprintf(stderr, "Timeout: gnuplot is not ready\n");
	exit(EXIT_FAILURE);
    }

    /* CRS: get the HWND of the parent window and text windows */
    EnumThreadWindows(piProcInfo.dwThreadId, cbGetTextWindow, 0);

    if (! hwndParent || ! hwndText) {
	/* Still no gnuplot window? Problem! */
	fprintf(stderr, "Can't find the gnuplot window");
	/* CRS: free the process and thread handles */
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
	exit(EXIT_FAILURE);
    }

    /* wait for commands on stdin, and pass them on to the wgnuplot text
     * window */
    while (fgets(psBuffer, BUFFER_SIZE, stdin) != NULL) {
	/* RWH: Check if wgnuplot is still alive */
	if (!ProcessAlive(piProcInfo.hProcess))
	    break;
	PostString(hwndText, psBuffer);
    }

    /* exit gracefully, unless -persist is requested */
    if (!bPersist && ProcessAlive(piProcInfo.hProcess)) {
	PostString(hwndText, "\nexit\n");
    }

    /* CRS: free the process and thread handles */
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    return EXIT_SUCCESS;
}
