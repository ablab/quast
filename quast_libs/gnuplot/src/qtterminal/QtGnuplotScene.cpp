/* GNUPLOT - QtGnuplotScene.cpp */

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

#include "QtGnuplotWidget.h"
#include "QtGnuplotScene.h"
#include "QtGnuplotEvent.h"
#include "QtGnuplotItems.h"

extern "C" {
#include "../mousecmn.h"
// This is defined in term_api.h, but including it there fails to compile
typedef enum t_fillstyle { FS_EMPTY, FS_SOLID, FS_PATTERN, FS_DEFAULT, FS_TRANSPARENT_SOLID, FS_TRANSPARENT_PATTERN } t_fillstyle;
}

#include <QtGui>
#include <QDebug>

QtGnuplotScene::QtGnuplotScene(QtGnuplotEventHandler* eventHandler, QObject* parent)
	: QGraphicsScene(parent)
{
	m_widget = dynamic_cast<QtGnuplotWidget*>(parent);
	m_eventHandler = eventHandler;
	m_lastModifierMask = 0;
	m_textAngle = 0.;
	m_textAlignment = Qt::AlignLeft;
	m_textOffset = QPoint(10,10);
	m_currentZ = 1.;
	m_currentPointSize = 1.;
	m_enhanced = 0;
	m_currentPlotNumber = 0;
	m_inKeySample = false;
	m_preserve_visibility = false;
	m_inTextBox = false;
	m_currentPointsItem = new QtGnuplotPoints;

	m_currentGroup.clear();

	resetItems();
}

QtGnuplotScene::~QtGnuplotScene()
{
	delete m_currentPointsItem;
}

/////////////////////////////////////////////////
// Gnuplot events

// Vertical and horizontal lines do not render well with antialiasing
// Clip then to the nearest integer pixel
QPolygonF& QtGnuplotScene::clipPolygon(QPolygonF& polygon, bool checkDiag) const
{
	if (checkDiag)
		for (int i = 1; i < polygon.size(); i++)
			if ((polygon[i].x() != polygon[i-1].x()) &&
				(polygon[i].y() != polygon[i-1].y()))
				return polygon;


	for (int i = 0; i < polygon.size(); i++)
	{
		polygon[i].setX(qRound(polygon[i].x() + 0.5) - 0.5);
		polygon[i].setY(qRound(polygon[i].y() + 0.5) - 0.5);
	}

	return polygon;
}

QPointF& QtGnuplotScene::clipPoint(QPointF& point) const
{
	point.setX(qRound(point.x() + 0.5) - 0.5);
	point.setY(qRound(point.y() + 0.5) - 0.5);

	return point;
}

QRectF& QtGnuplotScene::clipRect(QRectF& rect) const
{
	rect.setTop   (qRound(rect.top   () + 0.5) - 0.5);
	rect.setBottom(qRound(rect.bottom() + 0.5) - 0.5);
	rect.setLeft  (qRound(rect.left  () + 0.5) - 0.5);
	rect.setRight (qRound(rect.right () + 0.5) - 0.5);

	return rect;
}

// A succession of move and vector is gathered in a single polygon
// to make drawing faster
void QtGnuplotScene::flushCurrentPolygon()
{
	if (m_currentPolygon.size() < 2) {
		m_currentPolygon.clear();
		return;
	}

	clipPolygon(m_currentPolygon);
	if (!m_inKeySample)
		m_currentPointsItem->addPolygon(m_currentPolygon, m_currentPen);
	else
	{
		flushCurrentPointsItem();
		// Including the polygon in a path is necessary to avoid closing the polygon
		QPainterPath path;
		path.addPolygon(m_currentPolygon);
		QGraphicsPathItem *pathItem;
		pathItem = addPath(path, m_currentPen, Qt::NoBrush);
		pathItem->setZValue(m_currentZ++);
	}
	m_currentPolygon.clear();
}

void QtGnuplotScene::flushCurrentPointsItem()
{
	if (m_currentPointsItem->isEmpty())
		return;

	m_currentPointsItem->setZValue(m_currentZ++);
	addItem(m_currentPointsItem);
	m_currentGroup.append(m_currentPointsItem);
	m_currentPointsItem = new QtGnuplotPoints();
}

void QtGnuplotScene::update_key_box(const QRectF rect)
{
	if (m_currentPlotNumber > m_key_boxes.count()) {
		m_key_boxes.insert(m_currentPlotNumber, QtGnuplotKeybox(rect));
	} else if (m_key_boxes[m_currentPlotNumber-1].isEmpty()) {
		// Retain the visible/hidden flag when re-initializing the Keybox
		bool tmp = m_key_boxes[m_currentPlotNumber-1].ishidden();
		m_key_boxes[m_currentPlotNumber-1] = QtGnuplotKeybox(rect);
		m_key_boxes[m_currentPlotNumber-1].setHidden(tmp);
	} else {
		m_key_boxes[m_currentPlotNumber-1] |= rect;
	}
}

