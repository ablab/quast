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

#ifndef GPBitmap_h
#define GPBitmap_h

#include <SupportDefs.h>
#include <GraphicsDefs.h>
#include <List.h>
#include <Bitmap.h>
#include <Rect.h>

#define bmsgZoomIn		'zmin'
#define bmsgZoomOut		'zmot'
#define bmsgSetScale	'zset'

class GPBitmap {

	protected:

		BView *		m_view;
		BList 		m_editors;
		BBitmap *	m_bitmap;
		BBitmap *	m_bufbitmap;
		sem_id		m_readerLock;
		rgb_color	colors[16];
		bool		m_needRedraw;
		bool		m_redrawing;
		thread_id	drawing_thread;

	public:
		float 		width, height, nwidth, nheight, pointsize, xscale, yscale;
		int32		ncommands, max_commands;
		int 		cx, cy;
		char	**	commands;

//int gX = 100, gY = 100;
//unsigned int gW = 640, gH = 450;

					GPBitmap(float width, float height);
					GPBitmap(float width, float height, char **cmds);
					~GPBitmap();

		BBitmap *	RealBitmap() { return m_bitmap; };

		void		Lock() { m_bitmap->Lock(); };
		void		Unlock() { m_bitmap->Unlock(); };

		BRect 		Bounds() { return m_bitmap->Bounds(); };
		void *		Bits() { return m_bitmap->Bits(); };
		color_space	ColorSpace() { return m_bitmap->ColorSpace(); };
		int32		BitsLength() { return m_bitmap->BitsLength(); };
		int32		BytesPerRow() { return m_bitmap->BytesPerRow(); };

		BView *		View() { return m_view; };
		void		SetDirty(BRegion *r);

		void		ResizeTo(float width, float height, uint32 btns);

		rgb_color 	PixelAtRGB(int x, int y) {
			rgb_color c = *((rgb_color*)(((char*)Bits()) + x*4 + y*BytesPerRow()));
			return c;
		};

		int32 		drawing_loop(void *data);
		void 		addCommand(char *cmd);
		void 		addCommands(BMessage *msg, int32 numCmds);
		void 		clearCommands();
		void 		display(float v_width, float v_height);
		void 		doDiamond(int x, int y, int px, int py);
		void 		doPlus(int x, int y, int px, int py);
		void 		doBox(int x, int y, int px, int py);
		void 		doCross(int x, int y, int px, int py);
		void 		doTriangle(int x, int y, int px, int py);
		void 		doStar(int x, int y, int px, int py);
		void 		doDot(int x, int y);

inline float X(int x)  { return x * xscale; };
inline float Y(int y)  { return (4095-(y)) * yscale; }
};

#ifndef LEFT
#define LEFT 0
#endif
#ifndef CENTRE
#define CENTRE 1
#endif
#ifndef RIGHT
#define RIGHT 2
#endif

#endif
