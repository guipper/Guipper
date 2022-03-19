#include "jp_complexslider.h"
JPHandler::JPHandler() {}
JPHandler::~JPHandler() {}

void JPHandler::setup(float _x, float _y, float _w, float _h)
{
	JPdragobject::setup(_x, _y, _w, _h);
	paleta = 1;
	useTexture = true;
	isLeft = true;
	activeFlag = false;
}
void JPHandler::draw()
{

	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	if (ofGetMousePressed() && mouseOver())
	{
		activeFlag = true;
	}

	if (mouseGrab())
	{
		ofSetColor(100, 50);
	}
	else if (mouseOver())
	{
		ofSetColor(20, 50);
	}
	else
	{
		ofSetColor(150, 50);
	}
	// ofSetColor(255, 0, 0);
	ofDrawRectangle(x, y,
					width, height);
	ofSetColor(255, 255);
	if (useTexture)
	{
		// ofSetColor(255, 255);
		ofSetColor(0, 255);

		float tsize = 20;
		float twidth = 0.75;
		if (isLeft)
		{
			float offsexX = 0;
			float offsetY = -5;

			float xx1 = x + offsexX - tsize * twidth;
			float yy1 = y + offsetY;

			float xx2 = x + offsexX;
			float yy2 = y + offsetY;

			float xx3 = x + offsexX;
			float yy3 = y + offsetY + tsize;

			ofDrawTriangle(xx1, yy1, xx2, yy2, xx3, yy3);
		}
		else
		{
			float offsexX = 0;
			float offsetY = -5;
			float xx1 = x + offsexX + tsize * twidth;
			float yy1 = y + offsetY;

			float xx2 = x + offsexX;
			float yy2 = y + offsetY;

			float xx3 = x + offsexX;
			float yy3 = y + offsetY + tsize;
			ofDrawTriangle(xx1, yy1, xx2, yy2, xx3, yy3);
		}
	}
	else
	{
		ofDrawRectangle(x, y,
						width, height);
	}
}

JPComplexSlider::JPComplexSlider() {}
JPComplexSlider::~JPComplexSlider() {}

