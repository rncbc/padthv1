// padthv1widget_sample.cpp
//
/****************************************************************************
   Copyright (C) 2012-2017, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "padthv1widget_sample.h"

#include "padthv1_config.h"
#include "padthv1_sample.h"

#include <QPainter>
#include <QMouseEvent>

#include <QApplication>
#include <QMenu>
#include <QToolTip>

#include <math.h>


// Safe value capping.
inline float safe_value ( float x )
{
	return (x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x));
}


//----------------------------------------------------------------------------
// padthv1widget_sample -- Custom widget

// Constructor.
padthv1widget_sample::padthv1widget_sample (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QFrame(pParent, wflags), m_pSample(NULL),
		m_pPolyg(NULL), m_nrects(0), m_pRects(NULL)
{
	setMouseTracking(true);
	QFrame::setMinimumSize(QSize(240, 100));
	QFrame::setSizePolicy(
		QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

	QFrame::setFrameShape(QFrame::Panel);
	QFrame::setFrameShadow(QFrame::Sunken);

	// Trap for help/tool-tips events.
	QFrame::installEventFilter(this);

	m_dragCursor = DragNone;

	resetDragState();
}


// Destructor.
padthv1widget_sample::~padthv1widget_sample (void)
{
	setSample(NULL);
}


// Parameter accessors.
void padthv1widget_sample::setSample ( padthv1_sample *pSample )
{
	if (m_pPolyg) {
		delete m_pPolyg;
		m_pPolyg = NULL;
	}

	if (m_pRects) {
		delete [] m_pRects;
		m_pRects = NULL;
		m_nrects = 0;
	}

	m_pSample = pSample;

	if (m_pSample) {
		// Sample waveform...
		const int h = height();
		const int w = width() & 0x7ffe; // force even.
		const int w2 = (w >> 1);
		const unsigned int nframes = m_pSample->size();
		const unsigned int nperiod = nframes / w2;
		const float phase_inc = 1.0f / float(nframes);
		const float h2 = float(h >> 1);
		m_pPolyg = new QPolygon(w);
		float phase = 0.0f;
		float vmax = 0.0f;
		float vmin = 0.0f;
		int n = 0;
		int x = 1;
		unsigned int j = nperiod;
		for (unsigned int i = 0; i < nframes; ++i) {
			const float v = m_pSample->value(phase);
			if (vmax < v)
				vmax = v;
			if (vmin > v)
				vmin = v;
			if (++j > nperiod) {
				m_pPolyg->setPoint(n, x, h2 - int(vmax * h2));
				m_pPolyg->setPoint(w - n - 1, x, h2 - int(vmin * h2));
				j = 0; vmax = vmin = 0.0f;
				++n; x += 2;
			}
			phase += phase_inc;
		}
		while (n < w2) {
			m_pPolyg->setPoint(n, x, h2);
			m_pPolyg->setPoint(w - n - 1, x, h2);
			++n; x += 2;
		}
		// Sample harmonics...
		m_nrects = m_pSample->nh();
		if (m_nrects > 0) {
			m_pRects = new QRect [m_nrects];
			const int h1 = h - 8;
			const float dx = float(w - 8) / float(m_nrects);
			for (n = 0; n < m_nrects; ++n) {
				const int x = int(dx * (0.5f + float(n)));
				const int y = h1 - int(m_pSample->harmonic(n) * float(h1));
				m_pRects[n].setRect(x, y, 8, 8);
			}
		}
	}

	update();
}


padthv1_sample *padthv1widget_sample::sample (void) const
{
	return m_pSample;
}


// Widget resize handler.
void padthv1widget_sample::resizeEvent ( QResizeEvent * )
{
	setSample(m_pSample);
}


// Draw curve.
void padthv1widget_sample::paintEvent ( QPaintEvent *pPaintEvent )
{
	QPainter painter(this);

	const QRect& rect = QFrame::rect();
	const int h = rect.height();
	const int w = rect.width();

	const QPalette& pal = palette();
	const bool bDark = (pal.window().color().value() < 0x7f);
	const QColor& rgbLite = (isEnabled()
		? (bDark ? Qt::darkYellow : Qt::yellow) : pal.mid().color());
    const QColor& rgbDark = pal.window().color().darker(180);

    painter.fillRect(rect, rgbDark);

	if (m_pPolyg && m_pRects) {
		const int w2 = (w << 1);
		painter.setRenderHint(QPainter::Antialiasing, true);
		// Sample waveform...
		QLinearGradient grad1(0, 0, w2, h);
		grad1.setColorAt(0.0f, rgbLite);
		grad1.setColorAt(1.0f, Qt::black);
		painter.setPen(bDark ? Qt::gray : Qt::darkGray);
		painter.setBrush(grad1);
		painter.drawPolygon(*m_pPolyg);
		// Sample harmonics...
		QLinearGradient grad2(0, 0, w2, h);
		grad2.setColorAt(0.0f, rgbDark);
		grad2.setColorAt(1.0f, rgbLite);
		painter.setPen(QPen(grad2, 3));
		painter.setBrush(grad1);
		for (int n = 0; n < m_nrects; ++n) {
			const QRect& rect = m_pRects[n];
			const QPoint& cpos = rect.center();
			const int x = cpos.x() + 1;
			const int y = cpos.y() + 1;
			painter.drawLine(x, h, x, y);
			painter.drawEllipse(rect.adjusted(+1, +1, -1, -1));
		}
		painter.setRenderHint(QPainter::Antialiasing, false);
	}

	painter.end();

	QFrame::paintEvent(pPaintEvent);
}


// Draggable rectangular point.
int padthv1widget_sample::nodeIndex ( const QPoint& pos ) const
{
	for (int n = 0; m_pRects && n < m_nrects; ++n) {
		const QRect& rect = m_pRects[n];
		if (rect.contains(pos))
			return n;
	}

	return -1;
}


void padthv1widget_sample::dragSelect ( const QPoint& pos )
{
	if (m_pSample == NULL)
		return;

	if (m_pRects == NULL)
		return;

	const int h = height();
//	const int w = width();

	for (int n = 0; m_pRects && n < m_nrects; ++n) {
		QRect& rect = m_pRects[n];
		if (pos.x() >= rect.left() && pos.x() < rect.right()) {
		//	const int x = rect.x();
			const int y = pos.y();
			const int h1 = h - 8;
			const float v = safe_value(float(h1 - y) / float(h1));
			m_pSample->setHarmonic(n, v);
			rect.moveTop(h1 - int(v * float(h1)));
		//	m_posDrag = pos;
			update();
			showToolTip(pos, n);
			++m_iDragged;
			break;
		}
	}
}


void padthv1widget_sample::dragNode ( const QPoint& pos )
{
	if (m_pSample == NULL)
		return;

	if (m_pRects == NULL)
		return;

	const int h  = height();
//	const int w  = width();

//	const int dx = (pos.x() - m_posDrag.x());
	const int dy = (pos.y() - m_posDrag.y());

	if (dy && m_iDragNode >= 0) {
		const int n = m_iDragNode;
		QRect& rect = m_pRects[n];
	//	const int x = rect.x();
		const int y = rect.y() + dy;
		const int h1 = h - 8;
		const float v = safe_value(float(h1 - y) / float(h1));
		m_pSample->setHarmonic(n, v);
		rect.moveTop(h1 - int(v * float(h1)));
		m_posDrag = rect.topLeft();
		update();
		showToolTip(pos, n);
		++m_iDragged;
	}
}


// Mouse interaction.
void padthv1widget_sample::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos = pMouseEvent->pos();
		m_dragState = DragStart;
		m_posDrag = pos;
		const int iDragNode = nodeIndex(pos);
		if (iDragNode >= 0) {
			m_dragCursor = DragNode;
			m_iDragNode = iDragNode;
			QFrame::setCursor(Qt::SizeVerCursor);
		}
		else
		if (pMouseEvent->modifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier)) {
			m_dragCursor = DragSelect;
			QFrame::setCursor(
				QCursor(QPixmap(":/images/editSample.png"), 5, 18));
		}
	}

	QFrame::mousePressEvent(pMouseEvent);
}


void padthv1widget_sample::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	const QPoint& pos = pMouseEvent->pos();

	switch (m_dragState) {
	case DragNone:
		if (nodeIndex(pos) >= 0) {
			m_dragCursor = DragNode;
			QFrame::setCursor(Qt::PointingHandCursor);
		}
		else
		if (m_dragCursor != DragNone)
			QFrame::unsetCursor();
		break;
	case DragSelect:
		dragSelect(pos);
		break;
	case DragNode:
		dragNode(pos);
		break;
	case DragStart:
		if ((m_posDrag - pos).manhattanLength()
			> QApplication::startDragDistance()) {
			m_dragState = m_dragCursor;
			if (m_dragState == DragNode)
				dragNode(pos);
			else
			if (pMouseEvent->modifiers()
				& (Qt::ShiftModifier | Qt::ControlModifier)) {
				dragSelect(m_posDrag);
				dragSelect(pos);
			}
		}
		// Fall thru...
	default:
		break;
	}

	QFrame::mouseMoveEvent(pMouseEvent);
}


void padthv1widget_sample::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mouseReleaseEvent(pMouseEvent);

	switch (m_dragState) {
	case DragSelect:
		dragSelect(pMouseEvent->pos());
		break;
	case DragNode:
		dragNode(pMouseEvent->pos());
		break;
	default:
		break;
	}

	if (m_iDragged > 0)
		emit sampleChanged();

	resetDragState();
}


// Special context-menu.
void padthv1widget_sample::contextMenuEvent ( QContextMenuEvent *pContextMenuEvent )
{
	QMenu menu(this);

	QMenu resetMenu(tr("&Reset"));
	resetMenu.addAction(tr("&Normal"),      this, SLOT(resetNormal()));
	resetMenu.addAction(tr("Normal &Odd"),  this, SLOT(resetNormalOdd()));
	resetMenu.addAction(tr("Normal &Even"), this, SLOT(resetNormalEven()));
	resetMenu.addSeparator();
	resetMenu.addAction(tr("&Square"),      this, SLOT(resetSquare()));
	resetMenu.addAction(tr("Sq&uare Odd"),  this, SLOT(resetSquareOdd()));
	resetMenu.addAction(tr("Squ&are Even"), this, SLOT(resetSquareEven()));
	resetMenu.addSeparator();
	resetMenu.addAction(tr("Default"),      this, SLOT(resetDefault()));
	menu.addMenu(&resetMenu);

	menu.exec(pContextMenuEvent->globalPos());
}


// Harmonic value tool-tip.
void padthv1widget_sample::showToolTip ( const QPoint& pos, int n )
{
	if (m_pSample == NULL)
		return;

	const float v = m_pSample->harmonic(n);
	QToolTip::showText(mapToGlobal(pos),
		QString("[%1]  %2").arg(n + 1).arg(v, 0, 'f', 3), this);
}


// Reset drag/select state.
void padthv1widget_sample::resetDragState (void)
{
	if (m_dragCursor != DragNone)
		QFrame::unsetCursor();

	m_iDragged  = 0;
	m_iDragNode = -1;
	m_dragState = m_dragCursor = DragNone;
}


// Trap for help/tool-tip events.
bool padthv1widget_sample::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	if (static_cast<QWidget *> (pObject) == this) {
		if (pEvent->type() == QEvent::ToolTip) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				const QPoint& pos = pHelpEvent->pos();
				const int n = nodeIndex(pos);
				if (n >= 0) {
					showToolTip(pos, n);
					return true;
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave) {
			m_iDragNode = -1;
			QFrame::unsetCursor();
			return true;
		}
	}

	// Not handled here.
	return QFrame::eventFilter(pObject, pEvent);
}


// Default size hint.
QSize padthv1widget_sample::sizeHint (void) const
{
	return QSize(240, 100);
}


// Wavetable reset options.
void padthv1widget_sample::resetDefault (void)
{
	if (m_pSample == NULL)
		return;

	m_pSample->reset_nh();

	emit sampleChanged();

}


void padthv1widget_sample::resetNormal (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = 1.0f / float(n + 1);
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}

void padthv1widget_sample::resetNormalOdd (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = ((n & 1) ? 1.667f : 1.0f) / float(n + 1);
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}

void padthv1widget_sample::resetNormalEven (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = ((n & 1) || (n < 1) ? 1.0f : 1.667f) / float(n + 1);
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}


void padthv1widget_sample::resetSquare (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = 1.0f / ::sqrtf(float(n + 1));
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}

void padthv1widget_sample::resetSquareOdd (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = ((n & 1) ? 1.667f : 1.0f) / ::sqrtf(float(n + 1));
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}

void padthv1widget_sample::resetSquareEven (void)
{
	if (m_pSample == NULL)
		return;

	const int nh = m_pSample->nh();
	for (int n = 0; n < nh; ++n) {
		const float v = ((n & 1) || (n < 1) ? 1.0f : 1.667f) / ::sqrtf(float(n + 1));
		m_pSample->setHarmonic(n, v);
	}

	emit sampleChanged();
}


// end of padthv1widget_sample.cpp
