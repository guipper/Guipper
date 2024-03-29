#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "defines.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_image : public JPbox
{
public:
	JPbox_image(); // constructor declared
	~JPbox_image();

	ofImage img;
	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	void reload();
	void setup(string _dir, string _nombre);
	void update();
	void updateFBO();
	void draw();
	void clear();
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
		// setfbohandler_nodepos();
	}

	// METODOS Y VARIABLES PROPIAS DE LA CLASE :
	// void setfbohandler_nodepos();
	// void update_NonglobalUniforms();
	// void update_globalUniforms();//GLOBAL UNIFORMS
	// JPParameterGroup getUniformsToJPParameterGroup(string _dir, string _name);
	// void setUniforms(JPParameterGroup & _parameters, JPFbohandlerGroup & _fbohandlergroup, string _dir, string _name);
	// ofFbo fbo;
	// ofShader shader;
private:
	float lasttime_autoreload;
	float duration_autoreload;
};
