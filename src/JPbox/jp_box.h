#pragma once
#include "ofMain.h"
//#include "Shaderrender.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPgui/jp_toogle.h"

// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox : public JPdragobject
{
public:
	JPbox();
	~JPbox();

	bool isactiverender;

	float inlet_size;
	float outlet_x;
	float outlet_y;
	float outlet_size;

	float triangleangle;

	JPParameterGroup parameters;
	string dir;
	JPFbohandlerGroup fbohandlergroup;
	// Shaderrender shaderrender; //HOLDS THE RENDER OF THE SHADER

	ofFbo fbo;
	void reloadShaderonly(); // ESTA FUNCION LA VOY A PONER PARA DEBUGGEAR A VER.
	virtual void reload();
	void setup(ofTrueTypeFont &_font);
	virtual void setup(string _directory, string _name);
	virtual void update();
	virtual void draw();
	virtual void updateFBO();
	void draw_outlet();
	virtual void clear();
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
	}

	ofColor border;
	ofColor border_mouseover;
	ofColor border_grab;
	ofColor Cfront;
	string name;

	bool activeFlag;
	bool outletActiveFlag;
	bool mouseOverOutlet();

	enum type
	{
		SHADERBOX,
		IMAGEBOX,
		VIDEOBOX,
		CAMBOX,
		SPOUTBOX,
		PRESETBOX,
		FRAMEDIFFERENCEBOX,
		NDIBOX
	};

	int getTipo();
	time_t datemodified; // La pongo aca porque necesito que me recorra la cosa dentro del for. Aunque sea solo para los shaders.

	// Me perturba que el nombre de la variable sea tan verga.
	void setonoff(bool _val);
	bool getonoff();
	JPToogle onoff;

protected:
	int tipo; // Habra una manera menos cacuija de hacer esto? no se, pero ya me pudrio si, esta bien o mal me la chupa.

	float padding_top;
	float padding_leftright;
	float padding_bottom;

	float fbowidth;
	float fboheight;
};