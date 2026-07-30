#include "jp_toogle.h"

namespace
{
	void drawAutomationButton(float x, float y, float width, float height,
		bool active, bool hovered)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(active ? ofColor(COL_ACCENT_CYAN_DARK, 210) :
			(hovered ? ofColor(COL_BG_HOVER, 235) :
				ofColor(COL_BG_INPUT, 210)));
		ofDrawRectRounded(
			x - width / 2.0f,
			y - height / 2.0f,
			width,
			height,
			3.0f);

		ofNoFill();
		ofSetLineWidth(1.0f);
		ofSetColor(active ? ofColor(COL_ACCENT_CYAN, 220) :
			(hovered ? ofColor(COL_TEXT_SECONDARY, 210) :
				ofColor(COL_BORDER_MUTED, 155)));
		ofDrawRectRounded(
			x - width / 2.0f,
			y - height / 2.0f,
			width,
			height,
			3.0f);
		ofFill();
	}

	void drawArrowHead(float tipX, float tipY, float direction)
	{
		const float size = 4.0f;
		ofDrawLine(tipX, tipY,
			tipX - direction * size, tipY - size);
		ofDrawLine(tipX, tipY,
			tipX - direction * size, tipY + size);
	}
}

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
	activeFlag = false;
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
	const bool hovered = mouseOver();
	if (textureindex == COLLAPSE)
	{
		const bool automated = parameters->movtype != JPParameter::STANDART;
		drawAutomationButton(x, y, width, height, automated, hovered);
		ofSetColor(automated ? COL_ACCENT_CYAN :
			(hovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		ofSetLineWidth(1.8f);
		if (automated)
		{
			ofDrawLine(x - 4.0f, y - 2.0f, x, y + 2.0f);
			ofDrawLine(x, y + 2.0f, x + 4.0f, y - 2.0f);
		}
		else
		{
			ofDrawLine(x - 2.0f, y - 4.0f, x + 2.0f, y);
			ofDrawLine(x + 2.0f, y, x - 2.0f, y + 4.0f);
		}
		ofSetLineWidth(1.0f);
	}
	else
	{
		const bool active =
			(textureindex == IDAYVUELTA &&
				parameters->movtype == JPParameter::OSC) ||
			(textureindex == RAN &&
				parameters->movtype == JPParameter::RANDOM) ||
			(textureindex == GODER &&
				(parameters->movtype == JPParameter::GODER ||
				 parameters->movtype == JPParameter::GOIZQ)) ||
			(textureindex == BPM_SYNC &&
				parameters->movtype == JPParameter::BPM);
		drawAutomationButton(x, y, width, height, active, hovered);
		ofSetColor(active ? COL_ACCENT_CYAN :
			(hovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		ofSetLineWidth(1.7f);

		if (textureindex == BPM_SYNC)
		{
			const float left = x - 8.0f;
			const float right = x + 8.0f;
			ofBeginShape();
			ofVertex(left, y);
			ofVertex(x - 4.0f, y);
			ofVertex(x - 1.5f, y - 6.0f);
			ofVertex(x + 1.5f, y + 6.0f);
			ofVertex(x + 4.0f, y);
			ofVertex(right, y);
			ofEndShape(false);
		}
		else if (textureindex == RAN)
		{
			const float left = x - 7.0f;
			const float right = x + 7.0f;
			ofDrawLine(left, y - 5.0f, x - 2.0f, y - 5.0f);
			ofDrawLine(x - 2.0f, y - 5.0f, x + 2.0f, y + 5.0f);
			ofDrawLine(x + 2.0f, y + 5.0f, right, y + 5.0f);
			drawArrowHead(right, y + 5.0f, 1.0f);
			ofDrawLine(left, y + 5.0f, x - 2.0f, y + 5.0f);
			ofDrawLine(x - 2.0f, y + 5.0f, x, y + 2.0f);
			ofDrawLine(x, y - 2.0f, x + 2.0f, y - 5.0f);
			ofDrawLine(x + 2.0f, y - 5.0f, right, y - 5.0f);
			drawArrowHead(right, y - 5.0f, 1.0f);
		}
		else if (textureindex == GODER)
		{
			const bool reverse =
				parameters->movtype == JPParameter::GOIZQ;
			const float direction = reverse ? -1.0f : 1.0f;
			const float startX = x - direction * 7.0f;
			const float endX = x + direction * 7.0f;
			ofDrawLine(startX, y, endX, y);
			drawArrowHead(endX, y, direction);
		}
		else if (textureindex == IDAYVUELTA)
		{
			const float left = x - 8.0f;
			const float right = x + 8.0f;
			ofDrawLine(left, y, right, y);
			drawArrowHead(left, y, -1.0f);
			drawArrowHead(right, y, 1.0f);
		}
		ofSetLineWidth(1.0f);
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
		if (!useTexture && parameters != nullptr && parameters->variabletype == JPParameter::BOOL)
		{
			parameters->boolValue = boolValue;
		}
		ofSetColor(jp_constants::Cactive[paleta]);
		// cout << "TRIGGER" << endl ;
	}

	if (useTexture)
	{
		drawSelectedTexture();
	}
	else
	{
		// Semantic two-state color: ON = green (live), OFF = dim red.
		if (boolValue)
		{
			ofSetColor(COL_ACCENT_GREEN);
		}
		else
		{
			ofSetColor(COL_ACCENT_RED_DIM);
		}
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofRect(x, y, width, height);
	}
	if (showtext)
	{
		string Strvalue = name;
		ofSetColor(boolValue ? ofColor(0) : ofColor(255));
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
