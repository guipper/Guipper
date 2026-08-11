#include "jp_audio.h"
#include "jp_audio_analyzer.h"
#include "jp_audio_queue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace
{
	constexpr std::size_t QueueBlocks = 64;
	constexpr std::size_t MaxBlockFrames = 512;
	using AudioQueue = jp_audio_internal::SpscAudioQueue<MaxBlockFrames, QueueBlocks>;

	AudioQueue gQueue;
	jp_audio_internal::AudioAnalyzer gAnalyzer;
	std::atomic<bool> gAccept{false};
	std::atomic<float> gGain{1.0f};
	std::atomic<int> gChannelMode{jp_audio::CHANNEL_MIX};
	std::atomic<float> gInputPeak{0.0f};
	std::atomic<bool> gClippingAtomic{false};

	struct AudioListener : public ofBaseSoundInput
	{
		void audioIn(ofSoundBuffer &buffer) override
		{
			if (gAccept.load(std::memory_order_acquire)) jp_audio::audioIn(buffer);
		}
	};
	AudioListener gListener;
	ofSoundStream gStream;
	std::vector<std::string> gDeviceNames;
	std::vector<ofSoundDevice> gDevices;
	std::string gDeviceName;
	std::string gStatus = "audio off";
	bool gRunning = false;
	bool gEnabled = true;
	int gShaderDiv = jp_audio::DIV_1;
	int gSampleRate = 44100;
	int gTestMode = 0;
	float gTestTime = 0.0f;
	float gClipHoldUntil = -1.0f;
	float gClipClock = 0.0f;
	float gNextRecoveryAttempt = 0.0f;
	jp_audio::AudioSnapshot gSnapshot;

	void synthesize(float *block, std::size_t count, float &time, int mode)
	{
		constexpr float pi = 3.14159265358979323846f;
		for (std::size_t i = 0; i < count; ++i)
		{
			const float t = time + float(i) / gSampleRate;
			float value = 0.0f;
			if (mode == 2)
			{
				const float p = std::fmod(t, 10.0f) / 10.0f;
				const float hz = 20.0f * std::pow(900.0f, p);
				value = 0.7f * std::sin(2.0f * pi * hz * t);
			}
			else
			{
				const float beat = t * 2.0f;
				const float phase = beat - std::floor(beat);
				const int beatIndex = int(std::floor(beat));
				value += 0.9f * std::pow(std::max(0.0f, 1.0f - phase), 8.0f) *
					std::sin(2.0f * pi * 60.0f * t);
				if (beatIndex % 2 == 1)
					value += 0.7f * std::pow(std::max(0.0f, 1.0f - phase), 20.0f) *
					std::sin(t * 12347.123f) * std::sin(t * 7919.731f);
				value += 0.15f * std::sin(2.0f * pi * 800.0f * t);
			}
			block[i] = value;
		}
		time += float(count) / gSampleRate;
	}

	void copyAnalyzerSnapshot()
	{
		const auto &source = gAnalyzer.snapshot();
		gSnapshot.low = source.low; gSnapshot.mid = source.mid;
		gSnapshot.high = source.high; gSnapshot.level = source.level;
		gSnapshot.kick = source.kick; gSnapshot.snare = source.snare;
		gSnapshot.kickTrigger = source.kickTrigger;
		gSnapshot.snareTrigger = source.snareTrigger;
		gSnapshot.kickLogic = source.kickLogic; gSnapshot.snareLogic = source.snareLogic;
		gSnapshot.beatPhase = source.beatPhase; gSnapshot.beatPulse = source.beatPulse;
		gSnapshot.detectedBpm = source.detectedBpm;
		gSnapshot.tempoConfidence = source.tempoConfidence;
		gSnapshot.inputPeak = gInputPeak.load(std::memory_order_relaxed);
		if (gClippingAtomic.exchange(false, std::memory_order_relaxed))
			gClipHoldUntil = gClipClock + 0.5f;
		gSnapshot.clipping = gClipClock < gClipHoldUntil;
		gSnapshot.calibrating = source.calibrating;
		gSnapshot.calibrationProgress = source.calibrationProgress;
		gSnapshot.droppedBlocks = gQueue.dropped();
		for (int i = 0; i < jp_audio::SPECTRUM_BINS; ++i)
			gSnapshot.spectrum[i] = source.spectrum[i];
	}
}

