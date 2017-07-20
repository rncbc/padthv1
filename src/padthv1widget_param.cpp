// padthv1widget_param.cpp
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

#include "padthv1widget_param.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>

#include <QGridLayout>
#include <QMouseEvent>

#include <math.h>


// Integer value round.
inline int iroundf(float x) { return int(x < 0.0f ? x - 0.5f : x + 0.5f); }


//-------------------------------------------------------------------------
// padthv1widget_dial - A better QDial widget.

padthv1widget_dial::DialMode
padthv1widget_dial::g_dialMode = padthv1widget_dial::DefaultMode;

// Set knob dial mode behavior.
void padthv1widget_dial::setDialMode ( DialMode dialMode )
	{ g_dialMode = dialMode; }

padthv1widget_dial::DialMode padthv1widget_dial::dialMode (void)
	{ return g_dialMode; }


// Constructor.
padthv1widget_dial::padthv1widget_dial ( QWidget *pParent )
	: QDial(pParent), m_bMousePressed(false), m_fLastDragValue(0.0f)
{
}


// Mouse angle determination.
float padthv1widget_dial::mouseAngle ( const QPoint& pos )
{
	const float dx = pos.x() - (width() >> 1);
	const float dy = (height() >> 1) - pos.y();
	return 180.0f * ::atan2f(dx, dy) / float(M_PI);
}


// Alternate mouse behavior event handlers.
void padthv1widget_dial::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (g_dialMode == DefaultMode) {
		QDial::mousePressEvent(pMouseEvent);
	} else if (pMouseEvent->button() == Qt::LeftButton) {
		m_bMousePressed = true;
		m_posMouse = pMouseEvent->pos();
		m_fLastDragValue = float(value());
		emit sliderPressed();
	}
}


void padthv1widget_dial::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	if (g_dialMode == DefaultMode) {
		QDial::mouseMoveEvent(pMouseEvent);
		return;
	}

	if (!m_bMousePressed)
		return;

	const QPoint& pos = pMouseEvent->pos();
	const int dx = pos.x() - m_posMouse.x();
	const int dy = pos.y() - m_posMouse.y();
	float fAngleDelta =  mouseAngle(pos) - mouseAngle(m_posMouse);
	int iNewValue = value();

	switch (g_dialMode)	{
	case LinearMode:
		iNewValue = int(m_fLastDragValue) + dx - dy;
		break;
	case AngularMode:
	default:
		// Forget about the drag origin to be robust on full rotations
		if (fAngleDelta > +180.0f) fAngleDelta -= 360.0f;
		else
		if (fAngleDelta < -180.0f) fAngleDelta += 360.0f;
		m_fLastDragValue += float(maximum() - minimum()) * fAngleDelta / 270.0f;
		if (m_fLastDragValue > float(maximum()))
			m_fLastDragValue = float(maximum());
		else
		if (m_fLastDragValue < float(minimum()))
			m_fLastDragValue = float(minimum());
		m_posMouse = pos;
		iNewValue = int(m_fLastDragValue + 0.5f);
		break;
	}

	setValue(iNewValue);
	update();

	emit sliderMoved(value());
}


void padthv1widget_dial::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	if (g_dialMode == DefaultMode
		&& pMouseEvent->button() != Qt::MidButton) {
		QDial::mouseReleaseEvent(pMouseEvent);
	} else if (m_bMousePressed) {
		m_bMousePressed = false;
	}
}


//-------------------------------------------------------------------------
// padthv1widget_param - Custom composite widget.
//

// Constructor.
padthv1widget_param::padthv1widget_param ( QWidget *pParent ) : QWidget(pParent)
{
	const QFont& font = QWidget::font();
	const QFont font2(font.family(), font.pointSize() - 2);
	QWidget::setFont(font2);

	m_fValue = 0.0f;

	m_fMinimum = 0.0f;
	m_fMaximum = 1.0f;

	m_fScale = 1.0f;

	resetDefaultValue();

	QWidget::setMaximumSize(QSize(52, 72));

	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setMargin(0);
	pGridLayout->setSpacing(0);
	QWidget::setLayout(pGridLayout);
}


// Accessors.
void padthv1widget_param::setText ( const QString& sText )
{
	setValue(sText.toFloat());
}


