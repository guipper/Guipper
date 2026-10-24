#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_kinect2.h"
#include "jp_box_iphonelidar.h"

// PointerCloud.
//
// Renders a true metric point cloud: it taps the shared capture of a depth
// device - Kinect v2 or an iPhone LiDAR through Record3D - for raw depth in
// millimetres and unprojects it through that device's depth intrinsics into
// camera-space XYZ, depth tested. It deliberately does not read geometry
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
	void setCaptureActive(bool active) override;
	void setPos(float x, float y) override;
	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;
	vector<string> inspectorChoiceLabels() const override;
	int inspectorChoice() const override;
	void setInspectorChoice(int choice) override;
	string inspectorChoiceStatus() const override;

	// Saved as an int: append only. Prefixed: KINECT2 and IPHONE_LIDAR are
	// build defines.
	enum Device
	{
		DEVICE_KINECT2 = 0,
		DEVICE_IPHONE_LIDAR = 1
	};
	Device getDevice() const;
	void setDevice(Device value);

	string getCaptureStatus() const;

private:
	// Inspector order. The first 16 rows are what MIDI can bind; DETAIL is a
	// setup choice, not something to play, so it may sit past them.
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
		DETAIL,
		PARAMETER_COUNT
	};
	enum InletIndex
	{
		COLOR_INLET
	};

	void ensureRenderTarget();
	void buildLattice(int width, int height);
	int currentUpsample();
	void uploadDepth();
	void acquireDevice();
	void releaseDevice();
	bool uploadMillimetres(const float *millimetres, int width, int height);
	void setInletPosition();

	Device device = DEVICE_KINECT2;
	// Only the selected device's handle is held, so the other one can close.
	shared_ptr<KinectV2CaptureSource> capture;
	shared_ptr<IPhoneLidarCaptureSource> phone;
	ofShader shader;
	ofVboMesh lattice;
	ofTexture depthTexture;
	JPbox_kinect2::DepthIntrinsics intrinsics;
	uint64_t lastFrameVersion = 0;
	int latticeWidth = 0;
	int latticeHeight = 0;
	int latticeUpsample = 0;
	int targetWidth = 0;
	int targetHeight = 0;
	bool captureEnabled = false;
	bool cleared = false;
};
