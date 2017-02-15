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

#ifndef __GNUPLOTAPP_H__
#define __GNUPLOTAPP_H__

extern const char *APP_SIGNATURE;

#define  MAX_WINDOWS	10

typedef struct plot_struct {
	BWindow *window;
//	GPBixmap *pixmap;
	unsigned int posn_flags;
	int x, y;
	unsigned int width, height;	/* window size */
	unsigned int px, py;	/* pointsize */
	int ncommands, max_commands;
	char **commands;
} plot_struct;

class GPApp : public BApplication {
	public:
						GPApp();
		virtual void	ReadyToRun(void);
		virtual void	MessageReceived(BMessage *message);
		virtual void	RefsReceived(BMessage *message);

	private:
		int32			window_count;
		int32			next_untitled_number;
		void 			prepare_plot(plot_struct *plot, int term_number);
		void 			store_command(char *buffer, plot_struct *plot);
		void 			display(plot_struct *plot);
		int32			io_task(plot_struct *data);
		int32			io_loop(void *data);
		thread_id 		io_thread;
//		BFilePanel		*openPanel;
};
#endif
