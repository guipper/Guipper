#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_slider.h"
#include "jp_knob.h"
#include "jp_toogle.h"
#include "jp_tooglelist.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_dragobject.h"

class JPHandler : public JPdragobject
{
public:
	JPHandler();
	~JPHandler();

	void setup(float _x, float _y, float _w, float _h);
	void draw();
	int paleta;
	bool useTexture;
	bool isLeft; // esta es solo para saber que imagen poner
};

class JPComplexSlider : public JPcontroller
{
public:
	JPComplexSlider();
	~JPComplexSlider();
	// JPTooglelist boton_collapse; //LO PONGO ACA PORQUE SI NO , NO ME DEJA OBTENERLO EN EL FOR.
	JPToogle boton_collapse;
	JPToogle boton_idayvuelta;
	JPToogle boton_random;
	JPToogle boton_direccion;

	JPKnob slider_speed;
	JPSlider slider_value;
	ofColor testcol;

	void setup(float _x,
			   float _y,
			   float _width,
			   float _height,
			   JPParameter *_parameters);

	float getValue();

	void draw();
	void update();
	// void setMoveType(int _movtype);

	// void setPos(float _x, float _y);
	void setPosAndSize();
	JPHandler handler1, handler2;
};
