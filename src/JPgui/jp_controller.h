#pragma once
#include "ofMain.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPutils/jp_parametergroup.h"
// Esta clase es para manejar los sliders, botones, etc etc. TODO LO QUE SEA GUI

class JPcontroller : public JPdragobject
{
public:
	JPcontroller();
	~JPcontroller();

	float value;

	bool boolValue;
	bool showtext;
	bool useTexture;

	bool activable2; // ESTA A DIFERENCIA DE LA DE 1 CHECKEA SI ES QUE NO HAY OTRO ITEM AGARRADO.

	string name;
	ofTrueTypeFont *font_p;
	JPParameter *parameters;
	void setParametersPointer(JPParameter *_parameters)
	{
		parameters = _parameters;
	}

	// ESTO ES PARA PODER CAZARLO DESPUES :
	enum controllerTypes
	{
		TOOGLE,
		SLIDER,
		TOOGLELIST,
		COMPLEXSLIDER
	};
	int controllertype;
	void setFontPointer(ofTrueTypeFont &_font)
	{
		font_p = &_font;
	}
	int paleta;

	// LO PONGO ACA POR QUE A PESAR DE QUE SOLO LO UTILIZA EL COMPLEXSLIDER Y NO EL TOOGLE.
	// LA UNICA MANERA PARA PODER ACCEDER DESDE EL ARRAY POLIMORFICO ES QUE EL OBJETO ESTE DECLARADO EN AMBAS CLASES.
	// HAY UNA MANERA MAS EFICIENTE Y CORRECTA DE HACER ESTO ?!
	// YO NO LO CONOZCO CUANDO LA SEPA LO CAMBIAMOS.
	// ESTOS LOS PONGO ACA PARA QUE LOS PUEDA OBTENER EN EL FOR . PERO SOLO SON DEL COMPLEX SLIDER. NO SABRIA COMO HACER.
	int movtype;
	bool overboton_collapse;
	float speed;
	float min;
	float max;

	virtual void draw();
	virtual void update();
	virtual float getValue();
};