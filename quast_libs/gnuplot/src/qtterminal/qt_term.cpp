/* GNUPLOT - qt_term.cpp */

/*[
 * Copyright 2009   Jérôme Lodewyck
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

/*
 *  Thomas Bleher -  October 2013
 *  The Qt terminal is changed to only create its data on initialization
 *  (to avoid the Static Initialization Order Fiasco) and to destroy its data
 *  in a gnuplot atexit handler.
 */

#include <QtCore>
#include <QtGui>
#include <QtNetwork>

extern "C" {
	#include "plot.h"      // for interactive
	#include "term_api.h"  // for stdfn.h, JUSTIFY, encoding, *term definition, color.h term_interlock
	#include "mouse.h"     // for do_event declaration
	#include "getcolor.h"  // for rgb functions
	#include "command.h"   // for paused_for_mouse, PAUSE_BUTTON1 and friends
	#include "util.h"      // for int_error
	#include "alloc.h"     // for gp_alloc
	#include "parse.h"     // for real_expression
	#include "axis.h"
#ifdef WIN32
	#include "win/winmain.h"  // for WinMessageLoop, ConsoleReadCh, stdin_pipe_reader
	#include "win/wgnuplib.h" // for TextStartEditing, TextStopEditing
	#include "win/wtext.h"    // for kbhit, getchar
	#include <io.h>           // for isatty
#endif
	#include <signal.h>
}


#include "qt_term.h"
#include "QtGnuplotEvent.h"
#include "QtGnuplotApplication.h"
#include "qt_conversion.cpp"

void qt_atexit();

static int argc = 1;
static char empty = '\0';
static char* emptyp = &empty;

struct QtGnuplotState {
    /// @todo per-window options

    // Create a QCoreApplication without event loop if any QObject needs it.
    QCoreApplication application;

    /*-------------------------------------------------------
     * State variables
     *-------------------------------------------------------*/

    bool gnuplot_qtStarted;
    int  currentFontSize;
    QString currentFontName;
    QString localServerName;
    QTextCodec* codec;

    // Events coming from gnuplot are processed by this file
    // and send to the GUI by a QDataStream writing on a
    // QByteArray sent trough a QLocalSocket (a cross-platform IPC protocol)
    QLocalSocket socket;
    QByteArray   outBuffer;
    QDataStream  out;

    bool       enhancedSymbol;
    QString    enhancedFontName;
    double     enhancedFontSize;
    double     enhancedBase;
    bool       enhancedWidthFlag;
    bool       enhancedShowFlag;
    int        enhancedOverprint;
    enum QFont::Weight enhancedFontWeight;
    enum QFont::Style enhancedFontStyle;
    QByteArray enhancedText;

    /// Constructor
    QtGnuplotState()
        : application(argc, &emptyp)

        , gnuplot_qtStarted(false)
        , currentFontSize()
        , currentFontName()
        , localServerName()
        , codec(QTextCodec::codecForLocale())

        , socket()
        , outBuffer()
        , out(&outBuffer, QIODevice::WriteOnly)

        , enhancedSymbol(false)
        , enhancedFontName()
        , enhancedFontSize()
        , enhancedBase()
        , enhancedWidthFlag()
        , enhancedShowFlag()
        , enhancedOverprint()
        , enhancedFontWeight(QFont::Normal)
        , enhancedFontStyle(QFont::StyleNormal)
        , enhancedText()
    {
    }

};

static QtGnuplotState* qt = NULL;

static const int qt_oversampling = 10;
static const double qt_oversamplingF = double(qt_oversampling);

/*-------------------------------------------------------
 * Terminal options with default values
 *-------------------------------------------------------*/
static int  qt_optionWindowId = 0;
static bool qt_optionEnhanced = true;
static bool qt_optionPersist  = false;
static bool qt_optionRaise    = true;
static bool qt_optionCtrl     = false;
static bool qt_optionDash     = true;
static int  qt_optionWidth    = 640;
static int  qt_optionHeight   = 480;
static int  qt_optionFontSize = 9;
static double qt_optionDashLength = 1.0;

/* Encapsulates all Qt options that have a constructor and destructor. */
struct QtOption {
    QtOption()
        : FontName("Sans")
    {}

    QString FontName;
    QString Title;
    QString Widget;
	QPoint  position;
};
static QtOption* qt_option = NULL;

static void ensureOptionsCreated()
{
	if (!qt_option)
	    qt_option = new QtOption();
}

static bool qt_setPosition = false;
static bool qt_setSize   = true;
static int  qt_setWidth  = qt_optionWidth;
static int  qt_setHeight = qt_optionHeight;

/* ------------------------------------------------------
 * Helpers
 * ------------------------------------------------------*/

// Convert gnuplot coordinates into floating point term coordinates
QPointF qt_termCoordF(int x, int y)
{
	return QPointF(double(x)/qt_oversamplingF, double(int(term->ymax) - y)/qt_oversamplingF);
}

// The same, but with coordinates clipped to the nearest pixel
QPoint qt_termCoord(int x, int y)
{
	return QPoint(qRound(double(x)/qt_oversamplingF), qRound(double(term->ymax - y)/qt_oversamplingF));
}

// Inverse of the previous function
QPoint qt_gnuplotCoord(int x, int y)
{
	return QPoint(x*qt_oversampling, int(term->ymax) - y*qt_oversampling);
}

#ifndef GNUPLOT_QT
# ifdef WIN32
#  define GNUPLOT_QT "gnuplot_qt.exe"
# else
#  define GNUPLOT_QT "gnuplot_qt"
# endif
#endif

// Start the GUI application
void execGnuplotQt()
{
	QString filename;
	char* path = getenv("GNUPLOT_DRIVER_DIR");
	if (path)
		filename = QString(path);
	if (filename.isEmpty()) {
#ifdef WIN32
		filename = QCoreApplication::applicationDirPath();
#else
		filename = QT_DRIVER_DIR;
#endif
	}

	filename += "/";
	filename += GNUPLOT_QT;

	qint64 pid;
	qt->gnuplot_qtStarted = QProcess::startDetached(filename, QStringList(), QString(), &pid);
	if (qt->gnuplot_qtStarted)
		qt->localServerName = "qtgnuplot" + QString::number(pid);
	else
		fprintf(stderr, "Could not start gnuplot_qt with path %s\n", filename.toUtf8().data());
}

