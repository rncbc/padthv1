// padthv1.cpp
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

#include "padthv1.h"

#include "padthv1_sample.h"

#include "padthv1_wave.h"
#include "padthv1_ramp.h"

#include "padthv1_list.h"

#include "padthv1_filter.h"
#include "padthv1_formant.h"

#include "padthv1_fx.h"
#include "padthv1_reverb.h"

#include "padthv1_config.h"
#include "padthv1_controls.h"
#include "padthv1_programs.h"
#include "padthv1_tuning.h"

#include "padthv1_sched.h"


#ifdef CONFIG_DEBUG_0
#include <stdio.h>
#endif

#include <string.h>


//-------------------------------------------------------------------------
// padthv1_impl
//
// -- borrowed and revamped from synth.h of synth4
//    Copyright (C) 2007 jorgen, linux-vst.com
//

const uint8_t MAX_VOICES  = 64;			// max polyphony
const uint8_t MAX_NOTES   = 128;

const float MIN_ENV_MSECS = 0.5f;		// min 500 usec per stage
const float MAX_ENV_MSECS = 5000.0f;	// max 5 sec per stage (default)

const float DETUNE_SCALE  = 0.5f;
const float PHASE_SCALE   = 0.5f;
const float OCTAVE_SCALE  = 12.0f;
const float TUNING_SCALE  = 1.0f;
const float SWEEP_SCALE   = 0.5f;
const float PITCH_SCALE   = 0.5f;

const uint8_t MAX_DIRECT_NOTES = (MAX_VOICES >> 2);


// maximum helper

inline float padthv1_max ( float a, float b )
{
	return (a > b ? a : b);
}


// hyperbolic-tangent fast approximation

inline float padthv1_tanhf ( const float x )
{
	const float x2 = x * x;
	return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}


// sigmoids

inline float padthv1_sigmoid ( const float x )
{
//	return 2.0f / (1.0f + ::expf(-5.0f * x)) - 1.0f;
	return padthv1_tanhf(2.0f * x);
}

inline float padthv1_sigmoid_0 ( const float x, const float t0 )
{
	const float t1 = 1.0f - t0;
#if 0
	if (x > +t1)
		return +t1 + t0 * padthv1_tanhf(+(x - t1) / t0);
	else
	if (x < -t1)
		return -t1 - t0 * padthv1_tanhf(-(x + t1) / t0);
	else
		return x;
#else
	return (x < -1.0f ? -t1 : (x > +1.0f ? t1 : t1 * x * (1.5f - 0.5f * x * x)));
#endif
}

inline float padthv1_sigmoid_1 ( const float x, const float t0 = 0.01f )
{
	return 0.5f * (1.0f + padthv1_sigmoid_0(2.0f * x - 1.0f, t0));
}


// velocity hard-split curve

inline float padthv1_velocity ( const float x, const float p = 0.2f )
{
	return ::powf(x, (1.0f - p));
}


// pitchbend curve

inline float padthv1_pow2f ( const float x )
{
// simplest power-of-2 straight linearization
// -- x argument valid in [-1, 1] interval
//	return 1.0f + (x < 0.0f ? 0.5f : 1.0f) * x;
	return ::powf(2.0f, x);
}


// convert note to frequency (hertz)

inline float padthv1_freq2 ( float delta )
{
	return ::powf(2.0f, delta / 12.0f);
}

inline float padthv1_freq ( int note )
{
	return (440.0f / 32.0f) * padthv1_freq2(float(note - 9));
}


// parameter port (basic)

class padthv1_port
{
public:

	padthv1_port() : m_port(nullptr), m_value(0.0f), m_vport(0.0f) {}

	virtual ~padthv1_port() {}

	void set_port(float *port)
		{ m_port = port; }
	float *port() const
		{ return m_port; }

	virtual void set_value(float value)
		{ m_value = value; if (m_port) m_vport = *m_port; }

	float value() const
		{ return m_value; }
	float *value_ptr()
		{ tick(1); return &m_value; }

	virtual float tick(uint32_t /*nstep*/)
	{
		if (m_port && ::fabsf(*m_port - m_vport) > 0.001f)
			set_value(*m_port);

		return m_value;
	}

	float operator *()
		{ return tick(1); }

private:

	float *m_port;
	float  m_value;
	float  m_vport;
};


// parameter port (smoothed)

class padthv1_port2 : public padthv1_port
{
public:

	padthv1_port2() : m_vtick(0.0f), m_vstep(0.0f), m_nstep(0) {}

	static const uint32_t NSTEP = 32;

	void set_value(float value)
	{
		m_vtick = padthv1_port::value();

		m_nstep = NSTEP;
		m_vstep = (value - m_vtick) / float(m_nstep);

		padthv1_port::set_value(value);
	}

	float tick(uint32_t nstep)
	{
		if (m_nstep == 0)
			return padthv1_port::tick(nstep);

		if (m_nstep >= nstep) {
			m_vtick += m_vstep * float(nstep);
			m_nstep -= nstep;
		} else {
			m_vtick += m_vstep * float(m_nstep);
			m_nstep  = 0;
		}

		return m_vtick;
	}

private:

	float    m_vtick;
	float    m_vstep;
	uint32_t m_nstep;
};


// envelope

struct padthv1_env
{
	// envelope stages

	enum Stage { Idle = 0, Attack, Decay, Sustain, Release, End };

	// per voice

	struct State
	{
		// ctor.
		State() : running(false), stage(Idle),
			phase(0.0f), delta(0.0f), value(0.0f),
			c1(1.0f), c0(0.0f), frames(0) {}

		// process
		float tick()
		{
			if (running && frames > 0) {
				phase += delta;
				value = c1 * phase * (2.0f - phase) + c0;
				--frames;
			}
			return value;
		}

		// state
		bool running;
		Stage stage;
		float phase;
		float delta;
		float value;
		float c1, c0;
		uint32_t frames;
	};

	void start(State *p)
	{
		p->running = true;
		p->stage = Attack;
		p->frames = uint32_t(*attack * *attack * max_frames);
		if (p->frames < min_frames1) // prevent click on too fast attack
			p->frames = min_frames1;
		p->phase = 0.0f;
		p->delta = 1.0f / float(p->frames);
		p->value = 0.0f;
		p->c1 = 1.0f;
		p->c0 = 0.0f;
	}

	void next(State *p)
	{
		if (p->stage == Attack) {
			p->stage = Decay;
			p->frames = uint32_t(*decay * *decay * max_frames);
			if (p->frames < min_frames2) // prevent click on too fast decay
				p->frames = min_frames2;
			p->phase = 0.0f;
			p->delta = 1.0f / float(p->frames);
			p->c1 = *sustain - 1.0f;
			p->c0 = p->value;
		}
		else if (p->stage == Decay) {
			p->running = false; // stay at this stage until note_off received
			p->stage = Sustain;
			p->frames = 0;
			p->phase = 0.0f;
			p->delta = 0.0f;
			p->c1 = 0.0f;
			p->c0 = p->value;
		}
		else if (p->stage == Release) {
			p->running = false;
			p->stage = End;
			p->frames = 0;
			p->phase = 0.0f;
			p->delta = 0.0f;
			p->value = 0.0f;
			p->c1 = 0.0f;
			p->c0 = 0.0f;
		}
	}

	void note_off(State *p)
	{
		p->running = true;
		p->stage = Release;
		p->frames = uint32_t(*release * *release * max_frames);
		if (p->frames < min_frames2) // prevent click on too fast release
			p->frames = min_frames2;
		p->phase = 0.0f;
		p->delta = 1.0f / float(p->frames);
		p->c1 = -(p->value);
		p->c0 = p->value;
	}

	void note_off_fast(State *p)
	{
		p->running = true;
		p->stage = Release;
		p->frames = min_frames2;
		p->phase = 0.0f;
		p->delta = 1.0f / float(p->frames);
		p->c1 = -(p->value);
		p->c0 = p->value;
	}

	void restart(State *p, bool legato)
	{
		p->running = true;
		if (legato) {
			p->stage = Decay;
			p->frames = min_frames2;
			p->phase = 0.0f;
			p->delta = 1.0f / float(p->frames);
			p->c1 = *sustain - p->value;
			p->c0 = 0.0f;
		} else {
			p->stage = Attack;
			p->frames = uint32_t(*attack * *attack * max_frames);
			if (p->frames < min_frames1)
				p->frames = min_frames1;
			p->phase = 0.0f;
			p->delta = 1.0f / float(p->frames);
			p->c1 = 1.0f;
			p->c0 = 0.0f;
		}
	}

	// parameters

	padthv1_port attack;
	padthv1_port decay;
	padthv1_port sustain;
	padthv1_port release;

	uint32_t min_frames1;
	uint32_t min_frames2;
	uint32_t max_frames;
};


// midi control

