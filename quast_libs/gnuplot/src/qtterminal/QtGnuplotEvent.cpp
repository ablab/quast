/* GNUPLOT - QtGnuplotEvent.cpp */

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

#include "QtGnuplotEvent.h"
#include "QtGnuplotWidget.h"

extern "C" {
#include "../mousecmn.h"
}

#include <QtNetwork>

QString QtGnuplotEventReceiver::serverName()
{
	if (m_eventHandler)
		return m_eventHandler->serverName();

	return QString();
}

QtGnuplotEventHandler::QtGnuplotEventHandler(QObject* parent, const QString& socket)
	: QObject(parent)
{
	m_blockSize = 0;
	m_socket = 0;
	m_server = new QLocalServer(this);
	if (!m_server->listen(socket))
		qDebug() << "QtGnuplotApplication error : cannot open server";

	connect(m_server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

QString QtGnuplotEventHandler::serverName()
{
	return m_server->serverName();
}

void QtGnuplotEventHandler::newConnection()
{
	m_socket = m_server->nextPendingConnection();
	connect(m_socket, SIGNAL(readyRead()), this, SLOT(readEvent()));
	connect(m_socket, SIGNAL(disconnected()), this, SLOT(connectionClosed()));
	emit(connected());
}

void QtGnuplotEventHandler::connectionClosed()
{
	emit(disconnected());
}

void QtGnuplotEventHandler::readEvent()
{
	QDataStream in(m_socket);
	in.setVersion(QDataStream::Qt_4_4);

	QtGnuplotEventReceiver* receiver = dynamic_cast<QtGnuplotEventReceiver*>(parent());
	if (!receiver)
	{
		qDebug() << "QtGnuplotEventHandler::readEvent -- No receiver !";
		return;
	}

	while(!in.atEnd())
	{
		// Extract the message length
		if (m_blockSize == 0)
		{
			if (m_socket->bytesAvailable() < (int)sizeof(qint32))
				return;
			in >> m_blockSize;
		}

		// Break if the message is not entirely received yet
		if (m_socket->bytesAvailable() < m_blockSize)
			return;

		// Process the message
		int remaining = m_socket->bytesAvailable() - m_blockSize;
		while (m_socket->bytesAvailable() > remaining)
		{
			int type;
			in >> type;
			if ((type < 1000) || (type > GEDone))
			{
			// FIXME EAM - At this point the program cannot recover and
			// if we try to continue it will eventually become a zombie.
			// Better to just exit explicitly right now.
				qDebug() << "qt_gnuplot exiting on read error";
				exit(0);
				// return;
			}
			receiver->processEvent(QtGnuplotEventType(type), in);
		}
		m_blockSize = 0;
	}
}

bool QtGnuplotEventHandler::postTermEvent(int type, int mx, int my, int par1, int par2, QtGnuplotWidget* widget)
{
	if ((m_socket == 0) || (m_socket->state() != QLocalSocket::ConnectedState))
		return false;
		
	if (widget && !widget->isActive()) {
		if (type == GE_buttonrelease) {
			// qDebug() << "Rescued buttonrelease event with widget inactive";
		} else {
			// qDebug() << "Event lost because widget is not active";
			return false;
		}
	}
		
	gp_event_t event;
	event.type = type;
	event.mx = mx;
	event.my = my;
	event.par1 = par1;
	event.par2 = par2;
	event.winid = 0; // We don't forward any window id to gnuplot
	m_socket->write((char*) &event, sizeof(gp_event_t));

	return true;
}

// Catch events for which the receiver is not active
void QtGnuplotEventReceiver::swallowEvent(QtGnuplotEventType type, QDataStream& in)
{
	QPoint point;
	QString string;
	int i;
	bool b;

	     if (type == GESetCurrentWindow) in >> i;        // 1000
	else if (type == GEInitWindow)       ;               // 1001
	else if (type == GECloseWindow)      in >> i;        // 1002
	else if (type == GEExit)             ;               // 1003
	else if (type == GEPersist)          ;               // 1004
	else if (type == GEStatusText)       in >> string;   // 1005
	else if (type == GETitle)            in >> string;   // 1006
	else if (type == GESetCtrl)          in >> b;        // 1007
	else if (type == GECursor)           in >> i;        // 1009
	else if (type == GEZoomStart)        in >> string;   // 1022
	else if (type == GEZoomStop)         in >> string;   // 1023
	else if (type == GERaise)            ;               // 1034
	else if (type == GEDesactivate)      ;               // 1038
	else if (type == GESetPosition)      in >> point;
	else qDebug() << "Event not swallowed !" << type;
}
