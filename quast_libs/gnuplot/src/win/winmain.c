/*
 * $Id: winmain.c,v 1.77.2.2 2014/12/31 04:38:44 sfeam Exp $
 */

/* GNUPLOT - win/winmain.c */
/*[
 * Copyright 1992, 1993, 1998, 2004   Maurice Castro, Russell Lang
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
 *
 */

/* This file implements the initialization code for running gnuplot   */
/* under Microsoft Windows.                                           */
/*                                                                    */
/* The modifications to allow Gnuplot to run under Windows were made  */
/* by Maurice Castro. (maurice@bruce.cs.monash.edu.au)  3 Jul 1992    */
/* and Russell Lang (rjl@monu1.cc.monash.edu.au) 30 Nov 1992          */
/*                                                                    */

#include "syscfg.h"
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <htmlhelp.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef __WATCOMC__
# define mktemp _mktemp
#endif
#include <io.h>
#include <sys/stat.h>
#include "alloc.h"
#include "plot.h"
#include "setshow.h"
#include "version.h"
#include "command.h"
#include "winmain.h"
#include "wtext.h"
#include "wcommon.h"
#ifdef HAVE_GDIPLUS
#include "wgdiplus.h"
#endif
#ifdef WXWIDGETS
#include "wxterminal/wxt_term.h"
#endif
#ifdef HAVE_LIBCACA
# define TERM_PUBLIC_PROTO
# include "caca.trm"
# undef TERM_PUBLIC_PROTO
#endif


/* workaround for old header files */
#ifndef CSIDL_APPDATA
# define CSIDL_APPDATA (0x001a)
#endif

/* limits */
#define MAXSTR 255
#define MAXPRINTF 1024
  /* used if vsnprintf(NULL,0,...) returns zero (MingW 3.4) */

/* globals */
#ifndef WGP_CONSOLE
TW textwin;
MW menuwin;
#endif
LPGW graphwin; /* current graph window */
LPGW listgraphs; /* list of graph windows */
PW pausewin;
LPSTR szModuleName;
LPSTR szPackageDir;
LPSTR winhelpname;
LPSTR szMenuName;
static LPSTR szLanguageCode = NULL;
#if defined(WGP_CONSOLE) && defined(CONSOLE_SWITCH_CP)
BOOL cp_changed = FALSE;
UINT cp_input;  /* save previous codepage settings */
UINT cp_output;
#endif
HWND help_window = NULL;

char *authors[]={
                 "Colin Kelley",
                 "Thomas Williams"
                };

void WinExit(void);
static void WinCloseHelp(void);
int CALLBACK ShutDown();


static void
CheckMemory(LPSTR str)
{
        if (str == (LPSTR)NULL) {
                MessageBox(NULL, "out of memory", "gnuplot", MB_ICONSTOP | MB_OK);
                gp_exit(EXIT_FAILURE);
        }
}


int
Pause(LPSTR str)
{
        pausewin.Message = str;
        return (PauseBox(&pausewin) == IDOK);
}


void
kill_pending_Pause_dialog ()
{
	if (!pausewin.bPause) /* no Pause dialog displayed */
            return;
        /* Pause dialog displayed, thus kill it */
        DestroyWindow(pausewin.hWndPause);
        pausewin.bPause = FALSE;
}


/* atexit procedure */
void
WinExit(void)
{
	LPGW lpgw;

    /* Last chance, call before anything else to avoid a crash. */
    WinCloseHelp();

    term_reset();

#ifndef __MINGW32__ /* HBB 980809: FIXME: doesn't exist for MinGW32. So...? */
    fcloseall();
#endif

	/* Close all graph windows */
	for (lpgw = listgraphs; lpgw != NULL; lpgw = lpgw->next) {
		if (GraphHasWindow(lpgw))
			GraphClose(lpgw);
	}

#ifndef WGP_CONSOLE
    TextMessage();  /* process messages */
# ifndef __WATCOMC__
    /* revert C++ stream redirection */
    RedirectOutputStreams(FALSE);
# endif
#else
#ifdef CONSOLE_SWITCH_CP
    /* restore console codepages */
    if (cp_changed) {
		SetConsoleCP(cp_input);
		SetConsoleOutputCP(cp_output);
		/* file APIs are per process */
    }
#endif
#endif
#ifdef HAVE_GDIPLUS
    gdiplusCleanup();
#endif
    return;
}

