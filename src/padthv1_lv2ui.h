// padthv1_lv2ui.h
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

#ifndef __padthv1_lv2ui_h
#define __padthv1_lv2ui_h

#include "padthv1_ui.h"

#ifdef CONFIG_LV2_OLD_HEADERS
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#else
#include "lv2/ui/ui.h"
#endif

#define PADTHV1_LV2UI_URI PADTHV1_LV2_PREFIX "ui"

#if defined(CONFIG_LV2_UI_X11) || defined(CONFIG_LV2_UI_WINDOWS)
#include <QWindow>
#endif

#ifdef CONFIG_LV2_UI_X11
#define PADTHV1_LV2UI_X11_URI PADTHV1_LV2_PREFIX "ui_x11"
#endif

#ifdef CONFIG_LV2_UI_WINDOWS
#include <windows.h>
#define PADTHV1_LV2UI_WINDOWS_URI PADTHV1_LV2_PREFIX "ui_windows"

// Polyfill for windows size (minimal suitable size)
// Qt cannot determine the right window size on Windows.
#define UI_WINDOWS_RECOMMENDED_WIDTH 1380
#define UI_WINDOWS_RECOMMENDED_HEIGHT 650
#endif

#ifdef CONFIG_LV2_UI_EXTERNAL
#include "lv2_external_ui.h"
#define PADTHV1_LV2UI_EXTERNAL_URI PADTHV1_LV2_PREFIX "ui_external"
#endif


// Forward decls.
class padthv1_lv2;


//-------------------------------------------------------------------------
// padthv1_lv2ui - decl.
//

class padthv1_lv2ui : public padthv1_ui
{
public:

	// Constructor.
	padthv1_lv2ui(padthv1_lv2 *pSampl,
		LV2UI_Controller controller, LV2UI_Write_Function write_function);

	// Accessors.
	const LV2UI_Controller& controller() const;
	void write_function(padthv1::ParamIndex index, float fValue) const;

private:

	// Instance variables.
	LV2UI_Controller     m_controller;
	LV2UI_Write_Function m_write_function;
};


#endif	// __padthv1_lv2ui_h

// end of padthv1_lv2ui.h
