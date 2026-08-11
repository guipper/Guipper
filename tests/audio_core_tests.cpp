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

int main()
{
	testSilenceAndBounds();
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
