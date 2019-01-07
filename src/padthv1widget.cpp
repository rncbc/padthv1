// padthv1widget.cpp
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

#include "padthv1widget.h"
#include "padthv1_param.h"

#include "padthv1_sample.h"
#include "padthv1_sched.h"

#include "padthv1widget_config.h"
#include "padthv1widget_control.h"

#include "padthv1widget_keybd.h"

#include "padthv1_controls.h"
#include "padthv1_programs.h"

#include "ui_padthv1widget.h"

#include <QMessageBox>
#include <QDir>
#include <QTimer>

#include <QShowEvent>
#include <QHideEvent>


//-------------------------------------------------------------------------
// padthv1widget - impl.
//

// Constructor.
padthv1widget::padthv1widget ( QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags), p_ui(new Ui::padthv1widget), m_ui(*p_ui)
{
	Q_INIT_RESOURCE(padthv1);

#if QT_VERSION >= 0x050000
	// HACK: Dark themes grayed/disabled color group fix...
	QPalette pal;
	if (pal.base().color().value() < 0x7f) {
		const QColor& color = pal.window().color();
		const int iGroups = int(QPalette::Active | QPalette::Inactive) + 1;
		for (int i = 0; i < iGroups; ++i) {
			const QPalette::ColorGroup group = QPalette::ColorGroup(i);
			pal.setBrush(group, QPalette::Light,    color.lighter(150));
			pal.setBrush(group, QPalette::Midlight, color.lighter(120));
			pal.setBrush(group, QPalette::Dark,     color.darker(150));
			pal.setBrush(group, QPalette::Mid,      color.darker(120));
			pal.setBrush(group, QPalette::Shadow,   color.darker(200));
		}
		pal.setColor(QPalette::Disabled, QPalette::ButtonText, pal.mid().color());
		QWidget::setPalette(pal);
	}
#endif

	m_ui.setupUi(this);

	// Init sched notifier.
	m_sched_notifier = NULL;

	// Init swapable params A/B to default.
	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i)
		m_params_ab[i] = padthv1_param::paramDefaultValue(padthv1::ParamIndex(i));

	// Start clean.
	m_iUpdate = 0;

	// Replicate the stacked/pages
	for (int iTab = 0; iTab < m_ui.StackedWidget->count(); ++iTab)
		m_ui.TabBar->addTab(m_ui.StackedWidget->widget(iTab)->windowTitle());

	// Note names.
	QStringList notes;
	for (int note = 0; note < 128; ++note)
		notes << padthv1_ui::noteName(note).remove(QRegExp("/\\S+"));

	m_ui.Gen1Sample1Knob->setScale(1000.0f);
	m_ui.Gen1Sample1Knob->insertItems(0, notes);

	m_ui.Gen1Sample2Knob->setScale(1000.0f);
	m_ui.Gen1Sample2Knob->insertItems(0, notes);

	// Swappable params A/B group.
	QButtonGroup *pSwapParamsGroup = new QButtonGroup(this);
	pSwapParamsGroup->addButton(m_ui.SwapParamsAButton);
	pSwapParamsGroup->addButton(m_ui.SwapParamsBButton);
	pSwapParamsGroup->setExclusive(true);
	m_ui.SwapParamsAButton->setChecked(true);

	// Sample apodizerss.
	QStringList apods;
	apods << tr("Rect");
	apods << tr("Triang");
	apods << tr("Welch");
	apods << tr("Hann");
	apods << tr("Gauss");

	m_ui.Gen1Apod1Knob->insertItems(0, apods);
	m_ui.Gen1Apod2Knob->insertItems(0, apods);

	// Wave shapes.
	QStringList shapes;
	shapes << tr("Pulse");
	shapes << tr("Saw");
	shapes << tr("Sine");
	shapes << tr("Rand");
	shapes << tr("Noise");

	m_ui.Lfo1ShapeKnob->insertItems(0, shapes);

	// Filter types.
	QStringList types;
	types << tr("LPF");
	types << tr("BPF");
	types << tr("HPF");
	types << tr("BRF");

	m_ui.Dcf1TypeKnob->insertItems(0, types);

	// Filter slopes.
	QStringList slopes;
	slopes << tr("12dB/oct");
	slopes << tr("24dB/oct");
	slopes << tr("Biquad");
	slopes << tr("Formant");

	m_ui.Dcf1SlopeKnob->insertItems(0, slopes);

	// Dynamic states.
	QStringList states;
	states << tr("Off");
	states << tr("On");

	m_ui.Lfo1SyncKnob->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);

	// Special values
	const QString& sOff = states.first();
	m_ui.Gen1Glide1Knob->setSpecialValueText(sOff);
	m_ui.Gen1Glide2Knob->setSpecialValueText(sOff);
	m_ui.Gen1RingModKnob->setSpecialValueText(sOff);
	m_ui.Cho1WetKnob->setSpecialValueText(sOff);
	m_ui.Fla1WetKnob->setSpecialValueText(sOff);
	m_ui.Pha1WetKnob->setSpecialValueText(sOff);
	m_ui.Del1WetKnob->setSpecialValueText(sOff);
	m_ui.Rev1WetKnob->setSpecialValueText(sOff);

	const QString& sAuto = tr("Auto");
	m_ui.Gen1EnvTimeKnob->setSpecialValueText(sAuto);
	m_ui.Lfo1BpmKnob->setSpecialValueText(sAuto);
	m_ui.Del1BpmKnob->setSpecialValueText(sAuto);

	// Wave integer widths.
	m_ui.Lfo1WidthKnob->setDecimals(0);

	// GEN note limits.
	m_ui.Gen1Sample1Knob->setMinimum(0.0f);
	m_ui.Gen1Sample1Knob->setMaximum(127.0f);
	m_ui.Gen1Sample2Knob->setMinimum(0.0f);
	m_ui.Gen1Sample2Knob->setMaximum(127.0f);

	m_ui.Gen1Width1Knob->setScale(1.0f);
	m_ui.Gen1Width1Knob->setMinimum(2.0f);
	m_ui.Gen1Width1Knob->setMaximum(200.0f);
	m_ui.Gen1Width2Knob->setScale(1.0f);
	m_ui.Gen1Width2Knob->setMinimum(2.0f);
	m_ui.Gen1Width2Knob->setMaximum(200.0f);

	m_ui.Gen1Scale1Knob->setMinimum(-1.0f);
	m_ui.Gen1Scale1Knob->setMaximum(+1.0f);
	m_ui.Gen1Scale2Knob->setMinimum(-1.0f);
	m_ui.Gen1Scale2Knob->setMaximum(+1.0f);

	m_ui.Gen1Nh1Knob->setScale(1.0f);
	m_ui.Gen1Nh1Knob->setDecimals(0);
	m_ui.Gen1Nh1Knob->setMinimum(2.0f);
	m_ui.Gen1Nh1Knob->setMaximum(64.0f);
	m_ui.Gen1Nh2Knob->setScale(1.0f);
	m_ui.Gen1Nh2Knob->setDecimals(0);
	m_ui.Gen1Nh2Knob->setMinimum(2.0f);
	m_ui.Gen1Nh2Knob->setMaximum(64.0f);

	m_ui.Gen1Detune1Knob->setMinimum(-1.0f);
	m_ui.Gen1Detune1Knob->setMaximum(+1.0f);
	m_ui.Gen1Detune2Knob->setMinimum(-1.0f);
	m_ui.Gen1Detune2Knob->setMaximum(+1.0f);

	// GEN octave limits.
	m_ui.Gen1OctaveKnob->setMinimum(-4.0f);
	m_ui.Gen1OctaveKnob->setMaximum(+4.0f);

	// GEN balance limits.
	m_ui.Gen1BalanceKnob->setMinimum(-1.0f);
	m_ui.Gen1BalanceKnob->setMaximum(+1.0f);

	// GEN tune limits.
	m_ui.Gen1TuningKnob->setMinimum(-1.0f);
	m_ui.Gen1TuningKnob->setMaximum(+1.0f);

	// DCF volume (env.amount) limits.
	m_ui.Dcf1EnvelopeKnob->setMinimum(-1.0f);
	m_ui.Dcf1EnvelopeKnob->setMaximum(+1.0f);

	// LFO parameter limits.
	m_ui.Lfo1BpmKnob->setScale(1.0f);
	m_ui.Lfo1BpmKnob->setMinimum(0.0f);
	m_ui.Lfo1BpmKnob->setMaximum(360.0f);