void jp_audio::audioIn(ofSoundBuffer &buffer)
{
	// Audio thread: bounded stack storage, atomics and fixed queue copies only.
	const std::size_t frames = buffer.getNumFrames();
	const std::size_t channels = std::max<std::size_t>(1, buffer.getNumChannels());
	const float gain = gGain.load(std::memory_order_relaxed);
	const int mode = gChannelMode.load(std::memory_order_relaxed);
	std::array<float, MaxBlockFrames> mono{};
	float peak = 0.0f;
	for (std::size_t offset = 0; offset < frames; offset += MaxBlockFrames)
	{
		const std::size_t count = std::min(MaxBlockFrames, frames - offset);
		for (std::size_t i = 0; i < count; ++i)
		{
			const std::size_t frame = offset + i;
			mono[i] = jp_audio_internal::downmixFrame(
				&buffer[frame * channels], channels, mode, gain);
			peak = std::max(peak, std::abs(mono[i]));
		}
		if (!gQueue.push(mono.data(), count)) break;
	}
	gInputPeak.store(peak, std::memory_order_relaxed);
	if (peak >= 0.995f) gClippingAtomic.store(true, std::memory_order_relaxed);
}

void jp_audio::setup()
{
	gAnalyzer.reset(gSampleRate);
	const char *test = std::getenv("GUIPPER_AUDIO_TEST");
	gTestMode = test != nullptr ? ofToInt(test) : 0;
	if (gTestMode > 0)
	{
		gStatus = "TEST MODE " + ofToString(gTestMode) + " (synthetic signal)";
		gRunning = true;
		return;
	}
	refreshDevices();
	if (gEnabled) startStream();
}

void jp_audio::refreshDevices()
{
	gDevices.clear(); gDeviceNames.clear();
	try
	{
		for (const ofSoundDevice &device : gStream.getDeviceList())
		{
			if (device.inputChannels <= 0) continue;
			std::string unique = device.name;
			int duplicate = 1;
			while (std::find(gDeviceNames.begin(), gDeviceNames.end(), unique) != gDeviceNames.end())
				unique = device.name + " (" + ofToString(++duplicate) + ")";
			gDevices.push_back(device); gDeviceNames.push_back(unique);
		}
	}
	catch (const std::exception &error)
	{
		gStatus = std::string("device scan failed: ") + error.what();
		ofLogError("jp_audio") << gStatus;
	}
	catch (...) { gStatus = "device scan failed"; }
}

void jp_audio::startStream()
{
	stopStream();
	if (gTestMode > 0) { gRunning = true; return; }
	if (!gEnabled) { gStatus = "audio off"; return; }
	if (gDevices.empty()) { gStatus = "no audio input device"; return; }
	int selected = -1;
	for (std::size_t i = 0; i < gDeviceNames.size(); ++i)
		if (gDeviceNames[i] == gDeviceName) { selected = int(i); break; }
	const int use = selected >= 0 ? selected : 0;
	try
	{
		ofSoundStreamSettings settings;
		settings.setInDevice(gDevices[use]);
		settings.numInputChannels = std::min(2,
			std::max(1, static_cast<int>(gDevices[use].inputChannels)));
		settings.numOutputChannels = 0;
		const auto &rates = gDevices[use].sampleRates;
		auto supports = [&](unsigned int rate) {
			return rates.empty() || std::find(rates.begin(), rates.end(), rate) != rates.end();
		};
		settings.sampleRate = supports(48000) ? 48000 :
			(supports(44100) ? 44100 : (rates.empty() ? 44100 : rates.front()));
		settings.bufferSize = 256;
		settings.setInListener(&gListener);
		gStream.setup(settings);
		gSampleRate = settings.sampleRate;
		gAnalyzer.reset(gSampleRate);
		gQueue.reset();
		gInputPeak.store(0.0f, std::memory_order_relaxed);
		gClippingAtomic.store(false, std::memory_order_relaxed);
		gSnapshot = AudioSnapshot(); gClipClock = 0.0f; gClipHoldUntil = -1.0f;
		gRunning = true;
		gAccept.store(true, std::memory_order_release);
		gStatus = gDeviceNames[use] + " - " + ofToString(gSampleRate) + " Hz" +
			(selected < 0 && !gDeviceName.empty() ?
				"  (saved device '" + gDeviceName + "' not found)" : "");
		ofLogNotice("jp_audio") << gStatus;
	}
	catch (const std::exception &error)
	{
		try { gStream.stop(); gStream.close(); } catch (...) {}
		gRunning = false;
		gStatus = std::string("stream failed: ") + error.what();
		ofLogError("jp_audio") << gStatus;
	}
	catch (...)
	{
		try { gStream.stop(); gStream.close(); } catch (...) {}
		gRunning = false; gStatus = "stream failed";
	}
}