/* call back function from Text Window WM_CLOSE */
int CALLBACK
ShutDown()
{
	/* First chance for wgnuplot to close help system. */
	WinCloseHelp();
	gp_exit(EXIT_SUCCESS);
	return 0;
}


/* This function can be used to retrieve version information from
 * Window's Shell and common control libraries (Comctl32.dll,
 * Shell32.dll, and Shlwapi.dll) The code was copied from the MSDN
 * article "Shell and Common Controls Versions" */
DWORD
GetDllVersion(LPCTSTR lpszDllName)
{
    HINSTANCE hinstDll;
    DWORD dwVersion = 0;

    /* For security purposes, LoadLibrary should be provided with a
       fully-qualified path to the DLL. The lpszDllName variable should be
       tested to ensure that it is a fully qualified path before it is used. */
    hinstDll = LoadLibrary(lpszDllName);

    if (hinstDll) {
        DLLGETVERSIONPROC pDllGetVersion;
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll,
                          "DllGetVersion");

        /* Because some DLLs might not implement this function, you
        must test for it explicitly. Depending on the particular
        DLL, the lack of a DllGetVersion function can be a useful
        indicator of the version. */
        if (pDllGetVersion) {
            DLLVERSIONINFO dvi;
            HRESULT hr;

            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);
            hr = (*pDllGetVersion)(&dvi);
            if (SUCCEEDED(hr))
               dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
        }
        FreeLibrary(hinstDll);
    }
    return dwVersion;
}


BOOL IsWindowsXPorLater(void)
{
    OSVERSIONINFO versionInfo;

    /* get Windows version */
    ZeroMemory(&versionInfo, sizeof(OSVERSIONINFO));
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&versionInfo);
    return ((versionInfo.dwMajorVersion > 5) ||
           ((versionInfo.dwMajorVersion == 5) && (versionInfo.dwMinorVersion >= 1)));
}


char *
appdata_directory(void)
{
    HMODULE hShell32;
    FARPROC pSHGetSpecialFolderPath;
    static char dir[MAX_PATH] = "";

    if (dir[0])
        return dir;

    /* Make sure that SHGetSpecialFolderPath is supported. */
    hShell32 = LoadLibrary(TEXT("shell32.dll"));
    if (hShell32) {
        pSHGetSpecialFolderPath =
            GetProcAddress(hShell32,
                           TEXT("SHGetSpecialFolderPathA"));
        if (pSHGetSpecialFolderPath)
            (*pSHGetSpecialFolderPath)(NULL, dir, CSIDL_APPDATA, FALSE);
        FreeModule(hShell32);
        return dir;
    }

    /* use APPDATA environment variable as fallback */
    if (dir[0] == '\0') {
        char *appdata = getenv("APPDATA");
        if (appdata) {
            strcpy(dir, appdata);
            return dir;
        }
    }

    return NULL;
}


static void
WinCloseHelp(void)
{
	/* Due to a known bug in the HTML help system we have to
	 * call this as soon as possible before the end of the program.
	 * See e.g. http://helpware.net/FAR/far_faq.htm#HH_CLOSE_ALL
	 */
	if (IsWindow(help_window))
		SendMessage(help_window, WM_CLOSE, 0, 0);
	Sleep(0);
}


static char *
GetLanguageCode()
{
	static char lang[6] = "";

	if (lang[0] == NUL) {
		GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SABBREVLANGNAME, lang, sizeof(lang));
		//strcpy(lang, "JPN"); //TEST
		/* language definition files for Japanese already use "ja" as abbreviation */
		if (strcmp(lang, "JPN") == 0)
			lang[1] = 'A';
		/* prefer lower case */
		lang[0] = tolower((unsigned char)lang[0]);
		lang[1] = tolower((unsigned char)lang[1]);
		/* only use two character sequence */
		lang[2] = NUL;
	}

	return lang;
}