//	m_ui.Lfo1BpmKnob->setSingleStep(1.0f);
	m_ui.Lfo1SweepKnob->setMinimum(-1.0f);
	m_ui.Lfo1SweepKnob->setMaximum(+1.0f);
	m_ui.Lfo1PitchKnob->setMinimum(-1.0f);
	m_ui.Lfo1PitchKnob->setMaximum(+1.0f);
	m_ui.Lfo1BalanceKnob->setMinimum(-1.0f);
	m_ui.Lfo1BalanceKnob->setMaximum(+1.0f);
	m_ui.Lfo1RingModKnob->setMinimum(-1.0f);
	m_ui.Lfo1RingModKnob->setMaximum(+1.0f);
	m_ui.Lfo1CutoffKnob->setMinimum(-1.0f);
	m_ui.Lfo1CutoffKnob->setMaximum(+1.0f);
	m_ui.Lfo1ResoKnob->setMinimum(-1.0f);
	m_ui.Lfo1ResoKnob->setMaximum(+1.0f);
	m_ui.Lfo1PanningKnob->setMinimum(-1.0f);
	m_ui.Lfo1PanningKnob->setMaximum(+1.0f);
	m_ui.Lfo1VolumeKnob->setMinimum(-1.0f);
	m_ui.Lfo1VolumeKnob->setMaximum(+1.0f);

	// Channel filters
	QStringList channels;
	channels << tr("Omni");
	for (int iChannel = 0; iChannel < 16; ++iChannel)
		channels << QString::number(iChannel + 1);

	m_ui.Def1ChannelKnob->insertItems(0, channels);

	// Mono switches.
	QStringList modes;
	modes << tr("Poly");
	modes << tr("Mono");
	modes << tr("Legato");

	m_ui.Def1MonoKnob->insertItems(0, modes);

	// Output (stereo-)width limits.
	m_ui.Out1WidthKnob->setMinimum(-1.0f);
	m_ui.Out1WidthKnob->setMaximum(+1.0f);

	// Output (stereo-)panning limits.
	m_ui.Out1PanningKnob->setMinimum(-1.0f);
	m_ui.Out1PanningKnob->setMaximum(+1.0f);

	// Effects (delay BPM)
	m_ui.Del1BpmKnob->setScale(1.0f);
	m_ui.Del1BpmKnob->setMinimum(0.0f);
	m_ui.Del1BpmKnob->setMaximum(360.0f);
