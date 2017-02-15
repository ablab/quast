/*
 * $Id: wprinter.c,v 1.12 2014/03/30 18:33:21 markisch Exp $
 */

/* GNUPLOT - win/wprinter.c */
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
 */

/* Dump a file to the printer */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef __MSC__
# include <mem.h>
#endif
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"

GP_LPPRINT prlist = NULL;

static GP_LPPRINT PrintFind(HDC hdc);

INT_PTR CALLBACK
PrintSizeDlgProc(HWND hdlg, UINT wmsg, WPARAM wparam, LPARAM lparam)
{
    char buf[8];
    GP_LPPRINT lpr = (GP_LPPRINT)GetWindowLongPtr(GetParent(hdlg), 4);

    switch (wmsg) {
    case WM_INITDIALOG:
	wsprintf(buf,"%d",lpr->pdef.x);
	SetDlgItemText(hdlg, PSIZE_DEFX, buf);
	wsprintf(buf,"%d",lpr->pdef.y);
	SetDlgItemText(hdlg, PSIZE_DEFY, buf);
	wsprintf(buf,"%d",lpr->poff.x);
	SetDlgItemText(hdlg, PSIZE_OFFX, buf);
	wsprintf(buf,"%d",lpr->poff.y);
	SetDlgItemText(hdlg, PSIZE_OFFY, buf);
	wsprintf(buf,"%d",lpr->psize.x);
	SetDlgItemText(hdlg, PSIZE_X, buf);
	wsprintf(buf,"%d",lpr->psize.y);
	SetDlgItemText(hdlg, PSIZE_Y, buf);
	CheckDlgButton(hdlg, PSIZE_DEF, TRUE);
	EnableWindow(GetDlgItem(hdlg, PSIZE_X), FALSE);
	EnableWindow(GetDlgItem(hdlg, PSIZE_Y), FALSE);
	return TRUE;
    case WM_COMMAND:
	switch (wparam) {
	case PSIZE_DEF:
	    EnableWindow(GetDlgItem(hdlg, PSIZE_X), FALSE);
	    EnableWindow(GetDlgItem(hdlg, PSIZE_Y), FALSE);
	    return FALSE;
	case PSIZE_OTHER:
	    EnableWindow(GetDlgItem(hdlg, PSIZE_X), TRUE);
	    EnableWindow(GetDlgItem(hdlg, PSIZE_Y), TRUE);
	    return FALSE;
	case IDOK:
	    if (SendDlgItemMessage(hdlg, PSIZE_OTHER, BM_GETCHECK, 0, 0L)) {
		SendDlgItemMessage(hdlg, PSIZE_X, WM_GETTEXT, 7,
				   (LPARAM) (LPSTR) buf);
		GetInt(buf, (LPINT)&lpr->psize.x);
		SendDlgItemMessage(hdlg, PSIZE_Y, WM_GETTEXT, 7,
				   (LPARAM) (LPSTR) buf);
		GetInt(buf, (LPINT)&lpr->psize.y);
	    } else {
		lpr->psize.x = lpr->pdef.x;
		lpr->psize.y = lpr->pdef.y;
	    }
	    SendDlgItemMessage(hdlg, PSIZE_OFFX, WM_GETTEXT, 7,
			       (LPARAM) (LPSTR) buf);
	    GetInt(buf, (LPINT)&lpr->poff.x);
	    SendDlgItemMessage(hdlg, PSIZE_OFFY, WM_GETTEXT, 7,
			       (LPARAM) (LPSTR) buf);
	    GetInt(buf, (LPINT)&lpr->poff.y);

	    if (lpr->psize.x <= 0)
		lpr->psize.x = lpr->pdef.x;
	    if (lpr->psize.y <= 0)
		lpr->psize.y = lpr->pdef.y;

	    EndDialog(hdlg, IDOK);
	    return TRUE;
	case IDCANCEL:
	    EndDialog(hdlg, IDCANCEL);
	    return TRUE;
	} /* switch(wparam) */
	break;
    } /* switch(msg) */
    return FALSE;
}



