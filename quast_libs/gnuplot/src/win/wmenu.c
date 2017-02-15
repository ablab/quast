/*
 * $Id: wmenu.c,v 1.27.2.1 2016/09/12 17:22:56 markisch Exp $Id: wmenu.c,v 1.27.2.1 2016/09/12 17:22:56 markisch Exp $
 */

/* GNUPLOT - win/wmenu.c */
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

#include "syscfg.h"
#define STRICT
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <winuser.h>
#include <shlwapi.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "wgnuplib.h"
#include "wresourc.h"
#include "stdfn.h"
#include "wcommon.h"

/* Choose between the directory dialog of the windows shell and
   a modified version of the "file open" dialog */
#define SHELL_DIR_DIALOG

INT_PTR CALLBACK InputBoxDlgProc(HWND, UINT, WPARAM, LPARAM);

/* limits */
#define MAXSTR 255
#define MACROLEN 10000
/* #define NUMMENU 256  defined in wresourc.h */
#define MENUDEPTH 3

/* menu tokens */
#define CMDMIN 129
#define INPUT 129
#define EOS 130
#define OPEN 131
#define SAVE 132
#define DIRECTORY 133
#define OPTIONS 134
#define ABOUT 135
#define CMDMAX 135
char * keyword[] = {
	"[INPUT]", "[EOS]", "[OPEN]", "[SAVE]", "[DIRECTORY]",
	"[OPTIONS]", "[ABOUT]",
        "{ENTER}", "{ESC}", "{TAB}",
        "{^A}", "{^B}", "{^C}", "{^D}", "{^E}", "{^F}", "{^G}", "{^H}",
	"{^I}", "{^J}", "{^K}", "{^L}", "{^M}", "{^N}", "{^O}", "{^P}",
	"{^Q}", "{^R}", "{^S}", "{^T}", "{^U}", "{^V}", "{^W}", "{^X}",
	"{^Y}", "{^Z}", "{^[}", "{^\\}", "{^]}", "{^^}", "{^_}",
	NULL};
BYTE keyeq[] = {
	INPUT, EOS, OPEN, SAVE, DIRECTORY,
	OPTIONS, ABOUT,
        13, 27, 9,
        1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 28, 29, 30, 31,
	0};


#define GBUFSIZE 512
typedef struct tagGFILE {
	HFILE	hfile;
	char 	getbuf[GBUFSIZE];
	int	getnext;
	int	getleft;
} GFILE;


static GFILE * Gfopen(LPSTR lpszFileName, int fnOpenMode);
static void Gfclose(GFILE * gfile);
static int Gfgets(LPSTR lp, int size, GFILE *gfile);
static int GetLine(char * buffer, int len, GFILE *gfile);
static void LeftJustify(char *d, char *s);
static BYTE MacroCommand(LPTW lptw, UINT m);
static void TranslateMacro(char *string);


#ifdef SHELL_DIR_DIALOG

INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData);

/* This is missing in MingW 2.95 */
#ifndef BIF_EDITBOX
# define BIF_EDITBOX 0x0010
#endif

/* Note: this code has been bluntly copied from MSDN article KB179378
         "How To Browse for Folders from the Current Directory"
*/
INT CALLBACK
BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	TCHAR szDir[MAX_PATH];

	switch (uMsg) {
	case BFFM_INITIALIZED:
		if (GetCurrentDirectory(sizeof(szDir)/sizeof(TCHAR), szDir)) {
			/* WParam is TRUE since you are passing a path.
			  It would be FALSE if you were passing a pidl. */
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)szDir);
		}
		break;

	case BFFM_SELCHANGED:
		/* Set the status window to the currently selected path. */
		if (SHGetPathFromIDList((LPITEMIDLIST) lp, szDir)) {
			SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
		}
		break;
	}
	return 0;
}

#else /* SHELL_DIR_DIALOG */

/* Yes, you can use Windows shell functions even without C++ !
   These functions are not defined in shlobj.h, so we do it ourselves:
*/
#ifndef IShellFolder_BindToObject
#define IShellFolder_BindToObject(This,pidl,pbcReserved,riid,ppvOut) \
		(This)->lpVtbl -> BindToObject(This,pidl,pbcReserved,riid,ppvOut)
#endif
#ifndef IShellFolder_GetDisplayNameOf
#define IShellFolder_GetDisplayNameOf(This,pidl,uFlags,lpName) \
		(This)->lpVtbl -> GetDisplayNameOf(This,pidl,uFlags,lpName)
#endif