//	m_ui.Del1BpmKnob->setSingleStep(1.0f);

	// Reverb (stereo-)width limits.
	m_ui.Rev1WidthKnob->setMinimum(-1.0f);
	m_ui.Rev1WidthKnob->setMaximum(+1.0f);

	// GEN1
	setParamKnob(padthv1::GEN1_SAMPLE1, m_ui.Gen1Sample1Knob);
	setParamKnob(padthv1::GEN1_WIDTH1,  m_ui.Gen1Width1Knob);
	setParamKnob(padthv1::GEN1_SCALE1,  m_ui.Gen1Scale1Knob);
	setParamKnob(padthv1::GEN1_NH1,     m_ui.Gen1Nh1Knob);
	setParamKnob(padthv1::GEN1_APOD1,   m_ui.Gen1Apod1Knob);
	setParamKnob(padthv1::GEN1_DETUNE1, m_ui.Gen1Detune1Knob);
	setParamKnob(padthv1::GEN1_GLIDE1,  m_ui.Gen1Glide1Knob);
	setParamKnob(padthv1::GEN1_SAMPLE2, m_ui.Gen1Sample2Knob);
	setParamKnob(padthv1::GEN1_WIDTH2,  m_ui.Gen1Width2Knob);
	setParamKnob(padthv1::GEN1_SCALE2,  m_ui.Gen1Scale2Knob);
	setParamKnob(padthv1::GEN1_NH2,     m_ui.Gen1Nh2Knob);
	setParamKnob(padthv1::GEN1_APOD2,   m_ui.Gen1Apod2Knob);
	setParamKnob(padthv1::GEN1_DETUNE2, m_ui.Gen1Detune2Knob);
	setParamKnob(padthv1::GEN1_GLIDE2,  m_ui.Gen1Glide2Knob);
	setParamKnob(padthv1::GEN1_BALANCE, m_ui.Gen1BalanceKnob);
	setParamKnob(padthv1::GEN1_PHASE,   m_ui.Gen1PhaseKnob);
	setParamKnob(padthv1::GEN1_RINGMOD, m_ui.Gen1RingModKnob);
	setParamKnob(padthv1::GEN1_OCTAVE,  m_ui.Gen1OctaveKnob);
	setParamKnob(padthv1::GEN1_TUNING,  m_ui.Gen1TuningKnob);
	setParamKnob(padthv1::GEN1_ENVTIME, m_ui.Gen1EnvTimeKnob);

	// DCF1
	setParamKnob(padthv1::DCF1_CUTOFF,   m_ui.Dcf1CutoffKnob);
	setParamKnob(padthv1::DCF1_RESO,     m_ui.Dcf1ResoKnob);
	setParamKnob(padthv1::DCF1_TYPE,     m_ui.Dcf1TypeKnob);
	setParamKnob(padthv1::DCF1_SLOPE,    m_ui.Dcf1SlopeKnob);
	setParamKnob(padthv1::DCF1_ENVELOPE, m_ui.Dcf1EnvelopeKnob);
	setParamKnob(padthv1::DCF1_ATTACK,   m_ui.Dcf1AttackKnob);
	setParamKnob(padthv1::DCF1_DECAY,    m_ui.Dcf1DecayKnob);
	setParamKnob(padthv1::DCF1_SUSTAIN,  m_ui.Dcf1SustainKnob);
	setParamKnob(padthv1::DCF1_RELEASE,  m_ui.Dcf1ReleaseKnob);

	QObject::connect(
		m_ui.Dcf1Filt, SIGNAL(cutoffChanged(float)),
		m_ui.Dcf1CutoffKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1CutoffKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Filt, SLOT(setCutoff(float)));

	QObject::connect(
		m_ui.Dcf1Filt, SIGNAL(resoChanged(float)),
		m_ui.Dcf1ResoKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1ResoKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Filt, SLOT(setReso(float)));

	QObject::connect(
		m_ui.Dcf1TypeKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Filt, SLOT(setType(float)));
	QObject::connect(
		m_ui.Dcf1SlopeKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Filt, SLOT(setSlope(float)));

	QObject::connect(
		m_ui.Dcf1Env, SIGNAL(attackChanged(float)),
		m_ui.Dcf1AttackKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1AttackKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Env, SLOT(setAttack(float)));

	QObject::connect(
		m_ui.Dcf1Env, SIGNAL(decayChanged(float)),
		m_ui.Dcf1DecayKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1DecayKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Env, SLOT(setDecay(float)));

	QObject::connect(
		m_ui.Dcf1Env, SIGNAL(sustainChanged(float)),
		m_ui.Dcf1SustainKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1SustainKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Env, SLOT(setSustain(float)));

	QObject::connect(
		m_ui.Dcf1Env, SIGNAL(releaseChanged(float)),
		m_ui.Dcf1ReleaseKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dcf1ReleaseKnob, SIGNAL(valueChanged(float)),
		m_ui.Dcf1Env, SLOT(setRelease(float)));

	// LFO1
	setParamKnob(padthv1::LFO1_SHAPE,   m_ui.Lfo1ShapeKnob);
	setParamKnob(padthv1::LFO1_WIDTH,   m_ui.Lfo1WidthKnob);
	setParamKnob(padthv1::LFO1_BPM,     m_ui.Lfo1BpmKnob);
	setParamKnob(padthv1::LFO1_RATE,    m_ui.Lfo1RateKnob);
	setParamKnob(padthv1::LFO1_SYNC,    m_ui.Lfo1SyncKnob);
	setParamKnob(padthv1::LFO1_SWEEP,   m_ui.Lfo1SweepKnob);
	setParamKnob(padthv1::LFO1_PITCH,   m_ui.Lfo1PitchKnob);
	setParamKnob(padthv1::LFO1_BALANCE, m_ui.Lfo1BalanceKnob);
	setParamKnob(padthv1::LFO1_RINGMOD, m_ui.Lfo1RingModKnob);
	setParamKnob(padthv1::LFO1_CUTOFF,  m_ui.Lfo1CutoffKnob);
	setParamKnob(padthv1::LFO1_RESO,    m_ui.Lfo1ResoKnob);
	setParamKnob(padthv1::LFO1_PANNING, m_ui.Lfo1PanningKnob);
	setParamKnob(padthv1::LFO1_VOLUME,  m_ui.Lfo1VolumeKnob);
	setParamKnob(padthv1::LFO1_ATTACK,  m_ui.Lfo1AttackKnob);
	setParamKnob(padthv1::LFO1_DECAY,   m_ui.Lfo1DecayKnob);
	setParamKnob(padthv1::LFO1_SUSTAIN, m_ui.Lfo1SustainKnob);
	setParamKnob(padthv1::LFO1_RELEASE, m_ui.Lfo1ReleaseKnob);

	QObject::connect(
		m_ui.Lfo1ShapeKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Wave, SLOT(setWaveShape(float)));
	QObject::connect(
		m_ui.Lfo1Wave, SIGNAL(waveShapeChanged(float)),
		m_ui.Lfo1ShapeKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Lfo1WidthKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Wave, SLOT(setWaveWidth(float)));
	QObject::connect(
		m_ui.Lfo1Wave, SIGNAL(waveWidthChanged(float)),
		m_ui.Lfo1WidthKnob, SLOT(setValue(float)));

	QObject::connect(
		m_ui.Lfo1Env, SIGNAL(attackChanged(float)),
		m_ui.Lfo1AttackKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Lfo1AttackKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Env, SLOT(setAttack(float)));

	QObject::connect(
		m_ui.Lfo1Env, SIGNAL(decayChanged(float)),
		m_ui.Lfo1DecayKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Lfo1DecayKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Env, SLOT(setDecay(float)));

	QObject::connect(
		m_ui.Lfo1Env, SIGNAL(sustainChanged(float)),
		m_ui.Lfo1SustainKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Lfo1SustainKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Env, SLOT(setSustain(float)));

	QObject::connect(
		m_ui.Lfo1Env, SIGNAL(releaseChanged(float)),
		m_ui.Lfo1ReleaseKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Lfo1ReleaseKnob, SIGNAL(valueChanged(float)),
		m_ui.Lfo1Env, SLOT(setRelease(float)));

	// DCA1
	setParamKnob(padthv1::DCA1_VOLUME,  m_ui.Dca1VolumeKnob);
	setParamKnob(padthv1::DCA1_ATTACK,  m_ui.Dca1AttackKnob);
	setParamKnob(padthv1::DCA1_DECAY,   m_ui.Dca1DecayKnob);
	setParamKnob(padthv1::DCA1_SUSTAIN, m_ui.Dca1SustainKnob);
	setParamKnob(padthv1::DCA1_RELEASE, m_ui.Dca1ReleaseKnob);

	QObject::connect(
		m_ui.Dca1Env, SIGNAL(attackChanged(float)),
		m_ui.Dca1AttackKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dca1AttackKnob, SIGNAL(valueChanged(float)),
		m_ui.Dca1Env, SLOT(setAttack(float)));

	QObject::connect(
		m_ui.Dca1Env, SIGNAL(decayChanged(float)),
		m_ui.Dca1DecayKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dca1DecayKnob, SIGNAL(valueChanged(float)),
		m_ui.Dca1Env, SLOT(setDecay(float)));

	QObject::connect(
		m_ui.Dca1Env, SIGNAL(sustainChanged(float)),
		m_ui.Dca1SustainKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dca1SustainKnob, SIGNAL(valueChanged(float)),
		m_ui.Dca1Env, SLOT(setSustain(float)));

	QObject::connect(
		m_ui.Dca1Env, SIGNAL(releaseChanged(float)),
		m_ui.Dca1ReleaseKnob, SLOT(setValue(float)));
	QObject::connect(
		m_ui.Dca1ReleaseKnob, SIGNAL(valueChanged(float)),
		m_ui.Dca1Env, SLOT(setRelease(float)));

	// DEF1
	setParamKnob(padthv1::DEF1_PITCHBEND, m_ui.Def1PitchbendKnob);
	setParamKnob(padthv1::DEF1_MODWHEEL,  m_ui.Def1ModwheelKnob);
	setParamKnob(padthv1::DEF1_PRESSURE,  m_ui.Def1PressureKnob);
	setParamKnob(padthv1::DEF1_VELOCITY,  m_ui.Def1VelocityKnob);
	setParamKnob(padthv1::DEF1_CHANNEL,   m_ui.Def1ChannelKnob);
	setParamKnob(padthv1::DEF1_MONO,      m_ui.Def1MonoKnob);

	// OUT1
	setParamKnob(padthv1::OUT1_WIDTH,   m_ui.Out1WidthKnob);
	setParamKnob(padthv1::OUT1_PANNING, m_ui.Out1PanningKnob);
	setParamKnob(padthv1::OUT1_FXSEND,  m_ui.Out1FxSendKnob);
	setParamKnob(padthv1::OUT1_VOLUME,  m_ui.Out1VolumeKnob);

	// Make status-bar keyboard range active.
	m_ui.StatusBar->keybd()->setNoteRange(true);

	// Effects
	setParamKnob(padthv1::CHO1_WET,   m_ui.Cho1WetKnob);
	setParamKnob(padthv1::CHO1_DELAY, m_ui.Cho1DelayKnob);
	setParamKnob(padthv1::CHO1_FEEDB, m_ui.Cho1FeedbKnob);
	setParamKnob(padthv1::CHO1_RATE,  m_ui.Cho1RateKnob);
	setParamKnob(padthv1::CHO1_MOD,   m_ui.Cho1ModKnob);

	setParamKnob(padthv1::FLA1_WET,   m_ui.Fla1WetKnob);
	setParamKnob(padthv1::FLA1_DELAY, m_ui.Fla1DelayKnob);
	setParamKnob(padthv1::FLA1_FEEDB, m_ui.Fla1FeedbKnob);
	setParamKnob(padthv1::FLA1_DAFT,  m_ui.Fla1DaftKnob);

	setParamKnob(padthv1::PHA1_WET,   m_ui.Pha1WetKnob);
	setParamKnob(padthv1::PHA1_RATE,  m_ui.Pha1RateKnob);
	setParamKnob(padthv1::PHA1_FEEDB, m_ui.Pha1FeedbKnob);
	setParamKnob(padthv1::PHA1_DEPTH, m_ui.Pha1DepthKnob);
	setParamKnob(padthv1::PHA1_DAFT,  m_ui.Pha1DaftKnob);

	setParamKnob(padthv1::DEL1_WET,   m_ui.Del1WetKnob);
	setParamKnob(padthv1::DEL1_DELAY, m_ui.Del1DelayKnob);
	setParamKnob(padthv1::DEL1_FEEDB, m_ui.Del1FeedbKnob);
	setParamKnob(padthv1::DEL1_BPM,   m_ui.Del1BpmKnob);

	// Reverb
	setParamKnob(padthv1::REV1_WET,   m_ui.Rev1WetKnob);
	setParamKnob(padthv1::REV1_ROOM,  m_ui.Rev1RoomKnob);
	setParamKnob(padthv1::REV1_DAMP,  m_ui.Rev1DampKnob);
	setParamKnob(padthv1::REV1_FEEDB, m_ui.Rev1FeedbKnob);
	setParamKnob(padthv1::REV1_WIDTH, m_ui.Rev1WidthKnob);

	// Dynamics
	setParamKnob(padthv1::DYN1_COMPRESS, m_ui.Dyn1CompressKnob);
	setParamKnob(padthv1::DYN1_LIMITER,  m_ui.Dyn1LimiterKnob);


	// Preset management
	QObject::connect(m_ui.Preset,
		SIGNAL(newPresetFile()),
		SLOT(newPreset()));
	QObject::connect(m_ui.Preset,
		SIGNAL(loadPresetFile(const QString&)),
		SLOT(loadPreset(const QString&)));
	QObject::connect(m_ui.Preset,
		SIGNAL(savePresetFile(const QString&)),
		SLOT(savePreset(const QString&)));
	QObject::connect(m_ui.Preset,
		SIGNAL(resetPresetFile()),
		SLOT(resetParams()));

	// Swap params A/B
	QObject::connect(m_ui.SwapParamsAButton,
		SIGNAL(toggled(bool)),
		SLOT(swapParams(bool)));
	QObject::connect(m_ui.SwapParamsBButton,
		SIGNAL(toggled(bool)),
		SLOT(swapParams(bool)));

	QObject::connect(m_ui.Gen1Sample1,
		SIGNAL(sampleChanged()),
		SLOT(resetSample1()));
	QObject::connect(m_ui.Gen1Sample2,
		SIGNAL(sampleChanged()),
		SLOT(resetSample2()));

	// Direct stacked-page signal/slot
	QObject::connect(m_ui.TabBar, SIGNAL(currentChanged(int)),
		m_ui.StackedWidget, SLOT(setCurrentIndex(int)));

	// Direct status-bar keyboard input
	QObject::connect(m_ui.StatusBar->keybd(),
		SIGNAL(noteOnClicked(int, int)),
		SLOT(directNoteOn(int, int)));
	QObject::connect(m_ui.StatusBar->keybd(),
		SIGNAL(noteRangeChanged()),
		SLOT(noteRangeChanged()));

	// Menu actions
	QObject::connect(m_ui.helpConfigureAction,
		SIGNAL(triggered(bool)),
		SLOT(helpConfigure()));
	QObject::connect(m_ui.helpAboutAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAbout()));
	QObject::connect(m_ui.helpAboutQtAction,
		SIGNAL(triggered(bool)),
		SLOT(helpAboutQt()));

	// General knob/dial  behavior init...
	padthv1_config *pConfig = padthv1_config::getInstance();
	if (pConfig) {
		padthv1widget_dial::setDialMode(
			padthv1widget_dial::DialMode(pConfig->iKnobDialMode));
		padthv1widget_edit::setEditMode(
			padthv1widget_edit::EditMode(pConfig->iKnobEditMode));
	}

	// Epilog.
	// QWidget::adjustSize();

	m_ui.StatusBar->showMessage(tr("Ready"), 5000);
	m_ui.StatusBar->modified(false);
	m_ui.Preset->setDirtyPreset(false);
}