/* GetWindowLong(hwnd, 4) must be available for use */
BOOL
PrintSize(HDC printer, HWND hwnd, LPRECT lprect)
{
    HDC hdc;
    BOOL status = FALSE;
    GP_PRINT pr;

    SetWindowLongPtr(hwnd, 4, (LONG_PTR)&pr);
    pr.poff.x = 0;
    pr.poff.y = 0;
    pr.psize.x = GetDeviceCaps(printer, HORZSIZE);
    pr.psize.y = GetDeviceCaps(printer, VERTSIZE);
    hdc = GetDC(hwnd);
    GetClientRect(hwnd,lprect);
    pr.pdef.x = MulDiv(lprect->right-lprect->left, 254, 10*GetDeviceCaps(hdc, LOGPIXELSX));
    pr.pdef.y = MulDiv(lprect->bottom-lprect->top, 254, 10*GetDeviceCaps(hdc, LOGPIXELSX));
    ReleaseDC(hwnd,hdc);

    if (DialogBox (hdllInstance, "PrintSizeDlgBox", hwnd, PrintSizeDlgProc)
	== IDOK)
	{
	    lprect->left = MulDiv(pr.poff.x*10, GetDeviceCaps(printer, LOGPIXELSX), 254);
	    lprect->top = MulDiv(pr.poff.y*10, GetDeviceCaps(printer, LOGPIXELSY), 254);
	    lprect->right = lprect->left + MulDiv(pr.psize.x*10, GetDeviceCaps(printer, LOGPIXELSX), 254);
	    lprect->bottom = lprect->top + MulDiv(pr.psize.y*10, GetDeviceCaps(printer, LOGPIXELSY), 254);
	    status = TRUE;
	}
    SetWindowLong(hwnd, 4, (LONG)(0L));

    return status;
}

/* Win32 doesn't support OpenJob() etc. so we must use some old code
 * which attempts to sneak the output through a Windows printer driver */
void
PrintRegister(GP_LPPRINT lpr)
{
    GP_LPPRINT next;
    next = prlist;
    prlist = lpr;
    lpr->next = next;
}


static GP_LPPRINT
PrintFind(HDC hdc)
{
    GP_LPPRINT this;
    this = prlist;
    while (this && (this->hdcPrn!=hdc)) {
	this = this->next;
    }
    return this;
}

void
PrintUnregister(GP_LPPRINT lpr)
{
    GP_LPPRINT this, prev;
    prev = (GP_LPPRINT)NULL;
    this = prlist;
    while (this && (this!=lpr)) {
	prev = this;
	this = this->next;
    }
    if (this && (this == lpr)) {
	/* unhook it */
	if (prev)
	    prev->next = this->next;
	else
	    prlist = this->next;
    }
}

/* GetWindowLong(GetParent(hDlg), 4) must be available for use */
INT_PTR CALLBACK
PrintDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	GP_LPPRINT lpr;
	lpr = (GP_LPPRINT) GetWindowLongPtr(GetParent(hDlg), 4);
	/* FIXME: cause of crash in bug #3544949. No idea yet as to why this could happen, though. */
	if (lpr == NULL)
		return FALSE;

	switch (message) {
	case WM_INITDIALOG:
		lpr->hDlgPrint = hDlg;
		SetWindowText(hDlg, (LPSTR)lParam);
		EnableMenuItem(GetSystemMenu(hDlg, FALSE), SC_CLOSE, MF_GRAYED);
		return TRUE;
	case WM_COMMAND:
		lpr->bUserAbort = TRUE;
		lpr->hDlgPrint = 0;
		EnableWindow(GetParent(hDlg), TRUE);
		EndDialog(hDlg, FALSE);
		return TRUE;
	}
	return FALSE;
}


BOOL CALLBACK
PrintAbortProc(HDC hdcPrn, int code)
{
    MSG msg;
    GP_LPPRINT lpr;
    lpr = PrintFind(hdcPrn);

    while (!lpr->bUserAbort && PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		if (!lpr->hDlgPrint || !IsDialogMessage(lpr->hDlgPrint,&msg)) {
        	TranslateMessage(&msg);
        	DispatchMessage(&msg);
		}
    }
    return(!lpr->bUserAbort);
}