struct padthv1_ctl
{
	padthv1_ctl() { reset(); }

	void reset()
	{
		pressure = 0.0f;
		pitchbend = 1.0f;
		modwheel = 0.0f;
		panning = 0.0f;
		volume = 1.0f;
		sustain = false;
	}

	float pressure;
	float pitchbend;
	float modwheel;
	float panning;
	float volume;
	bool  sustain;
};


// dco

struct padthv1_gen
{
	padthv1_port sample1;
	padthv1_port width1;
	padthv1_port scale1;
	padthv1_port nh1;
	padthv1_port apod1;
	padthv1_port detune1;
	padthv1_port glide1;

	padthv1_port sample2;
	padthv1_port width2;
	padthv1_port scale2;
	padthv1_port nh2;
	padthv1_port apod2;
	padthv1_port detune2;
	padthv1_port glide2;

	padthv1_port balance;
	padthv1_port phase;
	padthv1_port ringmod;
	padthv1_port octave;
	padthv1_port tuning;
	padthv1_port envtime;

	float sample1_0, freq1;
	float sample2_0, freq2;

	float envtime0;
};


// dcf

struct padthv1_dcf
{
	padthv1_port  enabled;
	padthv1_port2 cutoff;
	padthv1_port2 reso;
	padthv1_port  type;
	padthv1_port  slope;
	padthv1_port2 envelope;

	padthv1_env   env;
};


// lfo
struct padthv1_voice;

struct padthv1_lfo
{
	padthv1_port  enabled;
	padthv1_port  shape;
	padthv1_port  width;
	padthv1_port2 bpm;
	padthv1_port2 rate;
	padthv1_port  sync;
	padthv1_port2 sweep;
	padthv1_port2 pitch;
	padthv1_port2 balance;
	padthv1_port2 ringmod;
	padthv1_port2 cutoff;
	padthv1_port2 reso;
	padthv1_port2 panning;
	padthv1_port2 volume;

	padthv1_env   env;

	padthv1_voice *psync;
};


// dca

struct padthv1_dca
{
	padthv1_port volume;

	padthv1_env  env;
};



// def (ranges)

struct padthv1_def
{
	padthv1_port pitchbend;
	padthv1_port modwheel;
	padthv1_port pressure;
	padthv1_port velocity;
	padthv1_port channel;
	padthv1_port mono;
};


// out (mix)

struct padthv1_out
{
	padthv1_port width;
	padthv1_port panning;
	padthv1_port fxsend;
	padthv1_port volume;
};


// chorus (fx)

struct padthv1_cho
{
	padthv1_port wet;
	padthv1_port delay;
	padthv1_port feedb;
	padthv1_port rate;
	padthv1_port mod;
};


// flanger (fx)

struct padthv1_fla
{
	padthv1_port wet;
	padthv1_port delay;
	padthv1_port feedb;
	padthv1_port daft;
};


// phaser (fx)

struct padthv1_pha
{
	padthv1_port wet;
	padthv1_port rate;
	padthv1_port feedb;
	padthv1_port depth;
	padthv1_port daft;
};


// delay (fx)

struct padthv1_del
{
	padthv1_port wet;
	padthv1_port delay;
	padthv1_port feedb;
	padthv1_port bpm;
};


// reverb

struct padthv1_rev
{
	padthv1_port wet;
	padthv1_port room;
	padthv1_port damp;
	padthv1_port feedb;
	padthv1_port width;
};


// dynamic(compressor/limiter)

struct padthv1_dyn
{
	padthv1_port compress;
	padthv1_port limiter;
};


// keyboard/note range

struct padthv1_key
{
	padthv1_port low;
	padthv1_port high;

	bool is_note(int key)
	{
		return (key >= int(*low) && int(*high) >= key);
	}
};


// glide (portamento)

struct padthv1_glide
{
	padthv1_glide(float& last) : m_last(last) { reset(); }

	void reset( uint32_t frames = 0, float freq = 0.0f )
	{
		m_frames = frames;

		if (m_frames > 0) {
			m_freq = m_last - freq;
			m_step = m_freq / float(m_frames);
		} else {
			m_freq = 0.0f;
			m_step = 0.0f;
		}

		m_last = freq;
	}

	float tick()
	{
		if (m_frames > 0) {
			m_freq -= m_step;
			--m_frames;
		}
		return m_freq;
	}

private:

	uint32_t m_frames;

	float m_freq;
	float m_step;

	float& m_last;
};


// balancing smoother (1 parameter)

class padthv1_bal : public padthv1_ramp2
{
public:

	padthv1_bal() : padthv1_ramp2(2) {}

protected:

	float evaluate(uint16_t i)
	{
		padthv1_ramp2::update();

		const float wbal = 0.25f * M_PI
			* (1.0f + m_param1_v)
			* (1.0f + m_param2_v);

		return M_SQRT2 * (i == 0 ? ::cosf(wbal) : ::sinf(wbal));
	}
};


// balance smoother (1 parameters)

class padthv1_bal1 : public padthv1_ramp1
{
public:

	padthv1_bal1() : padthv1_ramp1(2) {}

protected:

	float evaluate(uint16_t i)
	{
		padthv1_ramp1::update();

		const float wbal = 0.25f * M_PI
			* (1.0f + m_param1_v);

		return M_SQRT2 * (i & 1 ? ::sinf(wbal) : ::cosf(wbal));
	}
};


// balance smoother (2 parameters)

class padthv1_bal2 : public padthv1_ramp2
{
public:

	padthv1_bal2() : padthv1_ramp2(2) {}

protected:

	float evaluate(uint16_t i)
	{
		padthv1_ramp2::update();

		const float wbal = 0.25f * M_PI
			* (1.0f + m_param1_v)
			* (1.0f + m_param2_v);

		return M_SQRT2 * (i & 1 ? ::sinf(wbal) : ::cosf(wbal));
	}
};


// pressure smoother (3 parameters)

class padthv1_pre : public padthv1_ramp3
{
public:

	padthv1_pre() : padthv1_ramp3() {}

protected:

	float evaluate(uint16_t)
	{
		padthv1_ramp3::update();

		return m_param1_v * padthv1_max(m_param2_v, m_param3_v);
	}
};


// common phasor (LFO sync)

class padthv1_phasor
{
public:

	padthv1_phasor(uint32_t nsize = 1024)
		: m_nsize(nsize), m_nframes(0) {}

	void process(uint32_t nframes)
	{
		m_nframes += nframes;
		while (m_nframes >= m_nsize)
			m_nframes -= m_nsize;
	}

	float pshift() const
		{ return float(m_nframes) / float(m_nsize); }

private:

	uint32_t m_nsize;
	uint32_t m_nframes;
};


// forward decl.

class padthv1_impl;


// voice

struct padthv1_voice : public padthv1_list<padthv1_voice>
{
	padthv1_voice(padthv1_impl *pImpl);

	int note;									// voice note

	float vel;									// key velocity
	float pre;									// key pressure/after-touch

	padthv1_generator gen1_osc1, gen1_osc2;		// generators aka. oscillators

	float gen1_freq1, gen1_sample1;				// frequency and phase
	float gen1_freq2, gen1_sample2;

	padthv1_oscillator lfo1;					// low frequency oscilattor

	float lfo1_sample;

	padthv1_bal gen1_bal;						// oscillator balance

	float gen1_balance;

	padthv1_filter1 dcf11, dcf12;				// filters
	padthv1_filter2 dcf13, dcf14;
	padthv1_filter3 dcf15, dcf16;
	padthv1_formant dcf17, dcf18;

	padthv1_env::State dca1_env;				// envelope states
	padthv1_env::State dcf1_env;
	padthv1_env::State lfo1_env;

	padthv1_glide gen1_glide1, gen1_glide2;		// glides (portamento)

	padthv1_pre dca1_pre;

	float out1_panning;
	float out1_volume;

	padthv1_bal1  out1_pan;						// output panning
	padthv1_ramp1 out1_vol;						// output volume

	bool sustain;
};


// MIDI input asynchronous status notification

class padthv1_midi_in : public padthv1_sched
{
public:

	padthv1_midi_in (padthv1 *pSampl)
		: padthv1_sched(pSampl, MidiIn),
			m_enabled(false), m_count(0) {}

	void schedule_event()
		{ if (m_enabled && ++m_count < 2) schedule(-1); }
	void schedule_note(int key, int vel)
		{ if (m_enabled) schedule((vel << 7) | key); }

	void process(int) {}

	void enabled(bool on)
		{ m_enabled = on; m_count = 0; }

	uint32_t count()
	{
		const uint32_t ret = m_count;
		m_count = 0;
		return ret;
	}

private:

	bool     m_enabled;
	uint32_t m_count;
};


// micro-tuning/instance implementation

class padthv1_tun
{
public:

