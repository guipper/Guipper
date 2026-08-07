#include "jp_dragobject.h"
#include "jp_pointer.h"

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
bool JPdragobject::mouseOver()
{
	// A covered control neither highlights nor responds. This one test serves
	// every box, every inspector control and the two box buttons, and it is the
	// only thing that reaches the controls which actuate from inside draw().
	if (!jp_pointer::available()) return false;
	float mouseX = getMouseX();
	float mouseY = getMouseY();
	if (mouseX > x - width / 2 && mouseX < x + width / 2 && mouseY > y - height / 2 && mouseY < y + height / 2)
	{
		// cout << "MOUSEOVER" << endl;
		return true;
	}
	else
	{
		return false;
	}
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