void QtGnuplotScene::processEvent(QtGnuplotEventType type, QDataStream& in)
{
	if ((type != GEMove) && (type != GEVector) && !m_currentPolygon.empty())
	{
		QPointF point = m_currentPolygon.last();
		flushCurrentPolygon();
		m_currentPolygon << point;
	}

	if (type == GEClear)
	{
		resetItems();
		m_preserve_visibility = false;
	}
	else if (type == GELineWidth)
	{
		double width; in >> width;
		m_currentPen.setWidthF(width);
	}
	else if (type == GEMove)
	{
		QPointF point; in >> point;
		if (!m_currentPolygon.empty() && (m_currentPolygon.last() != point))
		{
			flushCurrentPolygon();
			m_currentPolygon.clear();
		}
		m_currentPolygon << point;
	}
	else if (type == GEVector)
	{
		QPointF point; in >> point;
		m_currentPolygon << point;
		if (m_inKeySample)
		{
			flushCurrentPointsItem();
			update_key_box( QRectF(point, QSize(0,1)) );
		}
	}
	else if (type == GEPenColor)
	{
		QColor color; in >> color;
		m_currentPen.setColor(color);
	}
	else if (type == GEBackgroundColor)
	{
		m_currentPen.setColor(m_widget->backgroundColor());
	}
	else if (type == GEPenStyle)
	{
		int style; in >> style;
		m_currentPen.setStyle(Qt::PenStyle(style));
		if (m_widget->rounded()) {
			m_currentPen.setJoinStyle(Qt::RoundJoin);
			m_currentPen.setCapStyle(Qt::RoundCap);
		} else {
			m_currentPen.setJoinStyle(Qt::MiterJoin);
			m_currentPen.setCapStyle(Qt::FlatCap);
		}
	}
	else if (type == GEDashPattern)
	{
		QVector<qreal> dashpattern; in >> dashpattern;
		m_currentPen.setDashPattern(dashpattern);
	}
	else if (type == GEPointSize)
		in >> m_currentPointSize;
	else if (type == GETextAngle)
		in >> m_textAngle;
	else if (type == GETextAlignment)
	{
		int alignment;
		in >> alignment;
		m_textAlignment = Qt::Alignment(alignment);
	}
	else if (type == GEFillBox)
	{
		flushCurrentPointsItem();
		QRect rect; in >> rect;
		QGraphicsRectItem *rectItem;
		rectItem = addRect(rect, Qt::NoPen, m_currentBrush);
		rectItem->setZValue(m_currentZ++);
		if (m_inKeySample)
			update_key_box(rect);
		else
			m_currentGroup.append(rectItem);
	}
	else if (type == GEFilledPolygon)
	{
		QPolygonF polygon; in >> polygon;

		if (!m_inKeySample)
			m_currentPointsItem->addFilledPolygon(clipPolygon(polygon, false), m_currentBrush);
		else
		{
			flushCurrentPointsItem();
			QPen pen = Qt::NoPen;
			if (m_currentBrush.style() == Qt::SolidPattern)
				pen = m_currentBrush.color();
			clipPolygon(polygon, false);
			QGraphicsPolygonItem *path;
			path = addPolygon(polygon, pen, m_currentBrush);
			path->setZValue(m_currentZ++);
		}
	}
	else if (type == GEBrushStyle)
	{
		int style; in >> style;
		setBrushStyle(style);
	}
	else if (type == GERuler)
	{
		QPoint point; in >> point;
		updateRuler(point);
	}
	else if (type == GESetFont)
	{
		QString fontName; in >> fontName;
		int size        ; in >> size;
		m_font.setFamily(fontName);
		m_font.setPointSize(size);
		m_font.setStyleStrategy(QFont::ForceOutline);	// pcf fonts die if rotated
	}
	else if (type == GEPoint)
	{
		QPointF point; in >> point;
		int style    ; in >> style;

		// Make sure point symbols are always drawn with a solid line
		m_currentPen.setStyle(Qt::SolidLine);

		// Append the point to a points item to speed-up drawing
		if (!m_inKeySample)
			m_currentPointsItem->addPoint(clipPoint(point), style, m_currentPointSize, m_currentPen);
		else
		{
			flushCurrentPointsItem();
			QtGnuplotPoint* pointItem = new QtGnuplotPoint(style, m_currentPointSize, m_currentPen);
			pointItem->setPos(clipPoint(point));
			pointItem->setZValue(m_currentZ++);
			addItem(pointItem);
			update_key_box( QRectF(point, QSize(2,2)) );
		}

		// Create a hypertext label that will become visible on mouseover.
		// The Z offset is a kludge to force the label into the foreground.
		if (!m_currentHypertext.isEmpty()) {
			QGraphicsTextItem* textItem = addText(m_currentHypertext, m_font);
			textItem->setPos(point + m_textOffset);
			textItem->setZValue(m_currentZ+10000);
			textItem->setVisible(false);
			m_hypertextList.append(textItem);
			m_currentHypertext.clear();
		}
	}
	else if (type == GEPutText)
	{
		flushCurrentPointsItem();
		QPoint point; in >> point;
		QString text; in >> text;
		QGraphicsTextItem* textItem = addText(text, m_font);
		textItem->setDefaultTextColor(m_currentPen.color());
		positionText(textItem, point);

		QRectF rect = textItem->boundingRect();
		if (m_textAlignment == Qt::AlignCenter) {
			rect.moveCenter(point);
			rect.moveBottom(point.y());
		} else if (m_textAlignment == Qt::AlignRight)
			rect.moveBottomRight(point);
		else
			rect.moveBottomLeft(point);
		rect.adjust(0, rect.height()*0.67, 0, rect.height()*0.25);

		if (m_inKeySample)
			update_key_box(rect);
		else
			m_currentGroup.append(textItem);
#ifdef EAM_BOXED_TEXT
		if (m_inTextBox)
			m_currentTextBox |= rect;
#endif
	}
	else if (type == GEEnhancedFlush)
	{
		flushCurrentPointsItem();
		QString fontName; in >> fontName;
		double fontSize ; in >> fontSize;
		int fontStyle   ; in >> fontStyle;	// really QFont::Style
		int fontWeight  ; in >> fontWeight;	// really QFont::Weight
		double base     ; in >> base;
		bool widthFlag  ; in >> widthFlag;
		bool showFlag   ; in >> showFlag;
		int overprint   ; in >> overprint;
		QString text    ; in >> text;
		if (m_enhanced == 0)
			m_enhanced = new QtGnuplotEnhanced();
		m_enhanced->addText(fontName, fontSize, 
				    (QFont::Style)fontStyle, (QFont::Weight)fontWeight,
				    base, widthFlag, showFlag,
		                    overprint, text, m_currentPen.color());
	}
	else if (type == GEEnhancedFinish)
	{
		flushCurrentPointsItem();
		QPoint point; in >> point;
		positionText(m_enhanced, point);
		m_enhanced->setZValue(m_currentZ++);
		addItem(m_enhanced);

		QRectF rect = m_enhanced->boundingRect();
		if (m_textAlignment == Qt::AlignCenter) {
			rect.moveCenter(point);
			rect.moveBottom(point.y());
		} else if (m_textAlignment == Qt::AlignRight)
			rect.moveBottomRight(point);
		else
			rect.moveBottomLeft(point);
		rect.adjust(-m_currentPen.width()*2, rect.height()/2, 
			    m_currentPen.width()*2, rect.height()/2);

		if (m_inKeySample)
			update_key_box(rect);
		else
			m_currentGroup.append(m_enhanced);
#ifdef EAM_BOXED_TEXT
		if (m_inTextBox)
			m_currentTextBox |= rect;
#endif
		m_enhanced = 0;
	}
	else if (type == GEImage)
	{
		flushCurrentPointsItem();
		QPointF p0; in >> p0;
		QPointF p1; in >> p1;
		QPointF p2; in >> p2;
		QPointF p3; in >> p3;
		QImage image; in >> image;
		QSize size(p1.x() - p0.x(), p1.y() - p0.y());
		QRectF clipRect(p2 - p0, p3 - p0);
		QPixmap pixmap = QPixmap::fromImage(image).scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
		QtGnuplotClippedPixmap* item = new QtGnuplotClippedPixmap(clipRect, pixmap);
		addItem(item);
		item->setZValue(m_currentZ++);
		item->setPos(p0);
		m_currentGroup.append(item);
	}
	else if (type == GEZoomStart)
	{
		QString text; in >> text;
		m_zoomBoxCorner = m_lastMousePos;
		m_zoomRect->setRect(QRectF());
		m_zoomRect->setVisible(true);
		m_zoomStartText->setVisible(true);
		m_zoomStartText->setPlainText(text); /// @todo font
		QSizeF size = m_zoomStartText->boundingRect().size();
		m_zoomStartText->setPos(m_zoomBoxCorner - QPoint(size.width(), size.height()));
	}
	else if (type == GEZoomStop)
	{
		QString text; in >> text;
		if (text.isEmpty())
		{
			m_zoomRect->setVisible(false);
			m_zoomStartText->setVisible(false);
			m_zoomStopText->setVisible(false);
		}
		else
		{
			m_zoomStopText->setVisible(true);
			m_zoomStopText->setPlainText(text); /// @todo font
			m_zoomStopText->setPos(m_lastMousePos);
			m_zoomRect->setRect(QRectF(m_zoomBoxCorner + QPointF(0.5, 0.5), m_lastMousePos + QPointF(0.5, 0.5)).normalized());
			m_zoomRect->setZValue(32767);  // make sure guide box is on top
		}
	}
	else if (type == GELineTo)
	{
		bool visible; in >> visible;
		m_lineTo->setVisible(visible);
		if (visible)
		{
			QLineF line = m_lineTo->line();
			line.setP2(m_lastMousePos);
			m_lineTo->setLine(line);
		}
	}
	else if (type == GESetSceneSize)
	{
		QSize size; in >> size;
		setSceneRect(QRect(QPoint(0, 0), size));
	}
	else if (type == GEScale)
	{
		for (int i = 0; i < 4; i++)
			in >> m_axisValid[i] >> m_axisMin[i] >> m_axisLower[i] >> m_axisScale[i] >> m_axisLog[i];
	}
	else if (type == GEAfterPlot) 
	{
		flushCurrentPointsItem();
		if (m_currentPlotNumber >= m_plot_group.count()) {
			// End of previous plot, create group holding the accumulated elements
			QGraphicsItemGroup *newgroup;
			newgroup = createItemGroup(m_currentGroup);
			newgroup->setZValue(m_currentZ++);
			// Copy the visible/hidden status from the previous plot/replot
			if (0 < m_currentPlotNumber && m_currentPlotNumber <= m_key_boxes.count())
				newgroup->setVisible( !(m_key_boxes[m_currentPlotNumber-1].ishidden()) );
			// Store it in an ordered list so we can toggle it by index
			m_plot_group.insert(m_currentPlotNumber, newgroup);
		} 
		m_currentPlotNumber = 0;
	}
	else if (type == GEPlotNumber) 
	{
		flushCurrentPointsItem();
		int newPlotNumber;  in >> newPlotNumber;
		if (newPlotNumber >= m_plot_group.count()) {
			// Initialize list of elements for next group
			m_currentGroup.clear();
		} 
		// Otherwise we are making a second pass through the same plots
		// (currently used only to draw key samples on an opaque key box)
		m_currentPlotNumber = newPlotNumber;
	}
	else if (type == GEModPlots)
	{
		int i = m_key_boxes.count();
		unsigned int ops_i;
		int plotno;
		in >> ops_i;
		in >> plotno;
		enum QtGnuplotModPlots ops = (enum QtGnuplotModPlots) ops_i;

		/* FIXME: This shouldn't happen, but it does. */
		/* Failure to reset lists after multiplot??   */
		if (i > m_plot_group.count())
		    i = m_plot_group.count();

		while (i-- > 0) {
			/* Does operation only affect a single plot? */
			if (plotno >= 0 && i != plotno)
				continue;

			bool isVisible = m_plot_group[i]->isVisible();

			switch (ops) {
			case QTMODPLOTS_INVERT_VISIBILITIES:
				isVisible = !isVisible;
				break;
			case QTMODPLOTS_SET_VISIBLE:
				isVisible = true;
				break;
			case QTMODPLOTS_SET_INVISIBLE:
				isVisible = false;
				break;
			}

			m_plot_group[i]->setVisible(isVisible);
			m_key_boxes[i].setHidden(!isVisible);
		}
	}
	else if (type == GELayer)
	{
		flushCurrentPointsItem();
		int layer; in >> layer;
		if (layer == QTLAYER_BEFORE_ZOOM) m_preserve_visibility = true;
		if (layer == QTLAYER_BEGIN_KEYSAMPLE) m_inKeySample = true;
		if (layer == QTLAYER_END_KEYSAMPLE)
		{
			m_inKeySample = false;

			// FIXME: this catches mislabeled opaque keyboxes in multiplot mode
			if (m_currentPlotNumber > m_key_boxes.length()) {
				return;
			}
			// Draw an invisible grey rectangle in the key box.
			// It will be set to visible if the plot is toggled off.
			QtGnuplotKeybox *keybox = &m_key_boxes[m_currentPlotNumber-1];
			m_currentBrush.setColor(Qt::lightGray);
			m_currentBrush.setStyle(Qt::Dense4Pattern);
			QGraphicsRectItem *statusBox = addRect(*keybox, Qt::NoPen, m_currentBrush);
			statusBox->setZValue(m_currentZ-1);
			keybox->showStatus(statusBox);
		}
	}
	else if (type == GEHypertext)
	{
		m_currentHypertext.clear();
		in >> m_currentHypertext;
	}
#ifdef EAM_BOXED_TEXT
	else if (type == GETextBox)
	{
		flushCurrentPointsItem();
		QPointF point; in >> point;
		int option; in >> option;
		QGraphicsRectItem *rectItem;
		QRectF outline;

		/* Must match the definition in ../term_api.h */
		enum t_textbox_options {
			TEXTBOX_INIT = 0,
			TEXTBOX_OUTLINE,
			TEXTBOX_BACKGROUNDFILL,
			TEXTBOX_MARGINS,
			TEXTBOX_FINISH,
			TEXTBOX_GREY
		};

		switch (option) {
		case TEXTBOX_INIT:
			/* Initialize bounding box */
			m_currentTextBox = QRectF(point, point);
			m_inTextBox = true;
			break;
		case TEXTBOX_OUTLINE:
			/* Stroke bounding box */
			outline = m_currentTextBox.adjusted(
				 -m_textMargin.x(), -m_textMargin.y(),
				  m_textMargin.x(), m_textMargin.y());
			rectItem = addRect(outline, m_currentPen, Qt::NoBrush);
			rectItem->setZValue(m_currentZ++);
			m_currentGroup.append(rectItem);
			m_inTextBox = false;
			break;
		case TEXTBOX_BACKGROUNDFILL:
			/* Fill bounding box */
			m_currentBrush.setColor(m_widget->backgroundColor());
			m_currentBrush.setStyle(Qt::SolidPattern);
			outline = m_currentTextBox.adjusted(
				 -m_textMargin.x(), -m_textMargin.y(),
				  m_textMargin.x(), m_textMargin.y());
			rectItem = addRect(outline, Qt::NoPen, m_currentBrush);
			rectItem->setZValue(m_currentZ++);
			m_currentGroup.append(rectItem);
			m_inTextBox = false;
			break;
		case TEXTBOX_MARGINS:
			/* Set margins of bounding box */
			m_textMargin = point;
			m_textMargin *= QFontMetrics(m_font).averageCharWidth();
			break;
		}
	}
#endif
	else if (type == GEFontMetricRequest)
	{
		QFontMetrics metrics(m_font);
		int par1 = (metrics.ascent() + metrics.descent());
		int par2 = metrics.width("0123456789")/10.;
		m_eventHandler->postTermEvent(GE_fontprops, 0, 0, par1, par2, m_widget);
	}
	else if (type == GEDone)
	{
		flushCurrentPointsItem();
		m_eventHandler->postTermEvent(GE_plotdone, 0, 0, 0, 0, m_widget);
	}
	else
		swallowEvent(type, in);
}