	padthv1_tun() : enabled(false), refPitch(440.0f), refNote(69) {}

	bool    enabled;
	float   refPitch;
	int     refNote;
	QString scaleFile;
	QString keyMapFile;
};


// polyphonic synth implementation

class padthv1_impl
{
public:

	padthv1_impl(padthv1 *pSampl, uint16_t nchannels, float srate);

	~padthv1_impl();

	void setChannels(uint16_t nchannels);
	uint16_t channels() const;

	void setSampleRate(float srate);
	float sampleRate() const;

	void setBufferSize(uint32_t nsize);
	uint32_t bufferSize() const;

	void setTempo(float bpm);
	float tempo() const;

	void setParamPort(padthv1::ParamIndex index, float *pfParam);
	padthv1_port *paramPort(padthv1::ParamIndex index);

	void setParamValue(padthv1::ParamIndex index, float fValue);
	float paramValue(padthv1::ParamIndex index);

	padthv1_controls *controls();
	padthv1_programs *programs();

	void setTuningEnabled(bool enabled);
	bool isTuningEnabled() const;

	void setTuningRefPitch(float refPitch);
	float tuningRefPitch() const;

	void setTuningRefNote(int refNote);
	int tuningRefNote() const;

	void setTuningScaleFile(const char *pszScaleFile);
	const char *tuningScaleFile() const;

	void setTuningKeyMapFile(const char *pszKeyMapFile);
	const char *tuningKeyMapFile() const;

	void resetTuning();

	void process_midi(uint8_t *data, uint32_t size);
	void process(float **ins, float **outs, uint32_t nframes);

	void stabilize();
	void reset();

	void midiInEnabled(bool on);
	uint32_t midiInCount();

	void directNoteOn(int note, int vel);

	bool running(bool on);

	padthv1_sample gen1_sample1, gen1_sample2;

	padthv1_wave_lf lfo1_wave;

	float gen1_last1, gen1_last2;

	padthv1_formant::Impl dcf1_formant;

protected:

	void updateEnvTimes();

	void allSoundOff();
	void allControllersOff();
	void allNotesOff();
	void allSustainOff();

	float get_bpm ( float bpm ) const
		{ return (bpm > 0.0f ? bpm : m_bpm); }

	padthv1_voice *alloc_voice ()
	{
		padthv1_voice *pv = m_free_list.next();
		if (pv) {
			m_free_list.remove(pv);
			m_play_list.append(pv);
			++m_nvoices;
		}
		return pv;
	}

	void free_voice ( padthv1_voice *pv )
	{
		if (m_lfo1.psync == pv)
			m_lfo1.psync = nullptr;

		m_play_list.remove(pv);
		m_free_list.append(pv);
		--m_nvoices;
	}

	void alloc_sfxs(uint32_t nsize);

private:

	padthv1_config   m_config;
	padthv1_controls m_controls;
	padthv1_programs m_programs;
	padthv1_midi_in  m_midi_in;
	padthv1_tun      m_tun;

	uint16_t m_nchannels;
	float    m_srate;
	float    m_bpm;

	float    m_freqs[MAX_NOTES];

	padthv1_ctl m_ctl1;

	padthv1_gen m_gen1;
	padthv1_dcf m_dcf1;
	padthv1_lfo m_lfo1;
	padthv1_dca m_dca1;
	padthv1_out m_out1;

	padthv1_def m_def;

	padthv1_cho m_cho;
	padthv1_fla m_fla;
	padthv1_pha m_pha;
	padthv1_del m_del;
	padthv1_rev m_rev;
	padthv1_dyn m_dyn;

	padthv1_key m_key;

	padthv1_voice **m_voices;
	padthv1_voice  *m_notes[MAX_NOTES];

	padthv1_list<padthv1_voice> m_free_list;
	padthv1_list<padthv1_voice> m_play_list;

	padthv1_ramp1 m_wid1;
	padthv1_bal2  m_pan1;
	padthv1_ramp3 m_vol1;

	float  **m_sfxs;
	uint32_t m_nsize;

	padthv1_fx_chorus   m_chorus;
	padthv1_fx_flanger *m_flanger;
	padthv1_fx_phaser  *m_phaser;
	padthv1_fx_delay   *m_delay;
	padthv1_fx_comp    *m_comp;

	padthv1_reverb m_reverb;

	// process direct note on/off...
	volatile uint16_t m_direct_note;

	struct direct_note {
		uint8_t status, note, vel;
	} m_direct_notes[MAX_DIRECT_NOTES];

	volatile int  m_nvoices;

	volatile bool m_running;
};


// voice constructor

padthv1_voice::padthv1_voice ( padthv1_impl *pImpl ) :
	note(-1),
	vel(0.0f),
	pre(0.0f),
	gen1_osc1(&pImpl->gen1_sample1),
	gen1_osc2(&pImpl->gen1_sample2),
	gen1_freq1(0.0f), gen1_sample1(0.0f),
	gen1_freq2(0.0f), gen1_sample2(0.0f),
	lfo1(&pImpl->lfo1_wave),
	lfo1_sample(0.0f),
	dcf17(&pImpl->dcf1_formant),
	dcf18(&pImpl->dcf1_formant),
	gen1_glide1(pImpl->gen1_last1),
	gen1_glide2(pImpl->gen1_last2),
	sustain(false)
{
}


// engine constructor

padthv1_impl::padthv1_impl (
	padthv1 *pSampl, uint16_t nchannels, float srate )
	: gen1_sample1(pSampl, 1), gen1_sample2(pSampl, 2),
		m_controls(pSampl), m_programs(pSampl),
		m_midi_in(pSampl), m_bpm(180.0f), m_nvoices(0), m_running(false)
{
	// null sample freqs.
	m_gen1.sample1_0 = m_gen1.sample2_0 = 0.0f;
	m_gen1.freq1 = m_gen1.freq2 = 0.0f;

	// max env. stage length (default)
	m_gen1.envtime0 = 0.0001f * MAX_ENV_MSECS;

	// glide note.
	gen1_last1 = gen1_last2 = 0.0f;

	// allocate voice pool.
	m_voices = new padthv1_voice * [MAX_VOICES];

	for (int i = 0; i < MAX_VOICES; ++i) {
		m_voices[i] = new padthv1_voice(this);
		m_free_list.append(m_voices[i]);
	}

	for (int note = 0; note < MAX_NOTES; ++note)
		m_notes[note] = nullptr;

	// local buffers none yet
	m_sfxs = nullptr;
	m_nsize = 0;

	// flangers none yet
	m_flanger = nullptr;

	// phasers none yet
	m_phaser = nullptr;

	// delays none yet
	m_delay = nullptr;

	// compressors none yet
	m_comp = nullptr;

	// Micro-tuning support, if any...
	resetTuning();

	// load controllers & programs database...
	m_config.loadControls(&m_controls);
	m_config.loadPrograms(&m_programs);

	// number of channels
	setChannels(nchannels);

	// set default sample rate
	setSampleRate(srate);

	// reset all voices
	allControllersOff();
	allNotesOff();

	running(true);
}


// destructor

padthv1_impl::~padthv1_impl (void)
{
#if 0
	// DO NOT save programs database here:
	// prevent multi-instance clash...
	m_config.savePrograms(&m_programs);
#endif

	// deallocate voice pool.
	for (int i = 0; i < MAX_VOICES; ++i)
		delete m_voices[i];

	delete [] m_voices;

	// deallocate local buffers
	alloc_sfxs(0);

	// deallocate channels
	setChannels(0);
}


void padthv1_impl::setChannels ( uint16_t nchannels )
{
	m_nchannels = nchannels;

	// deallocate flangers
	if (m_flanger) {
		delete [] m_flanger;
		m_flanger = nullptr;
	}

	// deallocate phasers
	if (m_phaser) {
		delete [] m_phaser;
		m_phaser = nullptr;
	}

	// deallocate delays
	if (m_delay) {
		delete [] m_delay;
		m_delay = nullptr;
	}

	// deallocate compressors
	if (m_comp) {
		delete [] m_comp;
		m_comp = nullptr;
	}
}


uint16_t padthv1_impl::channels (void) const
{
	return m_nchannels;
}


void padthv1_impl::setSampleRate ( float srate )
{
	// set internal sample rate
	m_srate = srate;

	// update waves sample rate
	gen1_sample1.setSampleRate(m_srate);
	gen1_sample2.setSampleRate(m_srate);
	lfo1_wave.setSampleRate(m_srate);

	updateEnvTimes();

	dcf1_formant.setSampleRate(m_srate);
}


float padthv1_impl::sampleRate (void) const
{
	return m_srate;
}


void padthv1_impl::setBufferSize ( uint32_t nsize )
{
	// set nominal buffer size
	if (m_nsize < nsize) alloc_sfxs(nsize);
}


