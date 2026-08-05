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

private:
	void updateSourceTexture();
	void drawSourceTexture();

	Stream stream = COLOR;
	shared_ptr<KinectV2CaptureSource> capture;
	ofTexture sourceTexture;
	ofPixels colorPixels;
	ofPixels monoPixels;
	vector<unsigned char> rawColor;
	vector<float> rawMono;
	uint64_t lastFrameVersion = 0;
	int colorFormat = 0;
	bool cleared = false;
};
