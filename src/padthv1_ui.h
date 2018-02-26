// padthv1_ui.h
//
/****************************************************************************
   Copyright (C) 2012-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __padthv1_ui_h
#define __padthv1_ui_h

#include "padthv1.h"

#include <QString>


//-------------------------------------------------------------------------
// padthv1_ui - decl.
//

class padthv1_ui
{
public:

	padthv1_ui(padthv1 *pSynth, bool bPlugin);

	padthv1 *instance() const;

	bool isPlugin() const;

	padthv1_sample *sample(int sid) const;

	bool loadPreset(const QString& sFilename);
	bool savePreset(const QString& sFilename);

	void setParamValue(padthv1::ParamIndex index, float fValue);
	float paramValue(padthv1::ParamIndex index) const;

	padthv1_controls *controls() const;
	padthv1_programs *programs() const;

	void reset();

	void updatePreset(bool bDirty);

	void midiInEnabled(bool bEnabled);
	uint32_t midiInCount();

	void directNoteOn(int note, int vel);


	void updateTuning();

	// MIDI note/octave name helper.
	static QString noteName(int note);

private:

	padthv1 *m_pSynth;

	bool m_bPlugin;
};


#endif// __padthv1_ui_h

// end of padthv1_ui.h
