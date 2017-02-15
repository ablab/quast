/* GNUPLOT - QtGnuplotApplication.cpp */

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

#include "QtGnuplotApplication.h"
#include "QtGnuplotWindow.h"
#include "QtGnuplotEvent.h"

#include <QDebug>

QtGnuplotApplication::QtGnuplotApplication(int& argc, char** argv)
	: QApplication(argc, argv)
{
	setQuitOnLastWindowClosed(false);
	setWindowIcon(QIcon(":/images/gnuplot"));
	m_currentWindow = NULL;
	m_lastId = 0;
	m_eventHandler = new QtGnuplotEventHandler(this, "qtgnuplot" + QString::number(applicationPid()));
	connect(m_eventHandler, SIGNAL(connected()), this, SLOT(exitPersistMode()));
	connect(m_eventHandler, SIGNAL(disconnected()), this, SLOT(enterPersistMode()));
}

QtGnuplotApplication::~QtGnuplotApplication()
{
}

void QtGnuplotApplication::windowDestroyed(QObject* object)
{
	// A window has been closed. Unregister it.
	int id = m_windows.key((QtGnuplotWindow*)(object));
	if (m_windows.take(id) == m_currentWindow)
		m_currentWindow = 0;
}

void QtGnuplotApplication::enterPersistMode()
{
	setQuitOnLastWindowClosed(true);
	// But if the plot window was already closed, this is our last chance to exit
	if (m_windows.isEmpty())
		quit();
	// Some programs executing gnuplot -persist may be waiting for all default
	// handles to be closed before they consider the sub-process finished.
	// Using freopen() ensures that debug fprintf()s won't crash.
	freopen("/dev/null","w",stdout);
	freopen("/dev/null","w",stderr);
}

void QtGnuplotApplication::exitPersistMode()
{
	setQuitOnLastWindowClosed(false);
}

void QtGnuplotApplication::processEvent(QtGnuplotEventType type, QDataStream& in)
{
	if (type == GESetCurrentWindow) // Select window
	{
		in >> m_lastId;
		m_currentWindow = m_windows[m_lastId];
	}
	else if ((type == GEInitWindow) && (!m_currentWindow)) // Create the window if necessary
	{
		m_currentWindow = new QtGnuplotWindow(m_lastId, m_eventHandler);
		connect(m_currentWindow, SIGNAL(destroyed(QObject*)), this, SLOT(windowDestroyed(QObject*)));
		m_windows.insert(m_lastId, m_currentWindow);
	}
	else if (type == GECloseWindow)
	{
		int id;
		in >> id;
		QtGnuplotWindow* closeWindow = m_windows.take(id);
		if (closeWindow)
			closeWindow->close();
	}
	else if (type == GEExit)
		quit();
	else if (type == GEPersist)
		enterPersistMode();
	else if (m_currentWindow) // Dispatch gnuplot events to widgets
		m_currentWindow->processEvent(type, in);
	else
		swallowEvent(type, in);
}