static char *
LocalisedFile(const char * name, const char * ext, const char * defaultname)
{
	char * lang;
	char * filename;

	/* Allow user to override language detection. */
	if (szLanguageCode)
		lang = szLanguageCode;
	else
		lang = GetLanguageCode();

	filename = (LPSTR) malloc(strlen(szModuleName) + strlen(name) + strlen(lang) + strlen(ext) + 1);
	if (filename) {
		strcpy(filename, szModuleName);
		strcat(filename, name);
		strcat(filename, lang);
		strcat(filename, ext);
		if (!existfile(filename)) {
			strcpy(filename, szModuleName);
			strcat(filename, defaultname);
		}
	}
	return filename;
}


static void
ReadMainIni(LPSTR file, LPSTR section)
{
	char profile[81] = "";
	const char hlpext[] = ".chm";
	const char name[] = "wgnuplot-";

	/* Language code override */
	GetPrivateProfileString(section, "Language", "", profile, 80, file);
	if (profile[0] != NUL)
		szLanguageCode = strdup(profile);
	else
		szLanguageCode = NULL;

	/* help file name */
	GetPrivateProfileString(section, "HelpFile", "", profile, 80, file);
	if (profile[0] != NUL) {
		winhelpname = (LPSTR) malloc(strlen(szModuleName) + strlen(profile) + 1);
		if (winhelpname) {
			strcpy(winhelpname, szModuleName);
			strcat(winhelpname, profile);
		}
	} else {
		/* default name is "wgnuplot-LL.chm" */
		winhelpname = LocalisedFile(name, hlpext, HELPFILE);
	}

	/* menu file name */
	GetPrivateProfileString(section, "MenuFile", "", profile, 80, file);
	if (profile[0] != NUL) {
		szMenuName = (LPSTR) malloc(strlen(szModuleName) + strlen(profile) + 1);
		if (szMenuName) {
			strcpy(szMenuName, szModuleName);
			strcat(szMenuName, profile);
		}
	} else {
		/* default name is "wgnuplot-LL.mnu" */
		szMenuName = LocalisedFile(name, ".mnu", "wgnuplot.mnu");
	}
}