void QtGnuplotScene::resetItems()
{
	clear();
	delete m_currentPointsItem;
	m_currentPointsItem = new QtGnuplotPoints;

	m_currentZ = 1.;

	m_zoomRect = addRect(QRect(), QPen(QColor(0, 0, 0, 200)), QBrush(QColor(0, 0, 255, 40)));
	m_zoomRect->setVisible(false);
	m_zoomStartText = addText("");
	m_zoomStopText  = addText("");
	m_zoomStartText->setVisible(false);
	m_zoomStopText->setVisible(false);
	m_horizontalRuler = addLine(QLine(0, 0, width(), 0) , QPen(QColor(0, 0, 0, 200)));
	m_verticalRuler   = addLine(QLine(0, 0, 0, height()), QPen(QColor(0, 0, 0, 200)));
	m_lineTo          = addLine(QLine()                 , QPen(QColor(0, 0, 0, 200)));
	m_horizontalRuler->setVisible(false);
	m_verticalRuler->setVisible(false);
	m_lineTo->setVisible(false);

	int i = m_key_boxes.count();
	while (i-- > 0) {
		m_key_boxes[i].setSize( QSizeF(0,0) );
		m_key_boxes[i].resetStatus();
		if (!m_preserve_visibility)
			m_key_boxes[i].setHidden(false);
	}

	m_hypertextList.clear();
	m_hypertextList.append(addRect(QRect(), QPen(QColor(0, 0, 0, 100)), QBrush(QColor(225, 225, 225, 200))));
	m_hypertextList[0]->setVisible(false);

	m_plot_group.clear();	// Memory leak?  Destroy groups first?
}

