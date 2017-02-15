/* GNUPLOT - QtGnuplotItems.cpp */

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

#include "QtGnuplotItems.h"

#include <QtGui>

/////////////////////////////
// QtGnuplotEnhanced

void QtGnuplotEnhanced::addText(const QString& fontName, double fontSize, 
				QFont::Style fontStyle, QFont::Weight fontWeight,
				double base, bool widthFlag,
                                bool showFlag, int overprint, const QString& text, QColor color)
{
	if ((overprint == 1) && !(m_overprintMark)) // Underprint
	{
		m_overprintPos = m_currentPos.x();
		m_overprintMark = true;
	}
	if (overprint == 3)                         // Save position
		m_savedPos = m_currentPos;
	else if (overprint == 4)                    // Recall saved position
		m_currentPos = m_savedPos;

	QFont font(fontName, fontSize);
	if (fontName.isEmpty()) {
		// qDebug() << "Empty font name";
		font.setFamily("Sans");		// FIXME: use default? use previous?
	}
	font.setStyle(fontStyle);
	font.setWeight(fontWeight);
	font.setStyleStrategy(QFont::ForceOutline);	// pcf fonts die if rotated
	QtGnuplotEnhancedFragment* item = new QtGnuplotEnhancedFragment(font, text, this);
	item->setPos(m_currentPos + QPointF(0., -base));
	if (showFlag)
		item->setPen(color);
	else
		item->setPen(Qt::NoPen);

	if (overprint == 2)                         // Overprint
	{
		item->setPos(QPointF((m_overprintPos + m_currentPos.x())/2. - item->width()/2., -base));
		m_overprintMark = false;
	}

	if (widthFlag && (overprint != 2))
		m_currentPos += QPointF(item->width(), 0.);
}

void QtGnuplotEnhanced::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(painter);
	Q_UNUSED(option);
	Q_UNUSED(widget);
}

QtGnuplotEnhancedFragment::QtGnuplotEnhancedFragment(const QFont& font, const QString& text, QGraphicsItem * parent)
	: QAbstractGraphicsShapeItem(parent)
{
	m_font = font;
	m_text = text;
}

QRectF QtGnuplotEnhancedFragment::boundingRect() const
{
	QFontMetricsF metrics(m_font);
	return metrics.boundingRect(m_text);
}

qreal QtGnuplotEnhancedFragment::width() const
{
	QFontMetricsF metrics(m_font);
	return metrics.width(m_text);
}

void QtGnuplotEnhancedFragment::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	painter->setPen(pen());
	painter->setFont(m_font);
	painter->drawText(QPointF(0.,0.), m_text);
}

/////////////////////////////
// QtGnuplotPoint

QtGnuplotPoint::QtGnuplotPoint(int style, double size, QPen pen, QGraphicsItem * parent)
	: QGraphicsItem(parent)
{
	m_pen   = pen;
	m_color = pen.color();
	m_style = style;
	m_size = 3.*size;
}

QRectF QtGnuplotPoint::boundingRect() const
{
	return QRectF(QPointF(-m_size, -m_size), QPointF(m_size, m_size));
}

void QtGnuplotPoint::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	const int style = m_style % 15;

	if ((style % 2 == 0) && (style > 3)) // Filled points
	{
		painter->setPen(m_color);
		painter->setBrush(m_color);
	}
	else
		painter->setPen(m_pen);

	drawPoint(painter, QPointF(0., 0.), m_size, style);
}

void QtGnuplotPoint::drawPoint(QPainter* painter, const QPointF& origin, double size, int style)
{
	if (style == -1) // dot
	{
		painter->drawPoint(origin);
		return;
	}

	if ((style == 0) || (style == 2)) // plus or star
	{
		painter->drawLine(origin + QPointF(0., -size), origin + QPointF(0., size));
		painter->drawLine(origin + QPointF(-size, 0.), origin + QPointF(size, 0.));
	}
	if ((style == 1) || (style == 2)) // cross or star
	{
		painter->drawLine(origin + QPointF(-size, -size), origin + QPointF(size, size));
		painter->drawLine(origin + QPointF(-size, size), origin + QPointF(size, -size));
	}
	else if ((style == 3) || (style == 4)) // box
		painter->drawRect(QRectF(origin + QPointF(-size, -size), origin + QPointF(size, size)));
	else if ((style == 5) || (style == 6)) // circle
		painter->drawEllipse(QRectF(origin + QPointF(-size, -size), origin + QPointF(size, size)));
	else if ((style == 7) || (style == 8)) // triangle
	{
		const QPointF p[3] = { origin + QPointF(0., -size),
							origin + QPointF(.866*size, .5*size),
							origin + QPointF(-.866*size, .5*size)};
		painter->drawPolygon(p, 3);
	}
	else if ((style == 9) || (style == 10)) // upside down triangle
	{
		const QPointF p[3] = { origin + QPointF(0., size),
							origin + QPointF(.866*size, -.5*size),
							origin + QPointF(-.866*size, -.5*size)};
		painter->drawPolygon(p, 3);
	}
	else if ((style == 11) || (style == 12)) // diamond
	{
		const QPointF p[4] = { origin + QPointF(0., size),
							origin + QPointF(size, 0.),
							origin + QPointF(0., -size),
							origin + QPointF(-size, 0.)};
		painter->drawPolygon(p, 4);
	}
	else if ((style == 13) || (style == 14)) // pentagon
	{
		const QPointF p[5] = { origin + QPointF(0., size),
						origin + QPointF( size*0.9511,  size*0.3090),
						origin + QPointF( size*0.5878, -size*0.8090),
						origin + QPointF(-size*0.5878, -size*0.8090),
						origin + QPointF(-size*0.9511,  size*0.3090)};
		painter->drawPolygon(p, 5);
	}
}