QString padthv1widget_param::text (void) const
{
	return QString::number(value());
}


void padthv1widget_param::setValue ( float fValue, bool bDefault )
{
	QPalette pal;

	if (bDefault) {
		m_fDefaultValue = fValue;
		m_iDefaultValue++;
	}
	else
	if (QWidget::isEnabled()
		&& ::fabsf(fValue - m_fDefaultValue) > 0.0001f) {
		pal.setColor(QPalette::Base,
			(pal.window().color().value() < 0x7f
				? QColor(Qt::darkYellow).darker()
				: QColor(Qt::yellow).lighter()));
	}

	QWidget::setPalette(pal);

	if (::fabsf(fValue - m_fValue) > 0.0001f) {
		m_fValue = fValue;
		emit valueChanged(m_fValue);
	}
}


float padthv1widget_param::value (void) const
{
	return m_fValue;
}


QString padthv1widget_param::valueText (void) const
{
	return QString::number(value());
}


void padthv1widget_param::setMaximum ( float fMaximum )
{
	m_fMaximum = fMaximum;
}

float padthv1widget_param::maximum (void) const
{
	return m_fMaximum;
}


void padthv1widget_param::setMinimum ( float fMinimum )
{
	m_fMinimum = fMinimum;
}

float padthv1widget_param::minimum (void) const
{
	return m_fMinimum;
}


void padthv1widget_param::resetDefaultValue (void)
{
	m_fDefaultValue = 0.0f;
	m_iDefaultValue = 0;
}


bool padthv1widget_param::isDefaultValue (void) const
{
	return (m_iDefaultValue > 0);
}


void padthv1widget_param::setDefaultValue ( float fDefaultValue )
{
	m_fDefaultValue = fDefaultValue;
	m_iDefaultValue++;
}


float padthv1widget_param::defaultValue (void) const
{
	return m_fDefaultValue;
}


// Mouse behavior event handler.
void padthv1widget_param::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (pMouseEvent->button() == Qt::MidButton) {
		if (m_iDefaultValue < 1) {
			m_fDefaultValue = 0.5f * (maximum() + minimum());
			m_iDefaultValue++;
		}
		setValue(m_fDefaultValue);
	}

	QWidget::mousePressEvent(pMouseEvent);
}


// Scale multiplier accessors.
void padthv1widget_param::setScale ( float fScale )
{
	m_fScale = fScale;
}


float padthv1widget_param::scale (void) const
{
	return m_fScale;
}


// Scale/value converters.
float padthv1widget_param::scaleFromValue ( float fValue ) const
{
	return (m_fScale * fValue);
}


float padthv1widget_param::valueFromScale ( float fScale ) const
{
	return (fScale / m_fScale);
}


//-------------------------------------------------------------------------
// padthv1widget_knob - Custom knob/dial widget.
//

// Constructor.
padthv1widget_knob::padthv1widget_knob ( QWidget *pParent ) : padthv1widget_param(pParent)
{
	m_pLabel = new QLabel();
	m_pLabel->setAlignment(Qt::AlignCenter);

	m_pDial = new padthv1widget_dial();
	m_pDial->setNotchesVisible(true);
	m_pDial->setMaximumSize(QSize(48, 48));

	QGridLayout *pGridLayout
		= static_cast<QGridLayout *> (padthv1widget_param::layout());
	pGridLayout->addWidget(m_pLabel, 0, 0, 1, 3);
	pGridLayout->addWidget(m_pDial,  1, 0, 1, 3);
	pGridLayout->setAlignment(m_pDial, Qt::AlignVCenter | Qt::AlignHCenter);

	QObject::connect(m_pDial,
		SIGNAL(valueChanged(int)),
		SLOT(dialValueChanged(int)));
}


void padthv1widget_knob::setText ( const QString& sText )
{
	m_pLabel->setText(sText);
}


QString padthv1widget_knob::text (void) const
{
	return m_pLabel->text();
}


void padthv1widget_knob::setValue ( float fValue, bool bDefault )
{
	const bool bDialBlock = m_pDial->blockSignals(true);
	padthv1widget_param::setValue(fValue, bDefault);
	m_pDial->setValue(scaleFromValue(fValue));
	m_pDial->blockSignals(bDialBlock);
}


