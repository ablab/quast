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

#ifndef QTGNUPLOTINSTANCE_H
#define QTGNUPLOTINSTANCE_H

#include <QtCore>

class QtGnuplotWidget;

class QtGnuplotInstance : public QObject
{
Q_OBJECT

public:
	QtGnuplotInstance(QtGnuplotWidget* widget = 0, QString gnuplotPath = "gnuplot");
	~QtGnuplotInstance();

public:
	/// Redirect the instance plotting to widget @p widget
	void setWidget(QtGnuplotWidget* widget);
	/// Returns the widget to which the instance plots
	QtGnuplotWidget* widget();
	/// Send the given command to gnuplot
	void exec(const QByteArray& command);
	/// Send the given command to gnuplot and return the result string. Timeout after @p msecs milliseconds
	QByteArray execAndRead(const QByteArray& command, int msecs = 30000);

signals:
	/// Emitted when gnuplot sends data through it standards outputs
	void gnuplotOutput(const QString& output);

private slots:
	void gnuplotDataReady();

private:
	QtGnuplotWidget* m_widget;
	QProcess m_gnuplot;
};

/// Overloaded from QtGnuplotInstance::exec
QtGnuplotInstance& operator<<(QtGnuplotInstance& instance, const QString& command);
/// Overloaded from QtGnuplotInstance::exec
QtGnuplotInstance& operator<<(QtGnuplotInstance& instance, const QVector<QPointF>& points);

#endif // QTGNUPLOTINSTANCE_H
