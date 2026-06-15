#include "jp_exposebutton.h"

void JPExposeButton::setup(float _x, float _y, float _size)
{
	x = _x;
	y = _y;
	width = _size;
	height = _size;
	activeFlag = false;
	boolValue = false;
	showtext = false;
	controllertype = TOOGLE;
	paleta = 0;
	useTexture = false;
	activable2 = true;
	activable = true;
}

void JPExposeButton::draw()
{
	// Toggle logic inherited from JPToogle
	if (ofGetMousePressed() && mouseOver() && activable && activable2)
	{
		activeFlag = true;
		activable = false;
	}
	if (!ofGetMousePressed())
	{
		activable = true;
	}
	if (activeFlag)
	{
		activeFlag = false;
		boolValue = !boolValue;
	}

	ofSetRectMode(OF_RECTMODE_CENTER);

	bool hover = mouseOver();

	if (boolValue)
	{
		// Exposed: green background
		if (hover)
			ofSetColor(100, 230, 150, 230);
		else
			ofSetColor(70, 190, 110, 220);
	}
	else
	{
		// Not exposed: dark background
		if (hover)
			ofSetColor(70, 70, 70, 200);
		else
			ofSetColor(45, 45, 45, 180);
	}
	ofDrawRectangle(x, y, width, height);

	// Border
	ofNoFill();
	ofSetLineWidth(1);
	if (hover)
		ofSetColor(200, 200, 200, 200);
	else
		ofSetColor(100, 100, 100, 180);
	ofDrawRectangle(x, y, width, height);
	ofFill();
	ofSetLineWidth(1);

	// Eye icon
	if (boolValue)
	{
		drawEyeIcon(x, y, width * 0.55f);
	}
	else
	{
		// Small dim dot when not exposed
		ofSetColor(120, 120, 120, 180);
		ofDrawRectangle(x, y, width * 0.3f, width * 0.3f);
	}

	ofSetColor(255);
}

void JPExposeButton::drawEyeIcon(float cx, float cy, float size)
{
	float halfW = size * 0.5f;
	float halfH = size * 0.35f;
	float left = cx - halfW;
	float right = cx + halfW;
	float top = cy - halfH;
	float bottom = cy + halfH;

	// Draw almond/eye shape using two bezier curves
	ofSetColor(255);
	ofNoFill();
	ofSetLineWidth(1.8f);

	// Left point, right point, control points for top curve
	float cpOffsetX = halfW * 0.7f;
	float cpOffsetY = halfH * 0.8f;

	// Top curve: left -> right, arching up
	ofDrawBezier(
		left, cy,                          // P0: left point
		left + cpOffsetX, cy - cpOffsetY,  // P1: control up-left
		right - cpOffsetX, cy - cpOffsetY, // P2: control up-right
		right, cy                          // P3: right point
	);

	// Bottom curve: right -> left, arching down
	ofDrawBezier(
		right, cy,                          // P0: right point
		right - cpOffsetX, cy + cpOffsetY,  // P1: control down-right
		left + cpOffsetX, cy + cpOffsetY,   // P2: control down-left
		left, cy                            // P3: left point
	);

	ofFill();

	// Iris (outer circle)
	ofSetColor(255);
	ofDrawCircle(cx, cy, size * 0.18f);

	// Pupil (inner circle)
	ofSetColor(0);
	ofDrawCircle(cx, cy, size * 0.10f);

	// Bright highlight dot on the iris
	ofSetColor(255);
	ofDrawCircle(cx - size * 0.05f, cy - size * 0.05f, size * 0.04f);

	ofSetLineWidth(1);
}
