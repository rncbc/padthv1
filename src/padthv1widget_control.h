// padthv1widget_control.h
//
/****************************************************************************
   Copyright (C) 2012-2022, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __padthv1widget_control_h
#define __padthv1widget_control_h

#include "padthv1_controls.h"
#include "padthv1_param.h"

#include <QDialog>


// forward decls.
namespace Ui { class padthv1widget_control; }

class QAbstractButton;
class QCloseEvent;


//----------------------------------------------------------------------------
// padthv1widget_control -- UI wrapper form.

class padthv1widget_control : public QDialog
{
	Q_OBJECT

public:

	// Pseudo-singleton instance.
	static padthv1widget_control *getInstance();

	// Pseudo-constructor.
	static void showInstance(
		padthv1_controls *pControls, padthv1::ParamIndex index,
		const QString& sTitle, QWidget *pParent = nullptr);

	// Control accessors.
	void setControls(padthv1_controls *pControls, padthv1::ParamIndex index);
	padthv1_controls *controls() const;
	padthv1::ParamIndex controlIndex() const;

	// Process incoming controller key event.
	void setControlKey(const padthv1_controls::Key& key);
	padthv1_controls::Key controlKey() const;

protected slots:

	void changed();

	void clicked(QAbstractButton *);
	void reset();

	void accept();
	void reject();

	void activateControlType(int);
	void editControlParamFinished();

	void stabilize();

protected:

	// Constructor.
	padthv1widget_control(QWidget *pParent = nullptr);

	// Destructor.
	~padthv1widget_control();

	// Pseudo-destructor.
	void cleanup();

	void closeEvent(QCloseEvent *pCloseEvent);

	// Control type dependency refresh.
	void updateControlType(int iControlType);

	void setControlType(padthv1_controls::Type ctype);
	padthv1_controls::Type controlType() const;

	void setControlParam(unsigned short param);
	unsigned short controlParam() const;

	unsigned short controlChannel() const;

	padthv1_controls::Type controlTypeFromIndex (int iIndex) const;
	int indexFromControlType(padthv1_controls::Type ctype) const;

	unsigned short controlParamFromIndex(int iIndex) const;
	int indexFromControlParam(unsigned short param) const;

private:

	// The Qt-designer UI struct...
	Ui::padthv1widget_control *p_ui;
	Ui::padthv1widget_control& m_ui;

	// Instance variables.
	padthv1_controls *m_pControls;

	// Target subject.
	padthv1_controls::Key m_key;
	padthv1::ParamIndex m_index;

	// Instance variables.
	int m_iControlParamUpdate;

	int m_iDirtyCount;
	int m_iDirtySetup;

	// Pseudo-singleton instance.
	static padthv1widget_control *g_pInstance;
};


#endif	// __padthv1widget_control_h


// end of padthv1widget_control.h