/////////////////////////////
// QtGnuplotPoints

QtGnuplotPoints::QtGnuplotPoints(QGraphicsItem * parent)
	: QGraphicsItem(parent)
{
	m_currentZ = 0;
}

QRectF QtGnuplotPoints::boundingRect() const
{
	return m_boundingRect;
}

void QtGnuplotPoints::addPoint(const QPointF& point, int style, double pointSize, const QPen& pen)
{
	QtGnuplotPoints_PointData pointData;
	pointData.point = point;
	pointData.style = style;
	pointData.pointSize = 3*pointSize;
	pointData.pen = pen;
	pointData.z = m_currentZ++;
	m_points << pointData;

	const double size = pointData.pointSize;
	const QPointF& origin = pointData.point;
	m_boundingRect = m_boundingRect.united(QRectF(origin + QPointF(-size, -size), origin + QPointF(size, size)));
}

void QtGnuplotPoints::addPolygon(const QPolygonF& polygon, const QPen& pen)
{
	QtGnuplotPoints_PolygonData polygonData;
	polygonData.polygon = polygon;
	polygonData.pen = pen;
	polygonData.z = m_currentZ++;
	m_polygons << polygonData;
	m_boundingRect = m_boundingRect.united(polygon.boundingRect());
}

void QtGnuplotPoints::addFilledPolygon(const QPolygonF& polygon, const QBrush& brush)
{
	QtGnuplotPoints_FilledPolygonData filledPolygonData;
	filledPolygonData.polygon = polygon;
	filledPolygonData.brush = brush;
	filledPolygonData.z = m_currentZ++;
	m_filledPolygons << filledPolygonData;
	m_boundingRect = m_boundingRect.united(polygon.boundingRect());
}

bool QtGnuplotPoints::isEmpty() const
{
	return m_points.isEmpty() && m_polygons.isEmpty() && m_filledPolygons.isEmpty();
}

void QtGnuplotPoints::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	painter->setBrush(Qt::NoBrush);

	int i = 0;
	int j = 0;
	int k = 0;
	unsigned int z = 0;

	while ((i < m_points.size()) || (j < m_polygons.size()) || (k < m_filledPolygons.size()))
	{
		for (; i < m_points.size() && m_points[i].z == z; i++, z++)
		{
			const int style = m_points[i].style % 15;

			painter->setPen(m_points[i].pen.color());
			if ((style % 2 == 0) && (style > 3)) // Filled points
				painter->setBrush(m_points[i].pen.color());
			else
			{
				painter->setPen(m_points[i].pen);
				painter->setBrush(Qt::NoBrush);
			}

			QtGnuplotPoint::drawPoint(painter, m_points[i].point, m_points[i].pointSize, style);
		}

		painter->setBrush(Qt::NoBrush);
		for (; j < m_polygons.size() && m_polygons[j].z == z; j++, z++)
		{
			painter->setPen(m_polygons[j].pen);
			painter->drawPolyline(m_polygons[j].polygon);
		}

		for (; k < m_filledPolygons.size() && m_filledPolygons[k].z == z; k++, z++)
		{
			QPen pen = Qt::NoPen;
			QBrush& brush = m_filledPolygons[k].brush;
			if (brush.style() == Qt::SolidPattern)
				pen = brush.color();
			painter->setPen(pen);
			painter->setBrush(brush);
			painter->drawPolygon(m_filledPolygons[k].polygon);
		}

	}
}

/////////////////////////////
// QtGnuplotKeyBox

/*
 * EAM - support for toggling plots by clicking on a key sample
 */

QtGnuplotKeybox::QtGnuplotKeybox(const QRectF& rect) : QRectF(rect)
{
	m_hidden = false;
	m_statusBox = NULL;
}

bool QtGnuplotKeybox::ishidden() const
{
	return m_hidden;
}

void QtGnuplotKeybox::setHidden(bool state)
{
	m_hidden = state;
	if (m_statusBox)
		m_statusBox->setVisible(m_hidden);
}

void QtGnuplotKeybox::showStatus(QGraphicsRectItem* me)
{
	m_statusBox = me;
	m_statusBox->setVisible(m_hidden);
}

void QtGnuplotKeybox::resetStatus()
{
	m_statusBox = NULL;
}