// Destructor.
padthv1widget::~padthv1widget (void)
{
	if (m_sched_notifier)
		delete m_sched_notifier;

	delete p_ui;
}


// Open/close the scheduler/work notifier.
void padthv1widget::openSchedNotifier (void)
{
	if (m_sched_notifier)
		return;

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	m_sched_notifier = new padthv1widget_sched(pSynthUi->instance(), this);

	QObject::connect(m_sched_notifier,
		SIGNAL(notify(int, int)),
		SLOT(updateSchedNotify(int, int)));

	pSynthUi->midiInEnabled(true);
}


void padthv1widget::closeSchedNotifier (void)
{
	if (m_sched_notifier) {
		delete m_sched_notifier;
		m_sched_notifier = NULL;
	}

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->midiInEnabled(false);
}


// Show/hide widget handlers.
void padthv1widget::showEvent ( QShowEvent *pShowEvent )
{
	QWidget::showEvent(pShowEvent);

	openSchedNotifier();
}


void padthv1widget::hideEvent ( QHideEvent *pHideEvent )
{
	closeSchedNotifier();

	QWidget::hideEvent(pHideEvent);
}


// Param kbob (widget) map accesors.
void padthv1widget::setParamKnob ( padthv1::ParamIndex index, padthv1widget_param *pParam )
{
	pParam->setDefaultValue(padthv1_param::paramDefaultValue(index));

	m_paramKnobs.insert(index, pParam);
	m_knobParams.insert(pParam, index);

	QObject::connect(pParam,
		SIGNAL(valueChanged(float)),
		SLOT(paramChanged(float)));

	pParam->setContextMenuPolicy(Qt::CustomContextMenu);

	QObject::connect(pParam,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(paramContextMenu(const QPoint&)));
}