/* My windows header files do not define these: */
#ifndef WC_NO_BEST_FIT_CHARS
#define WC_NO_BEST_FIT_CHARS      0x00000400  /* do not use best fit chars */
#endif

/* We really need this struct which is used by newer Windows versions */
typedef struct tagOFN {
  DWORD         lStructSize;
  HWND          hwndOwner;
  HINSTANCE     hInstance;
  LPCTSTR       lpstrFilter;
  LPTSTR        lpstrCustomFilter;
  DWORD         nMaxCustFilter;
  DWORD         nFilterIndex;
  LPTSTR        lpstrFile;
  DWORD         nMaxFile;
  LPTSTR        lpstrFileTitle;
  DWORD         nMaxFileTitle;
  LPCTSTR       lpstrInitialDir;
  LPCTSTR       lpstrTitle;
  DWORD         Flags;
  WORD          nFileOffset;
  WORD          nFileExtension;
  LPCTSTR       lpstrDefExt;
  LPARAM        lCustData;
  LPOFNHOOKPROC lpfnHook;
  LPCTSTR       lpTemplateName;
/* #if (_WIN32_WINNT >= 0x0500) */
  void *        pvReserved;
  DWORD         dwReserved;
  DWORD         FlagsEx;
/* #endif // (_WIN32_WINNT >= 0x0500) */
} NEW_OPENFILENAME, *NEW_LPOPENFILENAME;


UINT_PTR CALLBACK OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uiMsg) {
	case WM_INITDIALOG: {
		HWND parent;
		parent = GetParent(hdlg);
		/* Hint: The control codes for this can be found on MSDN */
		/* Hide "file type" display */
		CommDlg_OpenSave_HideControl(parent, stc2);
		CommDlg_OpenSave_HideControl(parent, cmb1);
		/* Hide "current file" display */
		CommDlg_OpenSave_HideControl(parent, stc3);
		CommDlg_OpenSave_HideControl(parent, cmb13);
		break;
		}

	case WM_NOTIFY: {
		LPOFNOTIFY lpOfNotify = (LPOFNOTIFY) lParam;
		switch(lpOfNotify->hdr.code) {
#if 0
		/* Well, this event is not called for ordinary files (sigh) */
		case CDN_INCLUDEITEM:
			return 0;
			break;
#endif
		/* But there's a solution which can be found here:
			http://msdn.microsoft.com/msdnmag/issues/03/10/CQA/default.aspx
			http://msdn.microsoft.com/msdnmag/issues/03/09/CQA/
		   It's C++ though (sigh again), so here is its analogue in plain C:
		*/
		/* case CDN_SELCHANGE: */
		case CDN_FOLDERCHANGE: {
			HWND parent, hlst2, list;
			LPCITEMIDLIST pidlFolder;
			LPMALLOC pMalloc;
			signed int count, i;
			unsigned len;

			/* find listbox control */
			parent = GetParent(hdlg);
			hlst2 = GetDlgItem(parent, lst2);
			list = GetDlgItem(hlst2, 1);

			SHGetMalloc(&pMalloc);

			/* First, get PIDL of current folder by sending CDM_GETFOLDERIDLIST
			   get length first, then allocate. */
			len = CommDlg_OpenSave_GetFolderIDList(parent, 0, 0);
			if (len>0) {
				LPSHELLFOLDER ishDesk;
				LPSHELLFOLDER ishFolder;
				HRESULT hr;
				STRRET str;

				pidlFolder = IMalloc_Alloc(pMalloc, len+1);
				CommDlg_OpenSave_GetFolderIDList(parent, (WPARAM)(void*)pidlFolder, len);

				/* Now get IShellFolder for pidlFolder */
				SHGetDesktopFolder(&ishDesk);
				hr = IShellFolder_BindToObject(ishDesk, pidlFolder, NULL, &IID_IShellFolder, &ishFolder);
				if (!SUCCEEDED(hr)) {
					ishFolder = ishDesk;
				}

				/* Enumerate listview items */
				count = ListView_GetItemCount(list);
				for (i = count-1; i >= 0; i--)
				{
					const ULONG flags = SHGDN_NORMAL | SHGDN_FORPARSING;
					LVITEM lvitem;
					LPCITEMIDLIST pidl;
#if 0
					/* The normal code to retrieve the item's text is
					   not very useful since user may select "hide common
					   extensions" */
					path = (char *)malloc(MAX_PATH+1);
					ListView_GetItemText(list, i, 0, path, MAX_PATH );
#endif
					/* The following code retrieves the real path of every
					   item in any case */
					/* Retrieve PIDL of current item */
					ZeroMemory(&lvitem,sizeof(lvitem));
					lvitem.iItem = i;
					lvitem.mask = LVIF_PARAM;
					ListView_GetItem(list, &lvitem);
					pidl = (LPCITEMIDLIST)lvitem.lParam;

					/* Finally, get the path name from pidlFolder */
					str.uType = STRRET_WSTR;
					hr = IShellFolder_GetDisplayNameOf(ishFolder, pidl, flags, &str);
					if (SUCCEEDED(hr)) {
						struct _stat itemStat;
						char path[MAX_PATH+1];

						/* (sigh) conversion would have been so easy...
						   hr = StrRetToBuf( str, pidl, path, MAX_PATH); */
						if (str.uType == STRRET_WSTR) {
							unsigned wlen = wcslen(str.pOleStr);
							wlen = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
														str.pOleStr, wlen+1, path, MAX_PATH,
														NULL, NULL);
							_wstat(str.pOleStr, &itemStat);
							/* Must free memory allocated by shell using shell's IMalloc  */
							IMalloc_Free(pMalloc, (LPVOID)str.pOleStr);
						} else if (str.uType == STRRET_CSTR) {
							strncpy(path, str.cStr, MAX_PATH);
							_stat(str.cStr, &itemStat);
						} else {
							/* this shouldn't happen */
							path[0] = '\0';
						}

						/* discard all non-directories from list */
						if ((itemStat.st_mode & _S_IFDIR) == 0) {
							ListView_DeleteItem(list, i);
						}
					}
				}  /* Enumerate listview items */
				IMalloc_Free(pMalloc, (void*)pidlFolder);
			}
			IMalloc_Release(pMalloc);
			break;
			} /* CDN_FOLDERCHANGE */
		} /* switch(hdr.code) */
		break;
		} /* WM_NOTIFY */
	}; /* switch(uiMsg) */
	return 0;
}

