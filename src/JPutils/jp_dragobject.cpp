#include "jp_dragobject.h"
#include "jp_pointer.h"
#include "jp_constants.h"   // palette for the hitbox overlay

bool JPdragobject::useMouseOverride = false;
ofVec2f JPdragobject::mouseOverride = ofVec2f(0, 0);

JPdragobject::JPdragobject() {}
JPdragobject::~JPdragobject() {}
void JPdragobject::setup(float _x, float _y, float _width, float _height)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
}
void JPdragobject::setMouseOverride(const ofVec2f &_mouse)
{
	mouseOverride = _mouse;
	useMouseOverride = true;
	// Canvas-space hit testing and the canvas pointer layer are the same
	// window, so binding them here means they cannot drift apart.
	jp_pointer::setLayer(jp_pointer::kCanvas);
}
void JPdragobject::clearMouseOverride()
{
	useMouseOverride = false;
	jp_pointer::clearLayer();
}
float JPdragobject::getMouseX()
{
	return useMouseOverride ? mouseOverride.x : ofGetMouseX();
}
float JPdragobject::getMouseY()
{
	return useMouseOverride ? mouseOverride.y : ofGetMouseY();
}
bool jp_hitbox::debugEnabled()
{
	static const bool enabled = std::getenv("GUIPPER_HITBOX") != nullptr;
	return enabled;
}

void jp_hitbox::draw(const ofRectangle &bounds, bool hovered, bool enabled)
{
	if (!debugEnabled() || bounds.width <= 0.0f || bounds.height <= 0.0f)
		return;
	ofPushStyle();
	// Explicit: this is called from draw paths that leave the rect mode on
	// CENTER, and a CENTER outline here would sit a half-rect off target -
	// which is exactly the kind of lie a debug overlay must not tell.
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofNoFill();
	ofSetLineWidth(hovered ? 2.0f : 1.0f);
	ofSetColor(hovered ? COL_ACCENT_GOLD :
		(enabled ? ofColor(COL_ACCENT_CYAN, 150) :
			ofColor(COL_BORDER_MUTED, 90)));
	ofDrawRectangle(bounds);
	ofPopStyle();
}

ofRectangle JPdragobject::hitBounds() const
{
	const float w = width + hitPaddingX * 2.0f;
	const float h = height + hitPaddingY * 2.0f;
	return ofRectangle(x - w * 0.5f, y - h * 0.5f, w, h);
}

bool JPdragobject::mouseOver()
{
	// A covered control neither highlights nor responds. This one test serves
	// every box, every inspector control and the two box buttons, and it is the
	// only thing that reaches the controls which actuate from inside draw().
	if (!jp_pointer::available()) return false;
	// Padded rect, so growing a hit area needs no change here and no control
	// can end up drawn in one place and clickable in another.
	return hitBounds().inside(getMouseX(), getMouseY());
}
bool JPdragobject::mouseGrab()
{
	if (mouseOver() && ofGetMousePressed())
	{
		return true;
	}
	else
	{
		return false;
	}
}
