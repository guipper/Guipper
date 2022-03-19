#pragma once
#include "ofMain.h"
#include "jp_controller.h"
#include "../JPutils/jp_constants.h"

// ESTE ES UN TOOGLE QUE SIRVE PARA CONTROLAR QUE TIPO DE MOVIMIENTO TIENE EL SLIDER AUTOMATIZADO :
class JPTooglelist : public JPcontroller
{
public:
	void setup(float _x, float _y, float _width, float _height);
	void draw();
	int movtype;
	bool activable; // VARIABLE DE CONTROL
private:
};