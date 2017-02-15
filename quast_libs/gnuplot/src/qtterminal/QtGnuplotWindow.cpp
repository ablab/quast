/* GNUPLOT - QtGnuplotWindow.cpp */

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

#include "QtGnuplotWindow.h"
#include "QtGnuplotWidget.h"
#include "QtGnuplotEvent.h"

extern "C" {
#include "../mousecmn.h"
}

#include <QtGui>

QtGnuplotWindow::QtGnuplotWindow(int id, QtGnuplotEventHandler* eventHandler, QWidget* parent)
	: QMainWindow(parent)
{
	m_ctrl = false;
	m_eventHandler = eventHandler;
	m_id = id;
	setWindowIcon(QIcon(":/images/gnuplot"));

//	Setting this attribute causes an error to be reported to the user if a plot
//	command is received after a plot command is closed.  Is this good or bad?
//		setAttribute(Qt::WA_DeleteOnClose);

	// Register as the main event receiver if not already created
	if (m_eventHandler == 0)
		m_eventHandler = new QtGnuplotEventHandler(this,
		                 "qtgnuplot" + QString::number(QCoreApplication::applicationPid()));

	// Central widget
	m_widget = new QtGnuplotWidget(m_id, m_eventHandler, this);
	connect(m_widget, SIGNAL(statusTextChanged(const QString&)), this, SLOT(on_setStatusText(const QString&)));
	setCentralWidget(m_widget);

	// Bars
	m_toolBar = addToolBar("Main tool bar");

	m_mouseToolBar = addToolBar("Mouse tool bar");
	m_mouseToolBarLabel = new QLabel();
	m_mouseToolBar->addWidget(m_mouseToolBarLabel);

	m_statusBar = statusBar();

	// Actions
	QAction* copyToClipboardAction = new QAction(QIcon(":/images/clipboard"   ), tr("Copy to clipboard"), this);
	QAction* printAction           = new QAction(QIcon(":/images/print"       ), tr("Print"            ), this);
	QAction* exportAction          = new QAction(QIcon(":/images/export"      ), tr("Export"           ), this);
	QAction* exportPdfAction       = new QAction(QIcon(":/images/exportPDF"   ), tr("Export to PDF"    ), this);
	QAction* exportEpsAction       = new QAction(QIcon(":/images/exportVector"), tr("Export to EPS"    ), this);
	QAction* exportSvgAction       = new QAction(QIcon(":/images/exportVector"), tr("Export to SVG"    ), this);
	QAction* exportPngAction       = new QAction(QIcon(":/images/exportRaster"), tr("Export to image"  ), this);
	QAction* settingsAction        = new QAction(QIcon(":/images/settings"    ), tr("Settings"         ), this);
	connect(copyToClipboardAction, SIGNAL(triggered()), m_widget, SLOT(copyToClipboard()));
	connect(printAction,           SIGNAL(triggered()), this, SLOT(print()));
	connect(exportPdfAction,       SIGNAL(triggered()), this, SLOT(exportToPdf()));
	connect(exportEpsAction,       SIGNAL(triggered()), m_widget, SLOT(exportToEps()));
	connect(exportSvgAction,       SIGNAL(triggered()), this, SLOT(exportToSvg()));
	connect(exportPngAction,       SIGNAL(triggered()), this, SLOT(exportToImage()));
	connect(settingsAction,        SIGNAL(triggered()), this, SLOT(showSettingsDialog()));
	QMenu* exportMenu = new QMenu(this);
	exportMenu->addAction(copyToClipboardAction);
	exportMenu->addAction(printAction);
	exportMenu->addAction(exportPdfAction);
//	exportMenu->addAction(exportEpsAction);
	exportMenu->addAction(exportSvgAction);
	exportMenu->addAction(exportPngAction);
	exportAction->setMenu(exportMenu);
	m_toolBar->addAction(exportAction);
	createAction(tr("Replot")       , 'e', ":/images/replot");
	createAction(tr("Show grid")    , 'g', ":/images/grid");
	createAction(tr("Previous zoom"), 'p', ":/images/zoomPrevious");
	createAction(tr("Next zoom")    , 'n', ":/images/zoomNext");
	createAction(tr("Autoscale")    , 'a', ":/images/autoscale");
	m_toolBar->addAction(settingsAction);

	loadSettings();
}

QtGnuplotWindow::~QtGnuplotWindow()
{
	saveSettings();
}

void QtGnuplotWindow::createAction(const QString& name, int key, const QString& icon)
{
	QAction* action = new QAction(QIcon(icon), name, this);
	connect(action, SIGNAL(triggered()), this, SLOT(on_keyAction()));
	action->setData(key);
	m_toolBar->addAction(action);
}

void QtGnuplotWindow::on_setStatusText(const QString& status)
{
	if (m_mouseToolBar->toggleViewAction()->isChecked())
		m_mouseToolBarLabel->setText(status);
	if (m_statusBar->isVisible())
		m_statusBar->showMessage(status);
}

void QtGnuplotWindow::on_keyAction()
{
	QAction* action = qobject_cast<QAction *>(sender());
	m_eventHandler->postTermEvent(GE_keypress, 0, 0, action->data().toInt(), 0, m_widget);
}

void QtGnuplotWindow::print()
{
	QPrinter printer;
	if (QPrintDialog(&printer).exec() == QDialog::Accepted)
		m_widget->print(printer);
}

void QtGnuplotWindow::exportToPdf()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export to PDF"), "", tr("PDF files (*.pdf)"));
	if (fileName.isEmpty())
		return;
	if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
		fileName += ".pdf";

	m_widget->exportToPdf(fileName);
}

