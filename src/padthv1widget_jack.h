// padthv1widget_jack.h
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

#ifndef __padthv1widget_jack_h
#define __padthv1widget_jack_h

#include "padthv1widget.h"


// Forward decls.
class padthv1_jack;

#ifdef CONFIG_NSM
class padthv1_nsm;
#endif


//-------------------------------------------------------------------------
// padthv1widget_jack - decl.
//

class padthv1widget_jack : public padthv1widget
{
public:

	// Constructor.
	padthv1widget_jack(padthv1_jack *pSynth);

	// Destructor.
	~padthv1widget_jack();

#ifdef CONFIG_NSM
	// NSM client accessors.
	void setNsmClient(padthv1_nsm *pNsmClient);
	padthv1_nsm *nsmClient() const;
#endif	// CONFIG_NSM

	// Dirty flag method.
	void updateDirtyPreset(bool bDirtyPreset);

protected:

	// Synth engine accessor.
	padthv1_ui *ui_instance() const;

	// Param port method.
	void updateParam(padthv1::ParamIndex index, float fValue) const;

	// Application close.
	void closeEvent(QCloseEvent *pCloseEvent);

#ifdef CONFIG_NSM
	// Optional GUI handlers.
	void showEvent(QShowEvent *pShowEvent);
	void hideEvent(QHideEvent *pHideEvent);
#endif	// CONFIG_NSM

private:

	// Instance variables.
	padthv1     *m_pSynth;
	padthv1_ui  *m_pSynthUi;

#ifdef CONFIG_NSM
	padthv1_nsm *m_pNsmClient;
#endif
};


#endif	// __padthv1widget_jack_h

// end of padthv1widget_jack.h