padthv1widget_param *padthv1widget::paramKnob ( padthv1::ParamIndex index ) const
{
	return m_paramKnobs.value(index, NULL);
}


// Param port accessors.
void padthv1widget::setParamValue ( padthv1::ParamIndex index, float fValue )
{
	++m_iUpdate;

	padthv1widget_param *pParam = paramKnob(index);
	if (pParam)
		pParam->setValue(fValue);

	updateParamEx(index, fValue);

	--m_iUpdate;
}

float padthv1widget::paramValue ( padthv1::ParamIndex index ) const
{
	float fValue = 0.0f;

	padthv1widget_param *pParam = paramKnob(index);
	if (pParam) {
		fValue = pParam->value();
	} else {
		padthv1_ui *pSynthUi = ui_instance();
		if (pSynthUi)
			fValue = pSynthUi->paramValue(index);
	}

	return fValue;
}


// Param knob (widget) slot.
void padthv1widget::paramChanged ( float fValue )
{
	if (m_iUpdate > 0)
		return;

	padthv1widget_param *pParam = qobject_cast<padthv1widget_param *> (sender());
	if (pParam) {
		const padthv1::ParamIndex index = m_knobParams.value(pParam);
		updateParam(index, fValue);
		updateParamEx(index, fValue);
		m_ui.StatusBar->showMessage(QString("%1: %2")
			.arg(pParam->toolTip())
			.arg(pParam->valueText()), 5000);
		updateDirtyPreset(true);
	}
}


