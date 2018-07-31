// padthv1_sched.h
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

#ifndef __padthv1_sched_h
#define __padthv1_sched_h

#include <stdint.h>

// forward decls.
class padthv1;


//-------------------------------------------------------------------------
// padthv1_sched - worker/scheduled stuff (pure virtual).
//

class padthv1_sched
{
public:

	// plausible sched types.
	enum Type { Sample, Programs, Controls, Controller, MidiIn };

	// ctor.
	padthv1_sched(padthv1 *pSynth, Type stype, uint32_t nsize = 8);

	// virtual dtor.
	virtual ~padthv1_sched();

	// instance access.
	padthv1 *instance() const;

	// schedule process.
	void schedule(int sid = 0);

	// test-and-set wait.
	bool sync_wait();
	
	// scheduled processor.
	void sync_process();

	// (pure) virtual processor.
	virtual void process(int sid) = 0;

	// signal broadcast (static).
	static void sync_notify(padthv1 *pSynth, Type stype, int sid);

private:

	// instance variables.
	padthv1 *m_pSynth;

	Type m_stype;

	// sched queue instance reference.
	uint32_t m_nsize;
	uint32_t m_nmask;

	int *m_items;

	volatile uint32_t m_iread;
	volatile uint32_t m_iwrite;

	volatile bool m_sync_wait;
};


//-------------------------------------------------------------------------
// padthv1_sched_notifier - worker/schedule proxy decl.
//

class padthv1_sched_notifier
{
public:

	// ctor.
	padthv1_sched_notifier(padthv1 *pSynth);

	// dtor.
	~padthv1_sched_notifier();

	// signal notifier.
	virtual void notify(padthv1_sched::Type stype, int sid) const = 0;

private:

	// instance variables.
	padthv1 *m_pSynth;
};


#endif	// __padthv1_sched_h

// end of padthv1_sched.h
