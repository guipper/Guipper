#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_media_state.h"
#include "defines.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
//#include "Shaderrender.h"

// One open grabber per physical device, shared by every box that wants it.
//
// Defined here rather than inside jp_box_cam.cpp because JPbox_camdepth needs
// the same camera: two ofVideoGrabbers on one /dev/video do not coexist, so the
// refcounted map below is the only correct way for two box types to show the
// same camera at once.
class JPCameraCaptureSource
{
public:
	JPCameraCaptureSource(int deviceId, int width, int height);
	~JPCameraCaptureSource();

	// Idempotent per frame: every box holding this source may call it, only the
	// first one in a given frame actually pumps the grabber.
	void updateOnce();
	bool isInitialized() const;
	void draw(float x, float y, float width, float height) const;
	// For boxes that feed the frame to a shader rather than blitting it.
	const ofTexture &getTexture() const;
	bool hasTexture() const;

private:
	int deviceId = -1;
	ofVideoGrabber grabber;
	bool initialized = false;
	uint64_t lastUpdateFrame;
};

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_cam : public JPbox
{
public:

	// Index of the uniform-zoom parameter. Resolved once in setup rather than
	// hardcoded, since it is appended after a different number of parameters in
	// each of these boxes.
	int scaleRatioIndex = -1;
	JPbox_cam(); // constructor declared
	~JPbox_cam();

	void setup(string _dir, string _name);
	int camsize;

	// Re-enumerates capture devices so a camera plugged in after startup shows
	// up. Static and generation counted: every camera box picks the new list up
	// on its next update, not just the one whose button was clicked.
	static void rescanCameraDevices();

	// Shared camera access for other box types. JPbox_camdepth uses these so it
	// can show the same device as a CAMARITA box without fighting it for the
	// hardware.
	static std::shared_ptr<JPCameraCaptureSource> acquireSharedCamera(
		int deviceId, int width, int height);
	static vector<int> availableCameraIds();
	static string cameraLabel(int deviceId);
	static uint64_t cameraRescanCount();

	// Which /dev/video devices currently have a live capture source, and how
	// many boxes are sharing each one. Exposed for the debug panel: the pool is
	// refcounted and file-local, so from outside there is no way to tell whether
	// a black camera box means "device busy" or "graph not wired".
	//
	// pair is {deviceId, users}. Only live entries are reported; expired weak
	// references are skipped rather than shown as zero-user devices.
	static vector<pair<int, int>> openCameraSources();

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
	uint64_t appliedRescanGeneration = 0;
	int camWidth = 640;
	int camHeight = 480;
	std::shared_ptr<JPCameraCaptureSource> cameraSource;
};