uint32_t padthv1_impl::bufferSize (void) const
{
	return m_nsize;
}


void padthv1_impl::setTempo ( float bpm )
{
	// set nominal tempo (BPM)
	m_bpm = bpm;
}


float padthv1_impl::tempo (void) const
{
	return m_bpm;
}


// allocate local buffers
void padthv1_impl::alloc_sfxs ( uint32_t nsize )
{
	if (m_sfxs) {
		for (uint16_t k = 0; k < m_nchannels; ++k)
			delete [] m_sfxs[k];
		delete [] m_sfxs;
		m_sfxs = nullptr;
		m_nsize = 0;
	}

	if (m_nsize < nsize) {
		m_nsize = nsize;
		m_sfxs = new float * [m_nchannels];
		for (uint16_t k = 0; k < m_nchannels; ++k)
			m_sfxs[k] = new float [m_nsize];
	}
}


void padthv1_impl::updateEnvTimes (void)
{
	// update envelope range times in frames
	const float srate_ms = 0.001f * m_srate;

	float envtime_msecs = 10000.0f * m_gen1.envtime0;
	if (envtime_msecs < MIN_ENV_MSECS)
		envtime_msecs = (gen1_sample1.size() >> 1) / srate_ms;
	if (envtime_msecs < MIN_ENV_MSECS)
		envtime_msecs = (gen1_sample2.size() >> 1) / srate_ms;
	if (envtime_msecs < MIN_ENV_MSECS)
		envtime_msecs = MIN_ENV_MSECS * 4.0f;

	const uint32_t min_frames1 = uint32_t(srate_ms * MIN_ENV_MSECS);
	const uint32_t min_frames2 = (min_frames1 << 2);
	const uint32_t max_frames  = uint32_t(srate_ms * envtime_msecs);

	m_dcf1.env.min_frames1 = min_frames1;
	m_dcf1.env.min_frames2 = min_frames2;
	m_dcf1.env.max_frames  = max_frames;

	m_lfo1.env.min_frames1 = min_frames1;
	m_lfo1.env.min_frames2 = min_frames2;
	m_lfo1.env.max_frames  = max_frames;

	m_dca1.env.min_frames1 = min_frames1;
	m_dca1.env.min_frames2 = min_frames2;
	m_dca1.env.max_frames  = max_frames;
}


void padthv1_impl::setParamPort ( padthv1::ParamIndex index, float *pfParam )
{
	static float s_fDummy = 0.0f;

	if (pfParam == nullptr)
		pfParam = &s_fDummy;

	padthv1_port *pParamPort = paramPort(index);
	if (pParamPort)
		pParamPort->set_port(pfParam);

	// check null connections.
	if (pfParam == &s_fDummy)
		return;

	// reset ramps after port (re)connection.
	switch (index) {
	case padthv1::OUT1_VOLUME:
	case padthv1::DCA1_VOLUME:
		m_vol1.reset(
			m_out1.volume.value_ptr(),
			m_dca1.volume.value_ptr(),
			&m_ctl1.volume);
		break;
	case padthv1::OUT1_WIDTH:
		m_wid1.reset(
			m_out1.width.value_ptr());
		break;
	case padthv1::OUT1_PANNING:
		m_pan1.reset(
			m_out1.panning.value_ptr(),
			&m_ctl1.panning);
		break;
	default:
		break;
	}
}


padthv1_port *padthv1_impl::paramPort ( padthv1::ParamIndex index )
{
	padthv1_port *pParamPort = nullptr;

	switch (index) {
	case padthv1::GEN1_SAMPLE1:   pParamPort = &m_gen1.sample1;     break;
	case padthv1::GEN1_WIDTH1:    pParamPort = &m_gen1.width1;      break;
	case padthv1::GEN1_SCALE1:    pParamPort = &m_gen1.scale1;      break;
	case padthv1::GEN1_NH1:       pParamPort = &m_gen1.nh1;         break;
	case padthv1::GEN1_APOD1:     pParamPort = &m_gen1.apod1;       break;
	case padthv1::GEN1_DETUNE1:   pParamPort = &m_gen1.detune1;     break;
	case padthv1::GEN1_GLIDE1:    pParamPort = &m_gen1.glide1;      break;
	case padthv1::GEN1_SAMPLE2:   pParamPort = &m_gen1.sample2;     break;
	case padthv1::GEN1_WIDTH2:    pParamPort = &m_gen1.width2;      break;
	case padthv1::GEN1_SCALE2:    pParamPort = &m_gen1.scale2;      break;
	case padthv1::GEN1_NH2:       pParamPort = &m_gen1.nh2;         break;
	case padthv1::GEN1_APOD2:     pParamPort = &m_gen1.apod2;       break;
	case padthv1::GEN1_DETUNE2:   pParamPort = &m_gen1.detune2;     break;
	case padthv1::GEN1_GLIDE2:    pParamPort = &m_gen1.glide2;      break;
	case padthv1::GEN1_BALANCE:   pParamPort = &m_gen1.balance;     break;
	case padthv1::GEN1_PHASE:     pParamPort = &m_gen1.phase;       break;
	case padthv1::GEN1_RINGMOD:   pParamPort = &m_gen1.ringmod;     break;
	case padthv1::GEN1_OCTAVE:    pParamPort = &m_gen1.octave;      break;
	case padthv1::GEN1_TUNING:    pParamPort = &m_gen1.tuning;      break;
	case padthv1::GEN1_ENVTIME:   pParamPort = &m_gen1.envtime;     break;
	case padthv1::DCF1_ENABLED:   pParamPort = &m_dcf1.enabled;     break;
	case padthv1::DCF1_CUTOFF:    pParamPort = &m_dcf1.cutoff;      break;
	case padthv1::DCF1_RESO:      pParamPort = &m_dcf1.reso;        break;
	case padthv1::DCF1_TYPE:      pParamPort = &m_dcf1.type;        break;
	case padthv1::DCF1_SLOPE:     pParamPort = &m_dcf1.slope;       break;
	case padthv1::DCF1_ENVELOPE:  pParamPort = &m_dcf1.envelope;    break;
	case padthv1::DCF1_ATTACK:    pParamPort = &m_dcf1.env.attack;  break;
	case padthv1::DCF1_DECAY:     pParamPort = &m_dcf1.env.decay;   break;
	case padthv1::DCF1_SUSTAIN:   pParamPort = &m_dcf1.env.sustain; break;
	case padthv1::DCF1_RELEASE:   pParamPort = &m_dcf1.env.release; break;
	case padthv1::LFO1_ENABLED:   pParamPort = &m_lfo1.enabled;     break;
	case padthv1::LFO1_SHAPE:     pParamPort = &m_lfo1.shape;       break;
	case padthv1::LFO1_WIDTH:     pParamPort = &m_lfo1.width;       break;
	case padthv1::LFO1_BPM:       pParamPort = &m_lfo1.bpm;         break;
	case padthv1::LFO1_RATE:      pParamPort = &m_lfo1.rate;        break;
	case padthv1::LFO1_SYNC:      pParamPort = &m_lfo1.sync;        break;
	case padthv1::LFO1_SWEEP:     pParamPort = &m_lfo1.sweep;       break;
	case padthv1::LFO1_PITCH:     pParamPort = &m_lfo1.pitch;       break;
	case padthv1::LFO1_BALANCE:   pParamPort = &m_lfo1.balance;     break;
	case padthv1::LFO1_RINGMOD:   pParamPort = &m_lfo1.ringmod;     break;
	case padthv1::LFO1_CUTOFF:    pParamPort = &m_lfo1.cutoff;      break;
	case padthv1::LFO1_RESO:      pParamPort = &m_lfo1.reso;        break;
	case padthv1::LFO1_PANNING:   pParamPort = &m_lfo1.panning;     break;
	case padthv1::LFO1_VOLUME:    pParamPort = &m_lfo1.volume;      break;
	case padthv1::LFO1_ATTACK:    pParamPort = &m_lfo1.env.attack;  break;
	case padthv1::LFO1_DECAY:     pParamPort = &m_lfo1.env.decay;   break;
	case padthv1::LFO1_SUSTAIN:   pParamPort = &m_lfo1.env.sustain; break;
	case padthv1::LFO1_RELEASE:   pParamPort = &m_lfo1.env.release; break;
	case padthv1::DCA1_VOLUME:    pParamPort = &m_dca1.volume;      break;
	case padthv1::DCA1_ATTACK:    pParamPort = &m_dca1.env.attack;  break;
	case padthv1::DCA1_DECAY:     pParamPort = &m_dca1.env.decay;   break;
	case padthv1::DCA1_SUSTAIN:   pParamPort = &m_dca1.env.sustain; break;
	case padthv1::DCA1_RELEASE:   pParamPort = &m_dca1.env.release; break;
	case padthv1::OUT1_WIDTH:     pParamPort = &m_out1.width;       break;
	case padthv1::OUT1_PANNING:   pParamPort = &m_out1.panning;     break;
	case padthv1::OUT1_FXSEND:    pParamPort = &m_out1.fxsend;      break;
	case padthv1::OUT1_VOLUME:    pParamPort = &m_out1.volume;      break;
	case padthv1::DEF1_PITCHBEND: pParamPort = &m_def.pitchbend;    break;
	case padthv1::DEF1_MODWHEEL:  pParamPort = &m_def.modwheel;     break;
	case padthv1::DEF1_PRESSURE:  pParamPort = &m_def.pressure;     break;
	case padthv1::DEF1_VELOCITY:  pParamPort = &m_def.velocity;     break;
	case padthv1::DEF1_CHANNEL:   pParamPort = &m_def.channel;      break;
	case padthv1::DEF1_MONO:      pParamPort = &m_def.mono;         break;
	case padthv1::CHO1_WET:       pParamPort = &m_cho.wet;          break;
	case padthv1::CHO1_DELAY:     pParamPort = &m_cho.delay;        break;
	case padthv1::CHO1_FEEDB:     pParamPort = &m_cho.feedb;        break;
	case padthv1::CHO1_RATE:      pParamPort = &m_cho.rate;         break;
	case padthv1::CHO1_MOD:       pParamPort = &m_cho.mod;          break;
	case padthv1::FLA1_WET:       pParamPort = &m_fla.wet;          break;
	case padthv1::FLA1_DELAY:     pParamPort = &m_fla.delay;        break;
	case padthv1::FLA1_FEEDB:     pParamPort = &m_fla.feedb;        break;
	case padthv1::FLA1_DAFT:      pParamPort = &m_fla.daft;         break;
	case padthv1::PHA1_WET:       pParamPort = &m_pha.wet;          break;
	case padthv1::PHA1_RATE:      pParamPort = &m_pha.rate;         break;
	case padthv1::PHA1_FEEDB:     pParamPort = &m_pha.feedb;        break;
	case padthv1::PHA1_DEPTH:     pParamPort = &m_pha.depth;        break;
	case padthv1::PHA1_DAFT:      pParamPort = &m_pha.daft;         break;
	case padthv1::DEL1_WET:       pParamPort = &m_del.wet;          break;
	case padthv1::DEL1_DELAY:     pParamPort = &m_del.delay;        break;
	case padthv1::DEL1_FEEDB:     pParamPort = &m_del.feedb;        break;
	case padthv1::DEL1_BPM:       pParamPort = &m_del.bpm;          break;
	case padthv1::REV1_WET:       pParamPort = &m_rev.wet;          break;
	case padthv1::REV1_ROOM:      pParamPort = &m_rev.room;         break;
	case padthv1::REV1_DAMP:      pParamPort = &m_rev.damp;         break;
	case padthv1::REV1_FEEDB:     pParamPort = &m_rev.feedb;        break;
	case padthv1::REV1_WIDTH:     pParamPort = &m_rev.width;        break;
	case padthv1::DYN1_COMPRESS:  pParamPort = &m_dyn.compress;     break;
	case padthv1::DYN1_LIMITER:   pParamPort = &m_dyn.limiter;      break;
	case padthv1::KEY1_LOW:       pParamPort = &m_key.low;          break;
	case padthv1::KEY1_HIGH:      pParamPort = &m_key.high;         break;
	default: break;
	}

	return pParamPort;
}