namespace QtGnuplot {
	const Qt::BrushStyle brushes[8] = {
	 Qt::NoBrush, Qt::DiagCrossPattern, Qt::Dense3Pattern, Qt::SolidPattern,
	 Qt::FDiagPattern, Qt::BDiagPattern, Qt::Dense4Pattern, Qt::Dense5Pattern};
}

void QtGnuplotScene::setBrushStyle(int style)
{
	int fillpar = style >> 4;
	int fillstyle = style & 0xf;

	m_currentBrush.setStyle(Qt::SolidPattern);

	QColor color = m_currentPen.color();

	if (fillstyle == FS_TRANSPARENT_SOLID)
		color.setAlphaF(double(fillpar)/100.);
	else if (fillstyle == FS_SOLID && (fillpar < 100))
	{
		double fact  = double(100 - fillpar)/100.;
		double factc = 1. - fact;

		if ((fact >= 0.) && (factc >= 0.))
		{
			color.setRedF  (color.redF()  *factc + fact);
			color.setGreenF(color.greenF()*factc + fact);
			color.setBlueF (color.blueF() *factc + fact);
		}
	}
	else if ((fillstyle == FS_TRANSPARENT_PATTERN) || (fillstyle == FS_PATTERN))
		/// @todo color & transparent. See other terms
		m_currentBrush.setStyle(QtGnuplot::brushes[abs(fillpar) % 8]);
	else if (fillstyle == FS_EMPTY) // fill with background plot->color
		color = m_widget->backgroundColor();

	m_currentBrush.setColor(color);
}

