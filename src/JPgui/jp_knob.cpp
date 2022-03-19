#include "jp_knob.h"

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
	ofSetRectMode(OF_RECTMODE_CENTER);
	if (mouseOver())
	{
		ofSetColor(20, 50);
		ofDrawRectangle(x, y,
						width, height);
	}
	if (mouseOver() && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
	}
	if (!ofGetMousePressed)
	{
		activeFlag = false;
	}
	// ofDrawRectangle(x, y, width, height);

	// ofSetColor(255,0,0, 255);
	// ofDrawRectangle(x, y, width, 20);
	ofSetColor(255, 255);
	//	ofSetColor(0, 255);
	jp_constants_img::speed.draw(x, y, width, width);
	// ofSetColor(86, 4,  255);
	ofSetColor(0);
	float offsetX = +3;
	string Strvalue = name + " " + ofToString(value, 2);
	jp_constants::p2_font.drawString(Strvalue,
									 x - jp_constants::p2_font.stringWidth(Strvalue) / 2 - offsetX,
									 y + jp_constants::p2_font.stringHeight(Strvalue) / 2);
	float angle = ofMap(value, min, max, TWO_PI, 0);
	float offsetx = sin(angle) * width / 2;
	float offsety = cos(angle) * width / 2;
	ofSetColor(0, 255);
	ofDrawEllipse(x + offsetx, y + offsety, 10, 10);

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
}
