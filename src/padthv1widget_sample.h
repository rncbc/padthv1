// padthv1widget_sample.h
//
/****************************************************************************
   Copyright (C) 2012-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __padthv1widget_sample_h
#define __padthv1widget_sample_h

#include <QFrame>

#include <stdint.h>


// Forward decl.
class padthv1_sample;


//----------------------------------------------------------------------------
// padthv1widget_sample -- Custom widget

class padthv1widget_sample : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	padthv1widget_sample(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);

	// Destructor.
	~padthv1widget_sample();

	// Parameter accessors.
	void setSample(padthv1_sample *pSample);
	padthv1_sample *sample() const;

signals:

	// Parameter change signals.
	void sampleChanged();

protected slots:

	// Wavetable reset options.
	void resetDefault();

	void resetNormal();
	void resetNormalOdd();
	void resetNormalEven();

	void resetSquare();
	void resetSquareOdd();
	void resetSquareEven();

	void resetSinc();

	// Randomize all current partials.
	void randomize();

protected:

	// Widget resize handler.
	void resizeEvent(QResizeEvent *);

	// Draw canvas.
	void paintEvent(QPaintEvent *);

	// Draggable rectangular point.
	int nodeIndex(const QPoint& pos) const;

	void dragSelect(const QPoint& pos);
	void dragNode(const QPoint& pos);

	// Mouse interaction.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Special context-menu.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Harmonic value tool-tip.
	void showToolTip(const QPoint& pos, int n);

	// Reset drag/select state.
	void resetDragState();

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// Default size hint.
	QSize sizeHint() const;

private:

	// Instance state.
	padthv1_sample *m_pSample;

	QPolygon *m_pPolyg;
	int       m_nrects;
	QRect    *m_pRects;

	// Drag state.
	enum DragState {
		DragNone = 0, DragStart, DragSelect, DragNode
	} m_dragState, m_dragCursor;

	int       m_iDragged;
	int       m_iDragNode;
	QPoint    m_posDrag;
};

#endif	// __padthv1widget_sample_h


// end of padthv1widget_sample.h