void padthv1widget_knob::setMaximum ( float fMaximum )
{
	padthv1widget_param::setMaximum(fMaximum);
	m_pDial->setMaximum(scaleFromValue(fMaximum));
}


void padthv1widget_knob::setMinimum ( float fMinimum )
{
	padthv1widget_param::setMinimum(fMinimum);
	m_pDial->setMinimum(scaleFromValue(fMinimum));
}


// Scale-step accessors.
void padthv1widget_knob::setSingleStep ( float fSingleStep )
{
	m_pDial->setSingleStep(scaleFromValue(fSingleStep));
}


float padthv1widget_knob::singleStep (void) const
{
	return valueFromScale(m_pDial->singleStep());
}


// Dial change slot.
void padthv1widget_knob::dialValueChanged ( int iDialValue )
{
	setValue(valueFromScale(iDialValue));
}


//-------------------------------------------------------------------------
// padthv1widget_edit - A better QDoubleSpinBox widget.

padthv1widget_edit::EditMode
padthv1widget_edit::g_editMode = padthv1widget_edit::DefaultMode;

// Set spin-box edit mode behavior.
void padthv1widget_edit::setEditMode ( EditMode editMode )
	{ g_editMode = editMode; }

padthv1widget_edit::EditMode padthv1widget_edit::editMode (void)
	{ return g_editMode; }


// Constructor.
padthv1widget_edit::padthv1widget_edit ( QWidget *pParent )
	: QDoubleSpinBox(pParent), m_iTextChanged(0)
{
	QObject::connect(lineEdit(),
		SIGNAL(textChanged(const QString&)),
		SLOT(lineEditTextChanged(const QString&)));
	QObject::connect(this,
		SIGNAL(editingFinished()),
		SLOT(spinBoxEditingFinished()));
	QObject::connect(this,
		SIGNAL(valueChanged(double)),
		SLOT(spinBoxValueChanged(double)));
}


// Alternate value change behavior handlers.
void padthv1widget_edit::lineEditTextChanged ( const QString& )
{
	if (g_editMode == DeferredMode)
		++m_iTextChanged;
}


void padthv1widget_edit::spinBoxEditingFinished (void)
{
	if (g_editMode == DeferredMode) {
		m_iTextChanged = 0;
		emit valueChangedEx(value());
	}
}


void padthv1widget_edit::spinBoxValueChanged ( double spinValue )
{
	if (g_editMode != DeferredMode || m_iTextChanged == 0)
		emit valueChangedEx(spinValue);
}


//-------------------------------------------------------------------------
// padthv1widget_spin - Custom knob/spin-box widget.
//

// Constructor.
padthv1widget_spin::padthv1widget_spin ( QWidget *pParent )
	: padthv1widget_knob(pParent)
{
	m_pSpinBox = new padthv1widget_edit();
	m_pSpinBox->setAccelerated(true);
	m_pSpinBox->setAlignment(Qt::AlignCenter);

	const QFontMetrics fm(padthv1widget_knob::font());
	m_pSpinBox->setMaximumHeight(fm.height() + 6);

	QGridLayout *pGridLayout
		= static_cast<QGridLayout *> (padthv1widget_knob::layout());
	pGridLayout->addWidget(m_pSpinBox, 2, 1, 1, 1);

	setScale(100.0f);

	setMinimum(0.0f);
	setMaximum(1.0f);

	setDecimals(1);

	QObject::connect(m_pSpinBox,
		SIGNAL(valueChangedEx(double)),
		SLOT(spinBoxValueChanged(double)));
}


// Virtual accessors.
void padthv1widget_spin::setValue ( float fValue, bool bDefault )
{
	const bool bSpinBlock = m_pSpinBox->blockSignals(true);
	padthv1widget_knob::setValue(fValue, bDefault);
	m_pSpinBox->setValue(scaleFromValue(fValue));
	m_pSpinBox->blockSignals(bSpinBlock);
}


void padthv1widget_spin::setMaximum ( float fMaximum )
{
	m_pSpinBox->setMaximum(scaleFromValue(fMaximum));
	padthv1widget_knob::setMaximum(fMaximum);
}