/*-------------------------------------------------------
 * Communication gnuplot -> terminal
 *-------------------------------------------------------*/

void qt_flushOutBuffer()
{
	if (!qt || !qt->socket.isValid())
		return;

	// Write the block size at the beginning of the block
	QDataStream sizeStream(&qt->socket);
	sizeStream << (quint32)(qt->outBuffer.size());
	// Write the block to the QLocalSocket
	qt->socket.write(qt->outBuffer);
	// waitForBytesWritten(-1) is supposed implement this loop, but it does not seem to work !
	// update: seems to work with Qt 4.5 on Linux and Qt 5.1 on Windows, but not on Mac
	while (qt->socket.bytesToWrite() > 0)
	{
		qt->socket.flush();
		// Avoid dead-locking when no more data is available
		if (qt->socket.bytesToWrite() > 0)
			qt->socket.waitForBytesWritten(-1);
	}
	// Reset the buffer
	qt->out.device()->seek(0);
	qt->outBuffer.clear();
}

// Helper function called by qt_connectToServer()
void qt_connectToServer(const QString& server, bool retry = true)
{
	ensureOptionsCreated();
	bool connectToWidget = (server != qt->localServerName);

	// The QLocalSocket::waitForConnected does not respect the time out argument when
	// the gnuplot_qt application is not yet started or has not yet self-initialized.
	// To wait for it, we need to implement the timeout ourselves
	QDateTime timeout = QDateTime::currentDateTime().addMSecs(30000);
	do
	{
		qt->socket.connectToServer(server);
		if (!qt->socket.waitForConnected(-1)) {
			// qDebug() << qt->socket.errorString();
			GP_SLEEP(0.2);	// yield CPU for 0.2 seconds
		}
	}
	while((qt->socket.state() != QLocalSocket::ConnectedState) && (QDateTime::currentDateTime() < timeout));

	// Still not connected...
	if ((qt->socket.state() != QLocalSocket::ConnectedState) && retry)
	{
		// The widget could not be reached: start a gnuplot_qt program which will create a QtGnuplotApplication
		if (connectToWidget)
		{
			fprintf(stderr, "Could not connect to existing qt widget. Starting a new one.\n");
			qt_option->Widget = QString();
			qt_connectToServer(qt->localServerName);
		}
		// The gnuplot_qt program could not be reached: try to start a new one
		else
		{
			fprintf(stderr, "Could not connect to existing gnuplot_qt. Starting a new one.\n");
			execGnuplotQt();
			qt_connectToServer(qt->localServerName, false);
		}
	}
}

// Called before a plot to connect to the terminal window, if needed
void qt_connectToServer()
{
	if (!qt)
		return;
	ensureOptionsCreated();

	// Determine to which server we should connect
	bool connectToWidget = !qt_option->Widget.isEmpty();
	QString server = connectToWidget ? qt_option->Widget : qt->localServerName;

	if (qt->socket.state() == QLocalSocket::ConnectedState)
	{
		// Check if we are already connected to the correct server
		if (qt->socket.serverName() == server)
			return;

		// Otherwise disconnect
		qt->socket.disconnectFromServer();
		while (qt->socket.state() == QLocalSocket::ConnectedState)
			qt->socket.waitForDisconnected(1000);
	}

	// Start the gnuplot_qt helper program if not already started
	if (!connectToWidget && !qt->gnuplot_qtStarted) {
		execGnuplotQt();
		server = qt->localServerName;
	}

	// Connect to the server, or local server if not available.
	qt_connectToServer(server);
}

/*-------------------------------------------------------
 * Communication terminal -> gnuplot
 *-------------------------------------------------------*/

bool qt_processTermEvent(gp_event_t* event)
{
	// Intercepts resize event
	if (event->type == GE_fontprops)
	{
		// This is an answer to a font metric request. We don't send it back to gnuplot
		if ((event->par1 > 0) && (event->par2 > 0))
		{
			fprintf(stderr, "qt_processTermEvent received a GE_fontprops event. This should not have happened\n");
			return false;
		}
		// This is a resize event
		qt_setSize   = true;
		qt_setWidth  = event->mx;
		qt_setHeight = event->my;
	}
	// Scale mouse events
	else
	{
		QPoint p = qt_gnuplotCoord(event->mx, event->my);
		event->mx = p.x();
		event->my = p.y();
	}

	// Send the event to gnuplot core
	do_event(event);
	// Process pause_for_mouse
	if ((event->type == GE_buttonrelease) && (paused_for_mouse & PAUSE_CLICK))
	{
		int button = event->par1;
		if (((button == 1) && (paused_for_mouse & PAUSE_BUTTON1)) ||
		    ((button == 2) && (paused_for_mouse & PAUSE_BUTTON2)) ||
		    ((button == 3) && (paused_for_mouse & PAUSE_BUTTON3)))
			paused_for_mouse = 0;
		if (paused_for_mouse == 0)
			return true;
	}
	if ((event->type == GE_keypress) && (paused_for_mouse & PAUSE_KEYSTROKE) && (event->par1 > '\0'))
	{
		paused_for_mouse = 0;
		return true;
	}

	return false;
}

/* ------------------------------------------------------
 * Functions called by gnuplot
 * ------------------------------------------------------*/

// Called before first plot after a set term command.
void qt_init()
{
	if (qt)
		return;
	ensureOptionsCreated();

	qt = new QtGnuplotState();

	// If we are not connecting to an existing QtGnuplotWidget, start a QtGnuplotApplication
	if (qt_option->Widget.isEmpty())
		execGnuplotQt();

	// The creation of a QApplication mangled our locale settings
#ifdef HAVE_LOCALE_H
	setlocale(LC_NUMERIC, "C");
	setlocale(LC_TIME, current_locale);
#endif

	qt->out.setVersion(QDataStream::Qt_4_4);
	term_interlock = (void *)qt_init;
	gp_atexit(qt_atexit);
}