void padthv1_impl::setParamValue ( padthv1::ParamIndex index, float fValue )
{
	padthv1_port *pParamPort = paramPort(index);
	if (pParamPort)
		pParamPort->set_value(fValue);
}


float padthv1_impl::paramValue ( padthv1::ParamIndex index )
{
	padthv1_port *pParamPort = paramPort(index);
	return (pParamPort ? pParamPort->value() : 0.0f);
}


// handle midi input

void padthv1_impl::process_midi ( uint8_t *data, uint32_t size )
{
	for (uint32_t i = 0; i < size; ++i) {

		// channel status
		const int channel = (data[i] & 0x0f) + 1;
		const int status  = (data[i] & 0xf0);

		// channel filter
		const int ch = int(*m_def.channel);
		const int on = (ch == 0 || ch == channel);

		// all system common/real-time ignored
		if (status == 0xf0)
			continue;

		// check data size (#1)
		if (++i >= size)
			break;

		const int key = (data[i] & 0x7f);

		// program change
		if (status == 0xc0) {
			if (on) m_programs.prog_change(key);
			continue;
		}

		// channel aftertouch
		if (status == 0xd0) {
			if (on) m_ctl1.pressure = float(key) / 127.0f;
			continue;
		}

		// check data size (#2)
		if (++i >= size)
			break;

		// channel value
		const int value = (data[i] & 0x7f);

		// channel/controller filter
		if (!on) {
			if (status == 0xb0)
				m_controls.process_enqueue(channel, key, value);
			continue;
		}

		// note on
		if (status == 0x90 && value > 0) {
			if (!m_key.is_note(key))
				continue;
			padthv1_voice *pv;
			// mono voice modes
			if (*m_def.mono > 0.0f) {
				int n = 0;
				for (pv = m_play_list.next(); pv; pv = pv->next()) {
					if (pv->note >= 0
						&& pv->dca1_env.stage != padthv1_env::Release) {
						m_dcf1.env.note_off_fast(&pv->dcf1_env);
						m_lfo1.env.note_off_fast(&pv->lfo1_env);
						m_dca1.env.note_off_fast(&pv->dca1_env);
						if (++n > 1) { // there shall be only one
							m_notes[pv->note] = nullptr;
							pv->note = -1;
						}
					}
				}
			}
			pv = m_notes[key];
			if (pv && pv->note >= 0/* && !m_ctl1.sustain*/) {
				// retrigger fast release
				m_dcf1.env.note_off_fast(&pv->dcf1_env);
				m_lfo1.env.note_off_fast(&pv->lfo1_env);
				m_dca1.env.note_off_fast(&pv->dca1_env);
				m_notes[pv->note] = nullptr;
				pv->note = -1;
			}
			// find free voice
			pv = alloc_voice();
			if (pv) {
				// waveform
				pv->note = key;
				// velocity
				const float vel = float(value) / 127.0f;
				// quadratic velocity law
				pv->vel = padthv1_velocity(vel * vel, *m_def.velocity);
				// balance
				pv->gen1_balance = 0.0f;
				pv->gen1_bal.reset(m_gen1.balance.value_ptr(), &pv->gen1_balance);
				// pressure/after-touch
				pv->pre = 0.0f;
				pv->dca1_pre.reset(
					m_def.pressure.value_ptr(),
					&m_ctl1.pressure, &pv->pre);
				// frequencies
				const float gen1_tuning
					= *m_gen1.octave * OCTAVE_SCALE
					+ *m_gen1.tuning * TUNING_SCALE;
				const float gen1_detune1
					= *m_gen1.detune1 * DETUNE_SCALE;
				const float gen1_detune2
					= *m_gen1.detune2 * DETUNE_SCALE;
				pv->gen1_freq1 = m_freqs[key] * padthv1_freq2(gen1_tuning + gen1_detune1);
				pv->gen1_freq2 = m_freqs[key] * padthv1_freq2(gen1_tuning + gen1_detune2);
				// phases
				const float gen1_phase = *m_gen1.phase * PHASE_SCALE;
				pv->gen1_sample1 = pv->gen1_osc1.start(      0.0f, pv->gen1_freq1);
				pv->gen1_sample2 = pv->gen1_osc2.start(gen1_phase, pv->gen1_freq2);
				// filters
				const int dcf1_type = int(*m_dcf1.type);
				pv->dcf11.reset(padthv1_filter1::Type(dcf1_type));
				pv->dcf12.reset(padthv1_filter1::Type(dcf1_type));
				pv->dcf13.reset(padthv1_filter2::Type(dcf1_type));
				pv->dcf14.reset(padthv1_filter2::Type(dcf1_type));
				pv->dcf15.reset(padthv1_filter3::Type(dcf1_type));
				pv->dcf16.reset(padthv1_filter3::Type(dcf1_type));
				// formant filters
				const float dcf1_cutoff = *m_dcf1.cutoff;
				const float dcf1_reso = *m_dcf1.reso;
				pv->dcf17.reset_filters(dcf1_cutoff, dcf1_reso);
				pv->dcf18.reset_filters(dcf1_cutoff, dcf1_reso);
				// envelopes
				m_dcf1.env.start(&pv->dcf1_env);
				m_lfo1.env.start(&pv->lfo1_env);
				m_dca1.env.start(&pv->dca1_env);
				// lfos
				const float lfo1_pshift
					= (m_lfo1.psync ? m_lfo1.psync->lfo1.pshift() : 0.0f);
				pv->lfo1_sample = pv->lfo1.start(lfo1_pshift);
				if (*m_lfo1.sync > 0.0f && m_lfo1.psync == nullptr)
					m_lfo1.psync = pv;
				// glides (portamentoa)
				const float gen1_frames1
					= uint32_t(*m_gen1.glide1 * *m_gen1.glide1 * m_srate);
				const float gen1_frames2
					= uint32_t(*m_gen1.glide2 * *m_gen1.glide2 * m_srate);
				pv->gen1_glide1.reset(gen1_frames1, pv->gen1_freq1);
				pv->gen1_glide2.reset(gen1_frames2, pv->gen1_freq2);
				// panning
				pv->out1_panning = 0.0f;
				pv->out1_pan.reset(&pv->out1_panning);
				// volume
				pv->out1_volume = 0.0f;
				pv->out1_vol.reset(&pv->out1_volume);
				// sustain
				pv->sustain = false;
				// allocated
				m_notes[key] = pv;
			}
			m_midi_in.schedule_note(key, value);
		}
		// note off
		else if (status == 0x80 || (status == 0x90 && value == 0)) {
			if (!m_key.is_note(key))
				continue;
			padthv1_voice *pv = m_notes[key];
			if (pv && pv->note >= 0) {
				if (m_ctl1.sustain)
					pv->sustain = true;
				else
				if (pv->dca1_env.stage != padthv1_env::Release) {
					m_dca1.env.note_off(&pv->dca1_env);
					m_dcf1.env.note_off(&pv->dcf1_env);
					m_lfo1.env.note_off(&pv->lfo1_env);
				}
				m_notes[pv->note] = nullptr;
				pv->note = -1;
				// mono legato?
				if (*m_def.mono > 0.0f) {
					do pv = pv->prev();	while (pv && pv->note < 0);
					if (pv && pv->note >= 0) {
						const bool legato = (*m_def.mono > 1.0f);
						m_dcf1.env.restart(&pv->dcf1_env, legato);
						m_lfo1.env.restart(&pv->lfo1_env, legato);
						m_dca1.env.restart(&pv->dca1_env, legato);
						m_notes[pv->note] = pv;
					}
				}
			}
			m_midi_in.schedule_note(key, 0);
		}
		// key pressure/poly.aftertouch
		else if (status == 0xa0) {
			if (!m_key.is_note(key))
				continue;
			padthv1_voice *pv = m_notes[key];
			if (pv && pv->note >= 0)
				pv->pre = *m_def.pressure * float(value) / 127.0f;
		}
		// control change
		else if (status == 0xb0) {
		switch (key) {
			case 0x00:
				// bank-select MSB (cc#0)
				m_programs.bank_select_msb(value);
				break;
			case 0x01:
				// modulation wheel (cc#1)
				m_ctl1.modwheel = *m_def.modwheel * float(value) / 127.0f;
				break;
			case 0x07:
				// channel volume (cc#7)
				m_ctl1.volume = float(value) / 127.0f;
				break;
			case 0x0a:
				// channel panning (cc#10)
				m_ctl1.panning = float(value - 64) / 64.0f;
				break;
			case 0x20:
				// bank-select LSB (cc#32)
				m_programs.bank_select_lsb(value);
				break;
			case 0x40:
				// sustain/damper pedal (cc#64)
				if (m_ctl1.sustain && value <  64)
					allSustainOff();
				m_ctl1.sustain = bool(value >= 64);
				break;
			case 0x78:
				// all sound off (cc#120)
				allSoundOff();
				break;
			case 0x79:
				// all controllers off (cc#121)
				allControllersOff();
				break;
			case 0x7b:
				// all notes off (cc#123)
				allNotesOff();
				break;
			}
			// process controllers...
			m_controls.process_enqueue(channel, key, value);
		}
		// pitch bend
		else if (status == 0xe0) {
			const float pitchbend = float(key + (value << 7) - 0x2000) / 8192.0f;
			m_ctl1.pitchbend = padthv1_pow2f(*m_def.pitchbend * pitchbend);
		}
	}

	// process pending controllers...
	m_controls.process_dequeue();

	// asynchronous event notification...
	m_midi_in.schedule_event();
}