void QtGnuplotWindow::exportToImage()
{
	/// @todo other image formats supported by Qt
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export to Image"), "",
	                       tr("Image files (*.png *.bmp)"));
	if (fileName.isEmpty())
		return;
	if (!fileName.endsWith(".png", Qt::CaseInsensitive) &&
	    !fileName.endsWith(".bmp", Qt::CaseInsensitive))
		fileName += ".png";

	m_widget->exportToImage(fileName);
}

void QtGnuplotWindow::exportToSvg()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export to SVG"), "", tr("SVG files (*.svg)"));
	if (fileName.isEmpty())
		return;
	if (!fileName.endsWith(".svg", Qt::CaseInsensitive))
		fileName += ".svg";

	m_widget->exportToSvg(fileName);
}

#include "ui_QtGnuplotSettings.h"

void QtGnuplotWindow::loadSettings()
{
	QSettings settings("gnuplot", "qtterminal");
	settings.beginGroup("view");
	m_widget->loadSettings(settings);
	m_statusBarActive = settings.value("statusBarActive", true).toBool();
	m_statusBar->setVisible(m_statusBarActive);
	bool mouseToolBarActive = settings.value("mouseToolBarActive", false).toBool();
	m_mouseToolBar->toggleViewAction()->setChecked(mouseToolBarActive);
	m_mouseToolBar->setVisible(mouseToolBarActive);
}

void QtGnuplotWindow::saveSettings() const
{
	QSettings settings("gnuplot", "qtterminal");
	settings.beginGroup("view");
	m_widget->saveSettings(settings);
	settings.setValue("statusBarActive", m_statusBarActive);
	settings.setValue("mouseToolBarActive", m_mouseToolBar->toggleViewAction()->isChecked());
}

void QtGnuplotWindow::showSettingsDialog()
{
	QDialog* settingsDialog = new QDialog(this);
	m_ui = new Ui_settingsDialog();
	m_ui->setupUi(settingsDialog);
	m_ui->antialiasCheckBox->setCheckState(m_widget->antialias() ? Qt::Checked : Qt::Unchecked);
	m_ui->roundedCheckBox->setCheckState(m_widget->rounded() ? Qt::Checked : Qt::Unchecked);
	m_ui->replotOnResizeCheckBox->setCheckState(m_widget->replotOnResize() ? Qt::Checked : Qt::Unchecked);
	if (m_statusBar->isVisible())
		m_ui->mouseLabelComboBox->setCurrentIndex(0);
	else if (m_mouseToolBar->toggleViewAction()->isChecked())
		m_ui->mouseLabelComboBox->setCurrentIndex(1);
	else if (m_widget->statusLabelActive())
		m_ui->mouseLabelComboBox->setCurrentIndex(2);
	else
		m_ui->mouseLabelComboBox->setCurrentIndex(3);
	QPixmap samplePixmap(m_ui->sampleColorLabel->size());
	samplePixmap.fill(m_widget->backgroundColor());
	m_ui->sampleColorLabel->setPixmap(samplePixmap);
	m_chosenBackgroundColor = m_widget->backgroundColor();
	connect(m_ui->backgroundButton, SIGNAL(clicked()), this, SLOT(settingsSelectBackgroundColor()));
	settingsDialog->exec();

	if (settingsDialog->result() == QDialog::Accepted)
	{
		m_widget->setBackgroundColor(m_chosenBackgroundColor);
		m_widget->setAntialias(m_ui->antialiasCheckBox->checkState() == Qt::Checked);
		m_widget->setRounded(m_ui->roundedCheckBox->checkState() == Qt::Checked);
		m_widget->setReplotOnResize(m_ui->replotOnResizeCheckBox->checkState() == Qt::Checked);
		int statusIndex = m_ui->mouseLabelComboBox->currentIndex();
		m_statusBarActive = (statusIndex == 0);
		m_statusBar->setVisible(m_statusBarActive);
		m_mouseToolBar->toggleViewAction()->setChecked(statusIndex == 1);
		m_mouseToolBar->setVisible(statusIndex == 1);
		m_widget->setStatusLabelActive(statusIndex == 2);
		saveSettings();
	}
}

void QtGnuplotWindow::settingsSelectBackgroundColor()
{
	m_chosenBackgroundColor = QColorDialog::getColor(m_chosenBackgroundColor, this);
	QPixmap samplePixmap(m_ui->sampleColorLabel->size());
	samplePixmap.fill(m_chosenBackgroundColor);
	m_ui->sampleColorLabel->setPixmap(samplePixmap);
}

void QtGnuplotWindow::closeEvent(QCloseEvent *event)
{
	m_eventHandler->postTermEvent(GE_reset, 0, 0, 0, 0, m_widget);
	event->accept();
}

void QtGnuplotWindow::processEvent(QtGnuplotEventType type, QDataStream& in)
{
	if (type == GETitle)
	{
		QString title;
		in >> title;
		if (title.isEmpty())
			title = tr("Gnuplot window ") + QString::number(m_id);
		setWindowTitle(title);
	}
	else if (type == GERaise)
		raise();
	else if (type == GESetCtrl)
		in >> m_ctrl;
	else if (type == GESetPosition)
	{
		QPoint pos;
		in >> pos;
		move(pos);
	}
	else
		m_widget->processEvent(type, in);
}

void QtGnuplotWindow::keyPressEvent(QKeyEvent* event)
{
	if ((event->key() == 'Q') && ( !m_ctrl || (QApplication::keyboardModifiers() & Qt::ControlModifier) ))
		close();

	QMainWindow::keyPressEvent(event);
}