// Send a "Set font" event to the GUI, and wait for the font metrics answer
void qt_sendFont()
{
	qt->out << GESetFont << qt->currentFontName << qt->currentFontSize;

	QPair<QString, int> currentFont(qt->currentFontName, qt->currentFontSize);
	static QPair<QString, int> lastFont("", 0);

	// The font has not changed
	if (currentFont == lastFont)
		return;

	static QMap<QPair<QString, int>, QPair<int, int> > fontMetricCache;
	QPair<int, int> metric;

	// Try to find the font metric in the cache or ask the GUI for the font metrics
	if (fontMetricCache.contains(currentFont))
		metric = fontMetricCache[currentFont];
	else
	{
		qt->out << GEFontMetricRequest;
		qt_flushOutBuffer();
		bool receivedFontProps = false;
		int waitcount = 0;
		while (!receivedFontProps)
		{
			qt->socket.waitForReadyRead(1000);
			if (qt->socket.bytesAvailable() < (int)sizeof(gp_event_t)) {
				fprintf(stderr, (waitcount++ % 10 > 0) ? "  ."
					: "\nWarning: slow font initialization");
#ifdef Q_OS_MAC
				// OSX can be slow (>30 seconds?!) in determining font metrics
				// Give it more time rather than failing after 1 second 
				// Possibly this is only relevant to Qt5
				GP_SLEEP(0.5);
				continue;
#endif
				return;
			}
			while (qt->socket.bytesAvailable() >= (int)sizeof(gp_event_t))
			{
				gp_event_t event;
				qt->socket.read((char*) &event, sizeof(gp_event_t));
				// Here, we discard other events than fontprops.
				if ((event.type == GE_fontprops) && (event.par1 > 0) && (event.par2 > 0))
				{
					receivedFontProps = true;
					metric = QPair<int, int>(event.par1, event.par2);
					fontMetricCache[currentFont] = metric;
					break;
				}
			}
		}
		if (waitcount > 0)
			fprintf(stderr,"\n");
	}

	term->v_char = qt_oversampling*metric.first;
	term->h_char = qt_oversampling*metric.second;
	lastFont = currentFont;
}

// Called just before a plot is going to be displayed.
void qt_graphics()
{
	ensureOptionsCreated();
	qt->out << GEDesactivate;
	qt_flushOutBuffer();
	qt_connectToServer();

	// Set text encoding
	if (!(qt->codec = qt_encodingToCodec(encoding)))
		qt->codec = QTextCodec::codecForLocale();

	// Set font
	qt->currentFontSize = qt_optionFontSize;
	qt->currentFontName = qt_option->FontName;

	// Set plot size
	if (qt_setSize)
	{
		term->xmax = qt_oversampling*qt_setWidth;
		term->ymax = qt_oversampling*qt_setHeight;
		qt_setSize = false;
	}

	// Initialize window
	qt->out << GESetCurrentWindow << qt_optionWindowId;
	qt->out << GEInitWindow;
	qt->out << GEActivate;
	qt->out << GETitle << qt_option->Title;
	qt->out << GESetCtrl << qt_optionCtrl;
	qt->out << GESetWidgetSize << QSize(term->xmax, term->ymax)/qt_oversampling;
	// Initialize the scene
	qt->out << GESetSceneSize << QSize(term->xmax, term->ymax)/qt_oversampling;
	qt->out << GEClear;
	// Initialize the font
	qt_sendFont();
	term->v_tic = (unsigned int) (term->v_char/2.5);
	term->h_tic = (unsigned int) (term->v_char/2.5);

	if (qt_setPosition)
	{
		qt->out << GESetPosition << qt_option->position;
		qt_setPosition = false;
	}
}

// Called after plotting is done
void qt_text()
{
	if (qt_optionRaise)
		qt->out << GERaise;
	qt->out << GEDone;
	qt_flushOutBuffer();
}

void qt_text_wrapper()
{
	if (!qt)
		return;

	// Remember scale to update the status bar while the plot is inactive
	qt->out << GEScale;

	const int axis_order[4] = {FIRST_X_AXIS, FIRST_Y_AXIS, SECOND_X_AXIS, SECOND_Y_AXIS};

	for (int i = 0; i < 4; i++)
	{
		qt->out << (axis_array[axis_order[i]].ticmode != NO_TICS); // Axis active or not
		qt->out << axis_array[axis_order[i]].min;
		double lower = double(axis_array[axis_order[i]].term_lower);
		double scale = double(axis_array[axis_order[i]].term_scale);
		// Reverse the y axis
		if (i % 2)
		{
			lower = term->ymax - lower;
			scale *= -1;
		}
		qt->out << lower/qt_oversamplingF << scale/qt_oversamplingF;
		qt->out << (axis_array[axis_order[i]].log ? axis_array[axis_order[i]].log_base : 0.);
	}

	qt_text();
}

void qt_reset()
{
	/// @todo
}

void qt_move(unsigned int x, unsigned int y)
{
	qt->out << GEMove << qt_termCoordF(x, y);
}

void qt_vector(unsigned int x, unsigned int y)
{
	qt->out << GEVector << qt_termCoordF(x, y);
}

void qt_enhanced_flush()
{
	qt->out << GEEnhancedFlush << qt->enhancedFontName << qt->enhancedFontSize
		<< (int)qt->enhancedFontStyle << (int)qt->enhancedFontWeight
		<< qt->enhancedBase << qt->enhancedWidthFlag << qt->enhancedShowFlag
		<< qt->enhancedOverprint 
		<< qt->codec->toUnicode(qt->enhancedText);
	qt->enhancedText.clear();
}

void qt_enhanced_writec(int c)
{
	if (qt->enhancedSymbol)
		qt->enhancedText.append(qt->codec->fromUnicode(qt_symbolToUnicode(c)));
	else
		qt->enhancedText.append(char(c));
}

