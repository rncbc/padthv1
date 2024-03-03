// padthv1widget_jack.cpp
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

#include "padthv1widget_jack.h"

#include "padthv1widget_palette.h"

#include "padthv1_jack.h"

#ifdef CONFIG_NSM
#include "padthv1_nsm.h"
#endif

#include <QApplication>
#include <QFileInfo>
#include <QDir>

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

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt4/plugins"
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt5/plugins"
#else
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt6/plugins"
#endif


//-------------------------------------------------------------------------
// padthv1widget_jack - impl.
//

// Constructor.
padthv1widget_jack::padthv1widget_jack ( padthv1_jack *pSynth )
	: padthv1widget(), m_pSynth(pSynth)
	#ifdef CONFIG_NSM
		, m_pNsmClient(nullptr)
	#endif
{
	// Special style paths...
	QString sPluginsPath = QApplication::applicationDirPath();
	sPluginsPath.remove(CONFIG_BINDIR);
	sPluginsPath.append(CONFIG_PLUGINSDIR);
	if (QDir(sPluginsPath).exists())
		QApplication::addLibraryPath(sPluginsPath);

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
	m_pSynthUi = new padthv1_ui(m_pSynth, false);

	// Initialise preset stuff...
	clearPreset();

	// Initial update, always...
	updateSample();

	resetParamValues();
	resetParamKnobs();

	// May initialize the scheduler/work notifier.
	openSchedNotifier();
}


// Destructor.
padthv1widget_jack::~padthv1widget_jack (void)
{
	delete m_pSynthUi;
}


// Synth engine accessor.
padthv1_ui *padthv1widget_jack::ui_instance (void) const
{
	return m_pSynthUi;
}

#ifdef CONFIG_NSM

// NSM client accessors.
void padthv1widget_jack::setNsmClient ( padthv1_nsm *pNsmClient )
{
	m_pNsmClient = pNsmClient;
}

padthv1_nsm *padthv1widget_jack::nsmClient (void) const
{
	return m_pNsmClient;
}

#endif	// CONFIG_NSM


// Param port method.
void padthv1widget_jack::updateParam (
	padthv1::ParamIndex index, float fValue ) const
{
	m_pSynthUi->setParamValue(index, fValue);
}


// Dirty flag method.
void padthv1widget_jack::updateDirtyPreset ( bool bDirtyPreset )
{
	padthv1widget::updateDirtyPreset(bDirtyPreset);

#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active() && bDirtyPreset)
		m_pNsmClient->dirty(true);
#endif
}


// Application close.
void padthv1widget_jack::closeEvent ( QCloseEvent *pCloseEvent )
{
#ifdef CONFIG_NSM
	if (m_pNsmClient && m_pNsmClient->is_active()) {
		pCloseEvent->ignore();
		padthv1widget::hide();
	}
	else
#endif
	// Let's be sure about that...
	if (queryClose()) {
		pCloseEvent->accept();
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QApplication::exit(0);
	#else
		QApplication::quit();
	#endif
	} else {
		pCloseEvent->ignore();
	}
}


#ifdef CONFIG_NSM

// Optional GUI handlers.
void padthv1widget_jack::showEvent ( QShowEvent *pShowEvent )
{
	padthv1widget::showEvent(pShowEvent);

	if (m_pNsmClient)
		m_pNsmClient->visible(true);
}

void padthv1widget_jack::hideEvent ( QHideEvent *pHideEvent )
{
	if (m_pNsmClient)
		m_pNsmClient->visible(false);

	padthv1widget::hideEvent(pHideEvent);
}

#endif	// CONFIG_NSM


// end of padthv1widget_jack.cpp
