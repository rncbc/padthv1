// padthv1_param.cpp
//
/****************************************************************************
   Copyright (C) 2012-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "padthv1_param.h"
#include "padthv1_config.h"

#include "padthv1_sample.h"

#include <QHash>

#include <QDomDocument>
#include <QTextStream>
#include <QDir>

#include <cmath>


//-------------------------------------------------------------------------
// state params description.

enum ParamType { PARAM_FLOAT = 0, PARAM_INT, PARAM_BOOL };

static
struct ParamInfo {

	const char *name;
	ParamType type;
	float def;
	float min;
	float max;

} padthv1_params[padthv1::NUM_PARAMS] = {

	// name            type,           def,    min,    max
	{ "GEN1_SAMPLE1",  PARAM_INT,    60.0f,   0.0f, 127.0f }, // GEN1 Sample 1
	{ "GEN1_WIDTH1",   PARAM_FLOAT,  40.0f,   2.0f, 200.0f }, // GEN1 Width 1
	{ "GEN1_SCALE1",   PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // GEN1 Scale 1
	{ "GEN1_NH1",      PARAM_INT,    32.0f,   2.0f,  64.0f }, // GEN1 Nh 1
	{ "GEN1_APOD1",    PARAM_INT,     4.0f,   0.0f,   4.0f }, // GEN1 Apodizer 1
	{ "GEN1_DETUNE1",  PARAM_FLOAT,  -0.1f,  -1.0f,   1.0f }, // GEN1 Detune 1
	{ "GEN1_GLIDE1",   PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // GEN1 Glide 1
	{ "GEN1_SAMPLE2",  PARAM_INT,    60.0f,   0.0f, 127.0f }, // GEN1 Sample 2
	{ "GEN1_WIDTH2",   PARAM_FLOAT,  40.0f,   2.0f, 200.0f }, // GEN1 Width 2
	{ "GEN1_SCALE2",   PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // GEN1 Scale 2
	{ "GEN1_NH2",      PARAM_INT,    32.0f,   2.0f,  64.0f }, // GEN1 Nh 2
	{ "GEN1_APOD2",    PARAM_INT,     4.0f,   0.0f,   4.0f }, // GEN1 Apodizer 2
	{ "GEN1_DETUNE2",  PARAM_FLOAT,   0.1f,  -1.0f,   1.0f }, // GEN1 Detune 2
	{ "GEN1_GLIDE2",   PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // GEN1 Glide 2
	{ "GEN1_BALANCE",  PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // GEN1 Balance
	{ "GEN1_PHASE",    PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // GEN1 Phase
	{ "GEN1_RINGMOD",  PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // GEN1 Ring Mod
	{ "GEN1_OCTAVE",   PARAM_FLOAT,   0.0f,  -4.0f,   4.0f }, // GEN1 Octave
	{ "GEN1_TUNING",   PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // GEN1 Tuning
	{ "GEN1_ENVTIME",  PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // GEN1 Env.Time
	{ "DCF1_ENABLED",  PARAM_BOOL,    1.0f,   0.0f,   1.0f }, // DCF1 Enabled
	{ "DCF1_CUTOFF",   PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // DCF1 Cutoff
	{ "DCF1_RESO",     PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // DCF1 Resonance
	{ "DCF1_TYPE",     PARAM_INT,     0.0f,   0.0f,   3.0f }, // DCF1 Type
	{ "DCF1_SLOPE",    PARAM_INT,     0.0f,   0.0f,   3.0f }, // DCF1 Slope
	{ "DCF1_ENVELOPE", PARAM_FLOAT,   1.0f,  -1.0f,   1.0f }, // DCF1 Envelope
	{ "DCF1_ATTACK",   PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // DCF1 Attack
	{ "DCF1_DECAY",    PARAM_FLOAT,   0.2f,   0.0f,   1.0f }, // DCF1 Decay
	{ "DCF1_SUSTAIN",  PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // DCF1 Sustain
	{ "DCF1_RELEASE",  PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // DCF1 Release
	{ "LFO1_ENABLED",  PARAM_BOOL,    1.0f,   0.0f,   1.0f }, // LFO1 Enabled
	{ "LFO1_SHAPE",    PARAM_INT,     1.0f,   0.0f,   4.0f }, // LFO1 Wave Shape
	{ "LFO1_WIDTH",    PARAM_FLOAT,   1.0f,   0.0f,   1.0f }, // LFO1 Wave Width
	{ "LFO1_BPM",      PARAM_FLOAT, 180.0f,   0.0f, 360.0f }, // LFO1 BPM
	{ "LFO1_RATE",     PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // LFO1 Rate
	{ "LFO1_SYNC",     PARAM_BOOL,    0.0f,   0.0f,   1.0f }, // LFO1 Sync
	{ "LFO1_SWEEP",    PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Sweep
	{ "LFO1_PITCH",    PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Pitch
	{ "LFO1_BALANCE",  PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Balance
	{ "LFO1_RINGMOD",  PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Ring Mod
	{ "LFO1_CUTOFF",   PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Cutoff
	{ "LFO1_RESO",     PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Resonance
	{ "LFO1_PANNING",  PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Panning
	{ "LFO1_VOLUME",   PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // LFO1 Volume
	{ "LFO1_ATTACK",   PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // LFO1 Attack
	{ "LFO1_DECAY",    PARAM_FLOAT,   0.1f,   0.0f,   1.0f }, // LFO1 Decay
	{ "LFO1_SUSTAIN",  PARAM_FLOAT,   1.0f,   0.0f,   1.0f }, // LFO1 Sustain
	{ "LFO1_RELEASE",  PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // LFO1 Release
	{ "DCA1_VOLUME",   PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // DCA1 Volume
	{ "DCA1_ATTACK",   PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // DCA1 Attack
	{ "DCA1_DECAY",    PARAM_FLOAT,   0.1f,   0.0f,   1.0f }, // DCA1 Decay
	{ "DCA1_SUSTAIN",  PARAM_FLOAT,   1.0f,   0.0f,   1.0f }, // DCA1 Sustain
	{ "DCA1_RELEASE",  PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // DCA1 Release
	{ "OUT1_WIDTH",    PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // OUT1 Stereo Width
	{ "OUT1_PANNING",  PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // OUT1 Panning
	{ "OUT1_FXSEND",   PARAM_FLOAT,   1.0f,   0.0f,   1.0f }, // OUT1 FX Send
	{ "OUT1_VOLUME",   PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // OUT1 Volume

	{ "DEF1_PITCHBEND",PARAM_FLOAT,   0.2f,   0.0f,   4.0f }, // DEF1 Pitchbend
	{ "DEF1_MODWHEEL", PARAM_FLOAT,   0.2f,   0.0f,   1.0f }, // DEF1 Modwheel
	{ "DEF1_PRESSURE", PARAM_FLOAT,   0.2f,   0.0f,   1.0f }, // DEF1 Pressure
	{ "DEF1_VELOCITY", PARAM_FLOAT,   0.2f,   0.0f,   1.0f }, // DEF1 Velocity
	{ "DEF1_CHANNEL",  PARAM_INT,     0.0f,   0.0f,  16.0f }, // DEF1 Channel
	{ "DEF1_MONO",     PARAM_INT,     0.0f,   0.0f,   2.0f }, // DEF1 Mono

	{ "CHO1_WET",      PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Chorus Wet
	{ "CHO1_DELAY",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Chorus Delay
	{ "CHO1_FEEDB",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Chorus Feedback
	{ "CHO1_RATE",     PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Chorus Rate
	{ "CHO1_MOD",      PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Chorus Modulation
	{ "FLA1_WET",      PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Flanger Wet
	{ "FLA1_DELAY",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Flanger Delay
	{ "FLA1_FEEDB",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Flanger Feedback
	{ "FLA1_DAFT",     PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Flanger Daft
	{ "PHA1_WET",      PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Phaser Wet
	{ "PHA1_RATE",     PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Phaser Rate
	{ "PHA1_FEEDB",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Phaser Feedback
	{ "PHA1_DEPTH",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Phaser Depth
	{ "PHA1_DAFT",     PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Phaser Daft
	{ "DEL1_WET",      PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Delay Wet
	{ "DEL1_DELAY",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Delay Delay
	{ "DEL1_FEEDB",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Delay Feedback
	{ "DEL1_BPM",      PARAM_FLOAT, 180.0f,   0.0f, 360.0f }, // Delay BPM
	{ "REV1_WET",      PARAM_FLOAT,   0.0f,   0.0f,   1.0f }, // Reverb Wet
	{ "REV1_ROOM",     PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Reverb Room
	{ "REV1_DAMP",     PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Reverb Damp
	{ "REV1_FEEDB",    PARAM_FLOAT,   0.5f,   0.0f,   1.0f }, // Reverb Feedback
	{ "REV1_WIDTH",    PARAM_FLOAT,   0.0f,  -1.0f,   1.0f }, // Reverb Width
	{ "DYN1_COMPRESS", PARAM_BOOL,    0.0f,   0.0f,   1.0f }, // Dynamic Compressor
	{ "DYN1_LIMITER",  PARAM_BOOL,    1.0f,   0.0f,   1.0f }, // Dynamic Limiter

	{ "KEY1_LOW",      PARAM_INT,     0.0f,   0.0f, 127.0f }, // Keyboard Low
	{ "KEY1_HIGH",     PARAM_INT,   127.0f,   0.0f, 127.0f }  // Keyboard High
};


const char *padthv1_param::paramName ( padthv1::ParamIndex index )
{
	return padthv1_params[index].name;
}


float padthv1_param::paramDefaultValue ( padthv1::ParamIndex index )
{
	return padthv1_params[index].def;
}


float padthv1_param::paramSafeValue ( padthv1::ParamIndex index, float fValue )
{
	const ParamInfo& param = padthv1_params[index];

	if (param.type == PARAM_BOOL)
		return (fValue > 0.5f ? 1.0f : 0.0f);

	if (fValue < param.min)
		return param.min;
	if (fValue > param.max)
		return param.max;

	if (param.type == PARAM_INT)
		return ::rintf(fValue);
	else
		return fValue;
}


float padthv1_param::paramValue ( padthv1::ParamIndex index, float fScale )
{
	const ParamInfo& param = padthv1_params[index];

	if (param.type == PARAM_BOOL)
		return (fScale > 0.5f ? 1.0f : 0.0f);

	const float fValue = param.min + fScale * (param.max - param.min);

	if (param.type == PARAM_INT)
		return ::rintf(fValue);
	else
		return fValue;
}


float padthv1_param::paramScale ( padthv1::ParamIndex index, float fValue )
{
	const ParamInfo& param = padthv1_params[index];

	if (param.type == PARAM_BOOL)
		return (fValue > 0.5f ? 1.0f : 0.0f);

	const float fScale = (fValue - param.min) / (param.max - param.min);

	if (param.type == PARAM_INT)
		return ::rintf(fScale);
	else
		return fScale;
}


bool padthv1_param::paramFloat ( padthv1::ParamIndex index )
{
	return (padthv1_params[index].type == PARAM_FLOAT);
}


// Preset serialization methods.
bool padthv1_param::loadPreset (
	padthv1 *pSynth, const QString& sFilename )
{
	if (pSynth == nullptr)
		return false;

	QFileInfo fi(sFilename);
	if (!fi.exists()) {
		padthv1_config *pConfig = padthv1_config::getInstance();
		if (pConfig) {
			const QString& sPresetFile
				= pConfig->presetFile(sFilename);
			if (sPresetFile.isEmpty())
				return false;
			fi.setFile(sPresetFile);
			if (!fi.exists())
				return false;
		}
	}

	QFile file(fi.filePath());
	if (!file.open(QIODevice::ReadOnly))
		return false;

	const bool running = pSynth->running(false);

	pSynth->setTuningEnabled(false);
	pSynth->reset();

	static QHash<QString, padthv1::ParamIndex> s_hash;
	if (s_hash.isEmpty()) {
		for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
			const padthv1::ParamIndex index = padthv1::ParamIndex(i);
			s_hash.insert(padthv1_param::paramName(index), index);
		}
	}

	const QDir currentDir(QDir::current());
	QDir::setCurrent(fi.absolutePath());

	QDomDocument doc(PADTHV1_TITLE);
	if (doc.setContent(&file)) {
		QDomElement ePreset = doc.documentElement();
		if (ePreset.tagName() == "preset") {
		//	&& ePreset.attribute("name") == fi.completeBaseName()) {
			for (QDomNode nChild = ePreset.firstChild();
					!nChild.isNull();
						nChild = nChild.nextSibling()) {
				QDomElement eChild = nChild.toElement();
				if (eChild.isNull())
					continue;
				if (eChild.tagName() == "params") {
					for (QDomNode nParam = eChild.firstChild();
							!nParam.isNull();
								nParam = nParam.nextSibling()) {
						QDomElement eParam = nParam.toElement();
						if (eParam.isNull())
							continue;
						if (eParam.tagName() == "param") {
							padthv1::ParamIndex index = padthv1::ParamIndex(
								eParam.attribute("index").toULong());
							const QString& sName = eParam.attribute("name");
							if (!sName.isEmpty()) {
								if (!s_hash.contains(sName))
									continue;
								index = s_hash.value(sName);
							}
							const float fValue = eParam.text().toFloat();
							pSynth->setParamValue(index,
								padthv1_param::paramSafeValue(index, fValue));
						}
					}
				}
				else
				if (eChild.tagName() == "samples") {
					padthv1_param::loadSamples(pSynth, eChild);
				}
				else
				if (eChild.tagName() == "tuning") {
					padthv1_param::loadTuning(pSynth, eChild);
				}
			}
		}
	}

	file.close();

	pSynth->stabilize();
	pSynth->reset();
	pSynth->reset_test();
	pSynth->running(running);

	QDir::setCurrent(currentDir.absolutePath());

	return true;
}


bool padthv1_param::savePreset (
	padthv1 *pSynth, const QString& sFilename, bool bSymLink )
{
	if (pSynth == nullptr)
		return false;

	pSynth->stabilize();

	const QFileInfo fi(sFilename);
	const QDir currentDir(QDir::current());
	QDir::setCurrent(fi.absolutePath());

	QDomDocument doc(PADTHV1_TITLE);
	QDomElement ePreset = doc.createElement("preset");
	ePreset.setAttribute("name", fi.completeBaseName());
	ePreset.setAttribute("version", PROJECT_VERSION);

	QDomElement eSamples = doc.createElement("samples");
	padthv1_param::saveSamples(pSynth, doc, eSamples, bSymLink);
	ePreset.appendChild(eSamples);

	QDomElement eParams = doc.createElement("params");
	for (uint32_t i = 0; i < padthv1::NUM_PARAMS; ++i) {
		QDomElement eParam = doc.createElement("param");
		const padthv1::ParamIndex index = padthv1::ParamIndex(i);
		eParam.setAttribute("index", QString::number(i));
		eParam.setAttribute("name", padthv1_param::paramName(index));
		const float fValue = pSynth->paramValue(index);
		eParam.appendChild(doc.createTextNode(QString::number(fValue)));
		eParams.appendChild(eParam);
	}
	ePreset.appendChild(eParams);

	if (pSynth->isTuningEnabled()) {
		QDomElement eTuning = doc.createElement("tuning");
		padthv1_param::saveTuning(pSynth, doc, eTuning, bSymLink);
		ePreset.appendChild(eTuning);
	}

	doc.appendChild(ePreset);

	QFile file(fi.filePath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	QTextStream(&file) << doc.toString();
	file.close();

	QDir::setCurrent(currentDir.absolutePath());

	return true;
}


// Sample serialization methods.
void padthv1_param::loadSamples (
	padthv1 *pSynth, const QDomElement& eSamples )
{
	if (pSynth == nullptr)
		return;

	QHash<int, padthv1_sample *> list;
	list.insert(0, pSynth->sample(1));
	list.insert(1, pSynth->sample(2));

	for (QDomNode nSample = eSamples.firstChild();
			!nSample.isNull();
				nSample = nSample.nextSibling()) {
		QDomElement eSample = nSample.toElement();
		if (eSample.isNull())
			continue;
		if (eSample.tagName() == "sample") {
			const int index = eSample.attribute("index").toInt();
			padthv1_sample *sample = list.value(index, nullptr);
			if (sample == nullptr)
				continue;
			sample->reset();
			sample->reset_nh(eSample.attribute("nh").toUInt());
			for (QDomNode nChild = eSample.firstChild();
					!nChild.isNull();
						nChild = nChild.nextSibling()) {
				QDomElement eChild = nChild.toElement();
				if (eChild.isNull())
					continue;
				if (eChild.tagName() == "items") {
					for (QDomNode nItem = eChild.firstChild();
							!nItem.isNull();
								nItem = nItem.nextSibling()) {
						QDomElement eItem = nItem.toElement();
						if (eItem.isNull())
							continue;
						if (eItem.tagName() == "item") {
							const uint16_t n = eItem.attribute("index").toUInt();
							const float h = eItem.text().toFloat();
							sample->setHarmonic(n, h);
						}
					}
				}
			}
		}
	}
}


void padthv1_param::saveSamples (
	padthv1 *pSynth, QDomDocument& doc, QDomElement& eSamples, bool /*bSymLink*/ )
{
	if (pSynth == nullptr)
		return;

	QHash<int, padthv1_sample *> list;
	list.insert(0, pSynth->sample(1));
	list.insert(1, pSynth->sample(2));

	QHash<int, padthv1_sample *>::ConstIterator iter = list.constBegin();
	const QHash<int, padthv1_sample *>::ConstIterator& iter_end = list.constEnd();
	for ( ; iter != iter_end; ++iter) {
		padthv1_sample *sample = iter.value();
		if (sample == nullptr)
			continue;
		const int index = iter.key();
		QDomElement eSample = doc.createElement("sample");
		eSample.setAttribute("index", QString::number(index));
		eSample.setAttribute("nh", QString::number(sample->nh()));
		QDomElement eItems = doc.createElement("items");
		for (uint16_t n = 0; n < sample->nh(); ++n) {
			QDomElement eItem = doc.createElement("item");
			eItem.setAttribute("index", QString::number(n));
			eItem.appendChild(doc.createTextNode(
				QString::number(sample->harmonic(n))));
			eItems.appendChild(eItem);
		}
		eSample.appendChild(eItems);
		eSamples.appendChild(eSample);
	}
}