/* GetWindowLong(hwnd, 4) must be available for use */
void WDPROC
DumpPrinter(HWND hwnd, LPSTR szAppName, LPSTR szFileName)
{
	HDC printer;
	PRINTDLG pd;
	/* FIXME: share these with CopyPrint */
	static DEVNAMES * pDevNames = NULL;
	static DEVMODE * pDevMode = NULL;
	LPCTSTR szDriver, szDevice, szOutput;
	GP_PRINT pr;
	DOCINFO di;
	char *buf;
	WORD *bufcount;
	int count;
	FILE *f;
	long lsize;
	long ldone;
	char pcdone[10];

	if ((f = fopen(szFileName, "rb")) == NULL)
		return;
	fseek(f, 0L, SEEK_END);
	lsize = ftell(f);
	if (lsize <= 0)
		lsize = 1;
	fseek(f, 0L, SEEK_SET);
	ldone = 0;

	/* Print Setup Dialog */

	/* See http://support.microsoft.com/kb/240082 */
	memset(&pd, 0, sizeof(pd));
	pd.lStructSize = sizeof(pd);
	pd.hwndOwner = hwnd;
	pd.Flags = PD_PRINTSETUP;
	pd.hDevNames = pDevNames;
	pd.hDevMode = pDevMode;

	if (PrintDlg(&pd)) {
		pDevNames = (DEVNAMES *) GlobalLock(pd.hDevNames);
		pDevMode = (DEVMODE *) GlobalLock(pd.hDevMode);

		szDriver = (LPCTSTR)pDevNames + pDevNames->wDriverOffset;
		szDevice = (LPCTSTR)pDevNames + pDevNames->wDeviceOffset;
		szOutput = (LPCTSTR)pDevNames + pDevNames->wOutputOffset;

		printer = CreateDC(szDriver, szDevice, szOutput, pDevMode);

		GlobalUnlock(pd.hDevMode);
		GlobalUnlock(pd.hDevNames);

		/* We no longer free these structures, but preserve them for the next time
		GlobalFree(pd.hDevMode);
		GlobalFree(pd.hDevNames);
		*/

		if (printer == NULL)
			return;	/* abort */

		pr.hdcPrn = printer;
		SetWindowLongPtr(hwnd, 4, (LONG_PTR)((GP_LPPRINT)&pr));
		PrintRegister((GP_LPPRINT)&pr);
		if ((buf = malloc(4096 + 2)) != NULL) {
			bufcount = (WORD *)buf;
			EnableWindow(hwnd,FALSE);
			pr.bUserAbort = FALSE;
			pr.hDlgPrint = CreateDialogParam(hdllInstance, "CancelDlgBox",
							 hwnd, PrintDlgProc, (LPARAM)szAppName);
			SetAbortProc(printer, PrintAbortProc);

			memset(&di, 0, sizeof(DOCINFO));
			di.cbSize = sizeof(DOCINFO);
			di.lpszDocName = szAppName;
			if (StartDoc(printer, &di) > 0) {
				while (pr.hDlgPrint && !pr.bUserAbort &&
					   (count = fread(buf + 2, 1, 4096, f)) != 0 ) {
					int ret;
					*bufcount = count;
					ret = Escape(printer, PASSTHROUGH, count + 2, (LPSTR)buf, NULL);
					ldone += count;
					if (ret != SP_ERROR) {
						sprintf(pcdone, "%d%% done", (int)(ldone * 100 / lsize));
						SetWindowText(GetDlgItem(pr.hDlgPrint, CANCEL_PCDONE), pcdone);
					} else {
						SetWindowText(GetDlgItem(pr.hDlgPrint, CANCEL_PCDONE), "Passthrough Error!");
					}
					if (pr.bUserAbort)
						AbortDoc(printer);
					else
						EndDoc(printer);
				}
				if (!pr.bUserAbort) {
					EnableWindow(hwnd, TRUE);
					DestroyWindow(pr.hDlgPrint);
				}
				free(buf);
			}
		}
		DeleteDC(printer);
		SetWindowLong(hwnd, 4, 0L);
		PrintUnregister((GP_LPPRINT)&pr);
	}

	fclose(f);
}