void qt_enhanced_open(char* fontname, double fontsize, double base, TBOOLEAN widthflag, TBOOLEAN showflag, int overprint)
{
	qt->enhancedFontSize  = fontsize;
	qt->enhancedBase      = base;
	qt->enhancedWidthFlag = widthflag;
	qt->enhancedShowFlag  = showflag;
	qt->enhancedOverprint = overprint;

	// strip Bold or Italic property out of font name
	QString tempname = fontname;
	if (tempname.contains(":italic", Qt::CaseInsensitive))
		qt->enhancedFontStyle = QFont::StyleItalic;
	else
		qt->enhancedFontStyle = QFont::StyleNormal;
	if (tempname.contains(":bold", Qt::CaseInsensitive))
		qt->enhancedFontWeight = QFont::Bold;
	else
		qt->enhancedFontWeight = QFont::Normal;
	int sep = tempname.indexOf(":");
	if (sep >= 0)
		tempname.truncate(sep);
	
	// Blank font name means keep using the previous font
	if (!tempname.isEmpty())
		qt->enhancedFontName = tempname;

	if (qt->enhancedFontName.toLower() == "symbol")
	{
		qt->enhancedSymbol = true;
		qt->enhancedFontName = "Sans";
	}
	else
		qt->enhancedSymbol = false;
}

void qt_put_text(unsigned int x, unsigned int y, const char* string)
{
	// if ignore_enhanced_text is set, draw with the normal routine.
	// This is meant to avoid enhanced syntax when the enhanced mode is on
	if (!qt_optionEnhanced || ignore_enhanced_text)
	{
		/// @todo Symbol font to unicode
		/// @todo bold, italic
		qt->out << GEPutText << qt_termCoord(x, y) << qt->codec->toUnicode(string);
		return;
	}

	// Uses enhanced_recursion() to analyse the string to print.
	// enhanced_recursion() calls _enhanced_open() to initialize the text drawing,
	// then it calls _enhanced_writec() which buffers the characters to draw,
	// and finally _enhanced_flush() to draw the buffer with the correct justification.

	// set up the global variables needed by enhanced_recursion()
	enhanced_fontscale = 1.0;
	strncpy(enhanced_escape_format, "%c", sizeof(enhanced_escape_format));

	// Set the recursion going. We say to keep going until a closing brace, but
	// we don't really expect to find one.  If the return value is not the nul-
	// terminator of the string, that can only mean that we did find an unmatched
	// closing brace in the string. We increment past it (else we get stuck
	// in an infinite loop) and try again.
	while (*(string = enhanced_recursion((char*)string, TRUE, qt->currentFontName.toUtf8().data(),
			qt->currentFontSize, 0.0, TRUE, TRUE, 0)))
	{
		qt_enhanced_flush();
		enh_err_check(string); // we can only get here if *str == '}'
		if (!*++string)
			break; // end of string
		// else carry on and process the rest of the string
	}

	qt->out << GEEnhancedFinish << qt_termCoord(x, y);
}

void qt_linetype(int lt)
{
	if (lt <= LT_NODRAW)
		lt = LT_NODRAW; // background color

	/* Version 5: dash pattern will be set later by term->dashtype */
	if (lt == LT_AXIS)
		qt->out << GEPenStyle << Qt::DotLine;
	else if (lt == LT_NODRAW)
		qt->out << GEPenStyle << Qt::NoPen;
	else 
		qt->out << GEPenStyle << Qt::SolidLine;

	if ((lt-1) == LT_BACKGROUND) {
		/* FIXME: Add parameter to this API to set the background color from the gnuplot end */
		qt->out << GEBackgroundColor;
	} else
		qt->out << GEPenColor << qt_colorList[lt % 9 + 3];
}

void qt_dashtype(int type, t_dashtype *custom_dash_type)
{
	double empirical_scale = 0.55;

	switch (type) {
	case DASHTYPE_SOLID:
		qt->out << GEPenStyle << Qt::SolidLine;
		break;
	case DASHTYPE_AXIS:
		qt->out << GEPenStyle << Qt::DotLine;
		break;
	case DASHTYPE_CUSTOM:
		if (custom_dash_type) {
			QVector<qreal> dashpattern;
			for (int j = 0; j < 8 && custom_dash_type->pattern[j] > 0; j++) {
				dashpattern.append( custom_dash_type->pattern[j]
					* qt_optionDashLength * empirical_scale);
			}
			qt->out << GEDashPattern << dashpattern;
			qt->out << GEPenStyle << Qt::CustomDashLine;
		}
		break;
	default:
		/* Fall back to whatever version 4 would have provided */
		if (type > 0) {
			Qt::PenStyle style;
			style =
				(type%5 == 1) ? Qt::DashLine :
				(type%5 == 2) ? Qt::DotLine :
				(type%5 == 3) ? Qt::DashDotLine :
				(type%5 == 4) ? Qt::DashDotDotLine :
					      Qt::SolidLine ;
			qt->out << GEPenStyle << style;
		} else {
			qt->out << GEPenStyle << Qt::SolidLine;
		}
		break;
	}
}

int qt_set_font(const char* font)
{
	ensureOptionsCreated();
	int  qt_previousFontSize = qt->currentFontSize;
	QString qt_previousFontName = qt->currentFontName;

	if (font && (*font))
	{
		QStringList list = QString(font).split(',');
		if (list.size() > 0)
			qt->currentFontName = list[0];
		if (list.size() > 1)
			qt->currentFontSize = list[1].toInt();
	} else {
		qt->currentFontSize = qt_optionFontSize;
		qt->currentFontName = qt_option->FontName;
	}

	if (qt->currentFontName.isEmpty())
		qt->currentFontName = qt_option->FontName;

	if (qt->currentFontSize <= 0)
		qt->currentFontSize = qt_optionFontSize;

	/* Optimize by leaving early if there is no change */
	if (qt->currentFontSize == qt_previousFontSize
	&&  qt->currentFontName == qt->currentFontName) {
		return 1;
	}

	qt_sendFont();

	return 1;
}

int qt_justify_text(enum JUSTIFY mode)
{
	if (mode == LEFT)
		qt->out << GETextAlignment << Qt::AlignLeft;
	else if (mode == RIGHT)
		qt->out << GETextAlignment << Qt::AlignRight;
	else if (mode == CENTRE)
		qt->out << GETextAlignment << Qt::AlignCenter;

	return 1; // We can justify
}

