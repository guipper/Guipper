#pragma once

#include "ofMain.h"
#include <atomic>
#include <string>
#include <vector>

// Audio input and analysis: the global reactive source, alongside jp_constants::bpm.
//
// Shape follows jp_constants: a static class with global access, because
// JPParameter::update() reads it from hundreds of call sites per frame and
// threading an object through all of them would be pure noise.
//
// THREADING. The callback only downmixes into a bounded SPSC queue. Completed
// queue slots are published with release/acquire atomics, so the producer can
// never overwrite a block that update() is reading. DSP and snapshots remain
// entirely on the main thread.
class jp_audio
{
public:
	static constexpr int SPECTRUM_BINS = 16;

	struct AudioSnapshot
	{
		float low = 0.0f, mid = 0.0f, high = 0.0f, level = 0.0f;
		float kick = 0.0f, snare = 0.0f;
		float kickTrigger = 0.0f, snareTrigger = 0.0f;
		float kickLogic = 0.0f, snareLogic = 0.0f;
		float beatPhase = 0.0f, beatPulse = 0.0f;
		float detectedBpm = 0.0f, tempoConfidence = 0.0f;
		float inputPeak = 0.0f;
		bool clipping = false;
		bool calibrating = false;
		float calibrationProgress = 0.0f;
		unsigned long long droppedBlocks = 0;
		float spectrum[SPECTRUM_BINS] = {0.0f};
	};

	enum ChannelMode
	{
		CHANNEL_MIX = 0,
		CHANNEL_LEFT,
		CHANNEL_RIGHT,
		CHANNEL_COUNT
	};
	// Assignable values. Index-stable: written into save files as <audiosource>,
	// so new entries append at the end and nothing is ever reordered.
	enum Source
	{
		SRC_LOW = 0,
		SRC_MID,
		SRC_HIGH,
		SRC_KICK,
		SRC_SNARE,
		SRC_LOWBASS,      // low + kick, the "blended driver" idea
		SRC_HIGHMID,      // high + snare
		SRC_LEVEL,        // overall RMS
		SRC_KICK_TRIGGER, // 1/0 pulse on the counted beat
		SRC_KICK_EXPRESS, // holds the envelope for a full count cycle
		SRC_KICK_LOGIC,   // toggle, flips on the counted beat
		SRC_SNARE_TRIGGER,
		SRC_SNARE_EXPRESS,
		SRC_SNARE_LOGIC,
		SRC_COUNT
	};

	// Beat subdivisions for the rhythm sources: fire every Nth onset.
	enum Div
	{
		DIV_1 = 0,
		DIV_2,
		DIV_4,
		DIV_8,
		DIV_16,
		DIV_COUNT
	};

	static void setup();      // call AFTER loadSettings()
	static void update();     // once per frame, main thread, BEFORE boxes.update()
	static void shutdown();
	// Deterministic synthetic-chain check used by CI and diagnostics.
	static bool runSelfTest(std::string *report = nullptr);

	// --- devices -----------------------------------------------------------
	static void refreshDevices();
	static const std::vector<std::string> &getInputDeviceNames();
	static std::string getDeviceName();               // "" == system default
	static bool setDevice(const std::string &name);   // restarts the stream
	static bool isRunning();
	static std::string getStatus();                   // human text for SETTINGS

	// --- config ------------------------------------------------------------
	static void setEnabled(bool enabled);
	static bool getEnabled();
	static void setGain(float gain);
	static float getGain();
	static void setAutoGain(bool autoGain);
	static bool getAutoGain();
	static void setChannelMode(int mode);
	static int getChannelMode();
	static const char *channelModeLabel(int mode);
	static void setNoiseGate(float value);
	static float getNoiseGate();
	static void beginCalibration();
	static void setShaderDiv(int div);
	static int getShaderDiv();

	// --- values (main thread, always safe, 0 when not running) --------------
	// The single entry point. `div` is ignored by the continuous sources, so
	// callers never have to branch.
	static float getValue(int source, int div = DIV_1);
	static AudioSnapshot getSnapshot();
	static float getLevel();
	static bool isRhythmSource(int source);
	static const char *sourceLabel(int source);
	static const char *divLabel(int div);
	// Small spectrum for the SETTINGS meter, `bins` values in 0..1.
	static void getSpectrum(float *out, int bins);
	// Onset flashes for the meter: seconds since the last kick / snare.
	static float secondsSinceKick();
	static float secondsSinceSnare();

	// Called by ofApp's ofSoundStream listener. Public only because the
	// listener needs it; nothing else should touch it.
	static void audioIn(ofSoundBuffer &buffer);

private:
	static void startStream();
	static void stopStream();
	static void analyzeHop(const float *block, float dt);
	static void synthesizeTestHop(float *block, float dt);
};
