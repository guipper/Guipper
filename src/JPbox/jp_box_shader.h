#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_shader : public JPbox
{
public:
	JPbox_shader(); // constructor declared
	~JPbox_shader();

	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	int frameNum;
	void reload();
	void reloadShaderonly();
	void setup2(string _dir, string _nombre);
	void setup(string _dir, string _nombre);
	void setup(ofTrueTypeFont &_font,
			   string dir,
			   string _nombre);
	void update();
	void draw();
	void updateFBO();
	void draw_outlet();
	void clear();
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
		setfbohandler_nodepos();
	}

	// METODOS Y VARIABLES PROPIAS DE LA CLASE :
	void setfbohandler_nodepos();
	void update_NonglobalUniforms();
	void update_globalUniforms(); // GLOBAL UNIFORMS
	// JPParameterGroup getUniformsToJPParameterGroup(string _dir, string _name);
	void setUniforms(JPParameterGroup &_parameters, JPFbohandlerGroup &_fbohandlergroup, string _dir, string _name);
	// ofFbo fbo;
	ofShader shader;

private:
	bool hasMoreThan1Param = false;
};