void padthv1widget_spin::setMinimum ( float fMinimum )
{
	m_pSpinBox->setMinimum(scaleFromValue(fMinimum));
	padthv1widget_knob::setMinimum(fMinimum);
}


QString padthv1widget_spin::valueText (void) const
{
	return QString::number(m_pSpinBox->value(), 'f', 1);
}


// Internal widget slots.
void padthv1widget_spin::spinBoxValueChanged ( double spinValue )
{
	padthv1widget_knob::setValue(valueFromScale(float(spinValue)));
}


// Special value text (minimum)
void padthv1widget_spin::setSpecialValueText ( const QString& sText )
{
	m_pSpinBox->setSpecialValueText(sText);
}


QString padthv1widget_spin::specialValueText (void) const
{
	return m_pSpinBox->specialValueText();
}


bool padthv1widget_spin::isSpecialValue (void) const
{
	return (m_pSpinBox->minimum() >= m_pSpinBox->value());
}


// Decimal digits allowed.
void padthv1widget_spin::setDecimals ( int iDecimals )
{
	m_pSpinBox->setDecimals(iDecimals);
	m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecimals)));

	setSingleStep(0.1f);
}

int padthv1widget_spin::decimals (void) const
{
	return m_pSpinBox->decimals();
}


//-------------------------------------------------------------------------
// padthv1widget_combo - Custom knob/combo-box widget.
//

// Constructor.
padthv1widget_combo::padthv1widget_combo ( QWidget *pParent )
	: padthv1widget_knob(pParent)
{
	m_pComboBox = new QComboBox();

	const QFontMetrics fm(padthv1widget_knob::font());
	m_pComboBox->setMaximumHeight(fm.height() + 6);

	QGridLayout *pGridLayout
		= static_cast<QGridLayout *> (padthv1widget_knob::layout());
	pGridLayout->addWidget(m_pComboBox, 2, 0, 1, 3);

//	setScale(1.0f);

	QObject::connect(m_pComboBox,
		SIGNAL(activated(int)),
		SLOT(comboBoxValueChanged(int)));
}


// Virtual accessors.
void padthv1widget_combo::setValue ( float fValue, bool bDefault )
{
	const bool bComboBlock = m_pComboBox->blockSignals(true);
	padthv1widget_knob::setValue(fValue, bDefault);
	m_pComboBox->setCurrentIndex(iroundf(fValue));
	m_pComboBox->blockSignals(bComboBlock);
}


QString padthv1widget_combo::valueText (void) const
{
	return m_pComboBox->currentText();
}


// Special combo-box mode accessors.
void padthv1widget_combo::insertItems ( int iIndex, const QStringList& items )
{
	m_pComboBox->insertItems(iIndex, items);

	setMinimum(0.0f);

	const int iItemCount = m_pComboBox->count();
	if (iItemCount > 0)
		setMaximum(float(iItemCount - 1));
	else
		setMaximum(1.0f);

	setSingleStep(1.0f);
}


void padthv1widget_combo::clear (void)
{
	m_pComboBox->clear();

	setMinimum(0.0f);
	setMaximum(1.0f);

	setSingleStep(1.0f);
}


// Internal widget slots.
void padthv1widget_combo::comboBoxValueChanged ( int iComboValue )
{
	padthv1widget_knob::setValue(float(iComboValue));
}


// Reimplemented mouse-wheel stepping.
void padthv1widget_combo::wheelEvent ( QWheelEvent *pWheelEvent )
{
	const int delta
		= (pWheelEvent->delta() / 120);
	if (delta) {
		float fValue = value() + float(delta);
		if (fValue < minimum())
			fValue = minimum();
		else
		if (fValue > maximum())
			fValue = maximum();
		setValue(fValue);
	}
}


//-------------------------------------------------------------------------
// padthv1widget_param_style - Custom widget style.
//

#include <QProxyStyle>
#include <QPainter>
#include <QIcon>


class padthv1widget_param_style : public QProxyStyle
{
public:

	// Constructor.
	padthv1widget_param_style() : QProxyStyle()
	{
		m_icon.addPixmap(
			QPixmap(":/images/ledOff.png"), QIcon::Normal, QIcon::Off);
		m_icon.addPixmap(
			QPixmap(":/images/ledOn.png"), QIcon::Normal, QIcon::On);
	}


