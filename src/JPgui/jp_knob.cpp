#include "jp_knob.h"
#include <algorithm>

namespace
{
	void drawKnobArc(float x, float y, float radius,
		float startDegrees, float endDegrees, const ofColor &color,
		float lineWidth)
	{
		const int segments = std::max(
			2, static_cast<int>(std::abs(endDegrees - startDegrees) / 8.0f));
		ofNoFill();
		ofSetColor(color);
		ofSetLineWidth(lineWidth);
		ofBeginShape();
		for (int i = 0; i <= segments; i++)
		{
			const float amount = i / static_cast<float>(segments);
			const float angle = ofDegToRad(
				ofLerp(startDegrees, endDegrees, amount));
			ofVertex(
				x + std::cos(angle) * radius,
				y + std::sin(angle) * radius);
		}
		ofEndShape(false);
		ofFill();
	}
}

JPKnob::JPKnob() {}
JPKnob::~JPKnob() {}

void JPKnob::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value, string _name)
{
	setup(_x, _y, _width, _height, _min, _max, _value);
	showtext = true;
	name = _name;
	cout << "WIDH " << _width << endl;
	cout << "HEIGHT " << _width << endl;
}
void JPKnob::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	min = _min;
	max = _max;
	value = _value;
	showtext = false;
	useSpecialColors = false;
	activeFlag = false;
	paleta = 0;
	useTexture = false;
	activable2 = true;
}
void JPKnob::setSpecialColors(ofColor _Cback,
							  ofColor _Cactive,
							  ofColor _CmouseOver,
							  ofColor _Cfront)
{

	/* Cback = _Cback;
	 Cactive = _Cactive;
	 Cmouseover = _CmouseOver;
	 Cfront = _Cfront;*/
}
float JPKnob::getValue()
{
	return value;
}
void JPKnob::draw()
{
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CENTER);
	const bool hovered = mouseOver();
	// A press only counts for this knob if it STARTED on it.
	//
	// Without pressStartedHere() the test was just "pressed AND hovered", so
	// holding the button anywhere - dragging a cable out of a box' OUT, moving
	// a box, sweeping a marquee - and passing over a knob latched it and
	// rewrote its value on the spot. Same defect, same fix, as JPToogle: the
	// origin belongs to the gesture, so it lives in JPdragobject and survives
	// the controller rebuild that every inspector click triggers.
	const bool pressed = ofGetMousePressed(OF_MOUSE_BUTTON_LEFT);
    if (!pressed) activeFlag = false;
    if (!activeFlag && hovered && pressed && pressStartedHere() && activable2)
    {
        activeFlag = true;
        dragLastMouseX = ofGetMouseX();
    }
    if (activeFlag && parameters != nullptr)
    {
        // Relative motion avoids jumping to the click position. Screen pixels
        // keep sensitivity stable outside the inspector's clipped hit area.
        const float mouseX = ofGetMouseX();
        const float precision = ofGetKeyPressed(OF_KEY_SHIFT) ? 0.1f : 1.0f;
        value = ofClamp(value + (mouseX - dragLastMouseX) * (max - min) * precision / 300.0f, min, max);
        dragLastMouseX = mouseX;
        parameters->speed = value;
    }

	const float diameter = std::max(
		12.0f, std::min(width, height) - 6.0f);
	const float radius = diameter / 2.0f;
	const float normalizedValue = ofMap(
		value, min, max, 0.0f, 1.0f, true);
	const float arcStart = 135.0f;
	const float arcEnd = 405.0f;
	const float valueAngle = ofLerp(
		arcStart, arcEnd, normalizedValue);

	ofSetColor(hovered ? ofColor(COL_BG_HOVER, 245) :
		ofColor(COL_BG_INPUT, 235));
	ofDrawCircle(x, y, radius - 2.0f);
	ofNoFill();
	ofSetLineWidth(1.0f);
	ofSetColor(ofColor(COL_BORDER_MUTED, 150));
	ofDrawCircle(x, y, radius - 5.0f);
	ofFill();

	drawKnobArc(x, y, radius - 1.5f, arcStart, arcEnd,
		ofColor(COL_TEXT_SECONDARY, 145), 2.2f);
	// Where something else is currently driving this value - the audio
	// modulator on an automation speed.
	//
	// A radial tick INSIDE the ring, not a second arc: at 34 px an arc on a
	// smaller radius lands on the inner border circle and reads as the border
	// having changed colour. A tick that sweeps is unmistakable, and it leaves
	// the cyan value arc and the number alone, which is the point - the
	// reference has to stay readable while the ghost moves.
	if (normalizedValue > 0.001f)
	{
		drawKnobArc(x, y, radius - 1.5f, arcStart, valueAngle,
			COL_ACCENT_CYAN,
			activeFlag || hovered ? 3.2f : 2.8f);
	}

	if (ghostValue >= 0.0f)
	{
		const float ghostAngle = ofDegToRad(ofLerp(arcStart, arcEnd,
			ofMap(ghostValue, min, max, 0.0f, 1.0f, true)));
		ofSetColor(COL_ACCENT_GREEN);
		ofSetLineWidth(2.0f);
		ofDrawLine(
			x + std::cos(ghostAngle) * (radius - 9.0f),
			y + std::sin(ghostAngle) * (radius - 9.0f),
			x + std::cos(ghostAngle) * (radius - 4.0f),
			y + std::sin(ghostAngle) * (radius - 4.0f));
		ofSetLineWidth(1.0f);
	}

	const float angle = ofDegToRad(valueAngle);
	const float indicatorInnerRadius = radius - 6.0f;
	const float indicatorOuterRadius = radius + 0.5f;
	const float indicatorStartX =
		x + std::cos(angle) * indicatorInnerRadius;
	const float indicatorStartY =
		y + std::sin(angle) * indicatorInnerRadius;
	const float indicatorX =
		x + std::cos(angle) * indicatorOuterRadius;
	const float indicatorY =
		y + std::sin(angle) * indicatorOuterRadius;
	ofSetColor(activeFlag || hovered ? COL_TEXT_PRIMARY :
		COL_ACCENT_CYAN);
	ofSetLineWidth(1.8f);
	ofDrawLine(indicatorStartX, indicatorStartY, indicatorX, indicatorY);
	ofDrawCircle(indicatorX, indicatorY,
		activeFlag || hovered ? 2.0f : 1.6f);
	ofSetLineWidth(1.0f);

	const string valueLabel = ofToString(value, 2);
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p2_font.drawString(
		valueLabel,
		x - jp_constants::p2_font.stringWidth(valueLabel) / 2.0f,
		y + jp_constants::p2_font.stringHeight(valueLabel) / 2.0f);

	ofPopStyle();
}
