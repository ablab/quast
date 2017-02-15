/*[
 * Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GPBitmap.h"
#include "GPView.h"
#include "constants.h"
#include <Region.h>
#include <ScrollBar.h>


/*******************************************************************/
// GPView

GPView::GPView(BRect rect, ulong resizeMode,
	ulong flags, GPBitmap *bitmap)
	: BView(rect,"BitmapEditor",resizeMode,flags|B_SUBPIXEL_PRECISE|B_FRAME_EVENTS)
{
	m_bitmap = (bitmap) ? bitmap : new GPBitmap(rect.Width(), rect.Height());
	SetScale(1);
};

void GPView::GetMaxSize(float *width, float *height)
{
	BRect r = m_bitmap->Bounds();
	*width = floor((r.right+1)*m_scale - 0.5);
	*height = floor((r.bottom+1)*m_scale - 0.5);
};

void GPView::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case bmsgBitmapDirty:
		{
//			m_bitmap->Lock();
//			m_bitmap->display();
//			m_bitmap->Unlock();
//			printf("view (dirty) displaying %d commands\n",m_bitmap->ncommands);
			Draw(Bounds());
			break;
		};
		case bmsgBitmapResize:
		{
			BRect r;
			msg->FindRect("rect",&r);
			m_bitmap->ResizeTo(r.Width(),r.Height(),0);
			break;
		};
		case bmsgNewCmd:
		{
			char *cmd = NULL;
			int32 i, num;
			num = msg->FindInt32("numcmds");
			m_bitmap->addCommands(msg,num);
//			printf("view (new cmds) displaying %d commands\n",m_bitmap->ncommands);
			Draw(Bounds());
			break;
		}
		case bmsgClrCmd:
		{
			m_bitmap->clearCommands();
			break;
		}
		default:
			BView::MessageReceived(msg);
	};
};

GPView::~GPView()
{
};

void GPView::SetScale(float scale)
{
	m_scale = scale;
//	FixupScrollbars();
	Invalidate();
};

float GPView::Scale()
{
	return m_scale;
};

void GPView::FrameResized(float width, float height)
{
	uint32 buttons;
	BPoint cursor;
	printf("resising\n");
//	ResizeTo(width, height);
//	GetMouse(&cursor, &buttons);
	m_bitmap->Lock();
	m_bitmap->ResizeTo(width, height,1);
	BBitmap *b = m_bitmap->RealBitmap();
	DrawBitmap(b,b->Bounds(),Bounds());
	Sync();
	m_bitmap->Unlock();
//	Draw(Bounds());
//	FixupScrollbars();
};

void GPView::AttachedToWindow()
{
//	FixupScrollbars();
	SetViewColor(B_TRANSPARENT_32_BIT);
};

void GPView::MouseDown(BPoint point)
{
	printf("Mouse Down\n");
};

void GPView::MouseUp(BPoint point)
{
	printf("Mouse Up\n");
	Draw(Bounds());
};

void GPView::Draw(BRect updateRect)
{
	m_bitmap->Lock();
	m_bitmap->display(Bounds().Width(),Bounds().Height());
	BBitmap *b = m_bitmap->RealBitmap();
	DrawBitmap(b,b->Bounds(),Bounds());
	Sync();
	m_bitmap->Unlock();
};

void GPView::FixupScrollbars()
{
};

