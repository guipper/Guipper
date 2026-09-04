#include "../src/JPutils/jp_audio_analyzer.h"
#include "../src/JPutils/jp_audio_queue.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{
	constexpr int SampleRate = 48000;
	constexpr float Pi = 3.14159265358979323846f;
	int failures = 0;

	void expect(bool condition, const std::string &message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}

	template <typename Generator>
	void feed(jp_audio_internal::AudioAnalyzer &analyzer, float seconds,
		Generator generator)
	{
		std::array<float, 256> block{};
		const int blocks = int(std::ceil(seconds * SampleRate / block.size()));
		int sample = 0;
		for (int b = 0; b < blocks; ++b)
		{
			for (float &value : block) value = generator(float(sample++) / SampleRate);
			analyzer.process(block.data(), block.size());
		}
	}

	void testSilenceAndBounds()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		feed(analyzer, 2.0f, [](float) { return 0.0f; });
		const auto &s = analyzer.snapshot();
		expect(s.level < 0.01f, "silence level is gated");
		expect(s.kickTrigger == 0.0f && s.snareTrigger == 0.0f,
			"silence emits no onset");
		for (float value : s.spectrum)
			expect(value >= 0.0f && value <= 1.0f, "spectrum remains bounded");
	}

	std::array<float, 3> analyzeTone(float hz)
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setAutoGain(false);
		analyzer.setNoiseGate(0.0001f);
		feed(analyzer, 1.5f, [=](float t) { return 0.6f * std::sin(2 * Pi * hz * t); });
		const auto &s = analyzer.snapshot();
		return {{s.low, s.mid, s.high}};
	}

	void testBandSeparationAndSweep()
	{
		const auto low = analyzeTone(80.0f);
		const auto mid = analyzeTone(800.0f);
		const auto high = analyzeTone(6000.0f);
		expect(low[0] > 0.01f && low[0] > low[1] * 5.0f && low[0] > low[2] * 5.0f,
			"80 Hz selects low band");
		expect(mid[1] > 0.01f && mid[1] > mid[0] * 5.0f && mid[1] > mid[2] * 5.0f,
			"800 Hz selects mid band");
		expect(high[2] > 0.01f && high[2] > high[0] * 5.0f && high[2] > high[1] * 5.0f,
			"6 kHz selects high band");

		jp_audio_internal::AudioAnalyzer sweep;
		sweep.reset(SampleRate); sweep.setNoiseGate(0.0001f);
		feed(sweep, 10.0f, [](float t) {
			const float p = std::fmod(t, 10.0f) / 10.0f;
			const float hz = 20.0f * std::pow(900.0f, p);
			return 0.5f * std::sin(2 * Pi * hz * t);
		});
		for (float value : sweep.snapshot().spectrum)
			expect(std::isfinite(value) && value >= 0.0f && value <= 1.0f,
				"sweep spectrum is finite and bounded");
	}

	void testNoiseCalibrationAndClippingSignal()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.beginCalibration();
		std::minstd_rand rng(42);
		std::uniform_real_distribution<float> noise(-0.008f, 0.008f);
		feed(analyzer, 3.4f, [&](float) { return noise(rng); });
		expect(!analyzer.snapshot().calibrating, "three-second calibration completes");
		expect(analyzer.noiseGate() > 0.005f && analyzer.noiseGate() < 0.03f,
			"calibration derives a plausible noise gate");
		feed(analyzer, 0.5f, [&](float) { return noise(rng) * 0.5f; });
		expect(analyzer.snapshot().level < 0.08f, "calibrated ambient noise is rejected");

		float peak = 0.0f;
		feed(analyzer, 0.1f, [&](float t) {
			const float value = 1.2f * std::sin(2 * Pi * 100.0f * t);
			peak = std::max(peak, std::abs(value)); return value;
		});
		expect(peak > 0.995f, "clipped input fixture crosses the clip threshold");
	}

	void testPinkNoiseAndLoudnessAdaptation()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate); analyzer.setNoiseGate(0.0001f);
		std::minstd_rand rng(7);
		std::uniform_real_distribution<float> white(-1.0f, 1.0f);
		float b0 = 0, b1 = 0, b2 = 0;
		feed(analyzer, 3.0f, [&](float) {
			const float w = white(rng);
			b0 = 0.99765f * b0 + w * 0.0990460f;
			b1 = 0.96300f * b1 + w * 0.2965164f;
			b2 = 0.57000f * b2 + w * 1.0526913f;
			return (b0 + b1 + b2 + w * 0.1848f) * 0.04f;
		});
		const auto &noise = analyzer.snapshot();
		expect(noise.low > 0.05f && noise.mid > 0.05f && noise.high > 0.05f,
			"pink noise excites all analysis bands");

		jp_audio_internal::AudioAnalyzer adaptive;
		adaptive.reset(SampleRate); adaptive.setNoiseGate(0.0001f);
		auto quietTone = [](float t) {
			const float envelope = 0.2f + 0.8f * std::pow(
				0.5f + 0.5f * std::sin(2 * Pi * 2.0f * t), 4.0f);
			return 0.04f * envelope * std::sin(2 * Pi * 80.0f * t);
		};
		feed(adaptive, 4.0f, quietTone);
		const float quietBefore = adaptive.snapshot().low;
		feed(adaptive, 2.0f, [](float t) {
			return 0.8f * (0.2f + 0.8f * std::pow(
				0.5f + 0.5f * std::sin(2 * Pi * 2.0f * t), 4.0f)) *
				std::sin(2 * Pi * 80.0f * t);
		});
		feed(adaptive, 6.0f, quietTone);
		const float quietAfter = adaptive.snapshot().low;
		expect(quietBefore > 0.15f && quietAfter > 0.05f,
			"percentile auto-gain recovers after a loudness change");
	}

	void testChannelDownmix()
	{
		const float stereo[2] = {0.25f, -0.75f};
		expect(std::abs(jp_audio_internal::downmixFrame(stereo, 2, 0, 1.0f) + 0.25f) < 0.0001f,
			"mix mode averages channels");
		expect(jp_audio_internal::downmixFrame(stereo, 2, 1, 2.0f) == 0.5f,
			"left mode selects and gains the left channel");
		expect(jp_audio_internal::downmixFrame(stereo, 2, 2, 1.0f) == -0.75f,
			"right mode selects the right channel");
		expect(std::abs(jp_audio_internal::downmixFrame(stereo, 2, 2, 2.0f)) >= 0.995f,
			"gained channel exposes clipping to the callback threshold");
	}

	void testTempoOnsetsAndPhase()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate); analyzer.setNoiseGate(0.001f);
		int kickEdges = 0, snareEdges = 0;
		float previousKick = 0.0f, previousSnare = 0.0f;
		std::array<float, 256> block{};
		int sample = 0;
		for (int b = 0; b < int(14.0f * SampleRate / block.size()); ++b)
		{
			for (float &value : block)
			{
				const float t = float(sample++) / SampleRate;
				const float beat = t * 2.0f, phase = beat - std::floor(beat);
				const int beatIndex = int(std::floor(beat));
				value = 0.9f * std::pow(1.0f - phase, 8.0f) *
					std::sin(2 * Pi * 60.0f * t);
				if (beatIndex % 2 == 1)
					value += 0.7f * std::pow(1.0f - phase, 20.0f) *
						std::sin(t * 12347.123f) * std::sin(t * 7919.731f);
			}
			analyzer.process(block.data(), block.size());
			const auto &s = analyzer.snapshot();
			if (s.kickTrigger > 0.5f && previousKick <= 0.5f) ++kickEdges;
			if (s.snareTrigger > 0.5f && previousSnare <= 0.5f) ++snareEdges;
			previousKick = s.kickTrigger; previousSnare = s.snareTrigger;
		}
		const auto &s = analyzer.snapshot();
		expect(kickEdges >= 20 && kickEdges <= 30, "kick onset count tracks 120 BPM material");
		expect(snareEdges >= 8 && snareEdges <= 16, "snare onset count tracks backbeats");
		expect(std::abs(s.detectedBpm - 120.0f) < 8.0f && s.tempoConfidence >= 0.45f,
			"tempo converges to 120 BPM with confidence");
		expect(s.beatPhase >= 0.0f && s.beatPhase < 1.0f, "confident beat phase is normalized");

		feed(analyzer, 4.0f, [](float) { return 0.0f; });
		expect(analyzer.snapshot().tempoConfidence < 0.45f &&
			analyzer.snapshot().beatPulse == 0.0f,
			"clock pulse is suppressed after confidence loss");
	}

	void testFrameInvariantSmoothing()
	{
		auto afterOneSecond = [](int fps) {
			float value = 1.0f;
			for (int frame = 0; frame < fps; ++frame)
				value = jp_audio_internal::smoothToward(value, 0.0f, 250.0f, 1.0f / fps);
			return value;
		};
		const float at30 = afterOneSecond(30);
		expect(std::abs(at30 - afterOneSecond(60)) < 0.0001f,
			"release is equivalent at 30 and 60 FPS");
		expect(std::abs(at30 - afterOneSecond(120)) < 0.0001f,
			"release is equivalent at 30 and 120 FPS");
	}

	void testQueue()
	{
		using Queue = jp_audio_internal::SpscAudioQueue<8, 4>;
		Queue queue;
		float sample[1] = {0.0f};
		for (int i = 0; i < 4; ++i) { sample[0] = float(i); expect(queue.push(sample, 1), "queue accepts capacity"); }
		expect(!queue.push(sample, 1) && queue.dropped() == 1, "full queue drops newest block");
		Queue::Block block;
		for (int i = 0; i < 4; ++i)
			expect(queue.pop(block) && block.samples[0] == float(i), "queue preserves FIFO order");
		expect(!queue.pop(block), "empty queue does not fabricate data");
		queue.reset(); expect(queue.dropped() == 0, "restart clears overflow diagnostics");

		Queue concurrent;
		constexpr int Count = 200000;
		std::atomic<bool> done{false};
		std::atomic<int> consumed{0};
		std::thread producer([&] {
			for (int i = 0; i < Count; ++i)
			{
				float value = float(i);
				while (!concurrent.push(&value, 1)) std::this_thread::yield();
			}
			done.store(true, std::memory_order_release);
		});
		std::thread consumer([&] {
			int previous = -1; Queue::Block item;
			for (;;)
			{
				if (!concurrent.pop(item))
				{
					if (done.load(std::memory_order_acquire)) break;
					std::this_thread::yield(); continue;
				}
				const int current = int(item.samples[0]);
				if (current <= previous) ++failures;
				previous = current; consumed.fetch_add(1, std::memory_order_relaxed);
			}
		});
		producer.join(); consumer.join();
		expect(consumed.load() > 0, "concurrent queue transfers blocks");
	}
}

	// ---------------------------------------------------------------- tuning
	// The global per-source stage the AUDIO screen drives. Everything here is
	// about one promise: the defaults are the IDENTITY, and each knob does
	// exactly the one thing its label says.

	using Tuning = jp_audio_internal::SourceTuning;

	// A repeatable signal with content in every band, run to a steady state.
	template <typename Setup>
	std::array<float, 6> tunedSources(Setup setup)
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setAutoGain(false);
		analyzer.setNoiseGate(0.0001f);
		setup(analyzer);
		feed(analyzer, 2.0f, [](float t) {
			return 0.35f * std::sin(2 * Pi * 80.0f * t) +
				0.25f * std::sin(2 * Pi * 900.0f * t) +
				0.20f * std::sin(2 * Pi * 5000.0f * t);
		});
		std::array<float, 6> out{};
		const int order[6] = {0, 1, 2, 5, 6, 7};   // jp_audio::Source indices
		for (int i = 0; i < 6; ++i)
			out[std::size_t(i)] = analyzer.sourceValue(order[i], 0);
		return out;
	}

	void testTuningDefaultsAreIdentity()
	{
		const auto plain = tunedSources([](jp_audio_internal::AudioAnalyzer &) {});
		const auto explicitDefaults = tunedSources(
			[](jp_audio_internal::AudioAnalyzer &a) {
				for (int i = 0; i < jp_audio_internal::TunedSources; ++i)
					a.setTuning(i, Tuning());
			});
		for (int i = 0; i < 6; ++i)
			expect(plain[std::size_t(i)] == explicitDefaults[std::size_t(i)],
				"writing the default tuning changes nothing at all");
		// Not "close to": exactly. smoothToward floors tau at 1 ms, so a
		// smoothMs of 0 that went through the filter would land at 99.5% and
		// this is the assertion that catches it.
		expect(plain[0] > 0.01f, "the fixture actually drives the low band");

		// The bypass, asserted directly. smoothToward floors tau at 1 ms, so
		// running the default through the filter leaves 0.5% of lag per hop -
		// invisible in a converged value, fatal for anything that tests a peak
		// of exactly 1.0. shaped and preSmooth must be the SAME float.
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setNoiseGate(0.0001f);
		bool everMoved = false, alwaysEqual = true;
		std::array<float, 256> block{};
		for (int hop = 0; hop < 400; ++hop)
		{
			for (std::size_t i = 0; i < block.size(); ++i)
			{
				const float t = float(hop * 256 + int(i)) / SampleRate;
				block[i] = 0.7f * std::sin(2 * Pi * 80.0f * t) *
					(std::fmod(t, 0.25f) < 0.05f ? 1.0f : 0.05f);
			}
			analyzer.process(block.data(), block.size());
			const auto &band = analyzer.diagnostics().bands[0];
			if (band.preSmooth > 0.01f) everMoved = true;
			if (band.shaped != band.preSmooth) alwaysEqual = false;
		}
		expect(everMoved, "the bypass fixture actually moves the band");
		expect(alwaysEqual, "smoothMs 0 is a true bypass, not a 1 ms filter");
	}

	void testTuningKnobsDoWhatTheySay()
	{
		const auto base = tunedSources([](jp_audio_internal::AudioAnalyzer &) {});

		Tuning silent; silent.gain = 0.0f;
		const auto zeroGain = tunedSources([&](jp_audio_internal::AudioAnalyzer &a) {
			a.setTuning(jp_audio_internal::TUNED_LOW, silent); });
		expect(zeroGain[0] == 0.0f, "gain 0 silences its own row");
		expect(zeroGain[1] == base[1], "gain 0 on low leaves mid untouched");

		Tuning gated; gated.threshold = 0.95f;
		const auto highGate = tunedSources([&](jp_audio_internal::AudioAnalyzer &a) {
			a.setTuning(jp_audio_internal::TUNED_MID, gated); });
		expect(highGate[1] <= base[1] || base[1] > 0.95f,
			"a high threshold can only gate, never invent signal");

		Tuning lifted; lifted.add = 1.0f;
		const auto pinned = tunedSources([&](jp_audio_internal::AudioAnalyzer &a) {
			a.setTuning(jp_audio_internal::TUNED_HIGH, lifted); });
		expect(pinned[2] == 1.0f, "add +1 pins the row at full scale");

		Tuning dropped; dropped.add = -1.0f;
		const auto floored = tunedSources([&](jp_audio_internal::AudioAnalyzer &a) {
			a.setTuning(jp_audio_internal::TUNED_HIGH, dropped); });
		expect(floored[2] == 0.0f, "add -1 removes the whole row - a floor killer");

		// Order matters: gain multiplies BEFORE add offsets. With gain 0 and
		// add 0.5 the row must rest at exactly 0.5; if the two were swapped it
		// would be 0.
		Tuning ordered; ordered.gain = 0.0f; ordered.add = 0.5f;
		const auto affine = tunedSources([&](jp_audio_internal::AudioAnalyzer &a) {
			a.setTuning(jp_audio_internal::TUNED_LEVEL, ordered); });
		expect(std::fabs(affine[5] - 0.5f) < 0.0001f,
			"gain applies before add");
	}

	void testTuningSettersClamp()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		Tuning wild;
		wild.threshold = 9.0f; wild.gain = -5.0f;
		wild.add = 7.0f; wild.smoothMs = 90000.0f;
		analyzer.setTuning(jp_audio_internal::TUNED_LOW, wild);
		const Tuning &held = analyzer.tuning(jp_audio_internal::TUNED_LOW);
		expect(held.threshold == 0.95f, "threshold clamps below the 1.0 divisor");
		expect(held.gain == 0.0f && held.add == 1.0f, "gain and add clamp");
		Tuning loud; loud.gain = 999.0f;
		analyzer.setTuning(jp_audio_internal::TUNED_HIGH, loud);
		expect(analyzer.tuning(jp_audio_internal::TUNED_HIGH).gain == 16.0f,
			"gain reaches 16x - MANUAL needs the headroom, 4x could not "
			"rescue a raw high band");
		expect(held.smoothMs == 1000.0f, "smooth clamps");
		analyzer.setOnsetSensitivity(jp_audio_internal::ONSET_KICK, 0.1f);
		expect(analyzer.onsetSensitivity(jp_audio_internal::ONSET_KICK) == 1.0f,
			"sensitivity below 1 would fire on its own mean");
		analyzer.setOnsetRefractory(jp_audio_internal::ONSET_SNARE, 10.0f);
		expect(analyzer.onsetRefractory(jp_audio_internal::ONSET_SNARE) == 0.5f,
			"refractory clamps");
		// Out of range indices are ignored rather than corrupting memory.
		analyzer.setTuning(-1, wild);
		analyzer.setTuning(99, wild);
		analyzer.setOnsetSensitivity(7, 2.0f);
	}

	void testTuningSurvivesResetButFilterStateDoesNot()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		Tuning slow; slow.smoothMs = 900.0f; slow.add = 0.25f;
		analyzer.setTuning(jp_audio_internal::TUNED_LOW, slow);
		analyzer.setOnsetSensitivity(jp_audio_internal::ONSET_KICK, 3.2f);
		analyzer.setOnsetRefractory(jp_audio_internal::ONSET_KICK, 0.25f);
		analyzer.setNoiseGate(0.0001f);
		feed(analyzer, 1.0f, [](float t) { return 0.6f * std::sin(2 * Pi * 80.0f * t); });
		expect(analyzer.sourceValue(0, 0) > 0.0f, "the low row is carrying a value");

		// A device change. Configuration must survive; DSP memory must not, or a
		// dead input would keep replaying the old level forever.
		analyzer.reset(SampleRate);
		expect(analyzer.tuning(jp_audio_internal::TUNED_LOW).smoothMs == 900.0f,
			"tuning survives reset");
		expect(analyzer.onsetSensitivity(jp_audio_internal::ONSET_KICK) == 3.2f,
			"onset sensitivity survives reset - applyTuning pushes it back in");
		expect(analyzer.onsetRefractory(jp_audio_internal::ONSET_KICK) == 0.25f,
			"onset refractory survives reset");
		expect(analyzer.sourceValue(0, 0) == 0.0f,
			"the smoothing filter's memory is wiped by reset");
		// The snare defaults must come from the members now, not from the two
		// hardcoded lines reset() used to carry.
		expect(analyzer.onsetSensitivity(jp_audio_internal::ONSET_SNARE) == 1.4f,
			"snare keeps its own factory sensitivity through reset");
	}

	// Reading the member back only proves the member survived - which it does
	// for free, since *impl_ = Impl() cannot reach it. What has to be proven is
	// that reset() PUSHED it back into the fresh Impl, and the only way to see
	// that is through the DSP.
	//
	// Refractory is the probe, not sensitivity: a synthetic kick's flux sits far
	// above 4x its own running mean, so every sensitivity in range detects the
	// same beats. Refractory gates the count directly - on a 0.15 s pattern,
	// 30 ms lets every hit through and 500 ms cannot.
	unsigned long long onsetsAfterResetWith(float refractorySec)
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setOnsetRefractory(jp_audio_internal::ONSET_KICK, refractorySec);
		analyzer.reset(SampleRate);          // the device change
		analyzer.setNoiseGate(0.0001f);
		feed(analyzer, 3.0f, [](float t) {
			const float beat = std::fmod(t, 0.15f);
			return 0.9f * std::exp(-beat * 26.0f) * std::sin(2 * Pi * 60.0f * t);
		});
		return analyzer.diagnostics().onsets[jp_audio_internal::ONSET_KICK].count;
	}

	void testApplyTuningReachesTheDetector()
	{
		const unsigned long long open = onsetsAfterResetWith(0.030f);
		const unsigned long long held = onsetsAfterResetWith(0.500f);
		expect(open > 10, "a fast kick pattern fires repeatedly");
		expect(held * 3 < open,
			"refractory set before a reset still reaches the detector after it");
	}

	// A sound card with a DC offset is common and inaudible. It lands in FFT
	// bin 0, and with no high-pass anywhere in this chain that constant used to
	// sit inside the Low band forever - one of the five bins that make it up.
	void testDirectCurrentIsNotBass()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setAutoGain(false);
		analyzer.setNoiseGate(0.0001f);
		feed(analyzer, 2.0f, [](float) { return 0.3f; });
		expect(analyzer.snapshot().low < 0.05f,
			"a DC offset must not read as bass");
	}

	// A drone, a held pad, a sustained sub - completely ordinary material, and
	// the case the percentile normaliser is worst at. Its 10th percentile of a
	// CONSTANT signal is that signal, so the floor walks up to meet the peak and
	// the band degrades into a comparator that flips between 0 and 1 on noise.
	void testSustainedMaterialDoesNotCollapseTheNormaliser()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setNoiseGate(0.0001f);
		float lowest = 2.0f, highest = -1.0f;
		std::array<float, 256> block{};
		int sample = 0;
		const int blocks = int(30.0f * SampleRate / block.size());
		for (int b = 0; b < blocks; ++b)
		{
			for (std::size_t i = 0; i < block.size(); ++i)
			{
				const float t = float(sample++) / SampleRate;
				block[i] = 0.5f * std::sin(2 * Pi * 60.0f * t);
			}
			analyzer.process(block.data(), block.size());
			// Only the last third, once the floor has had time to climb.
			if (b < blocks * 2 / 3) continue;
			const float value = analyzer.sourceValue(0, 0);
			lowest = std::min(lowest, value);
			highest = std::max(highest, value);
		}
		expect(highest - lowest < 0.02f,
			"a steady tone must not leave the low band twitching");
		// The other half of the promise: do not "fix" the twitch by letting
		// sustained material decay to nothing. It is loud, and it should say so.
		expect(highest > 0.8f, "a steady loud tone must still read as loud");
	}

	void testDiagnosticsExplainTheDetectors()
	{
		jp_audio_internal::AudioAnalyzer analyzer;
		analyzer.reset(SampleRate);
		analyzer.setNoiseGate(0.0001f);
		feed(analyzer, 2.0f, [](float t) {
			const float beat = std::fmod(t, 0.5f);
			const float env = std::exp(-beat * 30.0f);
			return 0.9f * env * std::sin(2 * Pi * 60.0f * t);
		});
		const auto &d = analyzer.diagnostics();
		expect(d.onsets[jp_audio_internal::ONSET_KICK].count > 0,
			"the kick detector fired on a kick pattern");
		expect(d.onsets[jp_audio_internal::ONSET_KICK].threshold ==
			d.onsets[jp_audio_internal::ONSET_KICK].mean *
			analyzer.onsetSensitivity(jp_audio_internal::ONSET_KICK),
			"the reported threshold is mean * sensitivity, the real test");
		expect(d.rms > 0.0f, "the input RMS is reported");
		expect(d.bands[0].peakLevel >= d.bands[0].floorLevel,
			"the normaliser's floor and peak are exposed and ordered");
		expect(d.historyFilled > 100,
			"history is appended per hop, not per frame");
	}

int main()
{
	testSilenceAndBounds();
	testTuningDefaultsAreIdentity();
	testTuningKnobsDoWhatTheySay();
	testTuningSettersClamp();
	testTuningSurvivesResetButFilterStateDoesNot();
	testApplyTuningReachesTheDetector();
	testDirectCurrentIsNotBass();
	testSustainedMaterialDoesNotCollapseTheNormaliser();
	testDiagnosticsExplainTheDetectors();
	testBandSeparationAndSweep();
	testNoiseCalibrationAndClippingSignal();
	testPinkNoiseAndLoudnessAdaptation();
	testChannelDownmix();
	testTempoOnsetsAndPhase();
	testFrameInvariantSmoothing();
	testQueue();
	if (failures != 0)
	{
		std::cerr << failures << " audio-core assertion(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "audio-core tests passed\n";
	return EXIT_SUCCESS;
}