void qt_point(unsigned int x, unsigned int y, int pointstyle)
{
	qt->out << GEPoint << qt_termCoordF(x, y) << pointstyle;
}

void qt_pointsize(double ptsize)
{
	if (ptsize < 0.) ptsize = 1.; // same behaviour as x11 terminal
	qt->out << GEPointSize << ptsize;
}

void qt_linewidth(double lw)
{
	qt->out << GELineWidth << lw;
}

int qt_text_angle(int angle)
{
	qt->out << GETextAngle << double(angle);
	return 1; // 1 means we can rotate
}

void qt_fillbox(int style, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	qt->out << GEBrushStyle << style;
	qt->out << GEFillBox << QRect(qt_termCoord(x, y + height), QSize(width, height)/qt_oversampling);
}

int qt_make_palette(t_sm_palette* palette)
{
	return 0; // We can do continuous colors
}

void qt_set_color(t_colorspec* colorspec)
{
	if (colorspec->type == TC_LT) {
		if (colorspec->lt <= LT_NODRAW)
			qt->out << GEBackgroundColor;
		else
			qt->out << GEPenColor << qt_colorList[colorspec->lt % 9 + 3];
	}
	else if (colorspec->type == TC_FRAC)
	{
		rgb_color rgb;
		rgb1maxcolors_from_gray(colorspec->value, &rgb);
		QColor color;
		color.setRgbF(rgb.r, rgb.g, rgb.b);
		qt->out << GEPenColor << color;
	}
	else if (colorspec->type == TC_RGB) {
		QColor color = QRgb(colorspec->lt);
		int alpha = (colorspec->lt >> 24) & 0xff;
		if (alpha > 0)
			color.setAlpha(255-alpha);
		qt->out << GEPenColor << color;
	}
}

void qt_filled_polygon(int n, gpiPoint *corners)
{
	QPolygonF polygon;
	for (int i = 0; i < n; i++)
		polygon << qt_termCoordF(corners[i].x, corners[i].y);

	qt->out << GEBrushStyle << corners->style;
	qt->out << GEFilledPolygon << polygon;
}

void qt_image(unsigned int M, unsigned int N, coordval* image, gpiPoint* corner, t_imagecolor color_mode)
{
	QImage qimage = qt_imageToQImage(M, N, image, color_mode);
	qt->out << GEImage;
	for (int i = 0; i < 4; i++)
		qt->out << qt_termCoordF(corner[i].x, corner[i].y);
	qt->out << qimage;
}

#ifdef USE_MOUSE

// Display temporary text, after
// erasing any temporary text displayed previously at this location.
// The int determines where: 0=statusline, 1,2: at corners of zoom
// box, with \r separating text above and below the point.
void qt_put_tmptext(int n, const char str[])
{
    if (!qt)
        return;

	if (n == 0)
		qt->out << GEStatusText << QString(str);
	else if (n == 1)
		qt->out << GEZoomStart << QString(str);
	else if (n == 2)
		qt->out << GEZoomStop << QString(str);

	qt_flushOutBuffer();
}

void qt_set_cursor(int c, int x, int y)
{
    if (!qt)
        return;

	// Cancel zoombox when Echap is pressed
	if (c == 0)
		qt->out << GEZoomStop << QString();

	if (c == -4)
		qt->out << GELineTo << false;
	else if (c == -3)
		qt->out << GELineTo << true;
	else if (c == -2) // warp the pointer to the given position
		qt->out << GEWrapCursor << qt_termCoord(x, y);
	else if (c == -1) // start zooming
		qt->out << GECursor << Qt::SizeFDiagCursor;
	else if (c ==  1) // Rotation
		qt->out << GECursor << Qt::ClosedHandCursor;
	else if (c ==  2) // Rescale
		qt->out << GECursor << Qt::SizeAllCursor;
	else if (c ==  3) // Zoom
		qt->out << GECursor << Qt::SizeFDiagCursor;
	else
		qt->out << GECursor << Qt::CrossCursor;

	qt_flushOutBuffer();
}

void qt_set_ruler(int x, int y)
{
	if (!qt)
		return;
	qt->out << GERuler << qt_termCoord(x, y);
	qt_flushOutBuffer();
}

void qt_set_clipboard(const char s[])
{
	if (!qt)
		return;
	qt->out << GECopyClipboard << QString(s);
	qt_flushOutBuffer();
}
#endif // USE_MOUSE


