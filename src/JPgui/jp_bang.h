#pragma once
#include "ofMain.h"
#include "jp_controller.h"
#include "../JPutils/jp_constants.h"
class JPBang : public JPcontroller
{
public:
	/*ofColor CmouseOver;
	ofColor Cfront;
	ofColor Cactive;
	ofColor textcolor;
	*/
	void setup(float _x, float _y, float _width, float _height);
	void draw();
};