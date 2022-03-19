#include "jp_bang.h"

void JPBang::setup(float _x, float _y, float _width, float _height)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	activeFlag = false;
	paleta = 0;
}

void JPBang::draw()
{

	if (ofGetMousePressed() && mouseOver())
	{
		activeFlag = true;
	}

	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}

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
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofRect(x, y, width, height);
	ofSetColor(ofColor::white);
	// lele.drawString(name, x - name.length() * 5, y + height * 0.1); // ESTA LINEA DE CODIGO ME GENERA UN ERROR TOTALMENTE INTENDIBLE MIKE
}
