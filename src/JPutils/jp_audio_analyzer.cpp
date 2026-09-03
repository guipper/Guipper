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
		// How far the percentile window is allowed to close, as a fraction of the
	// band's own peak. See normalize().
	constexpr float MinNormalizeSpan = 0.25f;
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
			// Last hop's detection evidence, kept for the debug screen: without
			// flux plotted against its own moving threshold there is no way to
			// answer "why did my kick not trigger".
			float lastFlux = 0.0f, lastMean = 0.0f;
			bool lastMaterialGate = true, lastBlockedByRefractory = false;
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
		// DC blocker state. DSP memory, so it belongs here and reset() wipes it.
		float dcPrevIn = 0.0f, dcPrevOut = 0.0f;
		// The shaping stage's own state. This is DSP memory, NOT configuration:
		// it belongs in Impl precisely so reset() wipes it. Leaving a smoothed
		// value alive across a device change would keep feeding the old level to
		// every shader while the new device delivers nothing.
		std::array<float, TunedSources> shaped{};
		std::array<float, TunedSources> shapedPre{};
		std::array<std::array<float, HistoryLength>, TunedSources> history{};
		int historyAt = 0;
		int historyFilled = 0;
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
		// No save/restore here. `*impl_ = Impl()` assigns through the pointer and
		// cannot touch an AudioAnalyzer member, so autoGain_, noiseGate_ and the
		// tuning arrays survive on their own - the old savedAutoGain dance was a
		// no-op. What DOES need doing is the other direction: applyTuning() at
		// the end pushes the members back into the freshly built Impl, which is
		// also why the snare's sensitivity is no longer hardcoded here.
		*impl_ = Impl();
		impl_->sampleRate = std::max(8000, sampleRate);
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
		snapshot_ = AnalyzerSnapshot();
		diagnostics_ = AnalyzerDiagnostics();
		applyTuning();
	}

	void AudioAnalyzer::applyTuning()
	{
		impl_->kick.sensitivity = onsetSensitivity_[ONSET_KICK];
		impl_->kick.refractory = onsetRefractorySec_[ONSET_KICK];
		impl_->snare.sensitivity = onsetSensitivity_[ONSET_SNARE];
		impl_->snare.refractory = onsetRefractorySec_[ONSET_SNARE];
	}

	void AudioAnalyzer::setTuning(int index, const SourceTuning &tuning)
	{
		if (index < 0 || index >= TunedSources) return;
		SourceTuning &slot = tuning_[index];
		slot.threshold = clampValue(tuning.threshold, 0.0f, 0.95f);
		slot.gain = clampValue(tuning.gain, 0.0f, 4.0f);
		slot.add = clampValue(tuning.add, -1.0f, 1.0f);
		slot.smoothMs = clampValue(tuning.smoothMs, 0.0f, 1000.0f);
	}

	const SourceTuning &AudioAnalyzer::tuning(int index) const
	{
		static const SourceTuning identity;
		if (index < 0 || index >= TunedSources) return identity;
		return tuning_[index];
	}

	void AudioAnalyzer::setOnsetSensitivity(int onset, float sensitivity)
	{
		if (onset < 0 || onset >= Onsets) return;
		// Below 1.0 the detector compares flux against a fraction of its own
		// running mean and fires on essentially everything.
		onsetSensitivity_[onset] = clampValue(sensitivity, 1.0f, 4.0f);
		applyTuning();
	}

	float AudioAnalyzer::onsetSensitivity(int onset) const
	{
		return onset >= 0 && onset < Onsets ? onsetSensitivity_[onset] : 1.6f;
	}

	void AudioAnalyzer::setOnsetRefractory(int onset, float seconds)
	{
		if (onset < 0 || onset >= Onsets) return;
		onsetRefractorySec_[onset] = clampValue(seconds, 0.030f, 0.500f);
		applyTuning();
	}

	float AudioAnalyzer::onsetRefractory(int onset) const
	{
		return onset >= 0 && onset < Onsets ? onsetRefractorySec_[onset] : 0.110f;
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
		// DC blocker, ahead of everything.
		//
		// A sound card with a standing offset is common and completely
		// inaudible, but it is not harmless here: it lands in FFT bin 0, and the
		// Hann window spreads it across bins 1 and 2 as well - which ARE real
		// bass. Excluding bin 0 alone does not remove it, so the offset used to
		// read as a permanent floor under the Low band and, worse, as a level
		// the noise gate had to clear.
		//
		// One-pole high pass at about 4 Hz, far below the 20 Hz the lowest band
		// starts at, so nothing musical is touched.
		Impl &s = *impl_;
		for (std::size_t i = 0; i < count; ++i)
		{
			const float in = samples[i];
			const float out = in - s.dcPrevIn + 0.9995f * s.dcPrevOut;
			s.dcPrevIn = in;
			s.dcPrevOut = out;
			impl_->input[impl_->fill++] = out;
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
		// Band energy, averaged in LOG frequency rather than linear.
		//
		// Two things were wrong with the plain mean this replaces.
		//
		// It divided by the bin count, and the bands are wildly uneven: Low is
		// 5 bins wide, Mid 37, High 299. Above roughly 8 kHz real music carries
		// almost nothing, but those ~150 near-empty bins counted for as much in
		// High's divisor as the ones that matter - so High sat permanently
		// squashed against its floor while Low did not. Averaging equal-RATIO
		// slices instead gives every octave the same say, which is also how
		// hearing works.
		//
		// And it started at bin 0, which is DC. That is now removed upstream by
		// the high pass in process(); excluding the bin here is belt and braces,
		// since a meaningless bin has no business in a band average.
		auto energy = [&](float lowHz, float highHz) {
			constexpr int Slices = 8;
			const float ratio =
				std::pow(highHz / std::max(1.0f, lowHz), 1.0f / float(Slices));
			float total = 0.0f;
			int counted = 0;
			float edge = lowHz;
			for (int slice = 0; slice < Slices; ++slice)
			{
				const float next = edge * ratio;
				const int low = std::max(1, binForHz(edge));
				const int high = std::max(low + 1, binForHz(next));
				float sum = 0.0f;
				int bins = 0;
				for (int i = low; i < high && i < int(s.magnitude.size()); ++i)
				{
					sum += s.magnitude[i];
					++bins;
				}
				if (bins > 0) { total += sum / float(bins); ++counted; }
				edge = next;
			}
			return std::log10(1.0f + 40.0f *
				(counted > 0 ? total / float(counted) : 0.0f));
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
			// Hold the window open.
			//
			// The 10th percentile of a CONSTANT signal is that signal, so a
			// drone or a held pad walks the floor up to meet the peak: measured
			// on a sustained 60 Hz tone the span fell from 0.59 to 0.12 in five
			// seconds and kept going. Once it reaches zero the band is no longer
			// a level, it is a comparator - the faintest ripple reads as full
			// scale, which is the "everything is pinned at 1.0" complaint.
			//
			// Clamping the span to a fraction of the peak and anchoring it under
			// the peak means sustained material reads as loud and steady, which
			// is what it is. It is a no-op whenever the window is genuinely
			// open: with peak - floor above the fraction, floorAt IS floor and
			// the arithmetic is unchanged.
			const float span = std::max(band.peak - band.floor,
				band.peak * MinNormalizeSpan);
			const float floorAt = band.peak - span;
			return band.norm = clampValue((value - floorAt) /
				std::max(0.0001f, span), 0.0f, 1.0f);
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

		// ---- the global tuning stage, step 1 of 3 --------------------------
		// Threshold, Gain and Add, WITHOUT the Smooth one-pole. Split out
		// because the onset block below samples a band value into express[] and
		// has to see the knobs applied but NOT the smoothing: express exists to
		// capture the level AT the transient, and a long Smooth would poison
		// exactly that.
		auto affine = [&](int index, float value) {
			const SourceTuning &t = tuning_[index];
			// The divisor floor matters: threshold arrives from a hand-editable
			// settings file, and at 1.0 this would divide by zero.
			float v = clampValue((value - t.threshold) /
				std::max(0.10f, 1.0f - t.threshold), 0.0f, 1.0f);
			v = v * t.gain + t.add;
			return clampValue(v, 0.0f, 1.0f);
		};
		std::array<float, TunedSources> pre{};
		pre[TUNED_LOW] = affine(TUNED_LOW, s.low.lagged);
		pre[TUNED_MID] = affine(TUNED_MID, s.mid.lagged);
		pre[TUNED_HIGH] = affine(TUNED_HIGH, s.high.lagged);
		pre[TUNED_LEVEL] = affine(TUNED_LEVEL, s.level.lagged);

		auto onset = [&](Impl::Onset &onsetState, float lowHz, float highHz,
			float envelope, bool extraGate) {
			const int low = binForHz(lowHz), high = std::max(low + 1, binForHz(highHz));
			float flux = 0.0f;
			for (int i = low; i < high && i < int(s.magnitude.size()); ++i)
				flux += std::max(0.0f, s.magnitude[i] - s.previousMagnitude[i]);
			float mean = 0.0f;
			for (float value : onsetState.flux) mean += value;
			mean /= onsetState.flux.size();
			const bool refractoryBlocked =
				(s.clock - onsetState.last) <= onsetState.refractory;
			onsetState.lastFlux = flux;
			onsetState.lastMean = mean;
			onsetState.lastMaterialGate = extraGate;
			onsetState.lastBlockedByRefractory = refractoryBlocked;
			const bool fired = flux > mean * onsetState.sensitivity + 0.00001f &&
				!refractoryBlocked && extraGate &&
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
		// ---- step 2 of 3: onsets ------------------------------------------
		// The `envelope` argument is the SHAPED, un-smoothed band value, so
		// SRC_KICK_EXPRESS honours the row's Threshold/Gain/Add. Detection
		// itself is deliberately NOT affected: the gates below read .norm and
		// level.raw, both upstream of the tuning stage, so turning Low's gain
		// down can never silently stop the kick from firing.
		const bool kickFired = onset(s.kick, 35.0f, 140.0f, pre[TUNED_LOW], true);
		onset(s.snare, 1500.0f, 6000.0f, pre[TUNED_HIGH],
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

		// ---- step 3 of 3: the derived rows, then Smooth --------------------
		// Low bass and High mid need kick.env / snare.env, which only exist
		// after step 2. Each row is shaped from the UNSHAPED components, so
		// turning Low's gain down does not move Low bass twice.
		pre[TUNED_LOWBASS] = affine(TUNED_LOWBASS,
			clampValue(s.low.lagged + s.kick.env, 0.0f, 1.0f));
		pre[TUNED_HIGHMID] = affine(TUNED_HIGHMID,
			clampValue(s.high.lagged + s.snare.env, 0.0f, 1.0f));
		for (int i = 0; i < TunedSources; ++i)
		{
			s.shapedPre[i] = pre[i];
			// Bypass explicitly rather than leaning on smoothToward's tau floor:
			// it clamps tau to 1 ms, which at a 5.33 ms hop still leaves 0.5% of
			// lag. With smoothMs at its default the stage has to be the exact
			// identity, or a kick envelope stepping to 1.0 in one hop would peak
			// at 0.995 and a shader testing step(0.999, ...) would stop firing.
			s.shaped[i] = tuning_[i].smoothMs <= 0.0f ? pre[i] :
				smoothToward(s.shaped[i], pre[i], tuning_[i].smoothMs, dt);
			s.history[i][s.historyAt] = s.shaped[i];
		}
		// One ring for every traced source, appended once per HOP. The UI cannot
		// do this: at 60 fps it would sample one hop in three and alias away the
		// 8 ms attack the screen exists to show.
		s.historyAt = (s.historyAt + 1) % HistoryLength;
		s.historyFilled = std::min(HistoryLength, s.historyFilled + 1);

		rebuildSnapshot();
		fillDiagnostics(rms, gated);
	}

	void AudioAnalyzer::rebuildSnapshot()
	{
		const Impl &s = *impl_;
		// The SHAPED values, so the shader uniforms, the parameters and the
		// SETTINGS meter all agree with what the AUDIO screen shows.
		snapshot_.low = s.shaped[TUNED_LOW]; snapshot_.mid = s.shaped[TUNED_MID];
		snapshot_.high = s.shaped[TUNED_HIGH];
		snapshot_.level = s.shaped[TUNED_LEVEL];
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

	void AudioAnalyzer::fillDiagnostics(float rms, bool gated)
	{
		const Impl &s = *impl_;
		const Impl::Band *bands[TunedSources] = {
			&s.low, &s.mid, &s.high, &s.low, &s.high, &s.level };
		for (int i = 0; i < TunedSources; ++i)
		{
			AnalyzerDiagnostics::BandInfo &info = diagnostics_.bands[i];
			// Low bass and High mid are sums, not bands: they borrow their
			// component's normaliser numbers, which is what you want to look at
			// when one of them misbehaves.
			info.raw = bands[i]->raw;
			info.norm = bands[i]->norm;
			info.lagged = bands[i]->lagged;
			info.floorLevel = bands[i]->floor;
			info.peakLevel = bands[i]->peak;
			info.shaped = s.shaped[i];
			info.preSmooth = s.shapedPre[i];
		}
		const Impl::Onset *onsets[Onsets] = { &s.kick, &s.snare };
		for (int i = 0; i < Onsets; ++i)
		{
			AnalyzerDiagnostics::OnsetInfo &info = diagnostics_.onsets[i];
			info.flux = onsets[i]->lastFlux;
			info.mean = onsets[i]->lastMean;
			info.threshold = onsets[i]->lastMean * onsets[i]->sensitivity;
			info.secondsSinceLast = s.clock - onsets[i]->last;
			info.refractorySec = onsets[i]->refractory;
			info.blockedByRefractory = onsets[i]->lastBlockedByRefractory;
			info.materialGate = onsets[i]->lastMaterialGate;
			info.count = onsets[i]->count;
		}
		diagnostics_.rms = rms;
		diagnostics_.gated = gated;
		for (int i = 0; i < TunedSources; ++i)
			std::copy(s.history[i].begin(), s.history[i].end(),
				diagnostics_.history[i]);
		diagnostics_.historyAt = s.historyAt;
		diagnostics_.historyFilled = s.historyFilled;
	}

	float AudioAnalyzer::sourceValue(int source, int division) const
	{
		const Impl &s = *impl_;
		const int div = clampValue(division, 0, Divisions - 1);
		switch (source)
		{
		// The six continuous sources come from the tuning stage; the two onset
		// envelopes do not - a detector is shaped by its own sensitivity and
		// refractory knobs instead.
		case 0: return s.shaped[TUNED_LOW]; case 1: return s.shaped[TUNED_MID];
		case 2: return s.shaped[TUNED_HIGH]; case 3: return s.kick.env;
		case 4: return s.snare.env;
		case 5: return s.shaped[TUNED_LOWBASS];
		case 6: return s.shaped[TUNED_HIGHMID];
		case 7: return s.shaped[TUNED_LEVEL];
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