#ifndef WGP_CONSOLE
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
#else
int
main(int argc, char **argv)
#endif
{
	LPSTR tail;
	int i;

#ifdef WGP_CONSOLE
	HINSTANCE hInstance = GetModuleHandle(NULL), hPrevInstance = NULL;
#endif


#ifndef WGP_CONSOLE
# if defined( __MINGW32__) && !defined(_W64)
#  define argc _argc
#  define argv _argv
# else /* MSVC, WATCOM, MINGW-W64 */
#  define argc __argc
#  define argv __argv
# endif
#endif /* WGP_CONSOLE */

        szModuleName = (LPSTR)malloc(MAXSTR+1);
        CheckMemory(szModuleName);

        /* get path to EXE */
        GetModuleFileName(hInstance, (LPSTR) szModuleName, MAXSTR);
        if ((tail = (LPSTR)_fstrrchr(szModuleName,'\\')) != (LPSTR)NULL)
        {
                tail++;
                *tail = 0;
        }
        szModuleName = (LPSTR)realloc(szModuleName, _fstrlen(szModuleName)+1);
        CheckMemory(szModuleName);

        if (_fstrlen(szModuleName) >= 5 && _fstrnicmp(&szModuleName[_fstrlen(szModuleName)-5], "\\bin\\", 5) == 0)
        {
                int len = _fstrlen(szModuleName)-4;
                szPackageDir = (LPSTR)malloc(len+1);
                CheckMemory(szPackageDir);
                _fstrncpy(szPackageDir, szModuleName, len);
                szPackageDir[len] = '\0';
        }
        else
                szPackageDir = szModuleName;

#ifndef WGP_CONSOLE
        textwin.hInstance = hInstance;
        textwin.hPrevInstance = hPrevInstance;
        textwin.nCmdShow = nCmdShow;
        textwin.Title = "gnuplot";
#endif

		/* create structure of first graph window */
		graphwin = (LPGW) calloc(1, sizeof(GW));
		listgraphs = graphwin;

		/* locate ini file */
		{
			char * inifile;
			get_user_env(); /* this hasn't been called yet */
			inifile = gp_strdup("~\\wgnuplot.ini");
			gp_expand_tilde(&inifile);

			/* if tilde expansion fails use current directory as
			   default - that was the previous default behaviour */
			if (inifile[0] == '~') {
				free(inifile);
				inifile = "wgnuplot.ini";
			}

#ifndef WGP_CONSOLE
			textwin.IniFile = inifile;
#endif
			graphwin->IniFile = inifile;

			ReadMainIni(inifile, "WGNUPLOT");
		}

#ifndef WGP_CONSOLE
        textwin.IniSection = "WGNUPLOT";
        textwin.DragPre = "load '";
        textwin.DragPost = "'\n";
        textwin.lpmw = &menuwin;
        textwin.ScreenSize.x = 80;
        textwin.ScreenSize.y = 80;
        textwin.KeyBufSize = 2048;
        textwin.CursorFlag = 1; /* scroll to cursor after \n & \r */
        textwin.shutdown = MakeProcInstance((FARPROC)ShutDown, hInstance);
        textwin.AboutText = (LPSTR)malloc(1024);
        CheckMemory(textwin.AboutText);
        sprintf(textwin.AboutText,
	    "Version %s patchlevel %s\n" \
	    "last modified %s\n" \
	    "%s\n%s, %s and many others\n" \
	    "gnuplot home:     http://www.gnuplot.info\n",
            gnuplot_version, gnuplot_patchlevel,
	    gnuplot_date,
	    gnuplot_copyright, authors[1], authors[0]);
        textwin.AboutText = (LPSTR)realloc(textwin.AboutText, _fstrlen(textwin.AboutText)+1);
        CheckMemory(textwin.AboutText);

        menuwin.szMenuName = szMenuName;
#endif

        pausewin.hInstance = hInstance;
        pausewin.hPrevInstance = hPrevInstance;
        pausewin.Title = "gnuplot pause";

        graphwin->hInstance = hInstance;
        graphwin->hPrevInstance = hPrevInstance;
#ifdef WGP_CONSOLE
        graphwin->lptw = NULL;
#else
        graphwin->lptw = &textwin;
#endif

		/* init common controls */
	{
	    INITCOMMONCONTROLSEX initCtrls;
	    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
	    initCtrls.dwICC = ICC_WIN95_CLASSES;
	    InitCommonControlsEx(&initCtrls);
	}

#ifndef WGP_CONSOLE
	if (TextInit(&textwin))
		gp_exit(EXIT_FAILURE);
	textwin.hIcon = LoadIcon(hInstance, "TEXTICON");
	SetClassLongPtr(textwin.hWndParent, GCLP_HICON, (LONG_PTR)textwin.hIcon);

	/* Note: we want to know whether this is an interactive session so that we can
	 * decide whether or not to write status information to stderr.  The old test
	 * for this was to see if (argc > 1) but the addition of optional command line
	 * switches broke this.  What we really wanted to know was whether any of the
	 * command line arguments are file names or an explicit in-line "-e command".
	 * (This is a copy of a code snippet from plot.c)
	 */
	for (i = 1; i < argc; i++) {
		if (!stricmp(argv[i], "/noend"))
			continue;
		if ((argv[i][0] != '-') || (argv[i][1] == 'e')) {
			interactive = FALSE;
			break;
		}
	}
	if (interactive)
		ShowWindow(textwin.hWndParent, textwin.nCmdShow);
	if (IsIconic(textwin.hWndParent)) { /* update icon */
		RECT rect;
		GetClientRect(textwin.hWndParent, (LPRECT) &rect);
		InvalidateRect(textwin.hWndParent, (LPRECT) &rect, 1);
		UpdateWindow(textwin.hWndParent);
	}
# ifndef __WATCOMC__
	/* Finally, also redirect C++ standard output streams. */
	RedirectOutputStreams(TRUE);
# endif
#else /* WGP_CONSOLE */
#ifdef CONSOLE_SWITCH_CP
        /* Change codepage of console to match that of the graph window.
           WinExit() will revert this.
           Attention: display of characters does not work correctly with
           "Terminal" font! Users will have to use "Lucida Console" or similar.
        */
        cp_input = GetConsoleCP();
        cp_output = GetConsoleOutputCP();
        if (cp_input != GetACP()) {
            cp_changed = TRUE;
            SetConsoleCP(GetACP()); /* keyboard input */
            SetConsoleOutputCP(GetACP()); /* screen output */
            SetFileApisToANSI(); /* file names etc. */
        }
#endif
#endif

	gp_atexit(WinExit);

	if (!isatty(fileno(stdin)))
		setmode(fileno(stdin), O_BINARY);

	gnu_main(argc, argv);

	/* First chance to close help system for console gnuplot,
	   second for wgnuplot */
	WinCloseHelp();
	gp_exit_cleanup();
	return 0;
}


