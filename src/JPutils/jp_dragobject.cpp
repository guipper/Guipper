#include "jp_dragobject.h"

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
}
void JPdragobject::clearMouseOverride()
{
	useMouseOverride = false;
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
