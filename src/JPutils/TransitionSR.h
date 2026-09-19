

#pragma once
#include "ofMain.h"
#include "jp_constants.h"
#include "jp_transition.h"
class TransitionSR {

public:
	static jp_transition::Config &preferences();
    static float &renderScaleLimit() { static float value=1.f; return value; }
	const jp_transition::Timeline &state() const { return timeline; }
	void setCapabilities(jp_transition::Capabilities value) { capabilities = value; }
	void captureInterruption();
    void freezeOutgoing();
    bool observeFrame(double milliseconds, double fps) { return timeline.sample(milliseconds, fps); }

	TransitionSR();
	~TransitionSR();
	void setup();
	void setup(ofFbo * _fbo1, ofFbo * _fbo2);
	void advance();
	// Explicit timestep for deterministic tests; advance() uses the wall clock.
	void advance(float deltaSeconds);
	void update(bool advanceClock = true);
	// Duration is independent of frame rate; new profiles default to 1500 ms.
	void setDurationMs(float _durationMs);
	float getDurationMs() const;

	// Stable effect identifiers consumed by the common compositor.
	// APPEND ONLY: the value is written into settings.xml, so inserting one
	// would silently change what an existing configuration means.
	enum Type
	{
		TYPE_MIX = 0,     // straight per-pixel crossfade, the original
		TYPE_WARP,        // both frames displaced through one shared flow
		TYPE_DITHER,      // ordered dither, no pixel is ever a mixture
		TYPE_CUT, TYPE_BEAT_CUT, TYPE_PLASMA, TYPE_RADIAL_IN, TYPE_RADIAL_OUT,
		TYPE_PUSH, TYPE_WIPE, TYPE_BLOCKS, TYPE_SPLIT_PUSH, TYPE_CENTER_PUSH,
		TYPE_CENTER_SQUEEZE, TYPE_DOTS, TYPE_FEEDBACK, TYPE_MORPH, TYPE_STAGED_MORPH, TYPE_RANDOM,
		TYPE_PALETTE_ECHO, TYPE_PALETTE_MOSH, TYPE_SPECTRAL_GLITCH,
		TYPE_COUNT
	};
	void setType(int _type);
	int getType() const;
	static const char *typeLabel(int _type);
	// Renders a straight-RGBA interpolation into the currently bound target.
	// The caller owns target clearing and blend state.
	bool renderStraightMix(ofFbo *first, ofFbo *second, float mixValue,
		float width, float height);
	void setLerpValue(float _val);
	void setLerpValue();
	void reload();
	void draw(float _x, float _y, float _w, float _h);
	// Draws a sub-rectangle of the master canvas, for the screen wall. Source
	// args are in canvas pixels. draw() routes through this so there is a
	// single draw path that cannot drift.
	void drawSubsection(float _x, float _y, float _w, float _h,
		float _sx, float _sy, float _sw, float _sh);
	bool isSourceAllocated() const;
	float getSourceWidth() const;
	float getSourceHeight() const;
	ofFbo *getOutput() { return este.isAllocated() ? &este : nullptr; }
	ofFbo *getFirstInput() const { return fbo1; }
	ofFbo *getSecondInput() const { return fbo2; }
	// No reallocate here on purpose. Nothing in the app resizes render FBOs at
	// runtime - the two box fbo.allocate loops are commented out because doing
	// it crashed the app (JPboxgroup.cpp:8289) - so callers must convert
	// normalized crops against getSourceWidth/Height rather than assume this
	// canvas matches jp_constants::renderWidth. The removed resize() was a
	// landmine for the same reason: unused, and sized to the main window.
	void setFboPointer1(ofFbo* _fbo1);
	void setFboPointer2(ofFbo* _fbo2);
	float getLerpValue() const;
	void draw();

	ofShader shader;
	ofFbo dummyfbo;
	//void update(Shaderrender * _Sh, Shaderrender * _Sh2);
private:
	bool ensureShader();

	float x;
	float y;
	float w;
	float h;
	ofFbo * fbo1; //OBJETIVO
	ofFbo * fbo2; //JUGADOR
	ofFbo este;
	vector <float> uniformValues;
	vector <string > uniformNames;
	string dir;

	float lerpValue;
	float durationMs = 1500.0f;
	jp_transition::Timeline timeline;
	jp_transition::Capabilities capabilities;
	bool armed = false;
    bool outgoingFrozen = false;
	double explicitClock = 0.;
	ofFbo interruptedFrame;
	ofFbo paletteFbo;
	ofFbo feedbackFrames[2];
	int feedbackIndex = 0, feedbackEffect = -1;
	bool feedbackValid = false;
	float feedbackProgress = 0.f;
	double feedbackTime = 0.;
	int transitionType = TYPE_MIX;
};
