// padthv1_sample.cpp
//
/****************************************************************************
   Copyright (C) 2012-2020, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "padthv1_sample.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>


//-------------------------------------------------------------------------
// padthv1_sample_sched - local module schedule thread stuff.
//

#include "padthv1_sched.h"
#include "padthv1.h"


class padthv1_sample_sched : public padthv1_sched
{
public:

	// ctor.
	padthv1_sample_sched (padthv1 *pSynth, padthv1_sample *sample)
		: padthv1_sched(pSynth, Sample), m_sample(sample), m_sync(0) {}

	void schedule(float freq0, float width, float scale, uint16_t nh,
		padthv1_sample::Apodizer apod, int sid = 0)
	{
		m_freq0 = freq0;
		m_width = width;
		m_scale = scale;
		m_nh    = nh;

		m_apod  = apod;

		if (++m_sync == 1)
			padthv1_sched::schedule(sid);
	}

	// process reset (virtual).
	void process(int)
	{
		padthv1_sched::instance()->reset();

		m_sample->reset(m_freq0, m_width, m_scale, m_nh, m_apod);
		m_sync = 0;
	}

private:

	// instance variables.
	padthv1_sample *m_sample;

	volatile uint32_t m_sync;

	float    m_freq0;
	float    m_width;
	float    m_scale;
	uint16_t m_nh;

	padthv1_sample::Apodizer m_apod;
};


//-------------------------------------------------------------------------
// padthv1_sample - PADsynth wave table.
//

// ctor.
padthv1_sample::padthv1_sample ( padthv1 *pSynth, int sid, uint32_t nsize )
	: m_freq0(0.0f), m_width(0.0f), m_scale(0.0f), m_nh(0), m_sid(sid),
		m_nh_max(0), m_ah(nullptr), m_nsize(nsize), m_srate(44100.0f),
		m_phase0(0.0f), m_apod(Gauss), m_srand(0)
{
	const uint32_t nsize2 = (m_nsize >> 1);

	// wave-table (in frames)
	m_table = new float [m_nsize + 4];

	// harmonic amplitutes
	m_freq_amp = new float [nsize2];

	// sine and cosine components
	m_freq_sin = new float [nsize2];
	m_freq_cos = new float [nsize2];

	// data plan
	m_fftw_data = new float [m_nsize];
	m_fftw_plan = ::fftwf_plan_r2r_1d(
		m_nsize, m_fftw_data, m_fftw_data, FFTW_HC2R, FFTW_ESTIMATE);

	m_sample_sched = new padthv1_sample_sched(pSynth, this);

	reset_nh_max(DEFAULT_NH);
}


// dtor.
padthv1_sample::~padthv1_sample (void)
{
	if (m_ah) delete [] m_ah;

	delete m_sample_sched;

	::fftwf_destroy_plan(m_fftw_plan);

	delete [] m_fftw_data;

	delete [] m_freq_cos;
	delete [] m_freq_sin;
	delete [] m_freq_amp;

	delete [] m_table;
}


// init.
void padthv1_sample::reset_test (
	float freq0, float width, float scale, uint16_t nh, Apodizer apod )
{
	int updated = 0;

	if (m_freq0 != freq0)
		++updated;
	if (m_width != width)
		++updated;
	if (m_scale != scale)
		++updated;
	if (m_nh != nh)
		++updated;
	if (m_apod != apod)
		++updated;

	if (updated > 0)
		m_sample_sched->schedule(freq0, width, scale, nh, apod, m_sid);
}


void padthv1_sample::reset (
	float freq0, float width, float scale, uint16_t nh, Apodizer apod )
{
	m_freq0 = freq0;
	m_width = width;
	m_scale = scale;
	m_nh    = nh;

	m_apod  = apod;

	reset_nh_max(nh);

	reset_table();
}


void padthv1_sample::reset (void)
{
	m_freq0 = 0.0f;
	m_width = 0.0f;
	m_scale = 0.0f;
	m_nh    = 0;
}


// reset harmonics table.
void padthv1_sample::reset_nh ( uint16_t nh )
{
	if (nh < 1)
		nh = DEFAULT_NH;

	m_nh_max = 0;
	m_nh = 0;

	reset_nh_max(nh);
}


// init harmonics table.
void padthv1_sample::reset_nh_max ( uint16_t nh )
{
	if (nh > m_nh_max) {
		const float *old_ah = m_ah;
		float *new_ah = new float [nh];
		if (old_ah) for (uint16_t n = 0; n < m_nh_max; ++n)
			new_ah[n] = old_ah[n];
		::memset(&new_ah[m_nh_max], 0, (nh - m_nh_max) * sizeof(float));
		if (m_sid & 1) {
			for (uint16_t n = m_nh_max; n < nh; ++n) // odd...
				new_ah[n] = ((n & 1) ? 1.667f : 1.0f) / float(n + 1);
		} else {
			for (uint16_t n = m_nh_max; n < nh; ++n) // even...
				new_ah[n] = ((n & 1) || (n < 1) ? 1.0f : 1.667f) / float(n + 1);
		}
		m_ah = new_ah;
		m_nh_max = nh;
		if (old_ah) delete [] old_ah;
	}
}


// windowing/apodizing functions

static inline float apod_rect ( float fi, float bwi )
{
	return (fi > -bwi && fi < +bwi ? 1.0f : 0.0f);
}

static inline float apod_triang ( float fi, float bwi )
{
	const float bw2 = 2.0f * bwi;
	if (fi > -bw2 && fi < +bw2) {
		const float x2 = fi / bw2;
		return (fi < 0.0f ? 1.0f + x2 : 1.0f - x2);
	} else {
		return 0.0f;
	}
}

static inline float apod_welch ( float fi, float bwi )
{
	if (fi > -bwi && fi < +bwi) {
		const float x1 = fi / bwi;
		return 1.0f - x1 * x1;
	} else {
		return 0.0f;
	}
}

static inline float apod_hann ( float fi, float bwi )
{
	const float bw2 = 2.0f * bwi;
	if (fi > -bw2 && fi < +bw2) {
		return 0.5f + 0.5f * ::cosf(M_PI * fi / bw2);
	} else {
		return 0.0f;
	}
}

static inline float apod_gauss ( float fi, float bwi )
{
	const float x1 = fi / bwi;
	const float x2 = x1 * x1;
	return (x2 < 14.71280603f ? ::expf(-x2) : 0.0f);
}


// relative frequency of the nth harmonic to fundamental.
static inline float fast_log2f ( float x )
{
	union { uint32_t i; float f; } u, v;
	u.f = x;
	v.i = (u.i & 0x007FFFFF) | 0x3f000000;
	const float y = float(u.i) * 1.1920928955078125e-7f - 124.22551499f;
	return y - 1.498030302f * v.f - 1.72587999f / (0.3520887068f + v.f);
}

static inline float fast_pow2f ( float p )
{
	const float z = p - int(p) + (p < 0.0f ? 1.0f : 0.0f);
	union { uint32_t i; float f; } u;
	u.i = uint32_t((1 << 23)
		* (p + 121.2740575f + 27.7280233f / (4.84252568f - z)
		- 1.49012907f * z));
	return u.f;
}

static inline float fast_powf ( float x, float p )
{
	return fast_pow2f(p * fast_log2f(x));
}

static inline float freq_powf ( float ni, float bws )
{
//	return (1.0f + 0.1f * bws * ni) * ni;
	return fast_powf(ni, 1.0f + bws);
}


// init wave table.
void padthv1_sample::reset_table (void)
{
	// reset random phase generator
	const float p0 = float(m_nsize);
	const float w0 = p0 * m_width;

	m_srand = uint32_t(w0) ^ 0x9631; // magic!

	// Implementation of padthv1_sample algorithm...
	//

	uint32_t i;

	// prepare harmonic amplitutes profiling
	const uint32_t nsize2 = (m_nsize >> 1);

	::memset(m_freq_amp, 0, nsize2 * sizeof(float));

	const float rate1 = m_srate / float(m_nsize);

	for (uint16_t n = 0; n < m_nh; ++n) {

		// n-th harmonic bandwidth scaling
		const float ni = float(n + 1);
		const float bws = m_scale * m_scale * m_scale;
		const float bwi
			= (fast_powf(2.0f, m_width / 1200.0f) - 1.0f)
			* 0.5f * m_freq0 * freq_powf(ni, bws);
		const float fi = m_freq0 * freq_powf(ni, bws);
		const float hi = 1.0f / ni;

		switch (m_apod) {
		case Rect:
			for (i = 0; i < nsize2; ++i) {
				const float hp = apod_rect(rate1 * float(i) - fi, bwi);
				m_freq_amp[i] += hp * hi * m_ah[n];
			}
			break;
		case Triang:
			for (i = 0; i < nsize2; ++i) {
				const float hp = apod_triang(rate1 * float(i) - fi, bwi);
				m_freq_amp[i] += hp * hi * m_ah[n];
			}
			break;
		case Welch:
			for (i = 0; i < nsize2; ++i) {
				const float hp = apod_welch(rate1 * float(i) - fi, bwi);
				m_freq_amp[i] += hp * hi * m_ah[n];
			}
			break;
		case Hann:
			for (i = 0; i < nsize2; ++i) {
				const float hp = apod_hann(rate1 * float(i) - fi, bwi);
				m_freq_amp[i] += hp * hi * m_ah[n];
			}
			break;
		case Gauss:
		default:
			for (i = 0; i < nsize2; ++i) {
				const float hp = apod_gauss(rate1 * float(i) - fi, bwi);
				m_freq_amp[i] += hp * hi * m_ah[n];
			}
			break;
		}
	}

	// convert to sine and cosine components w/random phases...
	for (i = 0; i < nsize2; ++i) {
		const float phase = pseudo_randf() * 2.0f * M_PI;
		m_freq_cos[i] = m_freq_amp[i] * ::cosf(phase);
		m_freq_sin[i] = m_freq_amp[i] * ::sinf(phase);
	};

	// process IFFT
	m_fftw_data[nsize2] = 0.0f;

	for (i = 0; i < nsize2; ++i) {
		m_fftw_data[i] = m_freq_cos[i];
		if (i > 0)
			m_fftw_data[m_nsize - i] = m_freq_sin[i];
	}

	::fftwf_execute(m_fftw_plan);

	for (i = 0; i < m_nsize; ++i)
		m_table[i] = m_fftw_data[i];

	// post-process
	reset_normalize();
	reset_interp();
}


// post-processors
void padthv1_sample::reset_normalize (void)
{
	uint32_t i;

	float pmax = 0.0f;
	float pmin = 0.0f;

	for (i = 0; i < m_nsize; ++i) {
		const float p = m_table[i];
		if (pmax < p)
			pmax = p;
		else
		if (pmin > p)
			pmin = p;
	}

	const float pmid = 0.5f * (pmax + pmin);

	pmax = 0.0f;
	for (i = 0; i < m_nsize; ++i) {
		m_table[i] -= pmid;
		const float p = ::fabsf(m_table[i]);
		if (pmax < p)
			pmax = p;
	}

	if (pmax > 0.0f) {
		const float gain = 1.0f / pmax;
		for (i = 0; i < m_nsize; ++i)
			m_table[i] *= gain;
	}
}


void padthv1_sample::reset_interp (void)
{
	uint32_t i, pk = 0;

	for (i = m_nsize; i < m_nsize + 4; ++i)
		m_table[i] = m_table[i - m_nsize];

	for (i = 1; i < m_nsize; ++i) {
		const float p1 = m_table[i - 1];
		const float p2 = m_table[i];
		if (p1 < 0.0f && p2 >= 0.0f) {
			pk = i;
			break;
		}
	}

	m_phase0 = float(pk);
}


// end of padthv1_sample.cpp
