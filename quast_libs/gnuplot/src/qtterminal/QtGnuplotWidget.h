/* GNUPLOT - QtGnuplotWidget.h */

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

#ifndef QTGNUPLOTWIDGET_H
#define QTGNUPLOTWIDGET_H

#include "QtGnuplotEvent.h"

#include <QWidget>
#include <QPainter>

/* I had to add these in order to link against qt5 rather than qt4 */
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#endif

class QtGnuplotScene;
class QGraphicsView;
class QSettings;
class QLabel;

class QtGnuplotWidget : public QWidget, public QtGnuplotEventReceiver
{
Q_OBJECT

public:
	QtGnuplotWidget(QWidget* parent);
	QtGnuplotWidget(int id = 0, QtGnuplotEventHandler* eventHandler = 0, QWidget* parent = 0);

	Q_PROPERTY(bool antialias READ antialias WRITE setAntialias);
	Q_PROPERTY(bool rounded READ rounded WRITE setRounded);
	Q_PROPERTY(bool replotOnResize READ replotOnResize WRITE setReplotOnResize);
	Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor);
	Q_PROPERTY(bool statusLabelActive READ statusLabelActive WRITE setStatusLabelActive);

public:
	bool isActive() const;
	void setStatusText(const QString& status);
	QSize plotAreaSize() const;
	virtual QSize sizeHint() const;

signals:
	void plotDone();
	void statusTextChanged(const QString& status);

public:
	void processEvent(QtGnuplotEventType type, QDataStream& in);

	bool antialias() const { return m_antialias; }
	bool rounded() const { return m_rounded; }
	bool replotOnResize() const { return m_replotOnResize; }
	const QColor& backgroundColor() const { return m_backgroundColor; }
	bool statusLabelActive() const { return m_statusLabelActive; }

	void setAntialias(bool value);
	void setRounded(bool value);
	void setReplotOnResize(bool value);
	void setBackgroundColor(const QColor& color);
	void setStatusLabelActive(bool active);

	void loadSettings(const QSettings& settings);
	void saveSettings(QSettings& settings) const;

public slots:
	void copyToClipboard();
	void print(QPrinter& printer);
	void exportToPdf(const QString& fileName);
	void exportToEps();
	void exportToImage(const QString& fileName);
	void exportToSvg(const QString& fileName);

// Qt functions
protected:
	virtual void resizeEvent(QResizeEvent* event);

private:
	void init();
	void setViewMatrix();
	QPixmap createPixmap();
	QPainter::RenderHints renderHints() const;

private:
	int m_id;
	bool m_active;
	QtGnuplotScene* m_scene;
	QGraphicsView* m_view;
	QLabel* m_statusLabel;
	QSize m_lastSizeRequest;
	QSize m_sizeHint;
	// these can be set from the tool widget or from the command line
	bool m_rounded;
	QColor m_backgroundColor;
	// Settings
	bool m_antialias;
	bool m_replotOnResize;
	bool m_statusLabelActive;

	static int m_widgetUid;
	bool m_skipResize;
};

#endif // QTGNUPLOTWIDGET_H
