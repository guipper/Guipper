#pragma once

#include <array>
#include <cstddef>

namespace jp_audio_internal
{
	constexpr int SpectrumBins = 16;
	constexpr int Divisions = 5;

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
		float secondsSinceKick() const;
		float secondsSinceSnare() const;

	private:
		struct Impl;
		Impl *impl_;
		AnalyzerSnapshot snapshot_;
		bool autoGain_ = true;
		float noiseGate_ = 0.015f;
		void analyzeHop(const float *window);
		void rebuildSnapshot();

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
