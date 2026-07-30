#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "defines.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
//#include "Shaderrender.h"

class JPCameraCaptureSource;

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_cam : public JPbox
{
public:
	JPbox_cam(); // constructor declared
	~JPbox_cam();

	void setup(string _dir, string _name);
	int camsize;

	// METODOS HEREDADOS :
	// void reload();
	void setup(ofTrueTypeFont &_font);
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
	void refreshCameraDevices();
	void applyCameraIndexFromParameter(bool force = false);
	void releaseCameraSource();

	vector<int> availableDeviceIds;
	int currentCameraListIndex = -1;
	int currentDeviceId = -1;
	int camWidth = 640;
	int camHeight = 480;
	std::shared_ptr<JPCameraCaptureSource> cameraSource;
};
