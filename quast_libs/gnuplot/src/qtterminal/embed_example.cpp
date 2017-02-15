/* GNUPLOT - embed_example.h */

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

#include "embed_example.h"
#include "QtGnuplotWidget.h"

QMainWindow* mainWindow;

GnuplotWidget::GnuplotWidget()
	: QWidget()
{
	QGridLayout* gridLayout = new QGridLayout();
	for (int i = 0; i < 4; i++)
	{
		widgets[i] = new QtGnuplotWidget();
		connect(widgets[i], SIGNAL(statusTextChanged(const QString&)), this, SLOT(statusText(const QString&)));
		widgets[i]->setFixedSize(400,250);
		gridLayout->addWidget(widgets[i], i/2, i%2);
	}

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addLayout(gridLayout);

	outputFrame = new QPlainTextEdit(this);
	outputFrame->setReadOnly(true);
/*	QFont font = outputFrame->document().defaultFont();
	font.setFamiliy("Mono");
	outputFrame->document().setDefaultFont(font);*/
	layout->addWidget(outputFrame);

	setLayout(layout);
	connect(&gp, SIGNAL(gnuplotOutput(const QString&)), this, SLOT(gnuplotOutput(const QString&)));
}

void GnuplotWidget::plot()
{
	gp.setWidget(widgets[0]);
	gp << "plot x w l lt 3\n";
	gp << "print pi\n";

	gp.setWidget(widgets[1]);
	gp << "set grid; plot x**2 w l lt 2 lw 4\n";

	gp.setWidget(widgets[2]);
	QVector<QPointF> points;
	srand(time(NULL));
	for (int i = 0; i < 100; i++)
		points << QPointF(double(rand())/double(RAND_MAX), double(rand())/double(RAND_MAX));
	gp << "plot '-'\n";
	gp << points;

	gp.setWidget(widgets[3]);
	gp << "unset grid\n";
	phi = 0.;
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
	timer.start(1000);
}

void GnuplotWidget::tick()
{
	gp << "plot sin(x + " + QString::number(phi) + ")\n";
	gp << "print pi, " + QString::number(phi) + "\n";
	phi += 0.3;
}

void GnuplotWidget::statusText(const QString& status)
{
	mainWindow->statusBar()->showMessage(status);
}

void GnuplotWidget::gnuplotOutput(const QString& output)
{
	outputFrame->appendPlainText(output);
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	mainWindow = new QMainWindow;
	GnuplotWidget* widget = new GnuplotWidget();
	mainWindow->setCentralWidget(widget);
	mainWindow->statusBar()->showMessage("Qt Gnuplot widgets embedding example");
	mainWindow->show();
	widget->plot();
	app.exec();

	return 0;
}