#ifndef WGP_CONSOLE

/* replacement stdio routines that use Text Window for stdin/stdout */
/* WARNING: Do not write to stdout/stderr with functions not listed
   in win/wtext.h */

#undef kbhit
#undef getche
#undef getch
#undef putch

#undef fgetc
#undef getchar
#undef getc
#undef fgets
#undef gets

#undef fputc
#undef putchar
#undef putc
#undef fputs
#undef puts

#undef fprintf
#undef printf
#undef vprintf
#undef vfprintf

#undef fwrite
#undef fread

#define isterm(f) (f==stdin || f==stdout || f==stderr)

int
MyPutCh(int ch)
{
    return TextPutCh(&textwin, (BYTE)ch);
}

int
MyKBHit()
{
    return TextKBHit(&textwin);
}

int
MyGetCh()
{
    return TextGetCh(&textwin);
}

int
MyGetChE()
{
    return TextGetChE(&textwin);
}

int
MyFGetC(FILE *file)
{
    if (isterm(file)) {
        return MyGetChE();
    }
    return fgetc(file);
}

char *
MyGetS(char *str)
{
    TextPutS(&textwin,"\nDANGER: gets() used\n");
    MyFGetS(str,80,stdin);
    if (strlen(str) > 0
        && str[strlen(str)-1]=='\n')
        str[strlen(str)-1] = '\0';
    return str;
}

char *
MyFGetS(char *str, unsigned int size, FILE *file)
{
    char *p;

    if (isterm(file)) {
        p = TextGetS(&textwin, str, size);
        if (p != (char *)NULL)
            return str;
        return (char *)NULL;
    }
    return fgets(str,size,file);
}

int
MyFPutC(int ch, FILE *file)
{
    if (isterm(file)) {
        MyPutCh((BYTE)ch);
#ifndef WGP_CONSOLE
        TextMessage();
#endif
        return ch;
    }
    return fputc(ch,file);
}

int
MyFPutS(const char *str, FILE *file)
{
    if (isterm(file)) {
        TextPutS(&textwin, (char*) str);
#ifndef WGP_CONSOLE
        TextMessage();
#endif
        return (*str);  /* different from Borland library */
    }
    return fputs(str,file);
}

int
MyPutS(char *str)
{
    TextPutS(&textwin, str);
    MyPutCh('\n');
    TextMessage();
    return 0;   /* different from Borland library */
}

int
MyFPrintF(FILE *file, const char *fmt, ...)
{
	int count;
	va_list args;

	va_start(args, fmt);
	if (isterm(file)) {
		char *buf;

		count = vsnprintf(NULL, 0, fmt, args) + 1;
		if (count == 0)
			count = MAXPRINTF;
		va_end(args);
		va_start(args, fmt);
		buf = (char *) malloc(count * sizeof(char));
		count = vsnprintf(buf, count, fmt, args);
		TextPutS(&textwin, buf);
		free(buf);
	} else {
		count = vfprintf(file, fmt, args);
	}
	va_end(args);
	return count;
}

