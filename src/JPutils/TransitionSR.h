

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
	void update();
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
};
