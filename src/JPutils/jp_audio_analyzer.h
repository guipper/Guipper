#pragma once

#include <array>
#include <cstddef>

namespace jp_audio_internal
{
	constexpr int SpectrumBins = 16;
	constexpr int Divisions = 5;
	// The six continuous sources, in the order the tuning arrays use.
	// Index-stable: the panel and the settings file both key off it.
	constexpr int TunedSources = 6;
	enum TunedSource { TUNED_LOW = 0, TUNED_MID, TUNED_HIGH,
		TUNED_LOWBASS, TUNED_HIGHMID, TUNED_LEVEL };
	constexpr int Onsets = 2;
	enum OnsetIndex { ONSET_KICK = 0, ONSET_SNARE };
	// Samples of history kept per traced value, appended once per HOP (187.5 Hz
	// at 48 kHz), not once per frame - a 60 fps UI would see one hop in three
	// and alias away the 8 ms attack this is here to show.
	constexpr int HistoryLength = 256;

	struct AnalyzerSnapshot
	{
		float low = 0.0f, mid = 0.0f, high = 0.0f, level = 0.0f;
		float kick = 0.0f, snare = 0.0f;
		float kickTrigger = 0.0f, snareTrigger = 0.0f;
		float kickLogic = 0.0f, snareLogic = 0.0f;
		float beatPhase = 0.0f, beatPulse = 0.0f;
		float detectedBpm = 0.0f, tempoConfidence = 0.0f;
		bool calibrating = false;
		float calibrationProgress = 0.0f;
		std::array<float, SpectrumBins> spectrum{};
	};

	// One knob row. Applied AFTER the band's own normalise+lag and, for the two
	// derived sources, after the onsets - see AudioAnalyzer::analyzeHop.
	// The defaults are the IDENTITY: this struct changes nothing until moved.
	struct SourceTuning
	{
		float threshold = 0.0f;   // gate + rescale, [0, 0.95]
		float gain = 1.0f;        // [0, 4]
		float add = 0.0f;         // bipolar [-1, +1]
		float smoothMs = 0.0f;    // 0 == bypassed, [0, 1000]
	};

	// Everything the debug screen needs and nobody else does.
	//
	// Deliberately NOT part of AnalyzerSnapshot: that one is copied by value
	// once per shader per frame, and this is read once per frame by one screen.
	struct AnalyzerDiagnostics
	{
		struct BandInfo
		{
			// raw energy -> percentile-normalised -> attack/release lagged ->
			// shaped by the knobs. Seeing all four is what makes "everything is
			// pinned at 1.0" diagnosable: it is floor and peak collapsing.
			float raw = 0.0f, norm = 0.0f, lagged = 0.0f, shaped = 0.0f;
			// The value after Threshold/Gain/Add but BEFORE Smooth. Shown on the
			// panel so the smoothing knob's effect is visible on its own, and
			// asserted in the tests: with smoothMs at 0 these two must be bit
			// identical, which is what proves the default is a true bypass.
			float preSmooth = 0.0f;
			float floorLevel = 0.0f, peakLevel = 0.0f;
		};
		struct OnsetInfo
		{
			// A detector fires when flux > threshold AND the refractory window
			// has passed AND the material gate holds. Plotting flux against its
			// own moving threshold is the only way to see why it did not.
			float flux = 0.0f, mean = 0.0f, threshold = 0.0f;
			float secondsSinceLast = 0.0f, refractorySec = 0.0f;
			bool blockedByRefractory = false, materialGate = true;
			unsigned long long count = 0;
			// Hits per minute over a rolling window. The cumulative count says
			// nothing about whether a detector is RIGHT: what you compare is
			// this against the detected tempo. Roughly double it means the
			// detector is firing twice per hit.
			float hitsPerMinute = 0.0f;
		};
		BandInfo bands[TunedSources];
		OnsetInfo onsets[Onsets];
		float rms = 0.0f;
		bool gated = false;
		// Rings, newest at `historyAt - 1`, appended once per HOP. Written
		// straight from analyzeHop rather than copied out of Impl each hop.
		float history[TunedSources][HistoryLength] = {};
		// The detectors' own evidence over time. A single frame's flux bar
		// cannot answer "is this firing once per hit": an onset lasts about one
		// hop, so at 60 fps you see its peak only by luck. The three together -
		// flux, the threshold it had to cross, and where it actually fired -
		// make a double trigger or a missed hit obvious at a glance.
		float onsetFlux[Onsets][HistoryLength] = {};
		float onsetThreshold[Onsets][HistoryLength] = {};
		bool onsetFired[Onsets][HistoryLength] = {};
		int historyAt = 0;
		int historyFilled = 0;
	};

	class AudioAnalyzer
	{
	public:
		AudioAnalyzer();
		void reset(int sampleRate);
		void process(const float *samples, std::size_t count);
		void setAutoGain(bool enabled) { autoGain_ = enabled; }
		bool autoGain() const { return autoGain_; }
		void setNoiseGate(float value);
		float noiseGate() const { return noiseGate_; }
		void beginCalibration();
		const AnalyzerSnapshot &snapshot() const { return snapshot_; }
		float sourceValue(int source, int division) const;
		// --- global per-source tuning (the AUDIO screen) ---------------------
		// Setters clamp, like setNoiseGate and jp_audio::setGain already do:
		// these values arrive from a hand-editable settings file.
		void setTuning(int index, const SourceTuning &tuning);
		const SourceTuning &tuning(int index) const;
		void setOnsetSensitivity(int onset, float sensitivity);
		float onsetSensitivity(int onset) const;
		void setOnsetRefractory(int onset, float seconds);
		float onsetRefractory(int onset) const;
		const AnalyzerDiagnostics &diagnostics() const { return diagnostics_; }
		float secondsSinceKick() const;
		float secondsSinceSnare() const;

	private:
		struct Impl;
		Impl *impl_;
		AnalyzerSnapshot snapshot_;
		AnalyzerDiagnostics diagnostics_;
		// MEMBERS survive reset(): `*impl_ = Impl()` cannot reach them. That is
		// the whole rule - configuration lives here, DSP state lives in Impl and
		// is meant to be wiped. Anything Impl needs from here has to be pushed
		// back in by applyTuning() after the wipe.
		bool autoGain_ = true;
		float noiseGate_ = 0.015f;
		SourceTuning tuning_[TunedSources];
		float onsetSensitivity_[Onsets] = {1.6f, 1.4f};
		float onsetRefractorySec_[Onsets] = {0.110f, 0.060f};
		void analyzeHop(const float *window);
		void rebuildSnapshot();
		// Pushes the tuning members into impl_. Must be the LAST thing reset()
		// does, and is called again by every setter so a live twist lands on the
		// next hop.
		void applyTuning();
		void fillDiagnostics(float rms, bool gated);

	public:
		~AudioAnalyzer();
		AudioAnalyzer(const AudioAnalyzer &) = delete;
		AudioAnalyzer &operator=(const AudioAnalyzer &) = delete;
	};

	float smoothToward(float current, float target, float milliseconds,
		float deltaSeconds);
	float downmixFrame(const float *channels, std::size_t channelCount,
		int channelMode, float gain);
}