int
MyVFPrintF(FILE *file, const char *fmt, va_list args)
{
	int count;

	if (isterm(file)) {
		char *buf;
		va_list args_copied;

		va_copy(args_copied, args);
		count = vsnprintf(NULL, 0U, fmt, args) + 1;
		if (count == 0)
			count = MAXPRINTF;
		va_end(args_copied);
		buf = (char *) malloc(count * sizeof(char));
		count = vsnprintf(buf, count, fmt, args);
		TextPutS(&textwin, buf);
		free(buf);
	} else {
		count = vfprintf(file, fmt, args);
	}
	return count;
}

int
MyPrintF(const char *fmt, ...)
{
	int count;
	char *buf;
	va_list args;

	va_start(args, fmt);
	count = vsnprintf(NULL, 0, fmt, args) + 1;
	if (count == 0)
		count = MAXPRINTF;
	va_end(args);
	va_start(args, fmt);
	buf = (char *) malloc(count * sizeof(char));
	count = vsnprintf(buf, count, fmt, args);
	TextPutS(&textwin, buf);
	free(buf);
	va_end(args);
	return count;
}

size_t
MyFWrite(const void *ptr, size_t size, size_t n, FILE *file)
{
    if (isterm(file)) {
        size_t i;
        for (i=0; i<n; i++)
            TextPutCh(&textwin, ((BYTE *)ptr)[i]);
        TextMessage();
        return n;
    }
    return fwrite(ptr, size, n, file);
}

size_t
MyFRead(void *ptr, size_t size, size_t n, FILE *file)
{
    if (isterm(file)) {
        size_t i;

        for (i=0; i<n; i++)
            ((BYTE *)ptr)[i] = TextGetChE(&textwin);
        TextMessage();
        return n;
    }
    return fread(ptr, size, n, file);
}


#ifdef USE_FAKEPIPES

static char pipe_type = NUL;
static char * pipe_filename = NULL;
static char * pipe_command = NULL;

FILE *
fake_popen(const char * command, const char * type)
{
	FILE * f = NULL;
	char tmppath[MAX_PATH];
	char tmpfile[MAX_PATH];
	DWORD ret;

	if (type == NULL) return NULL;

	pipe_type = NUL;
	if (pipe_filename != NULL)
		free(pipe_filename);

	/* Random temp file name in %TEMP% */
	ret = GetTempPath(sizeof(tmppath), tmppath);
	if ((ret == 0) || (ret > sizeof(tmppath)))
		return NULL;
	ret = GetTempFileName(tmppath, "gpp", 0, tmpfile);
	if (ret == 0)
		return NULL;
	pipe_filename = strdup(tmpfile);

	if (*type == 'r') {
		char * cmd;
		int rc;
		pipe_type = *type;
		/* Execute command with redirection of stdout to temporary file. */
		cmd = (char *) malloc(strlen(command) + strlen(pipe_filename) + 5);
		sprintf(cmd, "%s > %s", command, pipe_filename);
		rc = system(cmd);
		free(cmd);
		/* Now open temporary file. */
		/* system() returns 1 if the command could not be executed. */
		if (rc != 1)
			f = fopen(pipe_filename, "r");
		else {
			remove(pipe_filename);
			free(pipe_filename);
			pipe_filename = NULL;
			errno = EINVAL;
		}
	} else if (*type == 'w') {
		pipe_type = *type;
		/* Write output to temporary file and handle the rest in fake_pclose. */
		if (type[1] == 'b')
			int_error(NO_CARET, "Could not execute pipe '%s'. Writing to binary pipes is not supported.", command);
		else
			f = fopen(pipe_filename, "w");
		pipe_command = strdup(command);
	}

	return f;
}


