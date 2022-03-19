#pragma once
#include "ofMain.h"
// Esta clase es solo para trabajar los objetos que se agarran o no se agarran. CORTA LA BOCHEN
class JPdragobject
{
public:
	JPdragobject();
	~JPdragobject();

	float x, y;
	float width;
	float height;

	virtual void setPos(float _x, float _y)
	{
		x = _x;
		y = _y;
	}
	void setup(float _x, float _y, float _width, float _height);
	// virtual void draw();

	bool activeFlag;

	virtual bool mouseOver(); // Si esta encima del slider
	virtual bool mouseGrab(); // Si esta agarrado
							  // bool mouseClick();
protected:
	float isGrabbed2;
};