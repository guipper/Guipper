#include "jp_dragobject.h"

JPdragobject::JPdragobject(){}
JPdragobject::~JPdragobject(){}
void JPdragobject::setup(float _x, float _y, float _width, float _height)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
}
bool JPdragobject::mouseOver()
{
	if (ofGetMouseX() > x - width / 2
		&& ofGetMouseX() < x + width / 2
		&& ofGetMouseY() > y - height / 2
		&& ofGetMouseY() < y + height / 2) {
		//cout << "MOUSEOVER" << endl;
		return true;
	}
	else {
		return false;
	}
}
bool JPdragobject::mouseGrab()
{
	if (mouseOver() && ofGetMousePressed()) {
		return true;
	}
	else {
		return false;
	}
}