int fake_pclose(FILE *stream)
{
	int rc = 0;
	if (!stream) return ECHILD;

	/* Close temporary file */
	fclose(stream);

	/* Finally, execute command with redirected stdin. */
	if (pipe_type == 'w') {
		char * cmd;
		cmd = (char *) gp_alloc(strlen(pipe_command) + strlen(pipe_filename) + 10, "fake_pclose");
		/* FIXME: this won't work for binary data. We need a proper `cat` replacement. */
		sprintf(cmd, "type %s | %s", pipe_filename, pipe_command);
		rc = system(cmd);
		free(cmd);
	}

	/* Delete temp file again. */
	if (pipe_filename) {
		remove(pipe_filename);
		errno = 0;
		free(pipe_filename);
		pipe_filename = NULL;
	}

	if (pipe_command) {
		/* system() returns 255 if the command could not be executed.
		   The real popen would have returned an error already. */
		if (rc == 255)
			int_error(NO_CARET, "Could not execute pipe '%s'.", pipe_command);
		free(pipe_command);
	}

	return rc;
}
#endif

#else /* WGP_CONSOLE */


DWORD WINAPI stdin_pipe_reader(LPVOID param)
{
#if 0
    HANDLE h = (HANDLE)_get_osfhandle(fileno(stdin));
    char c;
    DWORD cRead;

    if (ReadFile(h, &c, 1, &cRead, NULL))
        return c;
#else
    unsigned char c;
    if (fread(&c, 1, 1, stdin) == 1)
        return (DWORD)c;
    return EOF;
#endif
}


int ConsoleGetch()
{
	int fd = fileno(stdin);
	HANDLE h;
	DWORD waitResult;

	if (!isatty(fd))
		h = CreateThread(NULL, 0, stdin_pipe_reader, NULL, 0, NULL);
	else
		h = (HANDLE)_get_osfhandle(fd);

	do {
		waitResult = MsgWaitForMultipleObjects(1, &h, FALSE, INFINITE, QS_ALLINPUT);
		if (waitResult == WAIT_OBJECT_0) {
				DWORD c;
			if (isatty(fd)) {
				c = ConsoleReadCh();
				if (c != NUL)
					return c;
			} else {
				GetExitCodeThread(h, &c);
				CloseHandle(h);
				return c;
			}
		} else if (waitResult == WAIT_OBJECT_0+1) {
			WinMessageLoop();
			if (ctrlc_flag)
				return '\r';
		} else
			break;
	} while (1);

	return '\r';
}

#endif /* WGP_CONSOLE */


int ConsoleReadCh()
{
	INPUT_RECORD rec;
	DWORD recRead;
	HANDLE h;

	h = GetStdHandle(STD_INPUT_HANDLE);
	if (h == NULL)
		return NUL;

	ReadConsoleInput(h, &rec, 1, &recRead);
	/* FIXME: We should handle rec.Event.KeyEvent.wRepeatCount > 1, too. */
	if (recRead == 1 && rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown &&
			(rec.Event.KeyEvent.wVirtualKeyCode < VK_SHIFT ||
				rec.Event.KeyEvent.wVirtualKeyCode > VK_MENU)) {
		if (rec.Event.KeyEvent.uChar.AsciiChar) {
			if ((rec.Event.KeyEvent.dwControlKeyState == SHIFT_PRESSED) && (rec.Event.KeyEvent.wVirtualKeyCode == VK_TAB))
				return 034; /* remap Shift-Tab */
			else
				return rec.Event.KeyEvent.uChar.AsciiChar;
		} else {
			switch (rec.Event.KeyEvent.wVirtualKeyCode) {
				case VK_UP: return 020;
				case VK_DOWN: return 016;
				case VK_LEFT: return 002;
				case VK_RIGHT: return 006;
				case VK_HOME: return 001;
				case VK_END: return 005;
				case VK_DELETE: return 0117;
			}
		}
	}

	/* Error reading event or, key up or, one of the following event records:
	   MOUSE_EVENT_RECORD, WINDOW_BUFFER_SIZE_RECORD, MENU_EVENT_RECORD, FOCUS_EVENT_RECORD */
	return NUL;
}


/* public interface to printer routines : Windows PRN emulation
 * (formerly in win.trm)
 */

