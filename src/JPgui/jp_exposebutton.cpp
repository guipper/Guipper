#include "jp_exposebutton.h"
#include "../JPutils/jp_tooltip.h"

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

	const ofRectMode previousRectMode = ofGetRectMode();
	// CORNER, because the plate below is given corner coordinates. Under CENTER
	// - which is what the panel usually leaves behind - those same numbers draw
	// the plate centred on its own top-left corner, so it lands half a box up and
	// to the left and sits on top of the lock button next to it.
	ofSetRectMode(OF_RECTMODE_CORNER);

	const bool hover = mouseOver();
	const float radius = std::min(width, height) * 0.28f;

	// Rounded, like every other action chip in the panel. A square plate read as
	// a different family of control from the lock button beside it.
	if (boolValue)
		ofSetColor(COL_ACCENT_CYAN, hover ? 90 : 60);
	else
		ofSetColor(COL_BG_INPUT, hover ? 220 : 170);
	ofFill();
	ofDrawRectRounded(x - width * 0.5f, y - height * 0.5f,
		width, height, radius);

	ofNoFill();
	ofSetLineWidth(1);
	if (boolValue) ofSetColor(COL_ACCENT_CYAN, hover ? 255 : 210);
	else ofSetColor(hover ? COL_TEXT_SECONDARY : COL_TEXT_DIM, 180);
	ofDrawRectRounded(x - width * 0.5f, y - height * 0.5f,
		width, height, radius);
	ofFill();

	// Same eye either way, open or struck through. The off state used to be an
	// unrelated little square, so the two states did not read as one object with
	// two settings - you could not tell at a glance which rows were exposed.
	drawEyeIcon(x, y, width * 0.58f);

	jp_tooltip::draw(boolValue ?
		"Exposed to the group. Click to hide" :
		"Expose parameter to the group",
		x - width / 2.0f, y - height / 2.0f, width, height);

	ofSetRectMode(previousRectMode);
	ofSetColor(255);
}

void JPExposeButton::drawEyeIcon(float cx, float cy, float size)
{
	const float halfW = size * 0.5f;
	const float halfH = size * 0.35f;
	const float left = cx - halfW;
	const float right = cx + halfW;
	const float cpOffsetX = halfW * 0.7f;
	const float cpOffsetY = halfH * 1.05f;

	const ofColor tint = boolValue ? ofColor(COL_ACCENT_CYAN)
								   : ofColor(COL_TEXT_DIM);
	const int alpha = boolValue ? 255 : 190;

	ofSetColor(tint, alpha);
	ofNoFill();
	ofSetLineWidth(boolValue ? 1.6f : 1.3f);

	ofDrawBezier(left, cy, left + cpOffsetX, cy - cpOffsetY,
		right - cpOffsetX, cy - cpOffsetY, right, cy);
	ofDrawBezier(right, cy, right - cpOffsetX, cy + cpOffsetY,
		left + cpOffsetX, cy + cpOffsetY, left, cy);

	ofFill();
	if (boolValue)
	{
		// Open: a solid pupil, so an exposed row is obvious from across the panel.
		ofSetColor(tint, 255);
		ofDrawCircle(cx, cy, size * 0.17f);
	}
	else
	{
		// Closed: a hollow pupil plus a slash, the usual "hidden" reading.
		ofNoFill();
		ofSetLineWidth(1.3f);
		ofSetColor(tint, alpha);
		ofDrawCircle(cx, cy, size * 0.15f);
		ofDrawLine(left + size * 0.06f, cy + halfH * 0.95f,
			right - size * 0.06f, cy - halfH * 0.95f);
		ofFill();
	}

	ofSetLineWidth(1);
}
