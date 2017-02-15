/*
 * $Id: wpause.c,v 1.27.2.3 2016/09/12 15:17:43 markisch Exp $
 */

/* GNUPLOT - win/wpause.c */
/*[
 * Copyright 1992, 1993, 1998, 2004   Russell Lang
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
 *   Russell Lang
 */

/* PauseBox() */

/* MessageBox ALWAYS appears in the middle of the screen so instead */
/* we use this PauseBox so we can decide where it is to be placed */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"
#include "winmain.h"

/* for paused_for_mouse */
#include "command.h"
/* for ctrlc_flag */
#include "plot.h"

#ifdef WXWIDGETS
#include "wxterminal/wxt_term.h"
#endif
#ifdef QTTERM
#include "qtterminal/qt_term.h"
#endif
#ifdef HAVE_LIBCACA
# define TERM_PUBLIC_PROTO
# include "caca.trm"
# undef TERM_PUBLIC_PROTO
#endif


/* Pause Window */
static void CreatePauseClass(LPPW lppw);
LRESULT CALLBACK WndPauseProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PauseButtonProc(HWND, UINT, WPARAM, LPARAM);


/* Non-blocking Sleep function, called by pause_command.
   This allows redrawing and (some) user interaction.
*/
void
win_sleep(DWORD dwMilliSeconds)
{
	DWORD t0, t1, tstop, rc;

	t0 = GetTickCount();
	tstop  = t0 + dwMilliSeconds;
	t1 = dwMilliSeconds; /* remaining time to wait */
	do {
#ifdef HAVE_LIBCACA
		HANDLE h;
#endif

		if (term->waitforinput != NULL)
			term->waitforinput(TERM_ONLY_CHECK_MOUSING);

#ifndef HAVE_LIBCACA
		rc = MsgWaitForMultipleObjects(0, NULL, FALSE, t1, QS_ALLINPUT);
		if (rc != WAIT_TIMEOUT) {
#else
		h = GetStdHandle(STD_INPUT_HANDLE);
		if (h != NULL)
			rc = MsgWaitForMultipleObjects(1, &h, FALSE, t1, QS_ALLINPUT);
		else
			rc = MsgWaitForMultipleObjects(0, NULL, FALSE, t1, QS_ALLINPUT);
		if (rc != WAIT_TIMEOUT) {

			if (strcmp(term->name, "caca") == 0)
				CACA_process_events();
#endif
			WinMessageLoop();

			/* calculate remaining time, detect overflow */
			t1 = GetTickCount();
			if (tstop > t0) {
				if ((t1 >= tstop) || (t1 < t0))
					rc = WAIT_TIMEOUT;
			} else {
				if ((t1 >= tstop) && (t1 < t0))
					rc = WAIT_TIMEOUT;
			}
			t1 = tstop - t1; /* remaining time to wait */
		}
	} while (rc != WAIT_TIMEOUT);
}


/* Create Pause Class */
/* called from PauseBox the first time a pause window is created */
static void
CreatePauseClass(LPPW lppw)
{
	WNDCLASS wndclass;

	wndclass.style = 0;
	wndclass.lpfnWndProc = (WNDPROC)WndPauseProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = sizeof(void *);
	wndclass.hInstance = lppw->hInstance;
	wndclass.hIcon = NULL;
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szPauseClass;
	RegisterClass(&wndclass);
}


TBOOLEAN
MousableWindowOpened(void)
{
	TBOOLEAN result = FALSE;

#ifdef USE_MOUSE
	/* only pause-for-mouse when a window is open */
	/* FIXME: we might want to have a terminal entry for that */
	if (term != NULL) {
		if ((strcmp(term->name, "windows") == 0) && GraphHasWindow(graphwin))
			result = TRUE;
#ifdef WXWIDGETS
		/* FIXME: this does not test if the current window is open */
		else if ((strcmp(term->name, "wxt") == 0) && wxt_active_window_opened())
			result = TRUE;
#endif
#ifdef HAVE_LIBCACA
		else if ((strcmp(term->name, "caca") == 0) && CACA_window_opened())
			result = TRUE;
#endif
#ifdef QTTERM
# if 0 /* FIXME: qt_window_opened() not yet implemented */
		if ((strcmp(term->name, "qt") == 0) && !qt_active_window_opened())
#else
		if ((strcmp(term->name, "qt") == 0))
# endif
			result = TRUE;
#endif
	}
#endif

	return result;
}


/* PauseBox */
int WDPROC
PauseBox(LPPW lppw)
{
	HDC hdc;
	int width, height;
	TEXTMETRIC tm;
	RECT rect;
	SIZE size;
	char *current_pause_title = lppw->Title;

#ifdef USE_MOUSE
	/* Do not try to wait for mouse events when there's no graph window open. */
	if (paused_for_mouse && !MousableWindowOpened())
		paused_for_mouse = 0;

	if (!paused_for_mouse)
#endif
	{
	if (!lppw->hPrevInstance)
		CreatePauseClass(lppw);
	GetWindowRect(GetDesktopWindow(), &rect);
	if ((lppw->Origin.x == CW_USEDEFAULT) || (lppw->Origin.x == 0))
		lppw->Origin.x = (rect.right + rect.left) / 2;
	if ((lppw->Origin.y == CW_USEDEFAULT) || (lppw->Origin.y == 0))
		lppw->Origin.y = (rect.bottom + rect.top) / 2;

	hdc = GetDC(NULL);
	SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
	/* determine actual text size */
	GetTextExtentPoint32(hdc, lppw->Message, strlen(lppw->Message), &size);
	GetTextMetrics(hdc, &tm);
	width = max(28 * tm.tmAveCharWidth, size.cx + 6 * tm.tmAveCharWidth);
	width = min(width, rect.right - rect.left);
	height = 8 * size.cy;
	ReleaseDC(NULL,hdc);

	lppw->hWndPause = CreateWindowEx(
		WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW,
		szPauseClass, current_pause_title,
		/* HBB 981202: WS_POPUPWINDOW would have WS_SYSMENU in it, but we don't
		 * want, nor need, a System menu in our Pause windows. */
		WS_POPUP | WS_BORDER | WS_CAPTION,
		lppw->Origin.x - width/2, lppw->Origin.y - height/2,
		width, height,
		lppw->hWndParent, NULL, lppw->hInstance, lppw);
	    ShowWindow(lppw->hWndPause, SW_SHOWNORMAL);
	    BringWindowToTop(lppw->hWndPause);
	    UpdateWindow(lppw->hWndPause);

	lppw->bPause = TRUE;
	lppw->bPauseCancel = IDCANCEL;

		while (lppw->bPause && !ctrlc_flag) {
		if (term->waitforinput == NULL) {
			/* Only handle message queue events */ 
			WinMessageLoop();
				if (lppw->bPause && !ctrlc_flag)
				WaitMessage();
		} else {
			/* Call the non-blocking sleep function,
			   which also handles console input (caca terminal)
			   and mousing of the current terminal (e.g. qt) */
			win_sleep(50);
		}
	}

	DestroyWindow(lppw->hWndPause);
	return lppw->bPauseCancel;
}
#ifdef USE_MOUSE
	else {
		/* Don't show the pause "OK CANCEL" dialog for "pause mouse ..."
		   Note: maybe gnuplot should display a message like
		     "gnuplot pausing (waiting for mouse click)"
		   in the window status or title bar or somewhere else. 
		*/

		while (paused_for_mouse && !ctrlc_flag) {
			if (term->waitforinput == NULL) {
				/* Only handle message queue events */ 
				WinMessageLoop();
				if (paused_for_mouse && !ctrlc_flag)
					WaitMessage();
			} else {
				/* Call the non-blocking sleep function,
				   which also handles console input (caca terminal)
				   and mousing of the current terminal (e.g. qt) */
				win_sleep(50);
			}
		}
		return !ctrlc_flag;
	}
#endif
}


LRESULT CALLBACK
WndPauseProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;
	TEXTMETRIC tm;
	LPPW lppw;
	int cxChar, cyChar, middle;
	HFONT hfont;

	lppw = (LPPW)GetWindowLongPtr(hwnd, 0);

	switch(message) {
		case WM_KEYDOWN:
			if (wParam == VK_RETURN)
				SendMessage(hwnd, WM_COMMAND, lppw->bDefOK ? IDOK : IDCANCEL, 0L);
			else if (wParam == VK_ESCAPE)
				SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0L);
			return 0;
		case WM_COMMAND:
			if ((LOWORD(wParam) == IDCANCEL) || (LOWORD(wParam) == IDOK)) {
				lppw->bPauseCancel = LOWORD(wParam);
				lppw->bPause = FALSE;
				break;
			}
			return 0;
		case WM_SETFOCUS:
			SetFocus(lppw->bDefOK ? lppw->hOK : lppw->hCancel);
			return 0;
		case WM_PAINT: {
			hdc = BeginPaint(hwnd, &ps);
			SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
			SetTextAlign(hdc, TA_CENTER);
			GetClientRect(hwnd, &rect);
			SetBkMode(hdc,TRANSPARENT);
			TextOut(hdc, (rect.right + rect.left) / 2, (rect.bottom + rect.top) / 6,
				lppw->Message, strlen(lppw->Message));
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_CREATE: {
			int ws_opts = WS_CHILD | WS_TABSTOP;

#ifdef USE_MOUSE
			if (!paused_for_mouse) /* don't show buttons during pausing for mouse or key */
				ws_opts |= WS_VISIBLE;
#endif
			lppw = (LPPW) ((CREATESTRUCT *)lParam)->lpCreateParams;
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)lppw);
			lppw->hWndPause = hwnd;
			hdc = GetDC(hwnd);
			hfont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
			SelectObject(hdc, hfont);
			GetTextMetrics(hdc, &tm);
			cxChar = tm.tmAveCharWidth;
			cyChar = tm.tmHeight + tm.tmExternalLeading;
			ReleaseDC(hwnd, hdc);
			middle = ((LPCREATESTRUCT) lParam)->cx / 2;
			lppw->hOK = CreateWindow((LPSTR)"button", (LPSTR)"OK",
					ws_opts | BS_DEFPUSHBUTTON,
					middle - 13 * cxChar, 3 * cyChar,
					10 * cxChar, 7 * cyChar / 4,
					hwnd, (HMENU)IDOK,
					((LPCREATESTRUCT) lParam)->hInstance, NULL);
			lppw->bDefOK = TRUE;
			lppw->hCancel = CreateWindow((LPSTR)"button", (LPSTR)"Cancel",
					ws_opts | BS_PUSHBUTTON,
					middle - 1 * cxChar, 3 * cyChar,
					10 * cxChar, 7 * cyChar / 4,
					hwnd, (HMENU)IDCANCEL,
					((LPCREATESTRUCT) lParam)->hInstance, NULL);
			lppw->lpfnOK = (WNDPROC) GetWindowLongPtr(lppw->hOK, GWLP_WNDPROC);
			SetWindowLongPtr(lppw->hOK, GWLP_WNDPROC, (LONG_PTR)PauseButtonProc);
			lppw->lpfnCancel = (WNDPROC) GetWindowLongPtr(lppw->hCancel, GWLP_WNDPROC);
			SetWindowLongPtr(lppw->hCancel, GWLP_WNDPROC, (LONG_PTR)PauseButtonProc);
			SendMessage(lppw->hOK, WM_SETFONT, (WPARAM)hfont, 0);
			SendMessage(lppw->hCancel, WM_SETFONT, (WPARAM)hfont, 0);
			if (GetParent(hwnd))
				EnableWindow(GetParent(hwnd), FALSE);
			return 0;
		}
		case WM_DESTROY:
			GetWindowRect(hwnd, &rect);
			lppw->Origin.x = (rect.right + rect.left) / 2;
			lppw->Origin.y = (rect.bottom + rect.top) / 2;
			lppw->bPause = FALSE;
			if (GetParent(hwnd))
				EnableWindow(GetParent(hwnd), TRUE);
			break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}


LRESULT CALLBACK
PauseButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPPW lppw;
	LONG n = GetWindowLong(hwnd, GWL_ID);
	lppw = (LPPW)GetWindowLongPtr(GetParent(hwnd), 0);
	switch (message) {
		case WM_KEYDOWN:
			switch (wParam) {
				case VK_TAB:
				case VK_BACK:
				case VK_LEFT:
				case VK_RIGHT:
				case VK_UP:
				case VK_DOWN:
					lppw->bDefOK = !(n == IDOK);
					if (lppw->bDefOK) {
						SendMessage(lppw->hOK,     BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, (LPARAM)TRUE);
						SendMessage(lppw->hCancel, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, (LPARAM)TRUE);
						SetFocus(lppw->hOK);
					} else {
						SendMessage(lppw->hOK,     BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, (LPARAM)TRUE);
						SendMessage(lppw->hCancel, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, (LPARAM)TRUE);
						SetFocus(lppw->hCancel);
					}
					break;
				default:
					SendMessage(GetParent(hwnd), message, wParam, lParam);
			}
			break;
	}
	return CallWindowProc(((n == IDOK) ? lppw->lpfnOK : lppw->lpfnCancel),
		hwnd, message, wParam, lParam);
}