#define MAX_PRT_LEN 256
static char win_prntmp[MAX_PRT_LEN+1];

FILE *
open_printer()
{
	char *temp;

	if ((temp = getenv("TEMP")) == (char *)NULL)
		*win_prntmp = '\0';
	else  {
		safe_strncpy(win_prntmp, temp, MAX_PRT_LEN);
		/* stop X's in path being converted by mktemp */
		for (temp = win_prntmp; *temp != NUL; temp++)
			*temp = tolower((unsigned char)*temp);
		if ((strlen(win_prntmp) > 0) && (win_prntmp[strlen(win_prntmp) - 1] != '\\'))
			strcat(win_prntmp, "\\");
	}
	strncat(win_prntmp, "_gptmp", MAX_PRT_LEN - strlen(win_prntmp));
	strncat(win_prntmp, "XXXXXX", MAX_PRT_LEN - strlen(win_prntmp));
	mktemp(win_prntmp);
	return fopen(win_prntmp, "w");
}

void
close_printer(FILE *outfile)
{
    fclose(outfile);
    DumpPrinter(graphwin->hWndGraph, graphwin->Title, win_prntmp);
}

void
screen_dump()
{
    GraphPrint(graphwin);
}


void
win_raise_terminal_window(int id)
{
	LPGW lpgw = listgraphs;
	while ((lpgw != NULL) && (lpgw->Id != id))
		lpgw = lpgw->next;
	if (lpgw != NULL) {
		ShowWindow(lpgw->hWndGraph, SW_SHOWNORMAL);
		BringWindowToTop(lpgw->hWndGraph);
	}
}

void
win_raise_terminal_group(void)
{
	LPGW lpgw = listgraphs;
	while (lpgw != NULL) {
		ShowWindow(lpgw->hWndGraph, SW_SHOWNORMAL);
		BringWindowToTop(lpgw->hWndGraph);
		lpgw = lpgw->next;
	}
}

void
win_lower_terminal_window(int id)
{
	LPGW lpgw = listgraphs;
	while ((lpgw != NULL) && (lpgw->Id != id))
		lpgw = lpgw->next;
	if (lpgw != NULL)
		SetWindowPos(lpgw->hWndGraph, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void
win_lower_terminal_group(void)
{
	LPGW lpgw = listgraphs;
	while (lpgw != NULL) {
		SetWindowPos(lpgw->hWndGraph, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
		lpgw = lpgw->next;
	}
}


/* returns true if there are any graph windows open (win terminal) */
static TBOOLEAN
WinWindowOpened(void)
{
	LPGW lpgw;

	lpgw = listgraphs;
	while (lpgw != NULL) {
		if (GraphHasWindow(lpgw))
			return TRUE;
		lpgw = lpgw->next;
	}
	return FALSE;
}


/* returns true if there are any graph windows open (wxt/caca/win terminals) */
/* Note: This routine is used to handle "persist". Do not test for qt windows here 
         since they run in a separate process */
TBOOLEAN
WinAnyWindowOpen(void)
{
	TBOOLEAN window_opened = WinWindowOpened();
#ifdef WXWIDGETS
	window_opened |= wxt_window_opened();
#endif
#ifdef HAVE_LIBCACA
	window_opened |= CACA_window_opened();
#endif
	return window_opened;
}


#ifndef WGP_CONSOLE
void
WinPersistTextClose(void)
{
	if (!WinAnyWindowOpen() &&
		(textwin.hWndParent != NULL) && !IsWindowVisible(textwin.hWndParent))
		PostMessage(textwin.hWndParent, WM_CLOSE, 0, 0);
}
#endif


void
WinMessageLoop(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		/* HBB 19990505: Petzold says we should check this: */
		if (msg.message == WM_QUIT)
			return;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}


void
WinRaiseConsole(void)
{
	HWND console = NULL;
#ifndef WGP_CONSOLE
	console = textwin.hWndParent;
#else
	console = GetConsoleWindow();
#endif
	if (console != NULL) {
		ShowWindow(console, SW_SHOWNORMAL);
		BringWindowToTop(console);
	}
}