	// Hints override.
	int styleHint(StyleHint hint, const QStyleOption *option,
		const QWidget *widget, QStyleHintReturn *retdata) const
	{
		if (hint == QStyle::SH_UnderlineShortcut)
			return 0;
		else
			return QProxyStyle::styleHint(hint, option, widget, retdata);
	}

	// Paint job.
	void drawPrimitive(PrimitiveElement element,
		const QStyleOption *option,
		QPainter *painter, const QWidget *widget) const
	{
		if (element == PE_IndicatorRadioButton ||
			element == PE_IndicatorCheckBox) {
			const QRect& rect = option->rect;
			if (option->state & State_Enabled) {
				if (option->state & State_On)
					m_icon.paint(painter, rect,
						Qt::AlignCenter, QIcon::Normal, QIcon::On);
				else
			//	if (option->state & State_Off)
					m_icon.paint(painter, rect,
						Qt::AlignCenter, QIcon::Normal, QIcon::Off);
			} else {
				m_icon.paint(painter, rect,
					Qt::AlignCenter, QIcon::Disabled, QIcon::Off);
			}
		}
		else
		QProxyStyle::drawPrimitive(element, option, painter, widget);
	}

	// Spiced up text margins
	void drawItemText(QPainter *painter, const QRect& rectangle,
		int alignment, const QPalette& palette, bool enabled,
		const QString& text, QPalette::ColorRole textRole) const
	{
		QRect rect = rectangle;
		rect.setLeft(rect.left() - 4);
		rect.setRight(rect.right() + 4);
		QProxyStyle::drawItemText(painter, rect,
			alignment, palette, enabled, text, textRole);
	}

	static void addRef ()
		{ if (++g_iRefCount == 1) g_pStyle = new padthv1widget_param_style(); }

	static void releaseRef ()
		{ if (--g_iRefCount == 0) { delete g_pStyle; g_pStyle = NULL; } }

	static padthv1widget_param_style *getRef ()
		{ return g_pStyle; }

private:

	QIcon m_icon;

	static padthv1widget_param_style *g_pStyle;
	static unsigned int g_iRefCount;
};


padthv1widget_param_style *padthv1widget_param_style::g_pStyle = NULL;
unsigned int padthv1widget_param_style::g_iRefCount = 0;


//-------------------------------------------------------------------------
// padthv1widget_radio - Custom radio/button widget.
//

// Constructor.
padthv1widget_radio::padthv1widget_radio ( QWidget *pParent )
	: padthv1widget_param(pParent), m_group(this)
{
	padthv1widget_param_style::addRef();
#if 0
	padthv1widget_param::setStyleSheet(
	//	"QRadioButton::indicator { width: 16px; height: 16px; }"
		"QRadioButton::indicator::unchecked { image: url(:/images/ledOff.png); }"
		"QRadioButton::indicator::checked   { image: url(:/images/ledOn.png);  }"
	);
#endif

	const QFont& font = padthv1widget_param::font();
	const QFont font1(font.family(), font.pointSize() - 1);
	QWidget::setFont(font1);

	QObject::connect(&m_group,
		SIGNAL(buttonClicked(int)),
		SLOT(radioGroupValueChanged(int)));
}


// Destructor.
padthv1widget_radio::~padthv1widget_radio (void)
{
	padthv1widget_param_style::releaseRef();
}


// Virtual accessors.
void padthv1widget_radio::setValue ( float fValue, bool bDefault )
{
	const int iRadioValue = iroundf(fValue);
	QRadioButton *pRadioButton
		= static_cast<QRadioButton *> (m_group.button(iRadioValue));
	if (pRadioButton) {
		const bool bRadioBlock = pRadioButton->blockSignals(true);
		padthv1widget_param::setValue(float(iRadioValue), bDefault);
		pRadioButton->setChecked(true);
		pRadioButton->blockSignals(bRadioBlock);
	}
}


QString padthv1widget_radio::valueText (void) const
{
	QString sValueText;
	const int iRadioValue = iroundf(value());
	QRadioButton *pRadioButton
		= static_cast<QRadioButton *> (m_group.button(iRadioValue));
	if (pRadioButton)
		sValueText = pRadioButton->text();
	return sValueText;
}