// all controllers off

void padthv1_impl::allControllersOff (void)
{
	m_ctl1.reset();
}


// all sound off

void padthv1_impl::allSoundOff (void)
{
	m_chorus.setSampleRate(m_srate);
	m_chorus.reset();

	for (uint16_t k = 0; k < m_nchannels; ++k) {
		m_phaser[k].setSampleRate(m_srate);
		m_delay[k].setSampleRate(m_srate);
		m_comp[k].setSampleRate(m_srate);
		m_flanger[k].reset();
		m_phaser[k].reset();
		m_delay[k].reset();
		m_comp[k].reset();
	}

	m_reverb.setSampleRate(m_srate);
	m_reverb.reset();
}


// all notes off

void padthv1_impl::allNotesOff (void)
{
	padthv1_voice *pv = m_play_list.next();
	while (pv) {
		if (pv->note >= 0)
			m_notes[pv->note] = nullptr;
		free_voice(pv);
		pv = m_play_list.next();
	}

	gen1_last1 = gen1_last2 = 0.0f;

	m_lfo1.psync = nullptr;

	m_direct_note = 0;
}


// all sustained notes off

void padthv1_impl::allSustainOff (void)
{
	padthv1_voice *pv = m_play_list.next();
	while (pv) {
		if (pv->note >= 0 && pv->sustain) {
			pv->sustain = false;
			if (pv->dca1_env.stage != padthv1_env::Release) {
				m_dca1.env.note_off(&pv->dca1_env);
				m_dcf1.env.note_off(&pv->dcf1_env);
				m_lfo1.env.note_off(&pv->lfo1_env);
				m_notes[pv->note] = nullptr;
				pv->note = -1;
			}
		}
		pv = pv->next();
	}
}


// direct note-on triggered on next cycle...
void padthv1_impl::directNoteOn ( int note, int vel )
{
	if (vel > 0 && m_nvoices >= MAX_DIRECT_NOTES)
		return;

	const uint32_t i = m_direct_note;
	if (i < MAX_DIRECT_NOTES) {
		const int ch1 = int(*m_def.channel);
		const int chan = (ch1 > 0 ? ch1 - 1 : 0) & 0x0f;
		direct_note& data = m_direct_notes[i];
		data.status = (vel > 0 ? 0x90 : 0x80) | chan;
		data.note = note;
		data.vel = vel;
		++m_direct_note;
	}
}


// controllers accessor

padthv1_controls *padthv1_impl::controls (void)
{
	return &m_controls;
}


// programs accessor

padthv1_programs *padthv1_impl::programs (void)
{
	return &m_programs;
}


// Micro-tuning support

void padthv1_impl::setTuningEnabled ( bool enabled )
{
	m_tun.enabled = enabled;
}

bool padthv1_impl::isTuningEnabled (void) const
{
	return m_tun.enabled;
}


void padthv1_impl::setTuningRefPitch ( float refPitch )
{
	m_tun.refPitch = refPitch;
}

float padthv1_impl::tuningRefPitch (void) const
{
	return m_tun.refPitch;
}


void padthv1_impl::setTuningRefNote ( int refNote )
{
	m_tun.refNote = refNote;
}

int padthv1_impl::tuningRefNote (void) const
{
	return m_tun.refNote;
}


void padthv1_impl::setTuningScaleFile ( const char *pszScaleFile )
{
	m_tun.scaleFile = QString::fromUtf8(pszScaleFile);
}

const char *padthv1_impl::tuningScaleFile (void) const
{
	return m_tun.scaleFile.toUtf8().constData();
}


void padthv1_impl::setTuningKeyMapFile ( const char *pszKeyMapFile )
{
	m_tun.keyMapFile = QString::fromUtf8(pszKeyMapFile);
}

const char *padthv1_impl::tuningKeyMapFile (void) const
{
	return m_tun.keyMapFile.toUtf8().constData();
}


void padthv1_impl::resetTuning (void)
{
	if (m_tun.enabled) {
		// Instance micro-tuning, possibly from Scala keymap and scale files...
		padthv1_tuning tuning(
			m_tun.refPitch,
			m_tun.refNote);
		if (m_tun.keyMapFile.isEmpty())
		if (!m_tun.keyMapFile.isEmpty())
			tuning.loadKeyMapFile(m_tun.keyMapFile);
		if (!m_tun.scaleFile.isEmpty())
			tuning.loadScaleFile(m_tun.scaleFile);
		for (int note = 0; note < MAX_NOTES; ++note)
			m_freqs[note] = tuning.noteToPitch(note);
		// Done instance tuning.
	}
	else
	if (m_config.bTuningEnabled) {
		// Global/config micro-tuning, possibly from Scala keymap and scale files...
		padthv1_tuning tuning(
			m_config.fTuningRefPitch,
			m_config.iTuningRefNote);
		if (!m_config.sTuningKeyMapFile.isEmpty())
			tuning.loadKeyMapFile(m_config.sTuningKeyMapFile);
		if (!m_config.sTuningScaleFile.isEmpty())
			tuning.loadScaleFile(m_config.sTuningScaleFile);
		for (int note = 0; note < MAX_NOTES; ++note)
			m_freqs[note] = tuning.noteToPitch(note);
		// Done global/config tuning.
	} else {
		// Native/default tuning, 12-tone equal temperament western standard...
		for (int note = 0; note < MAX_NOTES; ++note)
			m_freqs[note] = padthv1_freq(note);
		// Done native/default tuning.
	}
}


