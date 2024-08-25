// padthv1_sample.h
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

/*
 * -- based on PADsynth algorithm, by Paul Nasca
 *    as a special variant of additive synthesis
 *    http://zynaddsubfx.sourceforge.net/doc/PADsynth/PADsynth.htm
 */

#ifndef __padthv1_sample_h
#define __padthv1_sample_h

#include <cstdint>

#include <fftw3.h>


// forward decls.
class padthv1;


//-------------------------------------------------------------------------
// padthv1_sample - PADsynth wave table.
//

class padthv1_sample
{
public:

	// consts
	static const uint32_t DEFAULT_NH    = 32;
	static const uint32_t DEFAULT_NSIZE = 1024 << 6; // 64K

	// ctors.
	padthv1_sample(
		padthv1 *pSynth, int sid = 0,
		uint32_t nsize = DEFAULT_NSIZE);

	// copy ctor.
	padthv1_sample(const padthv1_sample& sample);

	// dtor.
	~padthv1_sample();

	// harmonics window/apodizer profiles
	enum Apodizer { Rect = 0, Triang, Welch, Hann, Gauss };

	// properties.
	float freq0() const
		{ return m_freq0; }
	float width() const
		{ return m_width; }
	float scale() const
		{ return m_scale; }
	uint16_t nh() const
		{ return m_nh; }
	Apodizer apod() const
		{ return m_apod; }

	// table size (in frames)
	uint32_t size() const
		{ return m_nsize; }

	// sample rate.
	void setSampleRate(float srate)
		{ m_srate = srate; }
	float sampleRate() const
		{ return m_srate; }

	// reset harmonics table.
	void reset_nh(uint16_t nh = 0);

	// harmonics accessors
	void setHarmonic(uint16_t n, float h)
		{ if (n < m_nh_max) m_ah[n] = h; }
	float harmonic(uint16_t n) const
		{ return (n < m_nh_max) ? m_ah[n] : 0.0f; }

	// init.
	void reset_test(
		float freq0, float width, float scale, uint16_t nh, Apodizer apod);
	void reset_sync(
		float freq0, float width, float scale, uint16_t nh, Apodizer apod);
	void reset();

	// begin.
	float start(float& phase, float pshift = 0.0f, float freq = 0.0f)
	{
		const float p0 = float(m_nsize);

		phase = m_phase0 + pshift * p0;
		if (phase >= p0)
			phase -= p0;

		return sample(phase, freq);
	}

	// iterate.
	float sample(float& phase, float freq) const
	{
		const uint32_t i = uint32_t(phase);
		const float alpha = phase - float(i);
		const float p0 = float(m_nsize);

		phase += (freq / m_freq0);

		if (phase >= p0)
			phase -= p0;

		const float x0 = m_table[i];
		const float x1 = m_table[i + 1];
	#if 1	// cubic interp.
		const float x2 = m_table[i + 2];
		const float x3 = m_table[i + 3];

		const float c1 = (x2 - x0) * 0.5f;
		const float b1 = (x1 - x2);
		const float b2 = (c1 + b1);
		const float c3 = (x3 - x1) * 0.5f + b2 + b1;
		const float c2 = (c3 + b2);

		return (((c3 * alpha) - c2) * alpha + c1) * alpha + x1;
	#else	// linear interp.
		return x0 + alpha * (x1 - x0);
	#endif
	}

	// absolute value.
	float value(float phase) const
	{
		const float p0 = float(m_nsize);

		phase *= p0;
		phase += m_phase0;
		if (phase >= p0)
			phase -= p0;

		return m_table[uint32_t(phase)];
	}

	// local schedule registry stuff.
	//
	class sched;

	static sched *sched_register(padthv1 *pSynth, int sid);
	static void sched_cleanup();

protected:

	// init harmonics table.
	void reset_nh_max(uint16_t nh);

	// init table.
	void reset_table();

	// post-processors
	void reset_normalize();
	void reset_interp();

	// Hal Chamberlain's pseudo-random linear congruential method.
	uint32_t pseudo_srand ()
		{ return (m_srand = (m_srand * 196314165) + 907633515); }
	float pseudo_randf ()
		{ return pseudo_srand() / float(0x8000U << 16) - 1.0f; }

private:

	float     m_freq0;
	float     m_width;
	float     m_scale;
	uint16_t  m_nh;

	int       m_sid;

	uint16_t  m_nh_max;
	float    *m_ah;

	uint32_t  m_nsize;

	float     m_srate;
	float    *m_table;
	float     m_phase0;

	Apodizer  m_apod;

	float    *m_freq_amp;
	float    *m_freq_sin;
	float    *m_freq_cos;
	float    *m_fftw_data;

	fftwf_plan m_fftw_plan;

	uint32_t  m_srand;

	volatile int m_reset;

	sched *m_sched;
};


//-------------------------------------------------------------------------
// padthv1_generator - synth oscillator (sort of:)

class padthv1_generator
{
public:

	// ctor.
	padthv1_generator(padthv1_sample *sample)
		{ reset(sample); }

	// wave and phase accessors.
	void reset(padthv1_sample *sample)
		{ m_sample = sample; m_phase = 0.0f; }

	// begin.
	float start(float pshift = 0.0f, float freq = 0.0f)
		{ return m_sample->start(m_phase, pshift, freq); }

	// iterate.
	float sample(float freq)
		{ return m_sample->sample(m_phase, freq); }

private:

	padthv1_sample *m_sample;

	float m_phase;
};


//-------------------------------------------------------------------------
// padthv1_sample_ref - PADsynth wave table (sample reference lists).
//
#include "padthv1_list.h"

class padthv1_sample_ref
{
public:

	// dtor.
	~padthv1_sample_ref()
		{ clear_refs(true); }

	// methods.
	void append(padthv1_sample *sample)
		{ m_play.append(new sample_ref(sample)); }

	padthv1_sample *next() const
		{ return m_play.next()->refp; }
	padthv1_sample *prev() const
		{ return m_play.prev()->refp; }

	void acquire()
		{ ++(m_play.next()->refc); }
	void release()
		{ --(m_play.next()->refc); free_refs(); }

	void free_refs()
	{
		sample_ref *ref = m_play.next();
		while (ref && ref->refc == 0 && ref != m_play.prev()) {
			m_play.remove(ref);
			m_free.append(ref);
			ref = m_play.next();
		}
	}

	void clear_refs(bool force = false)
	{
		sample_ref *ref;
		if (force) {
			ref = m_play.next();
			while (ref) {
				m_play.remove(ref);
				m_free.append(ref);
				ref = m_play.next();
			}
		}
		ref = m_free.next();
		while (ref) {
			m_free.remove(ref);
			delete ref->refp;
			delete ref;
			ref = m_free.next();
		}
	}

private:

	struct sample_ref : public padthv1_list<sample_ref>
	{
		sample_ref(padthv1_sample *sample) : refp(sample), refc(0) {}

		padthv1_sample *refp;
		uint32_t        refc;
	};

	padthv1_list<sample_ref> m_play;
	padthv1_list<sample_ref> m_free;
};


#endif	// __padthv1_sample_h