#endif /* !SHELL_DIR_DIALOG */


BYTE
MacroCommand(LPTW lptw, UINT m)
{
	BYTE *s = lptw->lpmw->macro[m];
	while (s && *s) {
		if ((*s >= CMDMIN) && (*s <= CMDMAX))
			return *s;
		s++;
	}
	return 0;
}


/* Send a macro to the text window */
void
SendMacro(LPTW lptw, UINT m)
{
BYTE *s;
char *d;
char *buf;
BOOL flag=TRUE;
int i;
LPMW lpmw = lptw->lpmw;
OPENFILENAME ofn;
char *szTitle;
char *szFile;
char *szFilter;

	if ( (buf = LocalAllocPtr(LHND, MAXSTR+1)) == (char *)NULL )
		return;

	if (m>=lpmw->nCountMenu)
		return;
	s = lpmw->macro[m];
	d = buf;
	*d = '\0';
	while (s && *s && (d-buf < MAXSTR)) {
	    if (*s>=CMDMIN  && *s<=CMDMAX) {
		switch (*s) {
			case SAVE: /* [SAVE] - get a save filename from a file list box */
			case OPEN: /* [OPEN] - get a filename from a file list box */
				{
				BOOL save;
				char cwd[MAX_PATH];

				if ( (szTitle = LocalAllocPtr(LHND, MAXSTR+1)) == (char *)NULL )
					return;
				if ( (szFile = LocalAllocPtr(LHND, MAXSTR+1)) == (char *)NULL )
					return;
				if ( (szFilter = LocalAllocPtr(LHND, MAXSTR+1)) == (char *)NULL )
					return;

				save = (*s==SAVE);
				s++;
						for(i=0; (*s >= 32 && *s <= 126); i++)
							szTitle[i] = *s++;	/* get dialog box title */
				szTitle[i]='\0';
				s++;
						for(i=0; (*s >= 32 && *s <= 126); i++)
							szFile[i] = *s++;	/* temporary copy of filter */
				szFile[i++]='\0';
				lstrcpy(szFilter,"Default (");
				lstrcat(szFilter,szFile);
				lstrcat(szFilter,")");
				i=lstrlen(szFilter);
				i++;	/* move past NULL */
				lstrcpy(szFilter+i,szFile);
				i+=lstrlen(szFilter+i);
				i++;	/* move past NULL */
				lstrcpy(szFilter+i,"All Files (*.*)");
				i+=lstrlen(szFilter+i);
				i++;	/* move past NULL */
				lstrcpy(szFilter+i,"*.*");
				i+=lstrlen(szFilter+i);
				i++;	/* move past NULL */
				szFilter[i++]='\0';	/* add a second NULL */
				flag = 0;

				szFile[0] = '\0';
				/* clear the structrure */
				_fmemset(&ofn, 0, sizeof(OPENFILENAME));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = lptw->hWndParent;
				ofn.lpstrFilter = szFilter;
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAXSTR;
				ofn.lpstrFileTitle = szFile;
				ofn.nMaxFileTitle = MAXSTR;
				ofn.lpstrTitle = szTitle;
				/* Windows XP has it's very special meaning of 'default directory'  */
				/* (search for OPENFILENAME on MSDN). So we set it here explicitly: */
				/* ofn.lpstrInitialDir = (LPSTR)NULL; */
				_getcwd(cwd, sizeof(cwd));
				ofn.lpstrInitialDir = cwd;
				ofn.Flags = OFN_SHOWHELP | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				flag = (save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn));
				if( flag ) {
					lpmw->nChar = lstrlen(ofn.lpstrFile);
					for (i=0; i<lpmw->nChar; i++)
						*d++=ofn.lpstrFile[i];
				}

				LocalFreePtr((void NEAR *)szTitle);
				LocalFreePtr((void NEAR *)szFilter);
				LocalFreePtr((void NEAR *)szFile);

				}
				break;

			case INPUT: /* [INPUT] - input a string of characters */
				s++;
				for(i=0; (*s >= 32 && *s <= 126); i++)
					lpmw->szPrompt[i] = *s++;
				lpmw->szPrompt[i]='\0';
				flag = DialogBox( hdllInstance, "InputDlgBox", lptw->hWndParent, InputBoxDlgProc);
				if( flag ) {
					for (i=0; i<lpmw->nChar; i++)
						*d++ = lpmw->szAnswer[i];
				}
				break;

			case DIRECTORY: /* [DIRECTORY] - show standard directory dialog */
				{
#ifdef SHELL_DIR_DIALOG
					BROWSEINFO bi;
					LPITEMIDLIST pidl;
#else
					NEW_OPENFILENAME ofn;
#endif
					/* allocate some space */
					if ( (szTitle = LocalAllocPtr(LHND, MAXSTR+1)) == (char *)NULL )
						return;

					/* get dialog box title */
					s++;
					for(i=0; (*s >= 32 && *s <= 126); i++)
						szTitle[i] = *s++;
					szTitle[i] = '\0';

					flag = 0;

#ifdef SHELL_DIR_DIALOG
					/* Option 1:
							use the Shell's internal directory chooser
					*/
					/* Note: This code does not work NT 3.51 and Win32s
							 Windows 95 has shell32.dll version 4.0, but does not
					         have a version number, so this will return FALSE.
					*/
					/* Make sure that the installed shell version supports this approach */
					if (GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4,0)) {
						ZeroMemory(&bi,sizeof(bi));
						bi.hwndOwner = lptw->hWndParent;
						bi.pidlRoot = NULL;
						bi.pszDisplayName = NULL;
						bi.lpszTitle = szTitle;
						/* BIF_NEWDIALOGSTYLE is supported by Win 2000 or later (Version 5.0)*/
						bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_EDITBOX |
									 BIF_STATUSTEXT |
									 BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS;
						bi.lpfn = BrowseCallbackProc;
						bi.lParam = 0;
						bi.iImage = 0;
						pidl = SHBrowseForFolder( &bi );

						if( pidl != NULL ) {
							LPMALLOC pMalloc;
							char szPath[MAX_PATH];
							unsigned int len;

							/* Convert the item ID list's binary
							   representation into a file system path */
							SHGetPathFromIDList(pidl, szPath);

							len = strlen( szPath );
							flag = len > 0;
							if (flag)
								for (i=0; i<len; i++)
									*d++ = szPath[i];

							/* Allocate a pointer to an IMalloc interface
							   Get the address of our task allocator's IMalloc interface */
							SHGetMalloc(&pMalloc) ;

							/* Free the item ID list allocated by SHGetSpecialFolderLocation */
							IMalloc_Free( pMalloc, pidl );

							/* Free our task allocator */
							IMalloc_Release( pMalloc );
						}
					}
#else /* SHELL_DIR_DIALOG */
					/* Option 2:
							use (abuse ?) standard "file open" dialog and discard the filename
							from result, have all non-directory items removed.
					*/
					/* Note: This code does not work NT 3.51 and Win32s
							 Windows 95 has shell32.dll version 4.0, but does not
					         have a version number, so this will return FALSE.
					*/
					/* Make sure that the installed shell version supports this approach */
					if (GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4,0)) {
						/* copy current working directory to szFile */
						if ( (szFile = LocalAllocPtr(LHND, MAX_PATH+1)) == (char *)NULL )
							return;
						GP_GETCWD( szFile, MAX_PATH );
						strcat( szFile, "\\*.*" );

						ZeroMemory(&ofn,sizeof(ofn));
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = lptw->hWndParent;
						ofn.lpstrFilter = (LPSTR)NULL;
						ofn.nFilterIndex = 0;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrFileTitle = szFile;
						ofn.nMaxFileTitle = MAXSTR;
						ofn.lpstrTitle = szTitle;
						ofn.lpstrInitialDir = (LPSTR)NULL;
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOVALIDATE |
									OFN_HIDEREADONLY | OFN_ENABLESIZING |
									OFN_EXPLORER | OFN_ENABLEHOOK;
						ofn.lpfnHook = OFNHookProc;
						flag = GetOpenFileName((LPOPENFILENAME)&ofn);

						if ((flag) && (ofn.nFileOffset >0)) {
							unsigned int len;

							/* strip filename from result */
							len = ofn.nFileOffset - 1;
							ofn.lpstrFile[len] = '\0';
							for (i=0; i<len; i++)
								*d++ = ofn.lpstrFile[i];
						}
						LocalFreePtr((void NEAR *)szFile);
					}
#endif /* !SHELL_DIR_DIALOG */
					else {
						strcpy(lpmw->szPrompt, szTitle);
						flag = DialogBox( hdllInstance, "InputDlgBox", lptw->hWndParent, InputBoxDlgProc);
						if( flag ) {
							for (i=0; i<lpmw->nChar; i++)
								*d++ = lpmw->szAnswer[i];
						}
					}
					LocalFreePtr((void NEAR *)szTitle);
				}
				break;

		    case OPTIONS: { /* [OPTIONS] - open popup menu */
				POINT pt;
				RECT rect;
				int index;
				s++;
				/* align popup with toolbar button */
				index = lpmw->nButton - (lpmw->nCountMenu - m);
				if (SendMessage(lpmw->hToolbar, TB_GETITEMRECT, (WPARAM)index, (LPARAM)&rect)) {
					pt.x = rect.left;
					pt.y = rect.bottom + 1;
					ClientToScreen(lptw->hWndParent, &pt);
				} else {
					GetCursorPos(&pt);
				}
				TrackPopupMenu(lptw->hPopMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
				    pt.x, pt.y, 0, lptw->hWndParent, NULL);
				break;
			}

		    case ABOUT: /* [ABOUT] - display About box */
				s++;
				SendMessage(lptw->hWndText, WM_COMMAND, M_ABOUT, (LPARAM)0);
				break;

		    case EOS: /* [EOS] - End Of String - do nothing */
				default:
				s++;
				break;
		}
		if (!flag) { /* abort */
			d = buf;
			s = (BYTE *)"";
		}
	    }
	    else {
		*d++ = *s++;
	    }
	}
	*d = '\0';
	if (buf[0]!='\0') {
		d = buf;
		while (*d) {
			SendMessage(lptw->hWndText,WM_CHAR,*d,1L);
			d++;
		}
	}
}


