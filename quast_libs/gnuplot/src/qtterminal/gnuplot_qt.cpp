/* GNUPLOT - gnuplot_qt.cpp */

/*[
 * Copyright 2011   Jérôme Lodewyck
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above. If you wish to allow
 * use of your version of this file only under the terms of the GPL and not
 * to allow others to use your version of this file under the above gnuplot
 * license, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the GPL. If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the GPL or the gnuplot license.
]*/

#include "QtGnuplotApplication.h"
#include <QtCore>
#include <signal.h>
#ifdef _WIN32
# include <windows.h>
#endif

int main(int argc, char* argv[])
{
	signal(SIGINT, SIG_IGN); // Do not listen to SIGINT signals anymore
	const char * qt_gnuplot_data_dir = QTGNUPLOT_DATA_DIR;

#if defined(_WIN32)
	/* On Windows, QTGNUPLOT_DATA_DIR is relative to installation dir. */
	char buf[MAX_PATH];
	if (GetModuleFileNameA(NULL, (LPCH) buf, sizeof(buf))) {
		char * p = strrchr(buf, '\\');
		if (p != NULL) {
			*p = '\0';
			if ((strlen(buf) >= 4) && (strnicmp(&buf[strlen(buf) - 4], "\\bin", 4) == 0))
				buf[strlen(buf) - 4] = '\0';
			p = (char *) malloc(strlen(buf) + strlen(QTGNUPLOT_DATA_DIR) + 2);
			strcpy(p, buf);
			strcat(p, "\\");
			strcat(p, QTGNUPLOT_DATA_DIR);
			qt_gnuplot_data_dir = p;
		}
	}
#endif

#if QT_VERSION < 0x040700
	/* 
	* FIXME: EAM Nov 2011
	* It is better to use environmental variable
	* QT_GRAPHICSSYSTEM but this requires qt >= 4.7
	* "raster" is ~5x faster than "native" (default).
	* Unfortunately "opengl" isn't recognized on my test systems :-(
	*/
	// This makes a huge difference to the speed of polygon rendering.
	// Alternatives are "native", "raster", "opengl"
	QApplication::setGraphicsSystem("raster");
#endif

	QtGnuplotApplication application(argc, argv);

	// Load translations for the qt library
	QTranslator qtTranslator;
	qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	application.installTranslator(&qtTranslator);

	// Load translations for the qt terminal
	QTranslator translator;
	translator.load("qtgnuplot_" + QLocale::system().name(), qt_gnuplot_data_dir);
	application.installTranslator(&translator);

	// Start
	application.exec();

	return 0;
}