// Update local tied widgets.
void padthv1widget::updateParamEx ( padthv1::ParamIndex index, float fValue )
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	++m_iUpdate;

	switch (index) {
	case padthv1::DCF1_SLOPE:
		m_ui.Dcf1TypeKnob->setEnabled(int(fValue) != 3); // !Formant
		break;
	case padthv1::KEY1_LOW:
		m_ui.StatusBar->keybd()->setNoteLow(int(fValue));
		break;
	case padthv1::KEY1_HIGH:
		m_ui.StatusBar->keybd()->setNoteHigh(int(fValue));
		// Fall thru...
	default:
		break;
	}

	--m_iUpdate;
}


// Update scheduled controllers param/knob widgets.
void padthv1widget::updateSchedParam ( padthv1::ParamIndex index, float fValue )
{
	++m_iUpdate;

	padthv1widget_param *pParam = paramKnob(index);
	if (pParam) {
		pParam->setValue(fValue);
		updateParam(index, fValue);
		updateParamEx(index, fValue);
		m_ui.StatusBar->showMessage(QString("%1: %2")
			.arg(pParam->toolTip())
			.arg(pParam->valueText()), 5000);
		updateDirtyPreset(true);
	}

	--m_iUpdate;
}


// Reset all param knobs to default values.
void padthv1widget::resetParams (void)
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	pSynthUi->reset();

	resetSwapParams();

	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		const padthv1::ParamIndex index = padthv1::ParamIndex(i);
		float fValue = padthv1_param::paramDefaultValue(index);
		padthv1widget_param *pParam = paramKnob(index);
		if (pParam && pParam->isDefaultValue())
			fValue = pParam->defaultValue();
		setParamValue(index, fValue);
		updateParam(index, fValue);
	//	updateParamEx(index, fValue);
		m_params_ab[i] = fValue;
	}

	m_ui.StatusBar->showMessage(tr("Reset preset"), 5000);
	updateDirtyPreset(false);
}


