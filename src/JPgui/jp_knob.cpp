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
	if (hovered && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
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
	if (normalizedValue > 0.001f)
	{
		drawKnobArc(x, y, radius - 1.5f, arcStart, valueAngle,
			COL_ACCENT_CYAN,
			activeFlag || hovered ? 3.2f : 2.8f);
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

	// ESTO DE ACA EN REALIDAD IRIA COMO EN UN UPDATE NO EN UN DRAW. PERO BUENO  POR AHORA QUEDA ACA TOTAL SON 2 IFS NOMA
	if (movtype == 0)
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		if (activeFlag)
		{
			float prevalue;
			value = ofMap(ofGetMouseX(), x - width / 2, x + width / 2, min, max);
			value = ofClamp(value, min, max);
			parameters->speed = value;
		}
	}
	else
	{
		if (activeFlag)
		{
			float prevalue;
			value = ofMap(ofGetMouseX(), x - width / 2, x + width / 2, min, max);
			value = ofClamp(value, min, max);
			parameters->speed = value;
		}
	}
	// ESTO DE ACA EN REALIDAD IRIA COMO EN UN UPDATE NO EN UN DRAW. PERO BUENO ; POR AHORA QUEDA ACA TOTAL SON 2 IFS NOMA
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	ofPopStyle();
}