void jp_audio::stopStream()
{
	gAccept.store(false, std::memory_order_release);
	if (!gRunning) return;
	if (gTestMode == 0) try { gStream.stop(); gStream.close(); } catch (...) {}
	gRunning = false;
}

void jp_audio::shutdown() { stopStream(); gStatus = "audio off"; }

bool jp_audio::runSelfTest(std::string *report)
{
	jp_audio_internal::AudioAnalyzer analyzer;
	analyzer.reset(48000); analyzer.setNoiseGate(0.001f);
	std::array<float, 256> block{};
	for (int i = 0; i < 188; ++i) analyzer.process(block.data(), block.size());
	const bool silence = analyzer.snapshot().level < 0.02f;
	float time = 0.0f;
	const int savedRate = gSampleRate; gSampleRate = 48000;
	for (int i = 0; i < 2250; ++i)
	{
		synthesize(block.data(), block.size(), time, 1);
		analyzer.process(block.data(), block.size());
	}
	gSampleRate = savedRate;
	const auto &snapshot = analyzer.snapshot();
	const bool bounded = snapshot.low >= 0 && snapshot.low <= 1 &&
		snapshot.mid >= 0 && snapshot.mid <= 1 && snapshot.high >= 0 && snapshot.high <= 1;
	const bool tempo = snapshot.detectedBpm >= 105 && snapshot.detectedBpm <= 135 &&
		snapshot.tempoConfidence >= 0.35f;
	const bool passed = silence && bounded && tempo;
	if (report) *report = "silence=" + ofToString(silence) + " bounded=" +
		ofToString(bounded) + " bpm=" + ofToString(snapshot.detectedBpm, 2) +
		" confidence=" + ofToString(snapshot.tempoConfidence, 2);
	return passed;
}

void jp_audio::update()
{
	if (!gRunning)
	{
		// Setup failures and unplugged interfaces recover on the main thread.
		// Keep retries slow so an absent device does not spam or stall frames.
		const float now = ofGetElapsedTimef();
		if (gEnabled && gTestMode == 0 && now >= gNextRecoveryAttempt)
		{
			gNextRecoveryAttempt = now + 2.0f;
			refreshDevices();
			startStream();
		}
		return;
	}
	if (gTestMode > 0)
	{
		std::array<float, 512> block{};
		const int blocks = std::max(1, std::min(10,
			int(ofGetLastFrameTime() * gSampleRate / block.size())));
		for (int i = 0; i < blocks; ++i)
		{
			synthesize(block.data(), block.size(), gTestTime, gTestMode);
			gAnalyzer.process(block.data(), block.size());
			gClipClock += float(block.size()) / gSampleRate;
		}
	}
	else
	{
		AudioQueue::Block block;
		int processed = 0;
		while (processed < 16 && gQueue.pop(block))
		{
			gAnalyzer.process(block.samples.data(), block.count);
			gClipClock += float(block.count) / gSampleRate;
			++processed;
		}
	}
	copyAnalyzerSnapshot();
}

