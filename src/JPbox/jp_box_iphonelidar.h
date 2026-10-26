#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_kinect2.h"
#include "jp_depth_filter.h"
#include "../JPutils/jp_hand_tracking.h"

class IPhoneLidarCaptureSource;

// iPhone/iPad LiDAR through the Record3D app's USB streaming mode.
//
// The phone is a second metric depth device next to the Kinect v2: depth is
// published in millimetres with pinhole intrinsics in depth pixels, the same
// contract JPbox_kinect2 gives PointerCloud and the audience tracker, so they
// can read either device without converting.
//
// Differences that matter downstream: 256x192 depth (the Kinect gives
// 512x424) whose size is only known once frames arrive, a usable range of
// roughly 0.3-5 m, and a per-pixel confidence map. Published depth already
// drops LOW confidence pixels - they are the flying edges between foreground
// and background that smear a point cloud.
class JPbox_iphonelidar : public JPbox
{
public:
	enum Stream
	{
		COLOR = 0,
		DEPTH = 1,
		CONFIDENCE = 2
	};

	// Depth and the intrinsics that belong to it travel together: the size
	// can change between frames (another device, TrueDepth instead of LiDAR),
	// and a frame unprojected with the previous frame's numbers is wrong.
	struct DepthFrame
	{
		vector<float> millimetres;
		int width = 0;
		int height = 0;
		JPbox_kinect2::DepthIntrinsics intrinsics;
	};
	using DepthHandle = shared_ptr<const DepthFrame>;

	JPbox_iphonelidar();
	~JPbox_iphonelidar() override;

	void setup(string directory, string name) override;
	void update() override;
	void updateFBO() override;
	void draw() override;
	void clear() override;
	void setCaptureActive(bool active) override;
	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;
	vector<string> inspectorChoiceLabels() const override;
	int inspectorChoice() const override;
	void setInspectorChoice(int choice) override;
	string inspectorChoiceStatus() const override;

	Stream getStream() const;
	void setStream(Stream value);
	string getStreamLabel() const;
	string getCaptureStatus() const;
	void requestReconnect();
	static bool isDriverAvailable();
	static void shutdownSharedCapture();

	// --- Shared device tap, same shape as JPbox_kinect2's -----------------
	static shared_ptr<IPhoneLidarCaptureSource> acquireSharedCapture();
	// Null when nothing new since `version`.
	static DepthHandle acquireDepth(
		const shared_ptr<IPhoneLidarCaptureSource> &source, uint64_t &version);
	static string captureStatus(
		const shared_ptr<IPhoneLidarCaptureSource> &source);
	static void reconnectShared(
		const shared_ptr<IPhoneLidarCaptureSource> &source);
	static std::shared_ptr<void> audienceLease(
		const shared_ptr<IPhoneLidarCaptureSource> &source);
	static std::shared_ptr<const jp_hand::Frame> audienceFrame(
		const shared_ptr<IPhoneLidarCaptureSource> &source);

	// ARKit camera-to-world pose of the latest frame, as Record3D sends it.
	// serial 0 means none has arrived since the source opened.
	struct PoseSample
	{
		float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
		float tx = 0.0f, ty = 0.0f, tz = 0.0f;
		double captured = 0.0;
		uint64_t serial = 0;
	};
	static PoseSample latestPose(
		const shared_ptr<IPhoneLidarCaptureSource> &source);

	// Usable LiDAR range. Wider than the Kinect's 500-4500.
	static constexpr float kMinRangeMm = 200.0f;
	static constexpr float kMaxRangeMm = 5000.0f;

private:
	enum ParameterIndex
	{
		MIRROR,
		INVERT,
		STRETCH,
		NEAR_MM,
		FAR_MM,
		CLIP_DEPTH,
		FLIP_VERTICAL,
		GAMMA,
		// Appended: saved sessions address parameters by position.
		ROTATE_90,
		SMOOTH,
		FILL_HOLES,
		PRECISE
	};

	void updateSourceTexture();
	void drawSourceTexture();
	void refreshToneCurve();
	void updateColorLease();
	static constexpr int kToneCurveSize = 1024;

	Stream stream = DEPTH;
	shared_ptr<IPhoneLidarCaptureSource> capture;
	// Held only while COLOR is shown: JPEG decoding is the most expensive
	// thing the capture thread does and nothing else reads colour.
	std::shared_ptr<void> colorLease;
	ofTexture sourceTexture;
	ofPixels monoPixels;
	DepthHandle rawDepth;
	// The DEPTH picture is drawn from this, not from rawDepth directly; it is
	// what "smooth" and "fill holes" produce.
	jp_depth_filter::Filter depthFilter;
	vector<float> filteredDepth;
	int textureChannels = 0;
	unsigned char toneCurve[kToneCurveSize] = {0};
	float toneCurveGamma = -1.0f;
	bool toneCurveInvert = false;
	uint64_t settingsVersion = 1;
	uint64_t builtSettingsVersion = 0;
	uint64_t lastFrameVersion = 0;
	uint64_t builtFrameVersion = 0;
	int textureStream = -1;
	bool cleared = false;
};