int qt_waitforinput(int options)
{
#ifdef USE_MOUSE
#ifndef WIN32
	fd_set read_fds;
	struct timeval one_msec;
	int stdin_fd  = fileno(stdin);
	int socket_fd = qt ? qt->socket.socketDescriptor() : -1;

	if (!qt || (socket_fd < 0) || (qt->socket.state() != QLocalSocket::ConnectedState)) {
		if (options == TERM_ONLY_CHECK_MOUSING)
			return '\0';
		else
			return getchar();
	}

	// Gnuplot event loop
	do
	{
		// Watch file descriptors
		struct timeval *timeout = NULL;
		FD_ZERO(&read_fds);
		FD_SET(socket_fd, &read_fds);
		if (!paused_for_mouse)
			FD_SET(stdin_fd, &read_fds);

		// When taking input from the console, we are willing to wait
		// here until the next character is typed.  But if input is from
		// a script we just want to check for hotkeys or mouse input and
		// then leave again without waiting on stdin.
		if (options == TERM_ONLY_CHECK_MOUSING) {
			timeout = &one_msec;
			one_msec.tv_sec = 0;
			one_msec.tv_usec = TERM_EVENT_POLL_TIMEOUT;
		}

		// Wait for input
		if (select(socket_fd+1, &read_fds, NULL, NULL, timeout) < 0)
		{
			// Display the error message except when Ctrl + C is pressed
			if (errno != 4)
				fprintf(stderr, "Qt terminal communication error: select() error %i %s\n", errno, strerror(errno));
			break;
		}

		// Terminal event coming
		if (FD_ISSET(socket_fd, &read_fds))
		{
			if (!(qt->socket.waitForReadyRead(-1))) {
				// Must be a socket error; we need to restart qt_gnuplot
				fprintf(stderr, "Error: plot window (gnuplot_qt) not responding - will restart\n");
				qt->gnuplot_qtStarted = false;
				return '\0';
			}

			// Temporary event for mouse move events. If several consecutive
			// move events are received, only transmit the last one.
			gp_event_t tempEvent;
			tempEvent.type = -1;
			if (qt->socket.bytesAvailable() < (int)sizeof(gp_event_t)) {
				fprintf(stderr, "Error: short read from gnuplot_qt socket\n");
				return '\0';
			}
			while (qt->socket.bytesAvailable() >= (int)sizeof(gp_event_t))
			{
				struct gp_event_t event;
				qt->socket.read((char*) &event, sizeof(gp_event_t));
				// Delay move events
				if (event.type == GE_motion)
					tempEvent = event;
				// Other events. Replay the last move event if present
				else
				{
					if (tempEvent.type == GE_motion)
					{
						qt_processTermEvent(&tempEvent);
						tempEvent.type = -1;
					}
					if (qt_processTermEvent(&event))
						return '\0'; // exit from paused_for_mouse
				}
			}
			// Replay move event
			if (tempEvent.type == GE_motion)
				qt_processTermEvent(&tempEvent);
		}

		else if (options == TERM_ONLY_CHECK_MOUSING) {
			return '\0';
		}
	} while (paused_for_mouse || !FD_ISSET(stdin_fd, &read_fds));

	if (options == TERM_ONLY_CHECK_MOUSING)
		return '\0';
	else
		return getchar();

#else // Windows console and wgnuplot
#ifdef WGP_CONSOLE
	int fd = fileno(stdin);
#endif
	HANDLE h[2];	// list of handles to wait for
	int idx = 0;	// count of handles to wait for and current index
	int idx_stdin = -1;	// return value MsgWaitForMultipleObjects for stdin
	int idx_socket = -1;	// return value MsgWaitForMultipleObjects for the Qt socket
	int idx_msg = -1;		// return value MsgWaitForMultipleObjects for message queue events
	int c = NUL;
	bool waitOK = true;
	bool quitLoop = false;
#ifndef WGP_CONSOLE
	if (options != TERM_ONLY_CHECK_MOUSING)
		TextStartEditing(&textwin);
#endif

	// stdin or console
	if (options != TERM_ONLY_CHECK_MOUSING) { // NOTE: change this if used also for the caca terminal
#ifdef WGP_CONSOLE
		if (!isatty(fd))
			h[0] = CreateThread(NULL, 0, stdin_pipe_reader, NULL, 0, NULL);
		else
#endif
			h[0] = GetStdHandle(STD_INPUT_HANDLE);
		if (h[0] != NULL)
			idx_stdin = WAIT_OBJECT_0 + idx++;
	}

	// Named pipe of QLocalSocket
	if (qt != NULL) {
		h[idx] = (HANDLE) qt->socket.socketDescriptor();
		DWORD flags;
		if (GetHandleInformation(h[idx], &flags) == 0)
			fprintf(stderr, "Error: QtLocalSocket handle is invalid\n");
		else
			idx_socket = WAIT_OBJECT_0 + idx++;
	}

	// Windows Messages
	idx_msg = WAIT_OBJECT_0 + idx; // do not increment count

	// Process any pending message queue events
	WinMessageLoop();

	do {
		int waitResult = -1;
		
#ifndef WGP_CONSOLE
		// Process pending key events of the text console
		if (kbhit() && (options != TERM_ONLY_CHECK_MOUSING))
			waitResult = idx_msg;
#endif

		// Process pending qt events
		if ((idx_socket != -1) && // (qt != NULL)) &&
			(qt->socket.waitForReadyRead(0)) && (qt->socket.bytesAvailable() >= (int)sizeof(gp_event_t)))
			waitResult = idx_socket; // data already available
		
		// Wait for a new event 
		if ((waitResult == -1) && (options != TERM_ONLY_CHECK_MOUSING))
			waitResult = MsgWaitForMultipleObjects(idx, h, FALSE, INFINITE, QS_ALLINPUT);  // wait for new data
		
		if ((waitResult == idx_stdin) && (idx_stdin != -1)) { // console windows or caca terminal (TBD)
#ifdef WGP_CONSOLE
			if (!isatty(fd)) {
				DWORD dw;

				GetExitCodeThread(h[0], &dw);
				CloseHandle(h[0]);
				c = dw;
				quitLoop = true;
			} else 
#endif
			{
				c = ConsoleReadCh();
				if (c != NUL)
					quitLoop = true;
				// Otherwise, this wasn't a key down event and we cycle again
			}

		} else if ((waitResult == idx_socket) && (idx_socket != -1)) { // qt terminal
			qt->socket.waitForReadyRead(0);
			// Temporary event for mouse move events. If several consecutive move events
			// are received, only transmit the last one.
			gp_event_t tempEvent;
			tempEvent.type = -1;
			while (qt->socket.bytesAvailable() >= (int)sizeof(gp_event_t)) {
				struct gp_event_t event;
				qt->socket.read((char*) &event, sizeof(gp_event_t));
				// Delay move events
				if (event.type == GE_motion)
					tempEvent = event;
				// Other events. Replay the last move event if present
				else {
					if (tempEvent.type == GE_motion) {
						qt_processTermEvent(&tempEvent);
						tempEvent.type = -1;
					}
					if (qt_processTermEvent(&event)) {
						c = NUL; // exit from paused_for_mouse
						quitLoop = true;
					}
				}
			}
			// Replay move event
			if (tempEvent.type == GE_motion)
				qt_processTermEvent(&tempEvent);

		} else if (waitResult == idx_msg) {	// Text window, windows and wxt terminals
			// process windows message queue events
			WinMessageLoop();
			if (options == TERM_ONLY_CHECK_MOUSING) {
				quitLoop = true;
			} else {
#ifdef WGP_CONSOLE
				if (ctrlc_flag) {
					c = '\r';
					quitLoop = true;
				}
#else
				// get key from text window if available
				if (kbhit()) {
					c = getchar();
					quitLoop = true;
				}
#endif
			}

		} else { // Time-out or Error
			waitOK = false;
			quitLoop = true;
		}
	} while (!quitLoop);


#ifndef WGP_CONSOLE
	if (options != TERM_ONLY_CHECK_MOUSING)
		TextStopEditing(&textwin);
	
	// This happens if neither the qt queue is alive, nor there is a console window.
	if ((options != TERM_ONLY_CHECK_MOUSING) && !waitOK)
		return getchar();
#endif
	return c;
#endif // WIN32
#else
	return getchar();
#endif // USE_MOUSE
}