// Special combo-box mode accessors.
void padthv1widget_radio::insertItems ( int iIndex, const QStringList& items )
{
	QGridLayout *pGridLayout
		= static_cast<QGridLayout *> (padthv1widget_param::layout());
	const QString sToolTipMask(padthv1widget_param::toolTip() + ": %1");
	QStringListIterator iter(items);
	while (iter.hasNext()) {
		const QString& sValueText = iter.next();
		QRadioButton *pRadioButton = new QRadioButton(sValueText);
		pRadioButton->setStyle(padthv1widget_param_style::getRef());
		pRadioButton->setToolTip(sToolTipMask.arg(sValueText));
		pGridLayout->addWidget(pRadioButton, iIndex, 0);
		m_group.addButton(pRadioButton, iIndex);
		++iIndex;
	}

	setMinimum(0.0f);

	const QList<QAbstractButton *> list = m_group.buttons();
	const int iRadioCount = list.count();
	if (iRadioCount > 0)
		setMaximum(float(iRadioCount - 1));
	else
		setMaximum(1.0f);
}


void padthv1widget_radio::clear (void)
{
	const QList<QAbstractButton *> list = m_group.buttons();
	QListIterator<QAbstractButton *> iter(list);
	while (iter.hasNext()) {
		QRadioButton *pRadioButton
			= static_cast<QRadioButton *> (iter.next());
		if (pRadioButton)
			m_group.removeButton(pRadioButton);
	}

	setMinimum(0.0f);
	setMaximum(1.0f);
}


void padthv1widget_radio::radioGroupValueChanged ( int iRadioValue )
{
	padthv1widget_param::setValue(float(iRadioValue));
}


//-------------------------------------------------------------------------
// padthv1widget_check - Custom check-box widget.
//

// Constructor.
padthv1widget_check::padthv1widget_check ( QWidget *pParent )
	: padthv1widget_param(pParent)
{
	padthv1widget_param_style::addRef();
#if 0
	padthv1widget_param::setStyleSheet(
	//	"QCheckBox::indicator { width: 16px; height: 16px; }"
		"QCheckBox::indicator::unchecked { image: url(:/images/ledOff.png); }"
		"QCheckBox::indicator::checked   { image: url(:/images/ledOn.png);  }"
	);
#endif
	m_pCheckBox = new QCheckBox();
	m_pCheckBox->setStyle(padthv1widget_param_style::getRef());

	m_alignment = Qt::AlignHCenter | Qt::AlignVCenter;

	QGridLayout *pGridLayout
		= static_cast<QGridLayout *> (padthv1widget_param::layout());
	pGridLayout->addWidget(m_pCheckBox, 0, 0);
	pGridLayout->setAlignment(m_pCheckBox, m_alignment);

	padthv1widget_param::setMaximumSize(QSize(72, 72));

	QObject::connect(m_pCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(checkBoxValueChanged(bool)));
}


// Destructor.
padthv1widget_check::~padthv1widget_check (void)
{
	padthv1widget_param_style::releaseRef();
}


// Accessors.
void padthv1widget_check::setText ( const QString& sText )
{
	m_pCheckBox->setText(sText);
}


QString padthv1widget_check::text (void) const
{
	return m_pCheckBox->text();
}


void padthv1widget_check::setAlignment ( Qt::Alignment alignment )
{
	m_alignment = alignment;

	QLayout *pLayout = padthv1widget_param::layout();
	if (pLayout)
		pLayout->setAlignment(m_pCheckBox, m_alignment);
}


Qt::Alignment padthv1widget_check::alignment (void) const
{
	return m_alignment;
}


// Virtual accessors.
void padthv1widget_check::setValue ( float fValue, bool bDefault )
{
	const bool bCheckValue = (fValue > 0.5f * (maximum() + minimum()));
	const bool bCheckBlock = m_pCheckBox->blockSignals(true);
	padthv1widget_param::setValue(bCheckValue ? maximum() : minimum(), bDefault);
	m_pCheckBox->setChecked(bCheckValue);
	m_pCheckBox->blockSignals(bCheckBlock);
}


void padthv1widget_check::checkBoxValueChanged ( bool bCheckValue )
{
	padthv1widget_param::setValue(bCheckValue ? maximum() : minimum());
}


// end of padthv1widget_param.cpp