// Tuning serialization methods.
void padthv1_param::loadTuning (
	padthv1 *pSynth, const QDomElement& eTuning )
{
	if (pSynth == nullptr)
		return;

	pSynth->setTuningEnabled(eTuning.attribute("enabled").toInt() > 0);

	for (QDomNode nChild = eTuning.firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;
		if (eChild.tagName() == "enabled") {
			pSynth->setTuningEnabled(eChild.text().toInt() > 0);
		}
		if (eChild.tagName() == "ref-pitch") {
			pSynth->setTuningRefPitch(eChild.text().toFloat());
		}
		else
		if (eChild.tagName() == "ref-note") {
			pSynth->setTuningRefNote(eChild.text().toInt());
		}
		else
		if (eChild.tagName() == "scale-file") {
			const QString& sScaleFile
				= eChild.text();
			const QByteArray aScaleFile
				= padthv1_param::loadFilename(sScaleFile).toUtf8();
			pSynth->setTuningScaleFile(aScaleFile.constData());
		}
		else
		if (eChild.tagName() == "keymap-file") {
			const QString& sKeyMapFile
				= eChild.text();
			const QByteArray aKeyMapFile
				= padthv1_param::loadFilename(sKeyMapFile).toUtf8();
			pSynth->setTuningScaleFile(aKeyMapFile.constData());
		}
	}

	// Consolidate tuning state...
	pSynth->updateTuning();
}