/*-------------------------------------------------------
 * Misc
 *-------------------------------------------------------*/

void qt_atexit()
{
	if (!qt)
		return;

	if (qt_optionPersist || persist_cl)
	{
		qt->out << GEDesactivate;
		qt->out << GEPersist;
	}
	else
		qt->out << GEExit;
	qt_flushOutBuffer();
        
        delete qt;
        qt = NULL;

	delete qt_option;
	qt_option = NULL;
}

/*-------------------------------------------------------
 * Term options
 *-------------------------------------------------------*/

enum QT_id {
	QT_WIDGET,
	QT_FONT,
	QT_ENHANCED,
	QT_NOENHANCED,
	QT_SIZE,
	QT_POSITION,
	QT_PERSIST,
	QT_NOPERSIST,
	QT_RAISE,
	QT_NORAISE,
	QT_CTRL,
	QT_NOCTRL,
	QT_TITLE,
	QT_CLOSE,
	QT_DASH,
	QT_DASHLENGTH,
	QT_SOLID,
	QT_OTHER
};

static struct gen_table qt_opts[] = {
	{"$widget",     QT_WIDGET},
	{"font",        QT_FONT},
	{"enh$anced",   QT_ENHANCED},
	{"noenh$anced", QT_NOENHANCED},
	{"s$ize",       QT_SIZE},
	{"pos$ition,",  QT_POSITION},
	{"per$sist",    QT_PERSIST},
	{"noper$sist",  QT_NOPERSIST},
	{"rai$se",      QT_RAISE},
	{"norai$se",    QT_NORAISE},
	{"ct$rlq",      QT_CTRL},
	{"noct$rlq",    QT_NOCTRL},
	{"ti$tle",      QT_TITLE},
	{"cl$ose",      QT_CLOSE},
	{"dash$ed",	QT_DASH},
	{"dashl$ength",	QT_DASHLENGTH},
	{"dl",		QT_DASHLENGTH},
	{"solid",	QT_SOLID},
	{NULL,          QT_OTHER}
};