static GFILE *
Gfopen(LPSTR lpszFileName, int fnOpenMode)
{
GFILE *gfile;

	gfile = (GFILE *)LocalAllocPtr(LHND, sizeof(GFILE));
	if (!gfile)
		return NULL;

	gfile->hfile = _lopen(lpszFileName, fnOpenMode);
	if (gfile->hfile == HFILE_ERROR) {
		LocalFreePtr((void NEAR *)gfile);
		return NULL;
	}
	gfile->getleft = 0;
	gfile->getnext = 0;
	return gfile;
}


static void
Gfclose(GFILE * gfile)
{

	_lclose(gfile->hfile);
	LocalFreePtr((void NEAR *)gfile);
	return;
}


/* returns number of characters read */
static int
Gfgets(LPSTR lp, int size, GFILE *gfile)
{
int i;
int ch;
	for (i=0; i<size; i++) {
		if (gfile->getleft <= 0) {
			if ( (gfile->getleft = _lread(gfile->hfile, gfile->getbuf, GBUFSIZE)) == 0)
				break;
			gfile->getnext = 0;
		}
		ch = *lp++ = gfile->getbuf[gfile->getnext++];
		gfile->getleft --;
		if (ch == '\r') {
			i--;
			lp--;
		}
		if (ch == '\n') {
			i++;
			break;
		}
	}
	if (i<size)
		*lp++ = '\0';
	return i;
}