void QtGnuplotScene::positionText(QGraphicsItem* item, const QPoint& point)
{
	item->setZValue(m_currentZ++);

	double cx = 0.;
	double cy = (item->boundingRect().bottom() + item->boundingRect().top())/2.;
	if (m_textAlignment == Qt::AlignLeft)
		cx = item->boundingRect().left();
	else if (m_textAlignment == Qt::AlignRight)
		cx = item->boundingRect().right();
	else if (m_textAlignment == Qt::AlignCenter)
		cx = (item->boundingRect().right() + item->boundingRect().left())/2.;

	item->setTransformOriginPoint(cx, cy);
	item->setRotation(-m_textAngle);
	item->setPos(point.x() - cx, point.y() - cy);
}

void QtGnuplotScene::updateRuler(const QPoint& point)
{
	if (point.x() < 0)
	{
		m_horizontalRuler->setVisible(false);
		m_verticalRuler->setVisible(false);
		m_lineTo->setVisible(false);
		return;
	}

	QPointF pointF = QPointF(point) + QPointF(0.5, 0.5);

	m_horizontalRuler->setVisible(true);
	m_verticalRuler->setVisible(true);

	m_horizontalRuler->setPos(0, pointF.y());
	m_verticalRuler->setPos(pointF.x(), 0);

	QLineF line = m_lineTo->line();
	line.setP1(pointF);
	m_lineTo->setLine(line);
}