// Called when terminal type is selected.
// This procedure should parse options on the command line.
// A list of the currently selected options should be stored in term_options[],
// in a form suitable for use with the set term command.
// term_options[] is used by the save command.  Use options_null() if no options are available." *
void qt_options()
{
	ensureOptionsCreated();
	char *s = NULL;
	QString fontSettings;
	bool duplication = false;
	bool set_enhanced = false, set_font = false;
	bool set_persist = false, set_number = false;
	bool set_raise = false, set_ctrl = false;
	bool set_title = false, set_close = false;
	bool set_size = false, set_position = false;
	bool set_widget = false;
	bool set_dash = false;
	bool set_dashlength = false;
	int previous_WindowId = qt_optionWindowId;

#ifndef WIN32
	if (term_interlock != NULL && term_interlock != (void *)qt_init) {
		term = NULL;
		int_error(NO_CARET, "The qt terminal cannot be used in a wxt session");
	}
#endif

#define SETCHECKDUP(x) { c_token++; if (x) duplication = true; x = true; }

	while (!END_OF_COMMAND)
	{
		FPRINTF((stderr, "processing token\n"));
		switch (lookup_table(&qt_opts[0], c_token)) {
		case QT_WIDGET:
			SETCHECKDUP(set_widget);
			if (!(s = try_to_get_string()))
				int_error(c_token, "widget: expecting string");
			if (*s)
				qt_option->Widget = QString(s);
			free(s);
			break;
		case QT_FONT:
			SETCHECKDUP(set_font);
			if (!(s = try_to_get_string()))
				int_error(c_token, "font: expecting string");
			if (*s)
			{
				fontSettings = QString(s);
				QStringList list = fontSettings.split(',');
				if ((list.size() > 0) && !list[0].isEmpty())
					qt_option->FontName = list[0];
				if ((list.size() > 1) && (list[1].toInt() > 0))
					qt_optionFontSize = list[1].toInt();
			}
			free(s);
			break;
		case QT_ENHANCED:
			SETCHECKDUP(set_enhanced);
			qt_optionEnhanced = true;
			term->flags |= TERM_ENHANCED_TEXT;
			break;
		case QT_NOENHANCED:
			SETCHECKDUP(set_enhanced);
			qt_optionEnhanced = false;
			term->flags &= ~TERM_ENHANCED_TEXT;
			break;
		case QT_SIZE:
			SETCHECKDUP(set_size);
			if (END_OF_COMMAND)
				int_error(c_token, "size requires 'width,heigth'");
			qt_optionWidth = real_expression();
			if (!equals(c_token++, ","))
				int_error(c_token, "size requires 'width,heigth'");
			qt_optionHeight = real_expression();
			if (qt_optionWidth < 1 || qt_optionHeight < 1)
				int_error(c_token, "size is out of range");
			break;
		case QT_POSITION:
			SETCHECKDUP(set_position);
			if (END_OF_COMMAND)
				int_error(c_token, "position requires 'x,y'");
			qt_option->position.setX(real_expression());
			if (!equals(c_token++, ","))
				int_error(c_token, "position requires 'x,y'");
			qt_option->position.setY(real_expression());
			break;
		case QT_PERSIST:
			SETCHECKDUP(set_persist);
			qt_optionPersist = true;
			break;
		case QT_NOPERSIST:
			SETCHECKDUP(set_persist);
			qt_optionPersist = false;
			break;
		case QT_RAISE:
			SETCHECKDUP(set_raise);
			qt_optionRaise = true;
			break;
		case QT_NORAISE:
			SETCHECKDUP(set_raise);
			qt_optionRaise = false;
			break;
		case QT_CTRL:
			SETCHECKDUP(set_ctrl);
			qt_optionCtrl = true;
			break;
		case QT_NOCTRL:
			SETCHECKDUP(set_ctrl);
			qt_optionCtrl = false;
			break;
		case QT_TITLE:
			SETCHECKDUP(set_title);
			if (!(s = try_to_get_string()))
				int_error(c_token, "title: expecting string");
			if (*s)
				qt_option->Title = qt_encodingToCodec(encoding)->toUnicode(s);
			free(s);
			break;
		case QT_CLOSE:
			SETCHECKDUP(set_close);
			break;
		case QT_DASH:
			SETCHECKDUP(set_dash);
			qt_optionDash = true;
			break;
		case QT_DASHLENGTH:
			SETCHECKDUP(set_dashlength);
			qt_optionDashLength = real_expression();
			break;
		case QT_SOLID:
			// Not wanted in version 5
			// SETCHECKDUP(set_dash);
			// qt_optionDash = false;
			c_token++;
			break;
		case QT_OTHER:
		default:
			qt_optionWindowId = int_expression();
			qt_option->Widget = "";
			if (set_number) duplication = true;
			set_number = true;
			break;
		}

		if (duplication)
			int_error(c_token-1, "Duplicated or contradicting arguments in qt term options.");
	}

	// We want this to happen immediately, hence the flush command.
	// We don't want it to change the _current_ window, just close an old one.
	if (set_close && qt) {
		qt->out << GECloseWindow << qt_optionWindowId;
		qt_flushOutBuffer();
		qt_optionWindowId = previous_WindowId;
	}

	// Save options back into options string in normalized format
	QString termOptions = QString::number(qt_optionWindowId);

	/* Initialize user-visible font setting */
	fontSettings = qt_option->FontName + "," + QString::number(qt_optionFontSize);

	if (set_title)
	{
		termOptions += " title \"" + qt_option->Title + '"';
	}

	if (set_size)
	{
		termOptions += " size " + QString::number(qt_optionWidth) + ", "
		                        + QString::number(qt_optionHeight);
		qt_setSize   = true;
		qt_setWidth  = qt_optionWidth;
		qt_setHeight = qt_optionHeight;
	}

	if (set_position)
	{
		termOptions += " position " + QString::number(qt_option->position.x()) + ", "
		                            + QString::number(qt_option->position.y());
		qt_setPosition = true;
	}

	if (set_enhanced) termOptions += qt_optionEnhanced ? " enhanced" : " noenhanced";
	                  termOptions += " font \"" + fontSettings + '"';
	if (set_dashlength) termOptions += " dashlength " + QString::number(qt_optionDashLength);
	if (set_widget)   termOptions += " widget \"" + qt_option->Widget + '"';
	if (set_persist)  termOptions += qt_optionPersist ? " persist" : " nopersist";
	if (set_raise)    termOptions += qt_optionRaise ? " raise" : " noraise";
	if (set_ctrl)     termOptions += qt_optionCtrl ? " ctrl" : " noctrl";

	/// @bug change Utf8 to local encoding
	strncpy(term_options, termOptions.toUtf8().data(), MAX_LINE_LEN);
}

void qt_layer( t_termlayer syncpoint )
{
    static int current_plotno = 0;
	if (!qt)
		return;

    /* We must ignore all syncpoints that we don't recognize */
    switch (syncpoint) {
	case TERM_LAYER_BEFORE_PLOT:
		current_plotno++;
		qt->out << GEPlotNumber << current_plotno; break;
	case TERM_LAYER_AFTER_PLOT:
		qt->out << GEAfterPlot; break;
	case TERM_LAYER_RESET_PLOTNO:
		// FIXME: This should handle the case of a multiplot with opaque keys
		// by resetting plotno to that of the 1st plot in the current panel.
		// For the non-multiplot case that's 0, so we can just fall through.
	case TERM_LAYER_RESET:
		if (!multiplot) {
			current_plotno = 0;
			qt->out << GEPlotNumber << 0;
		}
		break;
	case TERM_LAYER_BEGIN_KEYSAMPLE:
		qt->out << GELayer << QTLAYER_BEGIN_KEYSAMPLE; break;
	case TERM_LAYER_END_KEYSAMPLE:
		qt->out << GELayer << QTLAYER_END_KEYSAMPLE; break;
	case TERM_LAYER_BEFORE_ZOOM:
		qt->out << GELayer << QTLAYER_BEFORE_ZOOM; break;
    	default:
		break;
    }
}

void qt_hypertext( int type, const char *text )
{
	if (type == TERM_HYPERTEXT_TOOLTIP)
		qt->out << GEHypertext << qt->codec->toUnicode(text);
}

#ifdef EAM_BOXED_TEXT
void qt_boxed_text(unsigned int x, unsigned int y, int option)
{
	if (option == TEXTBOX_MARGINS)
	    qt->out << GETextBox << QPointF((double)x/(100*qt_oversamplingF), (double)y/(100*qt_oversamplingF)) << option;
	else
	    qt->out << GETextBox << qt_termCoordF(x, y) << option;
}
#endif

void qt_modify_plots(unsigned int ops, int plotno)
{
	if (!qt)
		return;
	if ((ops & MODPLOTS_INVERT_VISIBILITIES) == MODPLOTS_INVERT_VISIBILITIES) {
		qt->out << GEModPlots << QTMODPLOTS_INVERT_VISIBILITIES << plotno;
	} else if (ops & MODPLOTS_SET_VISIBLE) {
		qt->out << GEModPlots << QTMODPLOTS_SET_VISIBLE << plotno;
	} else if (ops & MODPLOTS_SET_INVISIBLE) {
		qt->out << GEModPlots << QTMODPLOTS_SET_INVISIBLE << plotno;
	}
	qt_flushOutBuffer();
}