// all stabilize

void padthv1_impl::stabilize (void)
{
	for (int i = 0; i < padthv1::NUM_PARAMS; ++i) {
		padthv1_port *pParamPort = paramPort(padthv1::ParamIndex(i));
		if (pParamPort)
			pParamPort->tick(padthv1_port2::NSTEP);
	}
}


// all reset clear

void padthv1_impl::reset (void)
{
	m_vol1.reset(
		m_out1.volume.value_ptr(),
		m_dca1.volume.value_ptr(),
		&m_ctl1.volume);
	m_pan1.reset(
		m_out1.panning.value_ptr(),
		&m_ctl1.panning);
	m_wid1.reset(
		m_out1.width.value_ptr());

	// flangers
	if (m_flanger == nullptr)
		m_flanger = new padthv1_fx_flanger [m_nchannels];

	// phasers
	if (m_phaser == nullptr)
		m_phaser = new padthv1_fx_phaser [m_nchannels];

	// delays
	if (m_delay == nullptr)
		m_delay = new padthv1_fx_delay [m_nchannels];

	// compressors
	if (m_comp == nullptr)
		m_comp = new padthv1_fx_comp [m_nchannels];

	// reverbs
	m_reverb.reset();

	// controllers reset.
	m_controls.reset();

	allSoundOff();
//	allControllersOff();
	allNotesOff();
}


// MIDI input asynchronous status notification accessors

void padthv1_impl::midiInEnabled ( bool on )
{
	m_midi_in.enabled(on);
}


uint32_t padthv1_impl::midiInCount (void)
{
	return m_midi_in.count();
}

 
// synthesize

void padthv1_impl::process ( float **ins, float **outs, uint32_t nframes )
{
	if (!m_running) return;

	float *v_outs[m_nchannels];
	float *v_sfxs[m_nchannels];

	// FIXME: fx-send buffer reallocation... seriously?
	if (m_nsize < nframes) alloc_sfxs(nframes);

	uint16_t k;

	for (k = 0; k < m_nchannels; ++k) {
		::memset(m_sfxs[k], 0, nframes * sizeof(float));
		::memcpy(outs[k], ins[k], nframes * sizeof(float));
	}

	// process direct note on/off...
	while (m_direct_note > 0) {
		const direct_note& data
			= m_direct_notes[--m_direct_note];
		process_midi((uint8_t *) &data, sizeof(data));
	}

	// controls

	const bool lfo1_enabled = (*m_lfo1.enabled > 0.0f);
	
	const float lfo1_freq = (lfo1_enabled
		? get_bpm(*m_lfo1.bpm) / (60.01f - *m_lfo1.rate * 60.0f) : 0.0f);

	const float modwheel1 = (lfo1_enabled
		? m_ctl1.modwheel + PITCH_SCALE * *m_lfo1.pitch : 0.0f);

	const bool dcf1_enabled = (*m_dcf1.enabled > 0.0f);
	
	const float fxsend1 = *m_out1.fxsend * *m_out1.fxsend;

	if (m_gen1.sample1_0 != *m_gen1.sample1) {
		m_gen1.sample1_0  = *m_gen1.sample1;
		m_gen1.freq1 = padthv1_freq(m_gen1.sample1_0);
	}

	if (m_gen1.sample2_0 != *m_gen1.sample2) {
		m_gen1.sample2_0  = *m_gen1.sample2;
		m_gen1.freq2 = padthv1_freq(m_gen1.sample2_0);
	}

	gen1_sample1.reset_test(m_gen1.freq1,
		*m_gen1.width1, *m_gen1.scale1, uint16_t(*m_gen1.nh1),
		padthv1_sample::Apodizer(*m_gen1.apod1));
	gen1_sample2.reset_test(m_gen1.freq2,
		*m_gen1.width2, *m_gen1.scale2, uint16_t(*m_gen1.nh2),
		padthv1_sample::Apodizer(*m_gen1.apod2));

	if (m_gen1.envtime0 != *m_gen1.envtime) {
		m_gen1.envtime0  = *m_gen1.envtime;
		updateEnvTimes();
	}

	if (lfo1_enabled) {
		lfo1_wave.reset_test(
			padthv1_wave::Shape(*m_lfo1.shape), *m_lfo1.width);
	}

	// per voice

	padthv1_voice *pv = m_play_list.next();

	while (pv) {

		padthv1_voice *pv_next = pv->next();

		// output buffers

		for (k = 0; k < m_nchannels; ++k) {
			v_outs[k] = outs[k];
			v_sfxs[k] = m_sfxs[k];
		}

		uint32_t nblock = nframes;

		while (nblock > 0) {

			uint32_t ngen = nblock;

			// process envelope stages

			if (pv->dca1_env.running && pv->dca1_env.frames < ngen)
				ngen = pv->dca1_env.frames;
			if (pv->dcf1_env.running && pv->dcf1_env.frames < ngen)
				ngen = pv->dcf1_env.frames;
			if (pv->lfo1_env.running && pv->lfo1_env.frames < ngen)
				ngen = pv->lfo1_env.frames;

			for (uint32_t j = 0; j < ngen; ++j) {

				// velocities

				const float vel1
					= (pv->vel + (1.0f - pv->vel) * pv->dca1_pre.value(j));

				// generators

				const float lfo1_env
					= (lfo1_enabled ? pv->lfo1_env.tick() : 0.0f);
				const float lfo1
					= (lfo1_enabled ? pv->lfo1_sample * lfo1_env : 0.0f);

				const float gen1 = pv->gen1_sample1 * pv->gen1_bal.value(j, 0);
				const float gen2 = pv->gen1_sample2 * pv->gen1_bal.value(j, 1);

				pv->gen1_sample1 = pv->gen1_osc1.sample(pv->gen1_freq1
					* (m_ctl1.pitchbend + modwheel1 * lfo1)
					+ pv->gen1_glide1.tick());
				pv->gen1_sample2 = pv->gen1_osc2.sample(pv->gen1_freq2
					* (m_ctl1.pitchbend + modwheel1 * lfo1)
					+ pv->gen1_glide2.tick());

				if (lfo1_enabled) {
					pv->lfo1_sample = pv->lfo1.sample(lfo1_freq
						* (1.0f + SWEEP_SCALE * *m_lfo1.sweep * lfo1_env));
				}

				// ring modulators

				const float ringmod1 = padthv1_sigmoid_1(
					*m_gen1.ringmod * (1.0f + *m_lfo1.ringmod * lfo1));

				float mod1 = gen1 * (1.0f - ringmod1) + gen1 * gen2 * ringmod1;
				float mod2 = gen2 * (1.0f - ringmod1) + gen2 * gen1 * ringmod1;

				// filters

				if (dcf1_enabled) {
					const float env1 = 0.5f
						* (1.0f + *m_dcf1.envelope * pv->dcf1_env.tick());
					const float cutoff1 = padthv1_sigmoid_1(*m_dcf1.cutoff
						* env1 * (1.0f + *m_lfo1.cutoff * lfo1));
					const float reso1 = padthv1_sigmoid_1(*m_dcf1.reso
						* env1 * (1.0f + *m_lfo1.reso * lfo1));
					switch (int(*m_dcf1.slope)) {
					case 3: // Formant
						mod1 = pv->dcf17.output(mod1, cutoff1, reso1);
						mod2 = pv->dcf18.output(mod2, cutoff1, reso1);
						break;
					case 2: // Biquad
						mod1 = pv->dcf15.output(mod1, cutoff1, reso1);
						mod2 = pv->dcf16.output(mod2, cutoff1, reso1);
						break;
					case 1: // 24db/octave
						mod1 = pv->dcf13.output(mod1, cutoff1, reso1);
						mod2 = pv->dcf14.output(mod2, cutoff1, reso1);
						break;
					case 0: // 12db/octave
					default:
						mod1 = pv->dcf11.output(mod1, cutoff1, reso1);
						mod2 = pv->dcf12.output(mod2, cutoff1, reso1);
						break;
					}
				}

				// volumes

				const float wid1 = m_wid1.value(j);
				const float mid1 = 0.5f * (mod1 + mod2);
				const float sid1 = 0.5f * (mod1 - mod2);
				const float vol1 = vel1 * m_vol1.value(j)
					* pv->dca1_env.tick()
					* pv->out1_vol.value(j);

				// outputs

				const float out1 = vol1 * (mid1 + sid1 * wid1)
					* pv->out1_pan.value(j, 0)
					* m_pan1.value(j, 0);
				const float out2 = vol1 * (mid1 - sid1 * wid1)
					* pv->out1_pan.value(j, 1)
					* m_pan1.value(j, 1);

				for (k = 0; k < m_nchannels; ++k) {
					const float dry = (k & 1 ? out2 : out1);
					const float wet = fxsend1 * dry;
					*v_outs[k]++ += dry - wet;
					*v_sfxs[k]++ += wet;
				}

				if (j == 0) {
					pv->gen1_balance = lfo1 * *m_lfo1.balance;
					pv->out1_panning = lfo1 * *m_lfo1.panning;
					pv->out1_volume  = lfo1 * *m_lfo1.volume + 1.0f;
				}
			}

			nblock -= ngen;

			// voice ramps countdown

			pv->gen1_bal.process(ngen);
			pv->dca1_pre.process(ngen);
			pv->out1_pan.process(ngen);
			pv->out1_vol.process(ngen);

			// envelope countdowns

			if (pv->dca1_env.running && pv->dca1_env.frames == 0)
				m_dca1.env.next(&pv->dca1_env);

			if (pv->dca1_env.stage == padthv1_env::End) {
				if (pv->note < 0)
					free_voice(pv);
				nblock = 0;
			} else {
				if (pv->dcf1_env.running && pv->dcf1_env.frames == 0)
					m_dcf1.env.next(&pv->dcf1_env);
				if (pv->lfo1_env.running && pv->lfo1_env.frames == 0)
					m_lfo1.env.next(&pv->lfo1_env);
			}
		}

		// next playing voice

		pv = pv_next;
	}

	// chorus
	if (m_nchannels > 1) {
		m_chorus.process(m_sfxs[0], m_sfxs[1], nframes, *m_cho.wet,
			*m_cho.delay, *m_cho.feedb, *m_cho.rate, *m_cho.mod);
	}

	// effects
	for (k = 0; k < m_nchannels; ++k) {
		float *in = m_sfxs[k];
		// flanger
		m_flanger[k].process(in, nframes, *m_fla.wet,
			*m_fla.delay, *m_fla.feedb, *m_fla.daft * float(k));
		// phaser
		m_phaser[k].process(in, nframes, *m_pha.wet,
			*m_pha.rate, *m_pha.feedb, *m_pha.depth, *m_pha.daft * float(k));
		// delay
		m_delay[k].process(in, nframes, *m_del.wet,
			*m_del.delay, *m_del.feedb, get_bpm(*m_del.bpm));
	}

	// reverb
	if (m_nchannels > 1) {
		m_reverb.process(m_sfxs[0], m_sfxs[1], nframes, *m_rev.wet,
			*m_rev.feedb, *m_rev.room, *m_rev.damp, *m_rev.width);
	}

	// output mix-down
	for (k = 0; k < m_nchannels; ++k) {
		uint32_t n;
		float *sfx = m_sfxs[k];
		// compressor
		if (int(*m_dyn.compress) > 0)
			m_comp[k].process(sfx, nframes);
		// limiter
		if (int(*m_dyn.limiter) > 0) {
			float *p = sfx;
			float *q = sfx;
			for (n = 0; n < nframes; ++n)
				*q++ = padthv1_sigmoid(*p++);
		}
		// mix-down
		float *out = outs[k];
		for (n = 0; n < nframes; ++n)
			*out++ += *sfx++;
	}

	// post-processing
	m_dca1.volume.tick(nframes);
	m_out1.width.tick(nframes);
	m_out1.panning.tick(nframes);
	m_out1.volume.tick(nframes);

	m_wid1.process(nframes);
	m_pan1.process(nframes);
	m_vol1.process(nframes);

	m_controls.process(nframes);
}


