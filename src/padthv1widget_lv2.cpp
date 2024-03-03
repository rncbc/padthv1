// padthv1widget_lv2.cpp
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

#include "padthv1widget_lv2.h"

#include "padthv1_lv2.h"

#include <QApplication>
#include <QFileInfo>
#include <QDir>

#include "padthv1widget_palette.h"

#include <QCloseEvent>

#include <QStyleFactory>

#ifndef CONFIG_BINDIR
#define CONFIG_BINDIR	CONFIG_PREFIX "/bin"
#endif

#ifndef CONFIG_LIBDIR
#if defined(__x86_64__)
#define CONFIG_LIBDIR CONFIG_PREFIX "/lib64"
#else
#define CONFIG_LIBDIR CONFIG_PREFIX "/lib"
#endif
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt5/plugins"
#else
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt6/plugins"
#endif


//-------------------------------------------------------------------------
// padthv1widget_lv2 - impl.
//

// Constructor.
padthv1widget_lv2::padthv1widget_lv2 ( padthv1_lv2 *pSampl,
	LV2UI_Controller controller, LV2UI_Write_Function write_function )
	: padthv1widget()
{
	// Check whether under a dedicated application instance...
	QApplication *pApp = padthv1_lv2::qapp_instance();
	if (pApp) {
		// Special style paths...
		QString sPluginsPath = pApp->applicationDirPath();
		sPluginsPath.remove(CONFIG_BINDIR);
		sPluginsPath.append(CONFIG_PLUGINSDIR);
		if (QDir(sPluginsPath).exists())
			pApp->addLibraryPath(CONFIG_PLUGINSDIR);
	}

	// Custom color/style themes...
	padthv1_config *pConfig = padthv1_config::getInstance();
	if (pConfig) {
		const QChar sep = QDir::separator();
		QString sPalettePath = QApplication::applicationDirPath();
		sPalettePath.remove(CONFIG_BINDIR);
		sPalettePath.append(CONFIG_DATADIR);
		sPalettePath.append(sep);
		sPalettePath.append(PROJECT_NAME);
		sPalettePath.append(sep);
		sPalettePath.append("palette");
		if (QDir(sPalettePath).exists()) {
			QStringList names;
			names.append("KXStudio");
			names.append("Wonton Soup");
			QStringListIterator name_iter(names);
			while (name_iter.hasNext()) {
				const QString& name = name_iter.next();
				const QFileInfo fi(sPalettePath, name + ".conf");
				if (fi.isReadable()) {
					padthv1widget_palette::addNamedPaletteConf(
						pConfig, name, fi.absoluteFilePath());
				}
			}
		}
		if (!pConfig->sCustomColorTheme.isEmpty()) {
			QPalette pal;
			if (padthv1widget_palette::namedPalette(
					pConfig, pConfig->sCustomColorTheme, pal))
				padthv1widget::setPalette(pal);
		}
		if (!pConfig->sCustomStyleTheme.isEmpty()) {
			padthv1widget::setStyle(
				QStyleFactory::create(pConfig->sCustomStyleTheme));
		}
	}

	// Initialize (user) interface stuff...
	m_pSynthUi = new padthv1_lv2ui(pSampl, controller, write_function);

#ifdef CONFIG_LV2_UI_EXTERNAL
	m_external_host = nullptr;
#endif
#ifdef CONFIG_LV2_UI_IDLE
	m_bIdleClosed = false;
#endif

	// Initialise preset stuff...
	clearPreset();

	// Initial update, always...
	updateSample();

	//resetParamValues();
	resetParamKnobs();

	// May initialize the scheduler/work notifier.
	openSchedNotifier();
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