/////////////////////////////////////////////////
// User events

void QtGnuplotScene::updateModifiers()
{
	const int modifierMask = (QApplication::keyboardModifiers() >> 25);

	if (modifierMask != m_lastModifierMask)
	{
		m_lastModifierMask = modifierMask;
		m_eventHandler->postTermEvent(GE_modifier, 0, 0, modifierMask, 0, m_widget);
	}
}

void QtGnuplotScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	m_lastMousePos = event->scenePos();
	updateModifiers();

	int button = 0;
	     if (event->button()== Qt::LeftButton)  button = 1;
	else if (event->button()== Qt::MidButton)   button = 2;
	else if (event->button()== Qt::RightButton) button = 3;

	m_eventHandler->postTermEvent(GE_buttonpress, 
			int(event->scenePos().x()), int(event->scenePos().y()), 
			button, 0, m_widget);
	QGraphicsScene::mousePressEvent(event);
}

void QtGnuplotScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	// Mousing for inactive widgets
	if (!m_widget->isActive())
	{
		m_lineTo->hide();
		QString status;
		if (m_axisValid[0])
			status += QString("x = ")
			+ QString::number(sceneToGraph(0,event->scenePos().x()));
		if (m_axisValid[1])
			status += QString(" y = ")
			+ QString::number(sceneToGraph(1,event->scenePos().y()));
		if (m_axisValid[2])
			status += QString(" x2 = ")
			+ QString::number(sceneToGraph(2,event->scenePos().x()));
		if (m_axisValid[3])
			status += QString(" y2 = ")
			+ QString::number(sceneToGraph(3,event->scenePos().y()));
		m_widget->setStatusText(status);
		QGraphicsScene::mouseMoveEvent(event);
		return;
	}

	m_lastMousePos = event->scenePos();
	updateModifiers();

	if (m_lineTo->isVisible())
	{
		QLineF line = m_lineTo->line();
		line.setP2(event->scenePos());
		m_lineTo->setLine(line);
	}

	// The first item in m_hypertextList is always a background rectangle for the text
	int i = m_hypertextList.count();
	bool hit = false;
	while (i-- > 1) {
		if (!hit && ((m_hypertextList[i]->pos() - m_textOffset) 
				- m_lastMousePos).manhattanLength() <= 5) {
			hit = true;
			m_hypertextList[i]->setVisible(true);
			((QGraphicsRectItem *)m_hypertextList[0])->setRect(m_hypertextList[i]->boundingRect());
			m_hypertextList[0]->setPos(m_hypertextList[i]->pos());
			m_hypertextList[0]->setZValue(m_hypertextList[i]->zValue()-1);
		} else {
			m_hypertextList[i]->setVisible(false);
		}
	}
	m_hypertextList[0]->setVisible(hit);

	m_eventHandler->postTermEvent(GE_motion, int(event->scenePos().x()), int(event->scenePos().y()), 0, 0, m_widget);
	QGraphicsScene::mouseMoveEvent(event);
}

void QtGnuplotScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	m_lastMousePos = event->scenePos();
	updateModifiers();

	int button = 0;
	     if (event->button()== Qt::LeftButton)  button = 1;
	else if (event->button()== Qt::MidButton)   button = 2;
	else if (event->button()== Qt::RightButton) button = 3;

	qint64 time = 301;	/* Only used the first time in, when timer not yet running */
	if (m_watches[button].isValid())
		time = m_watches[button].elapsed();
	m_eventHandler->postTermEvent(GE_buttonrelease,
		int(event->scenePos().x()), int(event->scenePos().y()), button, time, m_widget);
	m_watches[button].start();

	/* Check for left click in one of the keysample boxes */
	if (button == 1) {
	    int n = m_key_boxes.count();
	    for (int i = 0; i < n; i++) {
		    if (m_key_boxes[i].contains(m_lastMousePos)) {
			    if (m_plot_group[i]->isVisible()) {
				    m_plot_group[i]->setVisible(false);
				    m_key_boxes[i].setHidden(true);
			    } else {
				    m_plot_group[i]->setVisible(true);
				    m_key_boxes[i].setHidden(false);
			    }
			    break;
		    }
	    }
	}

	QGraphicsScene::mouseReleaseEvent(event);
}

/* http://developer.qt.nokia.com/doc/qt-4.8/qgraphicsscenewheelevent.html */
void QtGnuplotScene::wheelEvent(QGraphicsSceneWheelEvent* event)
{
	updateModifiers();
	if (event->orientation() == Qt::Horizontal) {
		// 6 = scroll left, 7 = scroll right
		m_eventHandler->postTermEvent(GE_buttonpress,
			int(event->scenePos().x()), int(event->scenePos().y()), 
			event->delta() > 0 ? 6 : 7, 0, m_widget);
	} else { /* if (event->orientation() == Qt::Vertical) */
		// 4 = scroll up, 5 = scroll down
		m_eventHandler->postTermEvent(GE_buttonpress,
			int(event->scenePos().x()), int(event->scenePos().y()), 
			event->delta() > 0 ? 4 : 5, 0, m_widget);
	} 
}

