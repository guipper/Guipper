#pragma once

#include "ofMain.h"
#include "jp_box.h"

class KinectV2CaptureSource;

class JPbox_kinect2 : public JPbox
{
public:
	enum Stream
	{
		COLOR = 0,
		DEPTH = 1,
		INFRARED = 2
	};

	JPbox_kinect2();
	~JPbox_kinect2() override;

	void setup(string directory, string name) override;
	void update() override;
	void updateFBO() override;
	void draw() override;
	void clear() override;
	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;

	Stream getStream() const;
	void setStream(Stream value);
	string getStreamLabel() const;
	string getCaptureStatus() const;
	void requestReconnect();
	static bool isDriverAvailable();

	// --- Raw depth tap -------------------------------------------------
	// The box itself only ever publishes an 8-bit grey visualisation of the
	// depth stream. These give other boxes the untouched libfreenect2 data so
	// they can do real metric geometry instead of guessing from luminance.

	// Pinhole parameters of the IR/depth camera, straight from the device.
	struct DepthIntrinsics
	{
		float fx = 0.0f;
		float fy = 0.0f;
		float cx = 0.0f;
		float cy = 0.0f;
		bool valid = false;
	};

	// Frames are published as immutable snapshots and handed out by refcount.
	// Copying them per consumer per frame meant an 8 MB memcpy for colour
	// alone, multiplied by every box reading the device.
	using ColorFrame = shared_ptr<const vector<unsigned char>>;
	using MonoFrame = shared_ptr<const vector<float>>;

	// Opens the shared device if nobody has yet, and keeps it alive for as
	// long as the returned handle is held. Consumers must store it.
	static shared_ptr<KinectV2CaptureSource> acquireSharedCapture();

	// Raw depth in millimetres, depthWidth() x depthHeight(), exactly as
	// captured. Null when the frame version is unchanged or nothing streams.
	static MonoFrame acquireRawDepth(
		const shared_ptr<KinectV2CaptureSource> &source, uint64_t &version);
	static DepthIntrinsics depthIntrinsics(
		const shared_ptr<KinectV2CaptureSource> &source);
	static string captureStatus(
		const shared_ptr<KinectV2CaptureSource> &source);
	static int depthWidth();
	static int depthHeight();

private:
	void updateSourceTexture();
	void drawSourceTexture();
	// The tone curve folds invert and gamma into a table so the per pixel
	// std::pow disappears from the conversion loop.
	void refreshToneCurve();
	static constexpr int kToneCurveSize = 1024;

	Stream stream = COLOR;
	shared_ptr<KinectV2CaptureSource> capture;
	ofTexture sourceTexture;
	ofPixels monoPixels;
	ColorFrame rawColor;
	MonoFrame rawMono;
	unsigned char toneCurve[kToneCurveSize] = {0};
	float toneCurveGamma = -1.0f;
	bool toneCurveInvert = false;
	// Bumped whenever a setting that affects the grey mapping changes, so the
	// picture also refreshes between device frames instead of looking stuck.
	uint64_t settingsVersion = 1;
	uint64_t builtSettingsVersion = 0;
	uint64_t lastFrameVersion = 0;
	uint64_t builtFrameVersion = 0;
	int colorFormat = 0;
	int textureStream = -1;
	bool cleared = false;
};
