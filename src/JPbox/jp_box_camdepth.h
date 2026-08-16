#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "defines.h"
#include "jp_box_cam.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"

// A camera that outputs a pseudo-depth map instead of its picture.
//
// NOT a depth sensor: it reads monocular cues (local detail, brightness,
// vertical position) out of a 2D frame, so a dark object close to the lens
// reads as far. Anything needing real metric geometry - the point cloud - must
// keep using the Kinect, which publishes millimetres.
//
// Shares the camera with CAMARITA through JPbox_cam::acquireSharedCamera: two
// ofVideoGrabbers cannot open one /dev/video, so pointing this box and a camera
// box at the same device has to go through the one refcounted source.
//
// Output matches JPbox_kinect2's grey convention (near = bright), which is what
// makes this and a Kinect DEPTH box interchangeable upstream of a displacement
// shader.
class JPbox_camdepth : public JPbox
{
public:
	JPbox_camdepth();
	~JPbox_camdepth();

	void setup(string _dir, string _name);
	void update();
	void updateFBO();
	void draw();
	void clear();

	// Parameter indices, resolved by name in setup rather than hardcoded at the
	// call sites - the array order is then free to change.
	int cameraIndexParam = -1;
	int focusWeightParam = -1;
	int brightWeightParam = -1;
	int verticalWeightParam = -1;
	int radiusParam = -1;
	int contrastParam = -1;
	int nearParam = -1;
	int farParam = -1;
	int smoothParam = -1;
	int edgeParam = -1;
	int curveParam = -1;
	int invertParam = -1;
	int floorParam = -1;
	int mirrorParam = -1;

private:
	void applyCameraIndexFromParameter(bool force = false);
	bool ensureShader();
	void ensureHistory();

	ofShader shader;
	// Previous output, for the temporal smoothing the shader does. A separate
	// target because a pass cannot read the FBO it is writing to.
	ofFbo history;
	std::shared_ptr<JPCameraCaptureSource> cameraSource;
	vector<int> availableDeviceIds;
	int currentDeviceId = -1;
	int currentCameraListIndex = -1;
	uint64_t appliedRescanGeneration = 0;
	int camWidth = 640;
	int camHeight = 480;
};