void padthv1_param::saveTuning (
	padthv1 *pSynth, QDomDocument& doc, QDomElement& eTuning, bool bSymLink )
{
	if (pSynth == nullptr)
		return;

	eTuning.setAttribute("enabled", int(pSynth->isTuningEnabled()));

	QDomElement eRefPitch = doc.createElement("ref-pitch");
	eRefPitch.appendChild(doc.createTextNode(
		QString::number(pSynth->tuningRefPitch())));
	eTuning.appendChild(eRefPitch);

	QDomElement eRefNote = doc.createElement("ref-note");
	eRefNote.appendChild(doc.createTextNode(
		QString::number(pSynth->tuningRefNote())));
	eTuning.appendChild(eRefNote);

	const char *pszScaleFile = pSynth->tuningScaleFile();
	if (pszScaleFile) {
		const QString& sScaleFile
			= QString::fromUtf8(pszScaleFile);
		if (!sScaleFile.isEmpty()) {
			QDomElement eScaleFile = doc.createElement("scale-file");
			eScaleFile.appendChild(doc.createTextNode(
				QDir::current().relativeFilePath(
					padthv1_param::saveFilename(sScaleFile, bSymLink))));
			eTuning.appendChild(eScaleFile);
		}
	}

	const char *pszKeyMapFile = pSynth->tuningKeyMapFile();
	if (pszKeyMapFile) {
		const QString& sKeyMapFile
			= QString::fromUtf8(pszKeyMapFile);
		if (!sKeyMapFile.isEmpty()) {
			QDomElement eKeyMapFile = doc.createElement("keymap-file");
			eKeyMapFile.appendChild(doc.createTextNode(
				QDir::current().relativeFilePath(
					padthv1_param::saveFilename(sKeyMapFile, bSymLink))));
			eTuning.appendChild(eKeyMapFile);
		}
	}
}


// Load/save and convert canonical/absolute filename helpers.
QString padthv1_param::loadFilename ( const QString& sFilename )
{
	QFileInfo fi(sFilename);
	if (fi.isSymLink())
		fi.setFile(fi.symLinkTarget());
	return fi.canonicalFilePath();
}


QString padthv1_param::saveFilename ( const QString& sFilename, bool bSymLink )
{
	QFileInfo fi(sFilename);
	if (bSymLink && fi.absolutePath() != QDir::current().absolutePath()) {
		const QString& sPath = fi.absoluteFilePath();
		const QString& sName = fi.baseName();
		const QString& sExt  = fi.completeSuffix();
		const QString& sLink = sName
			+ '-' + QString::number(qHash(sPath), 16)
			+ '.' + sExt;
		QFile(sPath).link(sLink);
		fi.setFile(QDir::current(), sLink);
	}
	else if (fi.isSymLink()) fi.setFile(fi.symLinkTarget());
	return fi.absoluteFilePath();
}


// end of padthv1_param.cpp