// Swap params A/B.
void padthv1widget::swapParams ( bool bOn )
{
	if (m_iUpdate > 0 || !bOn)
		return;

#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::swapParams(%d)", int(bOn));
#endif

	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		const padthv1::ParamIndex index = padthv1::ParamIndex(i);
		padthv1widget_param *pParam = paramKnob(index);
		if (pParam) {
			const float fOldValue = pParam->value();
			const float fNewValue = m_params_ab[i];
			setParamValue(index, fNewValue);
			updateParam(index, fNewValue);
			m_params_ab[i] = fOldValue;
		}
	}

	const bool bSwapA = m_ui.SwapParamsAButton->isChecked();
	m_ui.StatusBar->showMessage(tr("Swap %1").arg(bSwapA ? 'A' : 'B'), 5000);
	updateDirtyPreset(true);
}


// Reset swap params A/B group.
void padthv1widget::resetSwapParams (void)
{
	++m_iUpdate;
	m_ui.SwapParamsAButton->setChecked(true);
	--m_iUpdate;
}


// Initialize param values.
void padthv1widget::updateParamValues (void)
{
	resetSwapParams();

	padthv1_ui *pSynthUi = ui_instance();

	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		const padthv1::ParamIndex index = padthv1::ParamIndex(i);
		const float fValue = (pSynthUi
			? pSynthUi->paramValue(index)
			: padthv1_param::paramDefaultValue(index));
		setParamValue(index, fValue);
		updateParam(index, fValue);
	//	updateParamEx(index, fValue);
		m_params_ab[i] = fValue;
	}
}


// Reset all param default values.
void padthv1widget::resetParamValues (void)
{
	resetSwapParams();

	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		const padthv1::ParamIndex index = padthv1::ParamIndex(i);
		const float fValue = padthv1_param::paramDefaultValue(index);
		setParamValue(index, fValue);
		updateParam(index, fValue);
	//	updateParamEx(index, fValue);
		m_params_ab[i] = fValue;
	}
}


// Reset all knob default values.
void padthv1widget::resetParamKnobs (void)
{
	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		padthv1widget_param *pParam = paramKnob(padthv1::ParamIndex(i));
		if (pParam)
			pParam->resetDefaultValue();
	}
}


// Preset init.
void padthv1widget::initPreset (void)
{
	m_ui.Preset->initPreset();
}


// Preset clear.
void padthv1widget::clearPreset (void)
{
	m_ui.Preset->clearPreset();
}


// Preset renewal.
void padthv1widget::newPreset (void)
{
#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::newPreset()");
#endif

	clearSample();

	resetParamKnobs();
	resetParamValues();

	m_ui.StatusBar->showMessage(tr("New preset"), 5000);
	updateDirtyPreset(false);
}


// Preset file I/O slots.
void padthv1widget::loadPreset ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::loadPreset(\"%s\")", sFilename.toUtf8().constData());
#endif

	clearSample();

	resetParamKnobs();
	resetParamValues();

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->loadPreset(sFilename);

	updateLoadPreset(QFileInfo(sFilename).completeBaseName());
}


void padthv1widget::savePreset ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::savePreset(\"%s\")", sFilename.toUtf8().constData());
#endif

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->savePreset(sFilename);

	const QString& sPreset
		= QFileInfo(sFilename).completeBaseName();

	m_ui.StatusBar->showMessage(tr("Save preset: %1").arg(sPreset), 5000);
	updateDirtyPreset(false);
}


// Sample updater (sid: 3=both).
void padthv1widget::clearSample ( int sid )
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	if (sid & 1)
		pSynthUi->sample(1)->reset_nh();
	if (sid & 2)
		pSynthUi->sample(2)->reset_nh();
}


// Sample updater (sid: 3=both).
void padthv1widget::updateSample ( int sid )
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	if (sid & 1)
		m_ui.Gen1Sample1->setSample(pSynthUi->sample(1));
	if (sid & 2)
		m_ui.Gen1Sample2->setSample(pSynthUi->sample(2));
}


// Dirty close prompt,
bool padthv1widget::queryClose (void)
{
	return m_ui.Preset->queryPreset();
}


// Preset status updater.
void padthv1widget::updateLoadPreset ( const QString& sPreset )
{
	resetParamKnobs();
	updateParamValues();

	m_ui.Preset->setPreset(sPreset);
	m_ui.StatusBar->showMessage(tr("Load preset: %1").arg(sPreset), 5000);
	updateDirtyPreset(false);
}


// Notification updater.
void padthv1widget::updateSchedNotify ( int stype, int sid )
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("padthv1widget::updateSchedNotify(%d, 0x%04x)", stype, sid);
#endif

	switch (padthv1_sched::Type(stype)) {
	case padthv1_sched::MidiIn:
		if (sid >= 0) {
			const int key = (sid & 0x7f);
			const int vel = (sid >> 7) & 0x7f;
			m_ui.StatusBar->midiInNote(key, vel);
		}
		else
		if (pSynthUi->midiInCount() > 0) {
			m_ui.StatusBar->midiInLed(true);
			QTimer::singleShot(200, this, SLOT(midiInLedTimeout()));
		}
		break;
	case padthv1_sched::Controller: {
		padthv1widget_control *pInstance
			= padthv1widget_control::getInstance();
		if (pInstance) {
			padthv1_controls *pControls = pSynthUi->controls();
			pInstance->setControlKey(pControls->current_key());
		}
		break;
	}
	case padthv1_sched::Controls: {
		const padthv1::ParamIndex index = padthv1::ParamIndex(sid);
		updateSchedParam(index, pSynthUi->paramValue(index));
		break;
	}
	case padthv1_sched::Programs: {
		padthv1_programs *pPrograms = pSynthUi->programs();
		padthv1_programs::Prog *pProg = pPrograms->current_prog();
		if (pProg) updateLoadPreset(pProg->name());
		break;
	}
	case padthv1_sched::Sample:
		updateSample(sid);
		if (sid > 2) {
			updateParamValues();
			resetParamKnobs();
			updateDirtyPreset(false);
		}
		// Fall thru...
	default:
		break;
	}
}


