/* GNUPLOT - QtGnuplotScene.h */

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

#ifndef QTGNUPLOTSCENE_H
#define QTGNUPLOTSCENE_H

#define EAM_BOXED_TEXT 1

#include "QtGnuplotEvent.h"
#include "QtGnuplotItems.h"

#include <QGraphicsScene>
#include <QGraphicsItemGroup>
#include <QTime>

class QtGnuplotEnhanced;
class QtGnuplotWidget;

class QtGnuplotScene : public QGraphicsScene, public QtGnuplotEventReceiver
{
Q_OBJECT

public:
	QtGnuplotScene(QtGnuplotEventHandler* eventHandler, QObject* parent = 0);
	~QtGnuplotScene();

public:
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void wheelEvent(QGraphicsSceneWheelEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	void processEvent(QtGnuplotEventType type, QDataStream& in);

private:
	void resetItems();
	void updateModifiers();
	void positionText(QGraphicsItem* item, const QPoint& point);
	void setBrushStyle(int style);
	void updateRuler(const QPoint& point);
	void flushCurrentPolygon();
	void flushCurrentPointsItem();
	QPolygonF& clipPolygon(QPolygonF& polygon, bool checkDiag = true) const;
	QPointF&   clipPoint(QPointF& point) const;
	QRectF&    clipRect(QRectF& point) const;
	double sceneToGraph(int axis, double coord) const;
	void update_key_box(const QRectF rect);

private:
	QtGnuplotWidget* m_widget;

	QList <QGraphicsItemGroup*> m_plot_group;

	// State variables
	Qt::Alignment m_textAlignment;
	QPolygonF m_currentPolygon;
	QPen    m_currentPen;
	QBrush  m_currentBrush;
	QFont   m_font;
	QPointF m_currentPosition;
	QPointF m_zoomBoxCorner;
	double  m_currentPointSize;
	double  m_textAngle;
	QPoint  m_textOffset;
	double  m_currentZ;
	QTime   m_watches[4];
	int     m_currentPlotNumber;
	bool    m_inKeySample;
	bool    m_preserve_visibility;
	bool	m_inTextBox;
	QRectF	m_currentTextBox;
	QPointF m_textMargin;
	QList<QGraphicsItem*> m_currentGroup;
	QtGnuplotPoints* m_currentPointsItem;

	// User events data
	QPointF m_lastMousePos;
	int     m_lastModifierMask;

	// Special items
	QGraphicsLineItem* m_horizontalRuler;
	QGraphicsLineItem* m_verticalRuler;
	QGraphicsLineItem* m_lineTo;    // Line from ruler to cursor
	QGraphicsRectItem* m_zoomRect;
	QGraphicsTextItem* m_zoomStartText;
	QGraphicsTextItem* m_zoomStopText;
	QtGnuplotEnhanced* m_enhanced;  // Current enhanced text block
	QList<QtGnuplotKeybox> m_key_boxes;
	QString m_currentHypertext;
	QList<QGraphicsItem*> m_hypertextList;

	// Axis scales
	bool   m_axisValid[4];
	double m_axisMin  [4];
	double m_axisLower[4];
	double m_axisScale[4];
	double m_axisLog  [4];
};

#endif // QTGNUPLOTSCENE_H
