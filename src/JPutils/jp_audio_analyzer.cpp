#include "jp_audio_analyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace jp_audio_internal
{
	namespace
	{
		constexpr int Window = 1024;
		constexpr int Hop = 256;
		constexpr int FluxHistory = 43;
		constexpr float Pi = 3.14159265358979323846f;
		constexpr float TriggerWidth = 0.040f;
		constexpr float AttackTau = 0.008f;
		constexpr float ReleaseTau = 0.250f;

		template <typename T> T clampValue(T value, T low, T high)
		{
			return std::max(low, std::min(high, value));
		}

		int divisionFactor(int division)
		{
			static const int factors[Divisions] = {1, 2, 4, 8, 16};
			return factors[clampValue(division, 0, Divisions - 1)];
		}
	}

	struct AudioAnalyzer::Impl
	{
		struct Band
		{
			float raw = 0.0f, norm = 0.0f, lagged = 0.0f;
			float peak = 0.001f, floor = 0.0f;
			std::array<float, 128> history{};
			int at = 0, count = 0;
		};
		struct Onset
		{
			float env = 0.0f;
			std::array<float, FluxHistory> flux{};
			int fluxAt = 0;
			float last = -10.0f, sensitivity = 1.6f, refractory = 0.11f;
			unsigned long long count = 0;
			std::array<float, Divisions> triggerTime{{-10,-10,-10,-10,-10}};
			std::array<float, Divisions> express{};
			std::array<float, Divisions> logic{};
		};

		int sampleRate = 48000;
		float clock = 0.0f;
		std::array<float, Window> input{};
		int fill = 0;
		std::vector<int> bitReverse;
		std::vector<float> cosine, sine, hann, real, imag, magnitude, previousMagnitude;
		Band low, mid, high, level;
		Onset kick, snare;
		std::array<float, SpectrumBins> spectrum{};
		std::array<float, 32> kickTimes{};
		int kickTimesCount = 0, kickTimesAt = 0;
		float bpm = 0.0f, confidence = 0.0f, beatAnchor = 0.0f;
		float calibrationRemaining = 0.0f, calibrationSum = 0.0f;
		int calibrationSamples = 0;
	};

	AudioAnalyzer::AudioAnalyzer() : impl_(new Impl) { reset(48000); }
	AudioAnalyzer::~AudioAnalyzer() { delete impl_; }

	void AudioAnalyzer::reset(int sampleRate)
	{
		const bool savedAutoGain = autoGain_;
		const float savedNoiseGate = noiseGate_;
		*impl_ = Impl();
		impl_->sampleRate = std::max(8000, sampleRate);
		impl_->snare.sensitivity = 1.4f;
		impl_->snare.refractory = 0.060f;
		impl_->bitReverse.resize(Window);
		int bits = 0;
		while ((1 << bits) < Window) ++bits;
		for (int i = 0; i < Window; ++i)
		{
			int reversed = 0;
			for (int bit = 0; bit < bits; ++bit)
				if (i & (1 << bit)) reversed |= 1 << (bits - 1 - bit);
			impl_->bitReverse[i] = reversed;
		}
		impl_->cosine.resize(Window / 2);
		impl_->sine.resize(Window / 2);
		for (int i = 0; i < Window / 2; ++i)
		{
			const float angle = -2.0f * Pi * float(i) / float(Window);
			impl_->cosine[i] = std::cos(angle);
			impl_->sine[i] = std::sin(angle);
		}
		impl_->hann.resize(Window);
		for (int i = 0; i < Window; ++i)
			impl_->hann[i] = 0.5f * (1.0f - std::cos(2.0f * Pi * i / float(Window - 1)));
		impl_->real.assign(Window, 0.0f);
		impl_->imag.assign(Window, 0.0f);
		impl_->magnitude.assign(Window / 2 + 1, 0.0f);
		impl_->previousMagnitude.assign(Window / 2 + 1, 0.0f);
		autoGain_ = savedAutoGain;
		noiseGate_ = savedNoiseGate;
		snapshot_ = AnalyzerSnapshot();
	}

	void AudioAnalyzer::setNoiseGate(float value)
	{
		noiseGate_ = clampValue(value, 0.0f, 0.5f);
	}

	void AudioAnalyzer::beginCalibration()
	{
		impl_->calibrationRemaining = 3.0f;
		impl_->calibrationSum = 0.0f;
		impl_->calibrationSamples = 0;
	}

	void AudioAnalyzer::process(const float *samples, std::size_t count)
	{
		if (samples == nullptr) return;
		for (std::size_t i = 0; i < count; ++i)
		{
			impl_->input[impl_->fill++] = samples[i];
			if (impl_->fill != Window) continue;
			analyzeHop(impl_->input.data());
			std::move(impl_->input.begin() + Hop, impl_->input.end(), impl_->input.begin());
			impl_->fill = Window - Hop;
		}
	}

	void AudioAnalyzer::analyzeHop(const float *window)
	{
		Impl &s = *impl_;
		const float dt = float(Hop) / float(s.sampleRate);
		s.clock += dt;
		for (int i = 0; i < Window; ++i)
		{
			const int src = s.bitReverse[i];
			s.real[i] = window[src] * s.hann[src];
			s.imag[i] = 0.0f;
		}
		for (int stage = 2; stage <= Window; stage <<= 1)
		{
			const int half = stage / 2;
			const int step = Window / stage;
			for (int i = 0; i < Window; i += stage)
				for (int j = 0; j < half; ++j)
				{
					const int table = j * step, a = i + j, b = a + half;
					const float xr = s.real[b] * s.cosine[table] - s.imag[b] * s.sine[table];
					const float xi = s.real[b] * s.sine[table] + s.imag[b] * s.cosine[table];
					s.real[b] = s.real[a] - xr; s.imag[b] = s.imag[a] - xi;
					s.real[a] += xr; s.imag[a] += xi;
				}
		}

		float rms = 0.0f;
		for (int i = 0; i < Window; ++i) rms += window[i] * window[i];
		rms = std::sqrt(rms / float(Window));
		if (s.calibrationRemaining > 0.0f)
		{
			s.calibrationSum += rms;
			++s.calibrationSamples;
			s.calibrationRemaining = std::max(0.0f, s.calibrationRemaining - dt);
			if (s.calibrationRemaining <= 0.0f && s.calibrationSamples > 0)
				noiseGate_ = clampValue(s.calibrationSum / s.calibrationSamples * 2.5f,
					0.001f, 0.25f);
		}

		std::swap(s.magnitude, s.previousMagnitude);
		for (int i = 0; i <= Window / 2; ++i)
			s.magnitude[i] = std::sqrt(s.real[i] * s.real[i] + s.imag[i] * s.imag[i]) / Window;

		auto binForHz = [&](float hz) {
			return clampValue(int(clampValue(hz, 0.0f, s.sampleRate * 0.5f) /
				(s.sampleRate * 0.5f) * (Window / 2)), 0, Window / 2);
		};
		auto energy = [&](float lowHz, float highHz) {
			const int low = binForHz(lowHz), high = std::max(low + 1, binForHz(highHz));
			float sum = 0.0f;
			for (int i = low; i < high && i < int(s.magnitude.size()); ++i) sum += s.magnitude[i];
			return std::log10(1.0f + 40.0f * sum / std::max(1, high - low));
		};
		auto normalize = [&](Impl::Band &band, float value) {
			band.raw = value;
			if (!autoGain_) return band.norm = clampValue(value, 0.0f, 1.0f);
			band.history[band.at] = value;
			band.at = (band.at + 1) % int(band.history.size());
			band.count = std::min(int(band.history.size()), band.count + 1);
			std::array<float, 128> ordered{};
			std::copy_n(band.history.begin(), band.count, ordered.begin());
			const int lowAt = std::max(0, int(band.count * 0.10f));
			const int highAt = std::min(band.count - 1, int(band.count * 0.95f));
			std::nth_element(ordered.begin(), ordered.begin() + lowAt, ordered.begin() + band.count);
			const float floor = ordered[lowAt];
			std::nth_element(ordered.begin(), ordered.begin() + highAt, ordered.begin() + band.count);
			const float peak = std::max(value, ordered[highAt]);
			const float adapt = 1.0f - std::exp(-dt / 3.0f);
			band.peak += (peak - band.peak) * adapt;
			band.peak = std::max(band.peak, value);
			band.floor += (floor - band.floor) * adapt;
			return band.norm = clampValue((value - band.floor) /
				std::max(0.0001f, band.peak - band.floor), 0.0f, 1.0f);
		};
		auto lag = [&](Impl::Band &band) {
			const float tau = band.norm > band.lagged ? AttackTau : ReleaseTau;
			band.lagged += (band.norm - band.lagged) * (1.0f - std::exp(-dt / tau));
		};
		const bool gated = rms < noiseGate_;
		normalize(s.low, gated ? 0.0f : energy(20.0f, 250.0f));
		normalize(s.mid, gated ? 0.0f : energy(250.0f, 2000.0f));
		normalize(s.high, gated ? 0.0f : energy(2000.0f, 16000.0f));
		const float db = 20.0f * std::log10(std::max(0.000001f, rms));
		normalize(s.level, gated ? 0.0f : clampValue((db + 60.0f) / 60.0f, 0.0f, 1.0f));
		lag(s.low); lag(s.mid); lag(s.high); lag(s.level);

		auto onset = [&](Impl::Onset &onsetState, float lowHz, float highHz,
			float envelope, bool extraGate) {
			const int low = binForHz(lowHz), high = std::max(low + 1, binForHz(highHz));
			float flux = 0.0f;
			for (int i = low; i < high && i < int(s.magnitude.size()); ++i)
				flux += std::max(0.0f, s.magnitude[i] - s.previousMagnitude[i]);
			float mean = 0.0f;
			for (float value : onsetState.flux) mean += value;
			mean /= onsetState.flux.size();
			const bool fired = flux > mean * onsetState.sensitivity + 0.00001f &&
				(s.clock - onsetState.last) > onsetState.refractory && extraGate &&
				s.level.raw >= noiseGate_;
			if (fired)
			{
				onsetState.last = s.clock; onsetState.env = 1.0f; ++onsetState.count;
				for (int div = 0; div < Divisions; ++div)
					if (onsetState.count % divisionFactor(div) == 0)
					{
						onsetState.triggerTime[div] = s.clock;
						onsetState.express[div] = envelope;
						onsetState.logic[div] = onsetState.logic[div] > 0.5f ? 0.0f : 1.0f;
					}
			}
			onsetState.flux[onsetState.fluxAt] = flux;
			onsetState.fluxAt = (onsetState.fluxAt + 1) % FluxHistory;
			onsetState.env += (0.0f - onsetState.env) * (1.0f - std::exp(-dt / ReleaseTau));
			return fired;
		};
		const bool kickFired = onset(s.kick, 35.0f, 140.0f, s.low.lagged, true);
		onset(s.snare, 1500.0f, 6000.0f, s.high.lagged,
			s.high.norm > s.low.norm * 0.6f);

		if (kickFired)
		{
			s.kickTimes[s.kickTimesAt] = s.clock;
			s.kickTimesAt = (s.kickTimesAt + 1) % int(s.kickTimes.size());
			s.kickTimesCount = std::min(s.kickTimesCount + 1, int(s.kickTimes.size()));
			std::vector<float> bpms;
			for (int age = 0; age + 1 < s.kickTimesCount; ++age)
			{
				const int newer = (s.kickTimesAt - 1 - age + int(s.kickTimes.size())) % int(s.kickTimes.size());
				const int older = (newer - 1 + int(s.kickTimes.size())) % int(s.kickTimes.size());
				const float interval = s.kickTimes[newer] - s.kickTimes[older];
				if (interval <= 0.0f || interval > 2.0f) continue;
				float bpm = 60.0f / interval;
				while (bpm < 70.0f) bpm *= 2.0f;
				while (bpm > 180.0f) bpm *= 0.5f;
				bpms.push_back(bpm);
			}
			if (bpms.size() >= 3)
			{
				std::sort(bpms.begin(), bpms.end());
				const float median = bpms[bpms.size() / 2];
				float deviation = 0.0f;
				for (float bpm : bpms) deviation += std::abs(bpm - median);
				deviation /= bpms.size();
				s.confidence = clampValue(1.0f - deviation / 18.0f, 0.0f, 1.0f) *
					clampValue(float(bpms.size()) / 8.0f, 0.0f, 1.0f);
				s.bpm = s.bpm <= 0.0f ? median : s.bpm + (median - s.bpm) * 0.18f;
				if (s.confidence >= 0.45f) s.beatAnchor = s.clock;
			}
		}
		if (s.clock - s.kick.last > 3.0f)
			s.confidence = std::max(0.0f, s.confidence - dt * 0.35f);

		for (int bin = 0; bin < SpectrumBins; ++bin)
		{
			const float low = 20.0f * std::pow(900.0f, float(bin) / SpectrumBins);
			const float high = 20.0f * std::pow(900.0f, float(bin + 1) / SpectrumBins);
			s.spectrum[bin] = clampValue(energy(low, high) * 2.0f, 0.0f, 1.0f);
		}
		rebuildSnapshot();
	}

	void AudioAnalyzer::rebuildSnapshot()
	{
		const Impl &s = *impl_;
		snapshot_.low = s.low.lagged; snapshot_.mid = s.mid.lagged;
		snapshot_.high = s.high.lagged; snapshot_.level = s.level.lagged;
		snapshot_.kick = s.kick.env; snapshot_.snare = s.snare.env;
		snapshot_.kickTrigger = s.clock - s.kick.last < TriggerWidth ? 1.0f : 0.0f;
		snapshot_.snareTrigger = s.clock - s.snare.last < TriggerWidth ? 1.0f : 0.0f;
		snapshot_.kickLogic = s.kick.logic[0]; snapshot_.snareLogic = s.snare.logic[0];
		snapshot_.detectedBpm = s.bpm; snapshot_.tempoConfidence = s.confidence;
		if (s.confidence >= 0.45f && s.bpm > 0.0f)
		{
			const float period = 60.0f / s.bpm;
			snapshot_.beatPhase = std::fmod(std::max(0.0f, s.clock - s.beatAnchor), period) / period;
			snapshot_.beatPulse = snapshot_.beatPhase < TriggerWidth / period ? 1.0f : 0.0f;
		}
		else snapshot_.beatPhase = snapshot_.beatPulse = 0.0f;
		snapshot_.calibrating = s.calibrationRemaining > 0.0f;
		snapshot_.calibrationProgress = snapshot_.calibrating ?
			clampValue(1.0f - s.calibrationRemaining / 3.0f, 0.0f, 1.0f) : 1.0f;
		snapshot_.spectrum = s.spectrum;
	}

	float AudioAnalyzer::sourceValue(int source, int division) const
	{
		const Impl &s = *impl_;
		const int div = clampValue(division, 0, Divisions - 1);
		switch (source)
		{
		case 0: return s.low.lagged; case 1: return s.mid.lagged;
		case 2: return s.high.lagged; case 3: return s.kick.env;
		case 4: return s.snare.env;
		case 5: return clampValue(s.low.lagged + s.kick.env, 0.0f, 1.0f);
		case 6: return clampValue(s.high.lagged + s.snare.env, 0.0f, 1.0f);
		case 7: return s.level.lagged;
		case 8: return s.clock - s.kick.triggerTime[div] < TriggerWidth ? 1.0f : 0.0f;
		case 9: return s.kick.express[div]; case 10: return s.kick.logic[div];
		case 11: return s.clock - s.snare.triggerTime[div] < TriggerWidth ? 1.0f : 0.0f;
		case 12: return s.snare.express[div]; case 13: return s.snare.logic[div];
		default: return 0.0f;
		}
	}

	float AudioAnalyzer::secondsSinceKick() const { return impl_->clock - impl_->kick.last; }
	float AudioAnalyzer::secondsSinceSnare() const { return impl_->clock - impl_->snare.last; }

	float smoothToward(float current, float target, float milliseconds, float deltaSeconds)
	{
		const float tau = std::max(0.001f, milliseconds * 0.001f);
		const float dt = clampValue(deltaSeconds, 0.0f, 0.1f);
		return current + (target - current) * (1.0f - std::exp(-dt / tau));
	}

	float downmixFrame(const float *channels, std::size_t channelCount,
		int channelMode, float gain)
	{
		if (channels == nullptr || channelCount == 0) return 0.0f;
		float value = 0.0f;
		if (channelMode == 1 || channelCount == 1) value = channels[0];
		else if (channelMode == 2) value = channels[std::min<std::size_t>(1, channelCount - 1)];
		else
		{
			for (std::size_t channel = 0; channel < channelCount; ++channel)
				value += channels[channel];
			value /= channelCount;
		}
		return value * gain;
	}
}