/* Get a line from the menu file */
/* Return number of lines read from file including comment lines */
static int
GetLine(char * buffer, int len, GFILE *gfile)
{
BOOL  status;
int nLine = 0;

   status = (Gfgets(buffer,len,gfile) != 0);
   nLine++;
   while( status && ( buffer[0] == 0 || buffer[0] == '\n' || buffer[0] == ';' ) ) {
      /* blank line or comment - ignore */
   	  status = (Gfgets(buffer,len,gfile) != 0);
      nLine++;
   }
   if (lstrlen(buffer)>0)
      buffer[lstrlen(buffer)-1] = '\0';	/* remove trailing \n */

   if (!status)
      nLine = 0;	/* zero lines if file error */

    return nLine;
}

/* Left justify string */
static void
LeftJustify(char *d, char *s)
{
	while ( *s && (*s==' ' || *s=='\t') )
		s++;	/* skip over space */
	do {
		*d++ = *s;
	} while (*s++);
}

/* Translate string to tokenized macro */
static void
TranslateMacro(char *string)
{
int i, len;
LPSTR ptr;

    for( i=0; keyword[i]!=(char *)NULL; i++ ) {
        if( (ptr = _fstrstr( string, keyword[i] )) != NULL ) {
            len = lstrlen( keyword[i] );
            *ptr = keyeq[i];
            lstrcpy( ptr+1, ptr+len );
            i--;       /* allows for more than one occurrence of keyword */
            }
        }
}

