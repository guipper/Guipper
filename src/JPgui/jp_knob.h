#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_controller.h"
#include "../JPutils/jp_constants.h"
class JPKnob : public JPcontroller
{
public:
	JPKnob();
	~JPKnob();

	void setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value);
	void setSpecialColors(ofColor _Cback, ofColor _Cactive, ofColor _CmouseOver, ofColor _Cfront);
	void setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value, string _name);

	// A second, dimmer arc showing where something ELSE is currently driving
	// this value - the audio modulator on an automation speed. Negative means
	// nothing to show. The knob's own value and its number are untouched: the
	// reference has to stay readable while the ghost moves.
	float ghostValue = -1.0f;

	void draw();
	float getValue();

	// Determina si es el slider que se usa para velocidad o no, en base a eso decide el color
	// Tal vez convenga ponerlo en otro lugar? hmmm

	bool useSpecialColors;
	bool useTexture;
	ofColor Cback;
	ofColor Cactive;
	ofColor CmouseOver;
	ofColor Cfront;
	// ofColor
};