/* GNUPLOT - QtGnuplotItems.h */

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

#ifndef QTGNUPLOTITEMS_H
#define QTGNUPLOTITEMS_H

#include <QGraphicsItem>
#include <QFont>
#include <QPen>

class QtGnuplotPoint : public QGraphicsItem
{
public:
	QtGnuplotPoint(int style, double size, QPen pen, QGraphicsItem * parent = 0);

public:
	virtual QRectF boundingRect() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

public:
	static void drawPoint(QPainter* painter, const QPointF& origin, double size, int style);

private:
	QPen m_pen;
	QColor m_color;
	int m_style;
	double m_size;
};

class QtGnuplotClippedPixmap : public QGraphicsPixmapItem
{
public:
	QtGnuplotClippedPixmap(const QRectF& clipRect, const QPixmap& pixmap, QGraphicsItem* parent = 0)
		: QGraphicsPixmapItem(pixmap, parent)
		, m_clipRect(clipRect)
	{
		setFlags(flags() | QGraphicsItem::ItemClipsToShape);
	}

public:
	virtual QPainterPath shape() const
	{
		QPainterPath p;
		p.addRect(m_clipRect);
		return p;
	}

private:
	QRectF m_clipRect;
};

class QtGnuplotEnhancedFragment : public QAbstractGraphicsShapeItem
{
public:
	QtGnuplotEnhancedFragment(const QFont& font, const QString& text, QGraphicsItem * parent = 0);

public:
	virtual QRectF boundingRect() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
    qreal width() const;

private:
	QFont m_font;
	QString m_text;
};

class QtGnuplotEnhanced : public QGraphicsItem
{
public:
	QtGnuplotEnhanced(QGraphicsItem * parent = 0) : QGraphicsItem(parent) { m_overprintMark = false;}

public:
	virtual QRectF boundingRect() const { return childrenBoundingRect(); }
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

public:
	void addText(const QString& fontName, double fontSize, 
		     QFont::Style fontStyle, QFont::Weight fontWeight,
		     double base, bool widthFlag,
	             bool showFlag, int overprint, const QString& text, QColor color);

private:
	QPointF m_currentPos, m_savedPos;
	bool m_overprintMark;
	double m_overprintPos;
};


class QtGnuplotKeybox : public QRectF
{
public:
	QtGnuplotKeybox(const QRectF& rect);
	void setHidden(bool state);
	bool ishidden() const;
	void showStatus(QGraphicsRectItem* me);
	void resetStatus();

private:
	bool m_hidden;
	QGraphicsRectItem *m_statusBox;
};

struct QtGnuplotPoints_PointData
{
	unsigned int z;
	QPointF point;
	int style;
	double pointSize;
	QPen pen;
};

struct QtGnuplotPoints_PolygonData
{
	unsigned int z;
	QPolygonF polygon;
	QPen pen;
};

struct QtGnuplotPoints_FilledPolygonData
{
	unsigned int z;
	QPolygonF polygon;
	QBrush brush;
};

// Single item that gathers many points and line to increase the performance of the terminal
class QtGnuplotPoints : public QGraphicsItem
{
public:
	QtGnuplotPoints(QGraphicsItem* parent = 0);

public:
	void addPoint(const QPointF& point, int style, double pointSize, const QPen& pen);
	void addPolygon(const QPolygonF& polygon, const QPen& pen);
	void addFilledPolygon(const QPolygonF& polygon, const QBrush& brush);
	bool isEmpty() const;

public:
	virtual QRectF boundingRect() const;
	virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

private:
	QVector<QtGnuplotPoints_PointData> m_points;
	QVector<QtGnuplotPoints_PolygonData> m_polygons;
	QVector<QtGnuplotPoints_FilledPolygonData> m_filledPolygons;
	QRectF m_boundingRect;
	unsigned int m_currentZ;
};

#endif // QTGNUPLOTITEMS_H
