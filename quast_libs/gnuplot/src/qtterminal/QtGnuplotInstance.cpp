/* GNUPLOT - QtGnuplotInstance.h */

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

#include "QtGnuplotInstance.h"
#include "QtGnuplotWidget.h"

#include <QtCore>
#include <iostream>

QtGnuplotInstance::QtGnuplotInstance(QtGnuplotWidget* widget, QString gnuplotPath)
{
	m_gnuplot.setProcessChannelMode(QProcess::MergedChannels);
	m_gnuplot.start(gnuplotPath);
	m_gnuplot.waitForStarted();
	connect(&m_gnuplot, SIGNAL(readyReadStandardOutput()), this, SLOT(gnuplotDataReady()));

	if (m_gnuplot.state() == QProcess::NotRunning)
		qDebug() << "Error starting gnuplot" << m_gnuplot.error();

	setWidget(widget);
}

QtGnuplotInstance::~QtGnuplotInstance()
{
	m_gnuplot.close();
}

void QtGnuplotInstance::setWidget(QtGnuplotWidget* widget)
{
	m_widget = widget;

	if (m_widget)
	{
		QByteArray command;
		command.append("set term qt widget \"" + m_widget->serverName() + "\" size " +
		               QString::number(m_widget->plotAreaSize().width()) + "," +
		               QString::number(m_widget->plotAreaSize().height()) + "\n");
		exec(command);
	}
}

QtGnuplotWidget* QtGnuplotInstance::widget()
{
	return m_widget;
}

void QtGnuplotInstance::gnuplotDataReady()
{
	QByteArray result = m_gnuplot.readAllStandardOutput();
	emit(gnuplotOutput(result.constData()));
}

void QtGnuplotInstance::exec(const QByteArray& command)
{
	if (m_gnuplot.state() == QProcess::Running)
		m_gnuplot.write(command);
	else
		qDebug() << "Not running";
}

QByteArray QtGnuplotInstance::execAndRead(const QByteArray& command, int msecs)
{
	m_gnuplot.waitForReadyRead(0);
	QByteArray trailing;
	do
	{
		trailing = m_gnuplot.readAllStandardOutput();
		emit(gnuplotOutput(trailing.constData()));
	} while (!trailing.isEmpty());

	m_gnuplot.disconnect(SIGNAL(readyReadStandardOutput()));
	exec(command);
	m_gnuplot.waitForReadyRead(msecs);
	QByteArray answer = m_gnuplot.readAllStandardOutput();
	connect(&m_gnuplot, SIGNAL(readyReadStandardOutput()), this, SLOT(gnuplotDataReady()));
	return answer;
}

QtGnuplotInstance& operator<<(QtGnuplotInstance& instance, const QString& command)
{
	QByteArray array;
	array.append(command);
	instance.exec(array);

	return instance;
}

QtGnuplotInstance& operator<<(QtGnuplotInstance& instance, const QVector<QPointF>& points)
{
	QByteArray array;

	foreach (QPointF point, points)
		array += QByteArray::number(point.x()) + " " + QByteArray::number(point.y()) + "\n";

	array += "e\n";
	instance.exec(array);

	return instance;
}