void JPComplexSlider::setup(float _x, float _y, float _width, float _height, JPParameter *_parameters)
{
	parameters = _parameters;
	x = _x;
	y = _y;
	width = _width;
	height = _height;

	// parameters->min = 0.0;
	// parameters->max = 1.0;

	// min = parameters->min;
	// max = parameters->max;
	value = parameters->floatValue;
	name = parameters->name;
	speed = parameters->speed;
	// cout << "MOVTYPE" << parameters->movtype << endl;

	boton_collapse.movtype = parameters->movtype;
	slider_value.movtype = parameters->movtype;

	// cout << "CORRE ESTA MIERDA" << endl;
	//_parameters.saludar();

	boton_collapse.setParametersPointer(parameters);
	boton_direccion.setParametersPointer(parameters);
	slider_value.setParametersPointer(parameters);
	boton_idayvuelta.setParametersPointer(parameters);
	boton_random.setParametersPointer(parameters);
	slider_speed.setParametersPointer(parameters);

	activable2 = true;
	controllertype = COMPLEXSLIDER;
	testcol = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
	activeFlag = false;
	overboton_collapse = false;
	paleta = 0;
	setPosAndSize();

	useTexture = false;
	if (parameters->movtype == 0)
	{
		boton_collapse.boolValue = true;
	}
	else
	{
		boton_collapse.boolValue = false;
	}
	// cout << "SETUP JPPARAMETER" << endl;
}
float JPComplexSlider::getValue()
{
	return value;
}
void JPComplexSlider::draw()
{

	ofSetColor(255, 100);
	ofSetRectMode(OF_RECTMODE_CENTER);
	jp_constants_img::fondo_parametro.draw(x, y, width, height);

	// Dibujar cuadrito celeste :
	if (parameters->movtype != 0)
	{

		// ESTO TAL VEZ HABRIA QUE METERLO EN EL SLIDER VALUE !?=!?!?!?!?!?!?!?!
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(122, 108, 196);
		float xcuadraditoceleste = handler1.x;
		float cuadritoceleste_height = 20;
		ofDrawRectangle(xcuadraditoceleste,
						slider_value.y - cuadritoceleste_height,
						abs(handler1.x - handler2.x),
						cuadritoceleste_height);

		string Strvalue = ofToString(parameters->floatValue, 2);
		ofSetColor(255, 255);

		// DIBUJAR EL FONDO DEL VALOR:
		/*jp_constants_img::drawCenterImage(jp_constants_img::fondo_valor,
			x - jp_constants_img::fondo_valor.getWidth() * 0.25/2,
			y - jp_constants::p_font.stringHeight(Strvalue) - jp_constants_img::fondo_valor.getHeight() * 0.25 / 2,
		0.4);*/

		ofSetColor(0, 255);
		jp_constants::p_font.drawString(Strvalue,
										x - jp_constants::p_font.stringWidth(Strvalue),
										y - jp_constants::p_font.stringHeight(Strvalue));

		jp_constants::p_font.drawString(name,
										x - jp_constants::p_font.stringWidth(name) - 80,
										y - jp_constants::p_font.stringHeight(name));
	}
	slider_value.draw();
	boton_collapse.draw();

	// movtype = parameters->movtype;
	if (parameters->movtype != 0)
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		handler1.draw();
		handler2.draw();
		ofSetColor(0, 255);

		ofSetRectMode(OF_RECTMODE_CORNER);
		// slider_value.value = value;
		slider_speed.draw();
		speed = slider_speed.value;
		boton_idayvuelta.draw();
		boton_random.draw();
		boton_direccion.draw();
	}
}
void JPComplexSlider::update()
{
	overboton_collapse = boton_collapse.mouseOver();
	if (mouseOver())
	{
	}

	// UPDATE
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	if (mouseOver() && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
	}
	if (!activable2)
	{
		slider_speed.activable2 = false;
		boton_collapse.activable2 = false;
		boton_idayvuelta.activable2 = false;
		boton_random.activable2 = false;
		boton_direccion.activable2 = false;
		handler1.activeFlag = false;
		handler2.activeFlag = false;
	}
	slider_value.activable2 = activable2;
	if (activeFlag && activable2)
	{
		if (parameters->movtype != 0)
		{
			if (!slider_speed.activeFlag)
			{
				// slider_speed.activeFlag = false;
				if (handler1.activeFlag)
				{
					handler1.setPos(ofGetMouseX(), handler1.y);
					handler1.x = ofClamp(handler1.x,
										 slider_value.x - slider_value.width / 2 + handler1.width / 2,
										 handler2.x - handler1.width / 2);
					parameters->min = ofMap(handler1.x,
											slider_value.x - slider_value.width / 2 + handler1.width / 2,
											slider_value.x + slider_value.width / 2 - handler1.width / 2,
											0.0, 1.0);
				}
				else if (handler2.activeFlag)
				{
					handler2.setPos(ofGetMouseX(), handler2.y);
					handler2.x = ofClamp(handler2.x,
										 slider_value.x - slider_value.width / 2 + handler1.width / 2,
										 slider_value.x + slider_value.width / 2 - handler2.width / 2);
					parameters->max = ofMap(handler2.x,
											slider_value.x - slider_value.width / 2 + handler2.width / 2,
											slider_value.x + slider_value.width / 2 - handler2.width / 2,
											0.0, 1.0);
				}
			}
			else
			{
				slider_speed.activable2 = true;
			}
			if (!handler1.activeFlag && !handler2.activeFlag && !slider_speed.activeFlag)
			{
				boton_collapse.activable2 = true;
				boton_idayvuelta.activable2 = true;
				boton_random.activable2 = true;
				boton_direccion.activable2 = true;
			}
			else
			{
				boton_collapse.activable2 = false;
				boton_idayvuelta.activable2 = false;
				boton_random.activable2 = false;
				boton_direccion.activable2 = false;
			}
		}
		boton_collapse.activable2 = !slider_value.activeFlag;
	}
}
void JPComplexSlider::setPosAndSize()
{

	// Este es como el setup de todos los elementos :
	float b_cx = x - width / 2 + 20;
	boton_collapse.setup(b_cx,
						 y, 20, 20);

	if (parameters->movtype == 0)
	{
		float slidervaluewidth = width * 3 / 4;
		slider_value.setup(x, y,
						   slidervaluewidth,
						   height * 8 / 10,
						   0.0,
						   1.0,
						   value,
						   name);
	}
	else
	{
		float botonsepx = 35; // ESTA HABRIA QUE HACERLA VARIABLE GLOBAL EN OTRO LUGAR O COMO HACEMO ?!
		float slidervaluewidth = width * 2 / 4;

		float slidervaluex = boton_collapse.x +
							 slidervaluewidth / 2 +
							 boton_collapse.width * 0.75 +
							 botonsepx / 4;

		// slider_value.setPos(slidervaluex, y);
		// Esto lo hace con un rect mode center. Pero hay como que cambiarlo digamos
		slider_value.setup(slidervaluex,
						   y + height / 4,
						   slidervaluewidth,
						   height * 8 / 10,
						   0.0,
						   1.0,
						   value,
						   name);

		float sliderspeedw = 50;
		float sliderspeedh = 30;

		float pos = slidervaluex + slider_value.width / 2 + sliderspeedw / 2;

		slider_speed.setup(pos += botonsepx / 4,
						   y,
						   sliderspeedw,
						   sliderspeedh,
						   0.0,
						   1.0,
						   speed);

		float boton_siz = 15;

		float bt_siz_multiplyer1 = 1.0;
		float bt_siz_multiplyer2 = 1.0;
		float bt_siz_multiplyer3 = 1.0;

		boton_idayvuelta.setup(pos += sliderspeedw,
							   y,
							   jp_constants_img::idayvuelta.getWidth() * bt_siz_multiplyer1,
							   jp_constants_img::idayvuelta.getHeight() * bt_siz_multiplyer1);

		boton_random.setup(pos += botonsepx,
						   y,
						   jp_constants_img::ran.getWidth() * bt_siz_multiplyer2,
						   jp_constants_img::ran.getHeight() * bt_siz_multiplyer2);

		boton_direccion.setup(pos += botonsepx,
							  y,
							  jp_constants_img::una_direccion.getWidth() * bt_siz_multiplyer3,
							  jp_constants_img::una_direccion.getHeight() * bt_siz_multiplyer3);

		slider_speed.paleta = 1;
		controllertype = SLIDER;

		// speed = slider_speed.value;

		float handlerw = 20;
		float handlerh = slider_value.height;

		// parameters->min = ofClamp(parameters->min, 0.0, 1.0);
		//	parameters->max = ofClamp(parameters->max, 0.0, 1.0);
		// parameters->max = 1.0;

		float handler1_x = ofMap(parameters->min,
								 0.0, 1.0,
								 slider_value.x - slider_value.width / 2 + handlerw / 2,
								 slider_value.x + slider_value.width / 2 - handlerw / 2);

		float handler2_x = ofMap(parameters->max,
								 0.0, 1.0,
								 slider_value.x - slider_value.width / 2 + handlerw / 2,
								 slider_value.x + slider_value.width / 2 - handlerw / 2);

		handler1.setup(handler1_x,
					   y,
					   handlerw,
					   handlerh);

		handler2.setup(handler2_x,
					   y,
					   handlerw,
					   handlerh);
	}
	handler2.isLeft = false;
	boton_idayvuelta.setUseTexture(boton_idayvuelta.IDAYVUELTA);
	boton_random.setUseTexture(boton_idayvuelta.RAN);
	boton_direccion.setUseTexture(boton_idayvuelta.GODER);
	boton_collapse.setUseTexture(boton_collapse.COLLAPSE);
}
