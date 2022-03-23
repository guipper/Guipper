#pragma once
#include "ofMain.h"
#include "jp_controller.h"
#include "../JPutils/jp_constants.h"

class JPToogle : public JPcontroller
{
public:
	void setup(float _x, float _y, float _width, float _height, string _name, bool _boolValue);
	void setup(float _x, float _y, float _width, float _height);
	void setUseTexture(int _as);
	void drawSelectedTexture();
	void draw();

	void update_movtype(); // Esto es para poner dentro de una funcion directamente el trigger

	bool activable; // VARIABLE DE CONTROL

	int textureindex;

	// ESTO ES PARA EL TEXTUREINDEX
	// Collapse = Flechita para abrir el coso.
	enum type
	{
		COLLAPSE,
		IDAYVUELTA,
		GODER,
		GOIZQ,
		RAN
	};

private:
};