bool jp_audio::isRunning() { return gRunning; }
std::string jp_audio::getStatus()
{
	return gQueue.dropped() > 0 && gRunning ?
		gStatus + "  (dropped " + ofToString(gQueue.dropped()) + ")" : gStatus;
}
const std::vector<std::string> &jp_audio::getInputDeviceNames() { return gDeviceNames; }
std::string jp_audio::getDeviceName() { return gDeviceName; }
bool jp_audio::setDevice(const std::string &name)
{
	gDeviceName = name; if (gEnabled) startStream(); return gRunning;
}
void jp_audio::setEnabled(bool enabled)
{
	if (gEnabled == enabled) return;
	gEnabled = enabled;
	if (enabled) startStream(); else { stopStream(); gStatus = "audio off"; }
}
bool jp_audio::getEnabled() { return gEnabled; }
void jp_audio::setGain(float gain) { gGain.store(ofClamp(gain, 0.05f, 16.0f)); }
float jp_audio::getGain() { return gGain.load(std::memory_order_relaxed); }
void jp_audio::setAutoGain(bool enabled) { gAnalyzer.setAutoGain(enabled); }
bool jp_audio::getAutoGain() { return gAnalyzer.autoGain(); }
void jp_audio::setChannelMode(int mode)
{
	gChannelMode.store(ofClamp(mode, 0, CHANNEL_COUNT - 1), std::memory_order_relaxed);
}
int jp_audio::getChannelMode() { return gChannelMode.load(std::memory_order_relaxed); }
const char *jp_audio::channelModeLabel(int mode)
{
	return mode == CHANNEL_LEFT ? "LEFT" : mode == CHANNEL_RIGHT ? "RIGHT" : "MIX";
}
void jp_audio::setNoiseGate(float value) { gAnalyzer.setNoiseGate(value); }
float jp_audio::getNoiseGate() { return gAnalyzer.noiseGate(); }
void jp_audio::beginCalibration() { gAnalyzer.beginCalibration(); }
void jp_audio::setShaderDiv(int div) { gShaderDiv = ofClamp(div, 0, DIV_COUNT - 1); }
int jp_audio::getShaderDiv() { return gShaderDiv; }
bool jp_audio::isRhythmSource(int source) { return source >= SRC_KICK_TRIGGER && source < SRC_COUNT; }
float jp_audio::getValue(int source, int div)
{
	return gRunning ? gAnalyzer.sourceValue(source, div) : 0.0f;
}
float jp_audio::getLevel() { return gRunning ? gSnapshot.level : 0.0f; }
jp_audio::AudioSnapshot jp_audio::getSnapshot() { return gRunning ? gSnapshot : AudioSnapshot(); }
float jp_audio::secondsSinceKick() { return gAnalyzer.secondsSinceKick(); }
float jp_audio::secondsSinceSnare() { return gAnalyzer.secondsSinceSnare(); }
void jp_audio::getSpectrum(float *out, int bins)
{
	if (!out || bins <= 0) return;
	for (int i = 0; i < bins; ++i)
	{
		const int source = std::min(SPECTRUM_BINS - 1, int(float(i) / bins * SPECTRUM_BINS));
		out[i] = gRunning ? gSnapshot.spectrum[source] : 0.0f;
	}
}
const char *jp_audio::sourceLabel(int source)
{
	static const char *labels[SRC_COUNT] = {"Low", "Mid", "High", "Kick", "Snare",
		"Low bass", "High mid", "Level", "Kick trigger", "Kick envelope", "Kick logic",
		"Snare trigger", "Snare envelope", "Snare logic"};
	return source >= 0 && source < SRC_COUNT ? labels[source] : "Unknown";
}
const char *jp_audio::divLabel(int div)
{
	static const char *labels[DIV_COUNT] = {"1", "2", "4", "8", "16"};
	return labels[std::max(0, std::min(div, int(DIV_COUNT) - 1))];
}
