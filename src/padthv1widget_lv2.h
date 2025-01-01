// padthv1widget_lv2.h
//
/****************************************************************************
   Copyright (C) 2012-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __padthv1widget_lv2_h
#define __padthv1widget_lv2_h

#include "padthv1widget.h"
#include "padthv1_lv2ui.h"


//-------------------------------------------------------------------------
// padthv1widget_lv2 - decl.
//

class padthv1widget_lv2 : public padthv1widget
{
public:

	// Constructor.
	padthv1widget_lv2(padthv1_lv2 *pPadth,
		LV2UI_Controller controller, LV2UI_Write_Function write_function);

	// Destructor.
	~padthv1widget_lv2();

	// LV2 port event dispatcher.
	void port_event(uint32_t port_index,
		uint32_t buffer_size, uint32_t format, const void *buffer);

#ifdef CONFIG_LV2_UI_EXTERNAL
	void setExternalHost(LV2_External_UI_Host *external_host);
	const LV2_External_UI_Host *externalHost() const;
#endif

#ifdef CONFIG_LV2_UI_IDLE
	bool isIdleClosed() const;
#endif

protected:

	// Synth engine accessor.
	padthv1_ui *ui_instance() const;

	// Param methods.
	void updateParam(padthv1::ParamIndex index, float fValue) const;

	// Close event handler.
	void closeEvent(QCloseEvent *pCloseEvent);

private:

	// Instance variables.
	padthv1_lv2ui *m_pPadthUi;

#ifdef CONFIG_LV2_UI_EXTERNAL
	LV2_External_UI_Host *m_external_host;
#endif
#ifdef CONFIG_LV2_UI_IDLE
	bool m_bIdleClosed;
#endif
};


#endif	// __padthv1widget_lv2_h

// end of padthv1widget_lv2.h
