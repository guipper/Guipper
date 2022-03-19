#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_video : public JPbox
{
public:
	JPbox_video(); // constructor declared
	~JPbox_video();

	ofVideoPlayer movie;
	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	// void reload();
	void setup(ofTrueTypeFont &_font,
			   string _dir,
			   string _nombre);
	void setup(string _dir, string _nombre);
	void update();
	void updateFBO();
	void draw();
	void clear();
	/*void setPos(float _x, float _y) {
		JPdragobject::setPos(_x, _y);
		//setfbohandler_nodepos();
	}*/
};