void QtGnuplotScene::keyPressEvent(QKeyEvent* event)
{
	updateModifiers();

	int key = -1;
	int live;

	/// @todo quit on 'q' or Ctrl+'q'

	// Keypad keys
	if (event->modifiers() & Qt::KeypadModifier)
		switch (event->key())
		{
			case Qt::Key_Space    : key = GP_KP_Space    ; break;
			case Qt::Key_Tab      : key = GP_KP_Tab      ; break;
			case Qt::Key_Enter    : key = GP_KP_Enter    ; break;
			case Qt::Key_F1       : key = GP_KP_F1       ; break;
			case Qt::Key_F2       : key = GP_KP_F2       ; break;
			case Qt::Key_F3       : key = GP_KP_F3       ; break;
			case Qt::Key_F4       : key = GP_KP_F4       ; break;
			case Qt::Key_Insert   : key = GP_KP_Insert   ; break;
			case Qt::Key_End      : key = GP_KP_End      ; break;
			case Qt::Key_Home     : key = GP_KP_Home     ; break;
#ifdef __APPLE__
			case Qt::Key_Down     : key = GP_Down        ; break;
			case Qt::Key_Left     : key = GP_Left        ; break;
			case Qt::Key_Right    : key = GP_Right       ; break;
			case Qt::Key_Up       : key = GP_Up          ; break;
#else
			case Qt::Key_Down     : key = GP_KP_Down     ; break;
			case Qt::Key_Left     : key = GP_KP_Left     ; break;
			case Qt::Key_Right    : key = GP_KP_Right    ; break;
			case Qt::Key_Up       : key = GP_KP_Up       ; break;
#endif
			case Qt::Key_PageDown : key = GP_KP_Page_Down; break;
			case Qt::Key_PageUp   : key = GP_KP_Page_Up  ; break;
			case Qt::Key_Delete   : key = GP_KP_Delete   ; break;
			case Qt::Key_Equal    : key = GP_KP_Equal    ; break;
			case Qt::Key_Asterisk : key = GP_KP_Multiply ; break;
			case Qt::Key_Plus     : key = GP_KP_Add      ; break;
			case Qt::Key_Comma    : key = GP_KP_Separator; break;
			case Qt::Key_Minus    : key = GP_KP_Subtract ; break;
			case Qt::Key_Period   : key = GP_KP_Decimal  ; break;
			case Qt::Key_Slash    : key = GP_KP_Divide   ; break;
			case Qt::Key_0        : key = GP_KP_0        ; break;
			case Qt::Key_1        : key = GP_KP_1        ; break;
			case Qt::Key_2        : key = GP_KP_2        ; break;
			case Qt::Key_3        : key = GP_KP_3        ; break;
			case Qt::Key_4        : key = GP_KP_4        ; break;
			case Qt::Key_5        : key = GP_KP_5        ; break;
			case Qt::Key_6        : key = GP_KP_6        ; break;
			case Qt::Key_7        : key = GP_KP_7        ; break;
			case Qt::Key_8        : key = GP_KP_8        ; break;
			case Qt::Key_9        : key = GP_KP_9        ; break;
		}
	// ASCII keys
	else if ((event->key() <= 0xff) && (!event->text().isEmpty()))
		// event->key() does not respect the case
		key = event->text()[0].toLatin1();
	// Special keys
	else
		switch (event->key())
		{
			case Qt::Key_Backspace  : key = GP_BackSpace  ; break;
			case Qt::Key_Tab        : key = GP_Tab        ; break;
			case Qt::Key_Return     : key = GP_Return     ; break;
			case Qt::Key_Escape     : key = GP_Escape     ; break;
			case Qt::Key_Delete     : key = GP_Delete     ; break;
			case Qt::Key_Pause      : key = GP_Pause      ; break;
			case Qt::Key_ScrollLock : key = GP_Scroll_Lock; break;
			case Qt::Key_Insert     : key = GP_Insert     ; break;
			case Qt::Key_Home       : key = GP_Home       ; break;
			case Qt::Key_Left       : key = GP_Left       ; break;
			case Qt::Key_Up         : key = GP_Up         ; break;
			case Qt::Key_Right      : key = GP_Right      ; break;
			case Qt::Key_Down       : key = GP_Down       ; break;
			case Qt::Key_PageUp     : key = GP_PageUp     ; break;
			case Qt::Key_PageDown   : key = GP_PageDown   ; break;
			case Qt::Key_End        : key = GP_End        ; break;
			case Qt::Key_Enter      : key = GP_KP_Enter   ; break;
			case Qt::Key_F1         : key = GP_F1         ; break;
			case Qt::Key_F2         : key = GP_F2         ; break;
			case Qt::Key_F3         : key = GP_F3         ; break;
			case Qt::Key_F4         : key = GP_F4         ; break;
			case Qt::Key_F5         : key = GP_F5         ; break;
			case Qt::Key_F6         : key = GP_F6         ; break;
			case Qt::Key_F7         : key = GP_F7         ; break;
			case Qt::Key_F8         : key = GP_F8         ; break;
			case Qt::Key_F9         : key = GP_F9         ; break;
			case Qt::Key_F10        : key = GP_F10        ; break;
			case Qt::Key_F11        : key = GP_F11        ; break;
			case Qt::Key_F12        : key = GP_F12        ; break;
		}

	if (key >= 0)
		live = m_eventHandler->postTermEvent(GE_keypress,
			int(m_lastMousePos.x()), int(m_lastMousePos.y()), key, 0, m_widget);
	else
		live = true;

	// Key handling in persist mode 
	// !live means (I think!) that we are in persist mode
	if (!live) {
		switch (key) {
		default:
			break;
		case 'i':
			int i = m_key_boxes.count();
			/* FIXME: This shouldn't happen, but it does. */
			if (i > m_plot_group.count())
			    i = m_plot_group.count();
			while (i-- > 0) {
				bool isVisible = m_plot_group[i]->isVisible();
				isVisible = !isVisible;
				m_plot_group[i]->setVisible(isVisible);
				m_key_boxes[i].setHidden(!isVisible);
			}
			break;
		}
	}

	QGraphicsScene::keyPressEvent(event);
}

double QtGnuplotScene::sceneToGraph(int axis, double coord) const
{
	if (m_axisScale[axis] == 0.)
	    return 0;

	double result = m_axisMin[axis] + (coord - m_axisLower[axis])/m_axisScale[axis];
	if (m_axisLog[axis] > 0.)
		result = exp(result * m_axisLog[axis]);

	return result;
}
