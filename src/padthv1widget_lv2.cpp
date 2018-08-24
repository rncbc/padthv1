// padthv1widget_lv2.cpp
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

#include "padthv1widget_lv2.h"

#include "padthv1_lv2.h"


#include <QCloseEvent>


//-------------------------------------------------------------------------
// padthv1widget_lv2 - impl.
//

// Constructor.
padthv1widget_lv2::padthv1widget_lv2 ( padthv1_lv2 *pSampl,
	LV2UI_Controller controller, LV2UI_Write_Function write_function )
	: padthv1widget()
{
	m_pSynthUi = new padthv1_lv2ui(pSampl, controller, write_function);

#ifdef CONFIG_LV2_UI_EXTERNAL
	m_external_host = NULL;
#endif
#ifdef CONFIG_LV2_UI_IDLE
	m_bIdleClosed = false;
#endif

	// May initialize the scheduler/work notifier.
	openSchedNotifier();

	clearPreset();

	// Initial update, always...
	updateSample();
}


// Destructor.
padthv1widget_lv2::~padthv1widget_lv2 (void)
{
	delete m_pSynthUi;
}


// Synth engine accessor.
padthv1_ui *padthv1widget_lv2::ui_instance (void) const
{
	return m_pSynthUi;
}


#ifdef CONFIG_LV2_UI_EXTERNAL

void padthv1widget_lv2::setExternalHost ( LV2_External_UI_Host *external_host )
{
	m_external_host = external_host;

	if (m_external_host && m_external_host->plugin_human_id)
		padthv1widget::setWindowTitle(m_external_host->plugin_human_id);
}

const LV2_External_UI_Host *padthv1widget_lv2::externalHost (void) const
{
	return m_external_host;
}

#endif	// CONFIG_LV2_UI_EXTERNAL


#ifdef CONFIG_LV2_UI_IDLE

bool padthv1widget_lv2::isIdleClosed (void) const
{
	return m_bIdleClosed;
}

#endif	// CONFIG_LV2_UI_IDLE


// Close event handler.
void padthv1widget_lv2::closeEvent ( QCloseEvent *pCloseEvent )
{
	padthv1widget::closeEvent(pCloseEvent);

#ifdef CONFIG_LV2_UI_IDLE
	if (pCloseEvent->isAccepted())
		m_bIdleClosed = true;
#endif
#ifdef CONFIG_LV2_UI_EXTERNAL
	if (m_external_host && m_external_host->ui_closed) {
		if (pCloseEvent->isAccepted())
			m_external_host->ui_closed(m_pSynthUi->controller());
	}
#endif
}


// LV2 port event dispatcher.
void padthv1widget_lv2::port_event ( uint32_t port_index,
	uint32_t buffer_size, uint32_t format, const void *buffer )
{
	if (format == 0 && buffer_size == sizeof(float)) {
		const padthv1::ParamIndex index
			= padthv1::ParamIndex(port_index - padthv1_lv2::ParamBase);
		const float fValue = *(float *) buffer;
		setParamValue(index, fValue);
	}
}


// Param method.
void padthv1widget_lv2::updateParam (
	padthv1::ParamIndex index, float fValue ) const
{
	m_pSynthUi->write_function(index, fValue);
}


// end of padthv1widget_lv2.cpp
