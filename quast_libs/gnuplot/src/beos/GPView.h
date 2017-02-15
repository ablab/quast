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

#ifndef GPView_h
#define GPView_h

#include <SupportDefs.h>
#include <GraphicsDefs.h>
#include <List.h>
#include <Bitmap.h>
#include <Rect.h>


class GPView : public BView
{
	protected:

		GPBitmap 		*	m_bitmap;
		float				m_scale;

	public:
							GPView(BRect rect, ulong resizeMode,
								ulong flags, GPBitmap *bitmap);
							~GPView();

	GPBitmap *		Document() { return m_bitmap; };

				void		SetScale(float scale);
				float		Scale();

				void		GetMaxSize(float *width, float *height);

		virtual void		MouseDown(BPoint point);
		virtual void		MouseUp(BPoint point);
		virtual	void		Draw(BRect updateRect);
		virtual	void		AttachedToWindow();
		virtual	void		MessageReceived(BMessage *msg);
		virtual	void		FrameResized(float width, float height);
				void		FixupScrollbars();
};

#endif
