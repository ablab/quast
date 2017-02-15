/* GNUPLOT - QtGnuplotEvent.h */

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

#ifndef QTGNUPLOTEVENT_H
#define QTGNUPLOTEVENT_H

#include <QObject>

// Defines events used to communicate from qt_term.cpp to the GUI elements

enum QtGnuplotEventType {
// Events for QtGnuplotApplication
GESetCurrentWindow = 1000, GEInitWindow, GECloseWindow, GEExit, GEPersist,
// Events for QtGnuplotWindow
GEStatusText, GETitle, GESetCtrl, GESetPosition,
// Events for QtGnuplotWidget
GESetWidgetSize, GECursor,
// Events for QtGnuplotScene
GEPenColor, GEBackgroundColor, GEBrushStyle, GEPenStyle, GEPointSize, GELineWidth,
GEFillBox, GEPutText, GEFilledPolygon, GETextAngle, GETextAlignment, GEPoint, GEClear,
GEZoomStart, GEZoomStop, GERuler, GECopyClipboard, GEMove, GEVector, GELineTo,
GESetFont, GEEnhancedFlush, GEEnhancedFinish, GEImage, GESetSceneSize, GERaise,
GEWrapCursor, GEScale, GEActivate, GEDesactivate, GELayer, GEPlotNumber, GEHypertext,
GETextBox, GEModPlots, GEAfterPlot, GEFontMetricRequest, GEDashPattern,
GEDone
};

enum QtGnuplotModPlots {
	QTMODPLOTS_SET_VISIBLE,
	QTMODPLOTS_SET_INVISIBLE,
	QTMODPLOTS_INVERT_VISIBILITIES
};

enum QtGnuplotLayer {
QTLAYER_BEGIN_KEYSAMPLE, QTLAYER_END_KEYSAMPLE, QTLAYER_BEFORE_ZOOM
};

class QLocalServer;
class QLocalSocket;
class QtGnuplotEventHandler;
class QtGnuplotWidget;

/**
* A GUI object that wishes to receive gnuplot events has to inherit from
* this class and implement the processEvent function. The object is
* registered as the main receiver upon creation of a QtGnuplotEventHandler object
* For a given communication channel, only one QtGnuplotEventHandler should
* be created, hence only one main receiver should be registered.
*/
class QtGnuplotEventReceiver
{
public:
	virtual void processEvent(QtGnuplotEventType type, QDataStream& in) = 0;
	void swallowEvent(QtGnuplotEventType type, QDataStream& in);
	QString serverName();
	virtual ~QtGnuplotEventReceiver() {}

protected:
	QtGnuplotEventHandler* m_eventHandler;
};

/**
* The QtGnuplotEventHandler is responsible for passing message between
* Gnuplot and the GUI. It is shared between GUI objects. Its parent is
* the main GUI object that processes events and distribute them to other objects.
* Events are passed through a QLocalSocket (a crossplatform pipe-like IPC mechanism)
* between Gnuplot core and the event handler.
*/
class QtGnuplotEventHandler : public QObject
{
Q_OBJECT

public:
	QtGnuplotEventHandler(QObject* parent, const QString& socket);

public:
	/// Send an event from the GUI elements to gnuplot core
	bool postTermEvent(int type, int mx, int my, int par1, int par2, QtGnuplotWidget* widget);
	QString serverName();

signals:
	void connected();
	void disconnected();

private slots:
	void newConnection();
	void readEvent();
	void connectionClosed();

private:
	void init(const QString& inSocket);

private:
	QLocalServer* m_server;
	QLocalSocket* m_socket;
	quint32       m_blockSize;
};

#endif // QTGNUPLOTEVENT_H