// Direct note-on/off slot.
void padthv1widget::directNoteOn ( int iNote, int iVelocity )
{
#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::directNoteOn(%d, %d)", iNote, iVelocity);
#endif

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->directNoteOn(iNote, iVelocity); // note-on!
}


// Keyboard note range change.
void padthv1widget::noteRangeChanged (void)
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	const int iNoteLow  = m_ui.StatusBar->keybd()->noteLow();
	const int iNoteHigh = m_ui.StatusBar->keybd()->noteHigh();

#ifdef CONFIG_DEBUG
	qDebug("padthv1widget::noteRangeChanged(%d, %d)", iNoteLow, iNoteHigh);
#endif

	pSynthUi->setParamValue(padthv1::KEY1_LOW,  float(iNoteLow));
	pSynthUi->setParamValue(padthv1::KEY1_HIGH, float(iNoteHigh));

	m_ui.StatusBar->showMessage(QString("KEY Low: %1 (%2) High: %3 (%4)")
		.arg(padthv1_ui::noteName(iNoteLow)).arg(iNoteLow)
		.arg(padthv1_ui::noteName(iNoteHigh)).arg(iNoteHigh), 5000);

	updateDirtyPreset(true);
}


// MIDI In LED timeout.
void padthv1widget::midiInLedTimeout (void)
{
	m_ui.StatusBar->midiInLed(false);
}


// Menu actions.
void padthv1widget::helpConfigure (void)
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	padthv1widget_config(pSynthUi, this).exec();
}


void padthv1widget::helpAbout (void)
{
	// About...
	QStringList list;
#ifdef CONFIG_DEBUG
	list << tr("Debugging option enabled.");
#endif
#ifndef CONFIG_JACK
	list << tr("JACK stand-alone build disabled.");
#endif
#ifndef CONFIG_JACK_SESSION
	list << tr("JACK session support disabled.");
#endif
#ifndef CONFIG_JACK_MIDI
	list << tr("JACK MIDI support disabled.");
#endif
#ifndef CONFIG_ALSA_MIDI
	list << tr("ALSA MIDI support disabled.");
#endif
#ifndef CONFIG_LV2
	list << tr("LV2 plug-in build disabled.");
#endif

	QString sText = "<p>\n";
	sText += "<b>" PADTHV1_TITLE "</b> - " + tr(PADTHV1_SUBTITLE) + "<br />\n";
	sText += "<br />\n";
	sText += tr("Version") + ": <b>" CONFIG_BUILD_VERSION "</b><br />\n";
//	sText += "<small>" + tr("Build") + ": " CONFIG_BUILD_DATE "</small><br />\n";
	if (!list.isEmpty()) {
		sText += "<small><font color=\"red\">";
		sText += list.join("<br />\n");
		sText += "</font></small><br />\n";
	}
	sText += "<br />\n";
	sText += tr("Website") + ": <a href=\"" PADTHV1_WEBSITE "\">" PADTHV1_WEBSITE "</a><br />\n";
	sText += "<br />\n";
	sText += "<small>";
	sText += PADTHV1_COPYRIGHT "<br />\n";
	sText += "<br />\n";
	sText += tr("This program is free software; you can redistribute it and/or modify it") + "<br />\n";
	sText += tr("under the terms of the GNU General Public License version 2 or later.");
	sText += "</small>";
	sText += "</p>\n";

	QMessageBox::about(this, tr("About") + " " PADTHV1_TITLE, sText);
}


void padthv1widget::helpAboutQt (void)
{
	// About Qt...
	QMessageBox::aboutQt(this);
}


// Dirty flag (overridable virtual) methods.
void padthv1widget::updateDirtyPreset ( bool bDirtyPreset )
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->updatePreset(bDirtyPreset);

	m_ui.StatusBar->modified(bDirtyPreset);
	m_ui.Preset->setDirtyPreset(bDirtyPreset);
}


// Param knob context menu.
void padthv1widget::paramContextMenu ( const QPoint& pos )
{
	padthv1widget_param *pParam
		= qobject_cast<padthv1widget_param *> (sender());
	if (pParam == NULL)
		return;

	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi == NULL)
		return;

	padthv1_controls *pControls = pSynthUi->controls();
	if (pControls == NULL)
		return;

	if (!pControls->enabled())
		return;

	QMenu menu(this);

	QAction *pAction = menu.addAction(
		QIcon(":/images/padthv1_control.png"),
		tr("MIDI &Controller..."));

	if (menu.exec(pParam->mapToGlobal(pos)) == pAction) {
		const padthv1::ParamIndex index = m_knobParams.value(pParam);
		const QString& sTitle = pParam->toolTip();
		padthv1widget_control::showInstance(pControls, index, sTitle, this);
	}
}


// Sample harmonics edits.
void padthv1widget::resetSample1 (void)
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->sample(1)->reset();

	updateDirtyPreset(true);
}


void padthv1widget::resetSample2 (void)
{
	padthv1_ui *pSynthUi = ui_instance();
	if (pSynthUi)
		pSynthUi->sample(2)->reset();

	updateDirtyPreset(true);
}


// end of padthv1widget.cpp
