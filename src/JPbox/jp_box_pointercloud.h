#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_kinect2.h"

// PointerCloud.
//
// Renders a true metric point cloud: it taps the shared Kinect v2 capture for
// raw depth in millimetres and unprojects it through the IR camera intrinsics
// into camera-space XYZ, depth tested. It deliberately does not read geometry
// from an inlet - an inlet only ever carries the 8 bit visualisation, which is
// already clipped, gamma'd and quantised.
//
// The optional "color" inlet paints the points from any other box.
class JPbox_pointercloud : public JPbox
{
public:
	JPbox_pointercloud() = default;
	~JPbox_pointercloud() override;

	void setup(string directory, string name) override;
	void update() override;
	void updateFBO() override;
	void draw() override;
	void clear() override;
	void setPos(float x, float y) override;

	string getCaptureStatus() const;

private:
	// Inspector order. Everything must stay within the first 16 rows or it
	// falls off the end of what MIDI can bind.
	enum ParameterIndex
	{
		POINT_SIZE,
		DENSITY,
		DEPTH_SCALE,
		NEAR_MM,
		FAR_MM,
		ROTATE_X,
		ROTATE_Y,
		ROTATE_Z,
		ZOOM,
		FIELD_OF_VIEW,
		COLOR_CYCLE,
		BRIGHTNESS,
		TRAILS,
		ADDITIVE,
		MIRROR,
		FLIP_VERTICAL,
		PARAMETER_COUNT
	};
	enum InletIndex
	{
		COLOR_INLET
	};

	void ensureRenderTarget();
	void buildLattice();
	void uploadDepth();
	void setInletPosition();

	shared_ptr<KinectV2CaptureSource> capture;
	ofShader shader;
	ofVboMesh lattice;
	ofTexture depthTexture;
	JPbox_kinect2::DepthIntrinsics intrinsics;
	uint64_t lastFrameVersion = 0;
	int targetWidth = 0;
	int targetHeight = 0;
	bool cleared = false;
};
