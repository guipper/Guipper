#include "jp_slider.h"

JPSlider::JPSlider() {}
JPSlider::~JPSlider() {}

void JPSlider::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value, string _name)
{
	setup(_x, _y, _width, _height, _min, _max, _value);
	showtext = true;
	name = _name;
}
void JPSlider::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value)
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
}
void JPSlider::setSpecialColors(ofColor _Cback,
								ofColor _Cactive,
								ofColor _CmouseOver,
								ofColor _Cfront)
{

	/* Cback = _Cback;
	 Cactive = _Cactive;
	 Cmouseover = _CmouseOver;
	 Cfront = _Cfront;*/
}
float JPSlider::getValue()
{
	return value;
}
void JPSlider::draw()
{
	value = parameters->floatValue;
	/*if (mouseOver() && !activeFlag && ofGetMousePressed() ) {
		activeFlag = true;
	}*/

	if (mouseOver() && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
	}
	if (!ofGetMousePressed)
	{
		activeFlag = false;
	}
	if (activeFlag)
	{
		// cout << "MUEVE SLIDER " << endl;
		value = ofMap(ofGetMouseX(), x - width / 2, x + width / 2, min, max);
		value = ofClamp(value, min, max);
		parameters->floatLerpValue = value;
	}
	if (parameters->movtype == 0)
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		// isSpeedSlider = false;
		ofSetColor(jp_constants::Cback[paleta]);
		ofDrawRectangle(x, y, width, height);

		if (activeFlag)
		{

			ofSetColor(jp_constants::Cactive[paleta]);
		}
		else
		{
			if (mouseOver())
			{
				ofSetColor(jp_constants::CmouseOver[paleta]);
			}
			else
			{
				ofSetColor(jp_constants::Cfront[paleta]);
			}
		}
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofDrawRectangle(x - width / 2, y - height / 2, ofMap(parameters->floatValue, min, max, 0, width), height);
		ofSetColor(jp_constants::textcolor);

		if (showtext)
		{
			string Strvalue = name + " " + ofToString(parameters->floatValue, 2);
			jp_constants::p_font.drawString(Strvalue,
											x - jp_constants::p_font.stringWidth(Strvalue) / 2,
											y + jp_constants::p_font.stringHeight(Strvalue) / 2);
		}
	}
	else
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		// isSpeedSlider = false;
		// ofSetColor(255,255,0);
		// ofDrawRectangle(x, y, width, height);
		ofSetColor(255, 255);
		jp_constants_img::timeline.draw(x, y, width, jp_constants_img::timeline.getHeight() * .5);
		if (activeFlag)
		{
			/*float prevalue;
			parameters->floatValue = value;
			parameters->floatLerpValue = value;*/
		}
		else
		{
			if (mouseOver())
			{
				ofSetColor(jp_constants::CmouseOver[paleta]);
			}
			else
			{
				ofSetColor(jp_constants::Cfront[paleta]);
			}
		}
		jp_constants_img::actual.draw(x - width / 2 + ofMap(parameters->floatValue, min, max, 0, width),
									  y - jp_constants_img::actual.getHeight());
		ofSetColor(jp_constants::textcolor);
	}
	// ESTO DE ACA EN REALIDAD IRIA COMO EN UN UPDATE NO EN UN DRAW. PERO BUENO ; POR AHORA QUEDA ACA TOTAL SON 2 IFS NOMA
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
}