/* Load Macros, and create Menu from Menu file */
void
LoadMacros(LPTW lptw)
{
GFILE *menufile;
BYTE *macroptr;
char *buf;
int nMenuLevel;
HMENU hMenu[MENUDEPTH+1];
LPMW lpmw;
int nLine = 1;
int nInc;
HGLOBAL hmacro, hmacrobuf;

int i;
RECT rect;
char *ButtonText[BUTTONMAX];
int ButtonIcon[BUTTONMAX];
TBADDBITMAP bitmap = {0};

	lpmw = lptw->lpmw;

	/* mark all buffers and menu file as unused */
	buf = (char *)NULL;
	hmacro = 0;
	hmacrobuf = 0;
	lpmw->macro = (BYTE **)NULL;
	lpmw->macrobuf = (BYTE *)NULL;
	lpmw->szPrompt = (char *)NULL;
	lpmw->szAnswer = (char *)NULL;
	menufile = (GFILE *)NULL;

	/* open menu file */
	if ((menufile=Gfopen(lpmw->szMenuName,OF_READ)) == (GFILE *)NULL)
		goto errorcleanup;

	/* allocate buffers */
	if ((buf = LocalAllocPtr(LHND, MAXSTR)) == (char *)NULL)
		goto nomemory;
	hmacro = GlobalAlloc(GHND,(NUMMENU) * sizeof(BYTE *));
	if ((lpmw->macro = (BYTE **)GlobalLock(hmacro))  == (BYTE **)NULL)
		goto nomemory;
	hmacrobuf = GlobalAlloc(GHND, MACROLEN);
	if ((lpmw->macrobuf = (BYTE *)GlobalLock(hmacrobuf)) == (BYTE *)NULL)
		goto nomemory;
	if ((lpmw->szPrompt = LocalAllocPtr(LHND, MAXSTR)) == (char *)NULL)
		goto nomemory;
	if ((lpmw->szAnswer = LocalAllocPtr(LHND, MAXSTR)) == (char *)NULL)
		goto nomemory;

	macroptr = lpmw->macrobuf;
	lpmw->nButton = 0;
	lpmw->nCountMenu = 0;
	lpmw->hMenu = hMenu[0] = CreateMenu();
	nMenuLevel = 0;

	while ((nInc = GetLine(buf,MAXSTR,menufile)) != 0) {
	  nLine += nInc;
	  LeftJustify(buf,buf);
	  if (buf[0]=='\0') {
		/* ignore blank lines */
	  }
	  else if (!lstrcmpi(buf,"[Menu]")) {
		/* new menu */
		if (!(nInc = GetLine(buf,MAXSTR,menufile))) {
			nLine += nInc;
			wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		LeftJustify(buf,buf);
		if (nMenuLevel<MENUDEPTH)
			nMenuLevel++;
		else {
			wsprintf(buf, "Menu is too deep at line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		hMenu[nMenuLevel] = CreateMenu();
		AppendMenu(hMenu[nMenuLevel > 0 ? nMenuLevel-1 : 0],
			MF_STRING | MF_POPUP, (UINT_PTR)hMenu[nMenuLevel], (LPCSTR)buf);
	  }
	  else if (!lstrcmpi(buf,"[EndMenu]")) {
		if (nMenuLevel > 0)
			nMenuLevel--;	/* back up one menu */
	  }
	  else if (!lstrcmpi(buf,"[Button]")) {
		/* button macro */
		char *icon;
		if (lpmw->nButton >= BUTTONMAX) {
			wsprintf(buf, "Too many buttons at line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		if (!(nInc = GetLine(buf,MAXSTR,menufile))) {
			nLine += nInc;
			wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		LeftJustify(buf,buf);
		if (lstrlen(buf)+1 < MACROLEN - (macroptr-lpmw->macrobuf))
			lstrcpy((char *)macroptr,buf);
		else {
			wsprintf(buf,"Out of space for storing menu macros\n at line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		ButtonText[lpmw->nButton] = (char *)macroptr;
		ButtonIcon[lpmw->nButton] = I_IMAGENONE; /* comctl 5.81, Win 2000 */
		if ((icon = strchr((char *)macroptr, ';'))) {
			*icon = NUL;
			ButtonIcon[lpmw->nButton] = atoi(++icon);
		}
		macroptr += lstrlen((char *)macroptr)+1;
		*macroptr = '\0';
		if (!(nInc = GetLine(buf,MAXSTR,menufile))) {
			nLine += nInc;
			wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent,(LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		LeftJustify(buf,buf);
		TranslateMacro(buf);
		if (lstrlen(buf)+1 < MACROLEN - (macroptr - lpmw->macrobuf))
			lstrcpy((char *)macroptr,buf);
		else {
			wsprintf(buf,"Out of space for storing menu macros\n at line %d of %s \n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		lpmw->hButtonID[lpmw->nButton] = lpmw->nCountMenu;
		lpmw->macro[lpmw->nCountMenu] = macroptr;
		macroptr += lstrlen((char *)macroptr)+1;
		*macroptr = '\0';
		lpmw->nCountMenu++;
		lpmw->nButton++;
	  }
	  else {
		/* menu item */
		if (lpmw->nCountMenu>=NUMMENU) {
			wsprintf(buf, "Too many menu items at line %d of %s\n", nLine, lpmw->szMenuName);
			MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
			goto errorcleanup;
		}
		LeftJustify(buf,buf);
/* HBB 981202: added MF_SEPARATOR to the MF_MENU*BREAK items. This  is meant
 * to maybe avoid a CodeGuard warning about passing last argument zero
 * when item style is not SEPARATOR... Actually, a better solution would
 * have been to combine the '|' divider with the next menu item. */
		if (buf[0]=='-') {
		    if (nMenuLevel == 0)
				AppendMenu(hMenu[0], MF_SEPARATOR | MF_MENUBREAK, 0, (LPSTR)NULL);
		    else
			AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR, 0, (LPSTR)NULL);
		}
		else if (buf[0]=='|') {
			AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR | MF_MENUBARBREAK, 0, (LPSTR)NULL);
		}
		else {
			AppendMenu(hMenu[nMenuLevel],MF_STRING, lpmw->nCountMenu, (LPSTR)buf);
			if (!(nInc = GetLine(buf,MAXSTR,menufile))) {
				nLine += nInc;
				wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			LeftJustify(buf,buf);
			TranslateMacro(buf);
			if (lstrlen(buf)+1 < MACROLEN - (macroptr - lpmw->macrobuf))
				lstrcpy((char *)macroptr,buf);
			else {
				wsprintf(buf, "Out of space for storing menu macros\n at line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR) buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			lpmw->macro[lpmw->nCountMenu] = macroptr;
			macroptr += lstrlen((char *)macroptr)+1;
			*macroptr = '\0';
			lpmw->nCountMenu++;
		}
	  }
	}

	if ( (lpmw->nCountMenu - lpmw->nButton) > 0 ) {
		/* we have a menu bar so put it on the window */
		SetMenu(lptw->hWndParent,lpmw->hMenu);
		DrawMenuBar(lptw->hWndParent);
	}

	if (!lpmw->nButton)
		goto cleanup;		/* no buttons */

	/* create a toolbar */
	lpmw->hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
		WS_CHILD | TBSTYLE_LIST, // TBSTYLE_WRAPABLE
		0, 0, 0, 0,
		lptw->hWndParent, (HMENU)ID_TOOLBAR, lptw->hInstance, NULL);
	if (lpmw->hToolbar == NULL)
	    goto cleanup;

	SendMessage(lpmw->hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

	/* set size of toolbar icons */
	/* lparam is (height<<16 + width) / default 16,15 */
	SendMessage(lpmw->hToolbar, TB_SETBITMAPSIZE, (WPARAM)0, MAKELPARAM(16, 16));
	/* load standard toolbar icons: standard, history & view */

	bitmap.hInst = HINST_COMMCTRL;
	bitmap.nID = IDB_STD_SMALL_COLOR;
	SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);
	bitmap.nID = IDB_HIST_SMALL_COLOR;
	SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);
	bitmap.nID = IDB_VIEW_SMALL_COLOR;
	SendMessage(lpmw->hToolbar, TB_ADDBITMAP, 0, (WPARAM)&bitmap);

	/* create buttons */
	for (i = 0; i < lpmw->nButton; i++) {
		TBBUTTON button;
		ZeroMemory(&button, sizeof(button));
		button.iBitmap = ButtonIcon[i];
		button.idCommand = lpmw->hButtonID[i];
		button.fsState = TBSTATE_ENABLED;
		button.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_NOPREFIX;
		if (MacroCommand(lptw, lpmw->hButtonID[i]) == OPTIONS)
			button.fsStyle |= BTNS_WHOLEDROPDOWN;
		button.iString = (UINT_PTR)ButtonText[i];
		SendMessage(lpmw->hToolbar, TB_INSERTBUTTON, (WPARAM)i + 1, (LPARAM)&button);
	}

	/* auto-resize and show */
	SendMessage(lpmw->hToolbar, TB_AUTOSIZE, (WPARAM)0, (LPARAM)0);
	ShowWindow(lpmw->hToolbar, TRUE);

	/* move top of client text window down to allow space for toolbar */
	GetClientRect(lpmw->hToolbar, &rect);
	lptw->ButtonHeight = rect.bottom + 1;
	lptw->ButtonHeight++;
	GetClientRect(lptw->hWndParent, &rect);
	SetWindowPos(lptw->hWndText, (HWND)NULL, 0, lptw->ButtonHeight,
			rect.right, rect.bottom - lptw->ButtonHeight - lptw->StatusHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
	goto cleanup;

nomemory:
	MessageBox(lptw->hWndParent, "Out of memory", lptw->Title, MB_ICONEXCLAMATION);
errorcleanup:
	if (hmacro) {
		GlobalUnlock(hmacro);
		GlobalFree(hmacro);
	}
	if (hmacrobuf) {
		GlobalUnlock(hmacrobuf);
		GlobalFree(hmacrobuf);
	}
	if (lpmw->szPrompt != (char *)NULL)
		LocalFreePtr((void NEAR *)lpmw->szPrompt);
	if (lpmw->szAnswer != (char *)NULL)
		LocalFreePtr((void NEAR *)lpmw->szAnswer);

cleanup:
	if (buf != (char *)NULL)
		LocalFreePtr((void NEAR *)buf);
	if (menufile != (GFILE *)NULL)
		Gfclose(menufile);
	return;
}


void
CloseMacros(LPTW lptw)
{
HGLOBAL hglobal;
LPMW lpmw;

	lpmw = lptw->lpmw;

	hglobal = (HGLOBAL)GlobalHandle(lpmw->macro);
	if (hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	hglobal = (HGLOBAL)GlobalHandle(lpmw->macrobuf);
	if (hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	if (lpmw->szPrompt != (char *)NULL)
		LocalFreePtr(lpmw->szPrompt);
	if (lpmw->szAnswer != (char *)NULL)
		LocalFreePtr(lpmw->szAnswer);
}


/***********************************************************************/
/* InputBoxDlgProc() -  Message handling routine for Input dialog box         */
/***********************************************************************/

INT_PTR CALLBACK
InputBoxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPTW lptw;
	LPMW lpmw;

    lptw = (LPTW)GetWindowLongPtr(GetParent(hDlg), 0);
    lpmw = lptw->lpmw;

    switch( message) {
        case WM_INITDIALOG:
            SetDlgItemText( hDlg, ID_PROMPT, lpmw->szPrompt);
            return( TRUE);

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case ID_ANSWER:
                    return( TRUE);

                case IDOK:
                    lpmw->nChar = GetDlgItemText( hDlg, ID_ANSWER, lpmw->szAnswer, MAXSTR);
                    EndDialog( hDlg, TRUE);
                    return( TRUE);

                case IDCANCEL:
                    lpmw->szAnswer[0] = 0;
                    EndDialog( hDlg, FALSE);
                    return( TRUE);

                default:
                    return( FALSE);
                }
        default:
            return( FALSE);
        }
    }