// process running state...
bool padthv1_impl::running ( bool on )
{
	const bool running = m_running;
	m_running = on;
	return running;
}


//-------------------------------------------------------------------------
// padthv1 - decl.
//

padthv1::padthv1 ( uint16_t nchannels, float srate )
{
	m_pImpl = new padthv1_impl(this, nchannels, srate);
}


padthv1::~padthv1 (void)
{
	delete m_pImpl;
}


void padthv1::setChannels ( uint16_t nchannels )
{
	m_pImpl->setChannels(nchannels);
}


uint16_t padthv1::channels (void) const
{
	return m_pImpl->channels();
}


void padthv1::setSampleRate ( float srate )
{
	m_pImpl->setSampleRate(srate);
}


float padthv1::sampleRate (void) const
{
	return m_pImpl->sampleRate();
}


padthv1_sample *padthv1::sample ( int sid ) const
{
	if (sid == 1)
		return &(m_pImpl->gen1_sample1);
	else
	if (sid == 2)
		return &(m_pImpl->gen1_sample2);
	else
		return nullptr;
}


void padthv1::setBufferSize ( uint32_t nsize )
{
	m_pImpl->setBufferSize(nsize);
}


uint32_t padthv1::bufferSize (void) const
{
	return m_pImpl->bufferSize();
}


void padthv1::setTempo ( float bpm )
{
	m_pImpl->setTempo(bpm);
}


float padthv1::tempo (void) const
{
	return m_pImpl->tempo();
}


void padthv1::setParamPort ( ParamIndex index, float *pfParam )
{
	m_pImpl->setParamPort(index, pfParam);
}

padthv1_port *padthv1::paramPort ( ParamIndex index ) const
{
	return m_pImpl->paramPort(index);
}


void padthv1::setParamValue ( ParamIndex index, float fValue )
{
	m_pImpl->setParamValue(index, fValue);
}

float padthv1::paramValue ( ParamIndex index ) const
{
	return m_pImpl->paramValue(index);
}


void padthv1::process_midi ( uint8_t *data, uint32_t size )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "padthv1[%p]::process_midi(%u)", this, size);
	for (uint32_t i = 0; i < size; ++i)
		fprintf(stderr, " %02x", data[i]);
	fprintf(stderr, "\n");
#endif

	m_pImpl->process_midi(data, size);
}


void padthv1::process ( float **ins, float **outs, uint32_t nframes )
{
	m_pImpl->process(ins, outs, nframes);
}


// controllers accessor

padthv1_controls *padthv1::controls (void) const
{
	return m_pImpl->controls();
}


// programs accessor

padthv1_programs *padthv1::programs (void) const
{
	return m_pImpl->programs();
}


// process state

bool padthv1::running ( bool on )
{
	return m_pImpl->running(on);
}


// all stabilize

void padthv1::stabilize (void)
{
	m_pImpl->stabilize();
}


// all reset clear

void padthv1::reset (void)
{
	m_pImpl->reset();
}


// MIDI input asynchronous status notification accessors

void padthv1::midiInEnabled ( bool on )
{
	m_pImpl->midiInEnabled(on);
}


uint32_t padthv1::midiInCount (void)
{
	return m_pImpl->midiInCount();
}


// MIDI direct note on/off triggering

void padthv1::directNoteOn ( int note, int vel )
{
	m_pImpl->directNoteOn(note, vel);
}


// Micro-tuning support
void padthv1::setTuningEnabled ( bool enabled )
{
	m_pImpl->setTuningEnabled(enabled);
}

bool padthv1::isTuningEnabled (void) const
{
	return m_pImpl->isTuningEnabled();
}


void padthv1::setTuningRefPitch ( float refPitch )
{
	m_pImpl->setTuningRefPitch(refPitch);
}

float padthv1::tuningRefPitch (void) const
{
	return m_pImpl->tuningRefPitch();
}


void padthv1::setTuningRefNote ( int refNote )
{
	m_pImpl->setTuningRefNote(refNote);
}

int padthv1::tuningRefNote (void) const
{
	return m_pImpl->tuningRefNote();
}


void padthv1::setTuningScaleFile ( const char *pszScaleFile )
{
	m_pImpl->setTuningScaleFile(pszScaleFile);
}

const char *padthv1::tuningScaleFile (void) const
{
	return m_pImpl->tuningScaleFile();
}


void padthv1::setTuningKeyMapFile ( const char *pszKeyMapFile )
{
	m_pImpl->setTuningKeyMapFile(pszKeyMapFile);
}

const char *padthv1::tuningKeyMapFile (void) const
{
	return m_pImpl->tuningKeyMapFile();
}


void padthv1::resetTuning (void)
{
	m_pImpl->resetTuning();
}


// end of padthv1.cpp
