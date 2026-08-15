

#pragma once
#include "ofMain.h"
#include "jp_constants.h"
class TransitionSR {

public:
	TransitionSR();
	~TransitionSR();
	void setup();
	void setup(ofFbo * _fbo1, ofFbo * _fbo2);
	void advance();
	// Elapsed seconds to advance by. Defaults to ofGetLastFrameTime(); the
	// explicit form exists so a test can feed a fixed timestep.
	void advance(float deltaSeconds);
	void update();
	// How long a full 0..1 transition takes. Was a fixed 0.02 per FRAME, which
	// meant 833ms at 60fps and 2s at 25fps - the fade got slower exactly when
	// the machine was struggling. 833 is that old 60fps figure, so nothing
	// visibly changes until someone moves it.
	void setDurationMs(float _durationMs);
	float getDurationMs() const;

	// Which shader blends the two frames. All of them share one uniform
	// contract - textura1, textura2, mixst, resolution - so switching is a
	// shader swap and nothing at the call sites changes.
	//
	// APPEND ONLY: the value is written into settings.xml, so inserting one
	// would silently change what an existing configuration means.
	enum Type
	{
		TYPE_MIX = 0,     // straight per-pixel crossfade, the original
		TYPE_WARP,        // both frames displaced through one shared flow
		TYPE_DITHER,      // ordered dither, no pixel is ever a mixture
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
	float durationMs = 833.0f;
	int transitionType = TYPE_MIX;
};
