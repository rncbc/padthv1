// padthv1_param.h
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

#ifndef __padthv1_param_h
#define __padthv1_param_h

#include "padthv1.h"

#include <QString>

// forward decl.
class QDomElement;
class QDomDocument;


//-------------------------------------------------------------------------
// padthv1_param - decl.
//

namespace padthv1_param
{
	// Preset serialization methods.
	bool loadPreset(padthv1 *pSynth,
		const QString& sFilename);
	bool savePreset(padthv1 *pSynth,
		const QString& sFilename,
		bool bSymLink = false);

	// Sample serialization methods.
	void loadSamples(padthv1 *pSynth,
		const QDomElement& eSamples);
	void saveSamples(padthv1 *pSynth,
		QDomDocument& doc, QDomElement& eSamples,
		bool bSymLink = false);

	// Tuning serialization methods.
	void loadTuning(padthv1 *pSynth,
		const QDomElement& eTuning);
	void saveTuning(padthv1 *pSynth,
		QDomDocument& doc, QDomElement& eTuning,
		bool bSymLink = false);

	// Default parameter name/value helpers.
	const char *paramName(padthv1::ParamIndex index);
	float paramDefaultValue(padthv1::ParamIndex index);
	float paramSafeValue(padthv1::ParamIndex index, float fValue);
	float paramValue(padthv1::ParamIndex index, float fScale);
	float paramScale(padthv1::ParamIndex index, float fValue);
	bool paramFloat(padthv1::ParamIndex index);

	// Load/save and convert canonical/absolute filename helpers.
	QString loadFilename(const QString& sFilename);
	QString saveFilename(const QString& sFilename, bool bSymLink);
};


#endif	// __padthv1_param_h

// end of padthv1_param.h
