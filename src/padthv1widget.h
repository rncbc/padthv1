// padthv1widget.h
//
/****************************************************************************
   Copyright (C) 2012-2020, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __padthv1widget_h
#define __padthv1widget_h

#include "padthv1_config.h"
#include "padthv1_sched.h"

#include "padthv1_ui.h"

#include <QWidget>


// forward decls.
namespace Ui { class padthv1widget; }

class padthv1widget_param;
class padthv1widget_sched;


//-------------------------------------------------------------------------
// padthv1widget - decl.
//

class padthv1widget : public QWidget
{
	Q_OBJECT

public:

	// Constructor
	padthv1widget(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);

	// Destructor.
	virtual ~padthv1widget();

	// Open/close the scheduler/work notifier.
	void openSchedNotifier();
	void closeSchedNotifier();

	// Param port accessors.
	void setParamValue(padthv1::ParamIndex index, float fValue);
	float paramValue(padthv1::ParamIndex index) const;

	// Param kbob (widget) mapper.
	void setParamKnob(padthv1::ParamIndex index, padthv1widget_param *pKnob);
	padthv1widget_param *paramKnob(padthv1::ParamIndex index) const;

	// Preset init.
	void initPreset();
	// Preset clear.
	void clearPreset();

	// Dirty close prompt,
	bool queryClose();

public slots:

	// Preset file I/O.
	void loadPreset(const QString& sFilename);
	void savePreset(const QString& sFilename);

	// Direct note-on/off slot.
	void directNoteOn(int iNote, int iVelocity);

protected slots:

	// Preset renewal.
	void newPreset();

	// Param knob (widget) slots.
	void paramChanged(float fValue);

	// Reset param knobs to default value.
	void resetParams();

	// Randomize params (partial).
	void randomParams();

	// Swap params A/B.
	void swapParams(bool bOn);

	// Notification updater.
	void updateSchedNotify(int stype, int sid);

	// MIDI In LED timeout.
	void midiInLedTimeout();

	// Keyboard note range change.
	void noteRangeChanged();

	// Param knob context menu.
	void paramContextMenu(const QPoint& pos);

	// Sample harmonics edits.
	void resetSample1();
	void resetSample2();

	// Menu actions.
	void helpConfigure();

	void helpAbout();
	void helpAboutQt();

protected:

	// Synth engine accessor.
	virtual padthv1_ui *ui_instance() const = 0;

	// Reset swap params A/B group.
	void resetSwapParams();

	// Initialize all param/knob values.
	void updateParamValues();

	// Reset all param/knob default values.
	void resetParamValues();
	void resetParamKnobs();

	// Sample reset (3=both).
	void clearSample(int sid = 3);

	// Sample updater (3=both).
	void updateSample(int sid = 3);

	// Param port methods.
	virtual void updateParam(padthv1::ParamIndex index, float fValue) const = 0;

	// Update local tied widgets.
	void updateParamEx(padthv1::ParamIndex index, float fValue);

	// Update scheduled controllers param/knob widgets.
	void updateSchedParam(padthv1::ParamIndex index, float fValue);

	// Preset status updater.
	void updateLoadPreset(const QString& sPreset);

	// Dirty flag (overridable virtual) methods.
	virtual void updateDirtyPreset(bool bDirtyPreset);

	// Show/hide dget handlers.
	void showEvent(QShowEvent *pShowEvent);
	void hideEvent(QHideEvent *pHideEvent);

private:

	// Instance variables.
	Ui::padthv1widget *p_ui;
	Ui::padthv1widget& m_ui;

	padthv1widget_sched *m_sched_notifier;

	QHash<padthv1::ParamIndex, padthv1widget_param *> m_paramKnobs;
	QHash<padthv1widget_param *, padthv1::ParamIndex> m_knobParams;

	float m_params_ab[padthv1::NUM_PARAMS];

	int m_iUpdate;
};


//-------------------------------------------------------------------------
// padthv1widget_sched - worker/schedule proxy decl.
//

class padthv1widget_sched : public QObject
{
	Q_OBJECT

public:

	// ctor.
	padthv1widget_sched(padthv1 *pSampl, QObject *pParent = nullptr)
		: QObject(pParent), m_notifier(pSampl, this) {}

signals:

	// Notification signal.
	void notify(int stype, int sid);

protected:

	// Notififier visitor.
	class Notifier : public padthv1_sched::Notifier
	{
	public:

		Notifier(padthv1 *pSynth, padthv1widget_sched *pSched)
			: padthv1_sched::Notifier(pSynth), m_pSched(pSched) {}

		void notify(padthv1_sched::Type stype, int sid) const
			{ m_pSched->emit_notify(stype, sid); }

	private:

		padthv1widget_sched *m_pSched;
	};

	// Notification method.
	void emit_notify(padthv1_sched::Type stype, int sid)
		{ emit notify(int(stype), sid); }

private:

	// Instance variables.
	Notifier m_notifier;
};


#endif	// __padthv1widget_h

// end of padthv1widget.h
