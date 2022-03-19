#include "jp_toogle.h"

void JPToogle::setup(float _x, float _y, float _width, float _height, string _name, bool _boolValue)
{
	setup(_x, _y, _width, _height);
	name = _name;
	showtext = true;

	boolValue = _boolValue;
	activable = true;
	controllertype = TOOGLE;
}
void JPToogle::setup(float _x, float _y, float _width, float _height)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	// cout << "WIDTH " << width << endl;
	// cout << "HEIGHT" << height << endl;
	activeFlag = true;
	boolValue = true;

	showtext = false;
	controllertype = TOOGLELIST;
	paleta = 0;
	useTexture = false;
	activable2 = true;
}
void JPToogle::setUseTexture(int _as)
{
	textureindex = _as;
	useTexture = true;

	// DIOS ESTE ALGORITMO HORRENDO:
	if (parameters->needsUpdate == true)
	{
		activable = false;
	}
	else
	{
		activable = true;
	}
}

void JPToogle::drawSelectedTexture()
{
	/*ofSetColor(jp_constants::Cback[paleta]);
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofRect(x, y, width, height);
	ofSetColor(ofColor::white);
	*/
	if (textureindex == COLLAPSE)
	{

		/*ofSetColor(jp_constants::Cback[paleta]);
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofRect(x, y, width, height);
		ofSetColor(ofColor::white);

		ofSetColor(jp_constants::Cback[paleta]);*/
		ofSetRectMode(OF_RECTMODE_CENTER);
		// ofRect(x, y, width, height);
		ofSetColor(ofColor::white);
		ofPushMatrix();
		ofTranslate(x, y);
		if (parameters->movtype == 0)
		{
			ofRotate(-90);
		}
		jp_constants_img::actual.draw(0, 0, width, height);
		ofPopMatrix();
	}
	else
	{
		if (mouseOver())
		{
			/*ofSetColor(20, 50);
			ofDrawRectangle(x, y,
				width, height);*/
		}
		if (parameters->movtype == textureindex)
		{

			ofSetColor(120, 40, 244);
		}
		else
		{
			if (mouseOver())
			{
				/*ofSetColor(20, 50);
				ofDrawRectangle(x, y,
					width, height);*/
				ofSetColor(80, 20, 180);
			}
			else
			{
				ofSetColor(0, 0, 0);
			}
		}
		if (textureindex == RAN)
		{
			jp_constants_img::ran.draw(x, y, width, height);
		}
		else if (textureindex == GODER)
		{
			ofPushMatrix();
			ofTranslate(x, y);
			if (parameters->movtype == 3)
			{
				ofRotate(-180);
				/*ofSetColor(255, 150);
				ofDrawRectangle(0, 0, 50, 50);*/
			}

			jp_constants_img::una_direccion.draw(0, 0, width, height);
			ofPopMatrix();
		}
		else if (textureindex == IDAYVUELTA)
		{
			jp_constants_img::idayvuelta.draw(x, y, width, height);
		}
	}
}
void JPToogle::draw()
{

	// YO SE QUE ESTO ES UN CHOCLAZO Y QUE SE PUEDE SINTETIZAR. PERO ESTOY QUEMADEN
	if (ofGetMousePressed() && mouseOver() && activable && activable2)
	{
		activeFlag = true;
		activable = false;
		update_movtype();
	}

	if (!ofGetMousePressed())
	{
		activable = true;
	}

	if (activeFlag)
	{
		activeFlag = false;
		boolValue = !boolValue;
		ofSetColor(jp_constants::Cactive[paleta]);
		// cout << "TRIGGER" << endl ;
	}

	if (useTexture)
	{
		drawSelectedTexture();
	}
	else
	{
		if (boolValue)
		{
			ofSetColor(jp_constants::Cback[paleta]);
		}
		else
		{
			ofSetColor(jp_constants::Cactive[paleta]);
		}
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofRect(x, y, width, height);
		ofSetColor(ofColor::white);
	}
	if (showtext)
	{
		string Strvalue = name;
		jp_constants::p_font.drawString(Strvalue,
										x - jp_constants::p_font.stringWidth(Strvalue) / 2,
										y + jp_constants::p_font.stringHeight(Strvalue) / 2);
	}
	ofSetColor(255, 0, 0);
}
void JPToogle::update_movtype()
{
	if (useTexture)
	{
		cout << "TEXTURE INDEX " << textureindex << endl;
		cout << "parameters->movtype " << parameters->movtype << endl;
		if (textureindex == 0)
		{
			if (parameters->movtype == 0)
			{
				parameters->movtype = 1;
			}
			else if (parameters->movtype != 0)
			{
				parameters->movtype = 0;
			}
			parameters->needsUpdate = true;
		}
		else if (textureindex == 2)
		{
			// ES LA PUTA FLECHITA
			cout << "PUTA FLECHITA " << endl;
			if (parameters->movtype != 2)
			{
				parameters->movtype = 2;
			}
			else
			{
				parameters->movtype = 3;
			}
		}
		else
		{
			parameters->movtype = textureindex;
			// cout << "ANTERIOR" << parameters->movtype << endl;
			// cout << "SIGUIENTE" << parameters->movtype << endl;
		}
		// cout << "TEXTURE INDEX " << textureindex << endl;
		// cout << "parameters->movtype " << parameters->movtype << endl;
	}
}
