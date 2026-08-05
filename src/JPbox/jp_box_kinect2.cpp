#include "jp_box_kinect2.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

using std::atomic;
using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::unique_lock;

#ifdef KINECT2
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/packet_pipeline.h>
#endif

namespace
{
	constexpr int kColorWidth = 1920;
	constexpr int kColorHeight = 1080;
	constexpr int kDepthWidth = 512;
	constexpr int kDepthHeight = 424;
	constexpr uint64_t kRetryMillis = 2000;
	constexpr int kFrameWaitMillis = 500;
	constexpr int kFrameTimeoutCount = 20;

	mutex sharedCaptureMutex;
	weak_ptr<KinectV2CaptureSource> sharedCapture;
}

class KinectV2CaptureSource
{
public:
	KinectV2CaptureSource()
	{
#ifdef KINECT2
		worker = thread(&KinectV2CaptureSource::run, this);
#else
		setStatus("INSTALL LIBFREENECT2");
#endif
	}

	~KinectV2CaptureSource()
	{
		stopRequested = true;
		reconnectRequested = true;
		wake.notify_all();
		if (worker.joinable()) worker.join();
	}

	static shared_ptr<KinectV2CaptureSource> acquire()
	{
		lock_guard<mutex> lock(sharedCaptureMutex);
		shared_ptr<KinectV2CaptureSource> source = sharedCapture.lock();
		if (!source)
		{
			source = make_shared<KinectV2CaptureSource>();
			sharedCapture = source;
		}
		return source;
	}

	void reconnect()
	{
		reconnectRequested = true;
		wake.notify_all();
	}

	string status() const
	{
		lock_guard<mutex> lock(dataMutex);
		return statusText;
	}

	bool copyColorFrame(vector<unsigned char> &color, int &format,
		uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return false;
		color = colorFrame;
		format = colorFrameFormat;
		version = frameVersion;
		return true;
	}

	bool copyMonoFrame(bool infrared, vector<float> &pixels,
		uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return false;
		pixels = infrared ? infraredFrame : depthFrame;
		version = frameVersion;
		return true;
	}

private:
	void setStatus(const string &value)
	{
		lock_guard<mutex> lock(dataMutex);
		statusText = value;
	}

#ifdef KINECT2
	void run()
	{
		while (!stopRequested)
		{
			setStatus("CONNECTING");
			libfreenect2::Freenect2 context;
			if (context.enumerateDevices() == 0)
			{
				setStatus("KINECT NOT FOUND");
				waitBeforeRetry();
				continue;
			}

			const string serial = context.getDefaultDeviceSerialNumber();
			// libfreenect2 owns and deletes the pipeline on every open path.
			auto *pipeline = new libfreenect2::CpuPacketPipeline();
			libfreenect2::Freenect2Device *device =
				context.openDevice(serial, pipeline);
			if (device == nullptr)
			{
				setStatus("OPEN FAILED / CHECK USB PERMISSION");
				waitBeforeRetry();
				continue;
			}

			libfreenect2::SyncMultiFrameListener listener(
				libfreenect2::Frame::Color |
				libfreenect2::Frame::Depth |
				libfreenect2::Frame::Ir);
			device->setColorFrameListener(&listener);
			device->setIrAndDepthFrameListener(&listener);
			if (!device->start())
			{
				setStatus("STREAM START FAILED");
				device->close();
				waitBeforeRetry();
				continue;
			}

			setStatus("STREAMING");
			reconnectRequested = false;
			int consecutiveTimeouts = 0;
			while (!stopRequested && !reconnectRequested)
			{
				libfreenect2::FrameMap frames;
				if (!listener.waitForNewFrame(frames, kFrameWaitMillis))
				{
					if (++consecutiveTimeouts >= kFrameTimeoutCount)
					{
						setStatus("FRAME TIMEOUT");
						break;
					}
					continue;
				}
				consecutiveTimeouts = 0;
				libfreenect2::Frame *color = frames[libfreenect2::Frame::Color];
				libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];
				libfreenect2::Frame *infrared = frames[libfreenect2::Frame::Ir];
				if (color != nullptr && depth != nullptr && infrared != nullptr)
				{
					vector<unsigned char> nextColor(
						color->data,
						color->data + color->width * color->height * 4);
					const float *depthData =
						reinterpret_cast<const float *>(depth->data);
					const float *irData =
						reinterpret_cast<const float *>(infrared->data);
					vector<float> nextDepth(depthData,
						depthData + depth->width * depth->height);
					vector<float> nextInfrared(irData,
						irData + infrared->width * infrared->height);
					{
						lock_guard<mutex> lock(dataMutex);
						colorFrame.swap(nextColor);
						depthFrame.swap(nextDepth);
						infraredFrame.swap(nextInfrared);
						colorFrameFormat = (int)color->format;
						++frameVersion;
						statusText = "STREAMING";
					}
				}
				listener.release(frames);
			}

			device->stop();
			device->close();
			if (!stopRequested) waitBeforeRetry();
		}
	}

	void waitBeforeRetry()
	{
		unique_lock<mutex> lock(waitMutex);
		wake.wait_for(lock, std::chrono::milliseconds(kRetryMillis), [this]() {
			return stopRequested.load() || reconnectRequested.load();
		});
		reconnectRequested = false;
	}
#endif

	mutable mutex dataMutex;
	mutex waitMutex;
	condition_variable wake;
	thread worker;
	atomic<bool> stopRequested{false};
	atomic<bool> reconnectRequested{false};
	string statusText = "STARTING";
	vector<unsigned char> colorFrame;
	vector<float> depthFrame;
	vector<float> infraredFrame;
	int colorFrameFormat = 0;
	uint64_t frameVersion = 0;
};

JPbox_kinect2::JPbox_kinect2() = default;

JPbox_kinect2::~JPbox_kinect2()
{
	clear();
}

void JPbox_kinect2::setup(string directory, string boxName)
{
	JPbox::setup(directory, boxName);
	dir = "kinect2";
	name = boxName;
	tipo = KINECT2BOX;
	parameters.addBoolValue(false, "mirror");
	parameters.addBoolValue(false, "invert");
	parameters.addBoolValue(false, "stretch");
	parameters.addFloatValue(500.0f, "near mm");
	parameters.setMin(500.0f, 3);
	parameters.setMax(4499.0f, 3);
	parameters.addFloatValue(4500.0f, "far mm");
	parameters.setMin(501.0f, 4);
	parameters.setMax(4500.0f, 4);
	parameters.addFloatValue(1.0f, "IR gain");
	parameters.setMin(0.1f, 5);
	parameters.setMax(8.0f, 5);
	parameters.addBoolValue(true, "clip depth");
	parameters.addBoolValue(false, "flip vertical");
	parameters.addFloatValue(1.0f, "depth/IR gamma");
	parameters.setMin(0.1f, 8);
	parameters.setMax(4.0f, 8);
	capture = KinectV2CaptureSource::acquire();
	cleared = false;
}

void JPbox_kinect2::update()
{
	JPbox::update();
	const float nearMm = std::round(ofClamp(
		parameters.getFloatValue(3), 500.0f, 4499.0f));
	const float farMm = std::round(ofClamp(
		parameters.getFloatValue(4), nearMm + 1.0f, 4500.0f));
	const float irGain = ofClamp(parameters.getFloatValue(5), 0.1f, 8.0f);
	const float gamma = ofClamp(parameters.getFloatValue(8), 0.1f, 4.0f);
	parameters.setFloatValue(nearMm, 3);
	parameters.setFloatLerpValue(nearMm, 3);
	parameters.setFloatValue(farMm, 4);
	parameters.setFloatLerpValue(farMm, 4);
	parameters.setFloatValue(irGain, 5);
	parameters.setFloatLerpValue(irGain, 5);
	parameters.setFloatValue(gamma, 8);
	parameters.setFloatLerpValue(gamma, 8);
	updateSourceTexture();
	updateFBO();
}

void JPbox_kinect2::updateSourceTexture()
{
	if (!capture) return;
	const bool newFrame = stream == COLOR ?
		capture->copyColorFrame(rawColor, colorFormat, lastFrameVersion) :
		capture->copyMonoFrame(stream == INFRARED, rawMono, lastFrameVersion);
	if (!newFrame) return;

	if (stream == COLOR)
	{
		if (rawColor.size() != (size_t)kColorWidth * kColorHeight * 4) return;
		colorPixels.allocate(kColorWidth, kColorHeight, OF_PIXELS_RGBA);
		for (size_t i = 0; i < (size_t)kColorWidth * kColorHeight; ++i)
		{
			const size_t p = i * 4;
#ifdef KINECT2
			if (colorFormat == (int)libfreenect2::Frame::BGRX)
			{
				colorPixels[p] = rawColor[p + 2];
				colorPixels[p + 1] = rawColor[p + 1];
				colorPixels[p + 2] = rawColor[p];
			}
			else
#endif
			{
				colorPixels[p] = rawColor[p];
				colorPixels[p + 1] = rawColor[p + 1];
				colorPixels[p + 2] = rawColor[p + 2];
			}
			colorPixels[p + 3] = 255;
		}
		sourceTexture.loadData(colorPixels);
		return;
	}

	const vector<float> &source = rawMono;
	if (source.size() != (size_t)kDepthWidth * kDepthHeight) return;
	monoPixels.allocate(kDepthWidth, kDepthHeight, OF_PIXELS_GRAY);
	const bool invert = parameters.getBoolValue(1);
	const float gamma = parameters.getFloatValue(8);
	if (stream == DEPTH)
	{
		const float nearMm = parameters.getFloatValue(3);
		const float farMm = std::max(nearMm + 1.0f,
			parameters.getFloatValue(4));
		const bool clipDepth = parameters.getBoolValue(6);
		for (size_t i = 0; i < source.size(); ++i)
		{
			const float mm = source[i];
			if (!std::isfinite(mm) || mm <= 0.0f)
			{
				monoPixels[i] = 0;
				continue;
			}
			if (clipDepth && (mm < nearMm || mm > farMm))
			{
				monoPixels[i] = 0;
				continue;
			}
			float value = 1.0f - ofClamp((mm - nearMm) / (farMm - nearMm), 0.0f, 1.0f);
			if (invert) value = 1.0f - value;
			value = std::pow(value, 1.0f / gamma);
			monoPixels[i] = (unsigned char)std::lround(value * 255.0f);
		}
	}
	else
	{
		const float gain = parameters.getFloatValue(5);
		for (size_t i = 0; i < source.size(); ++i)
		{
			float value = ofClamp(source[i] / 65535.0f * gain, 0.0f, 1.0f);
			if (invert) value = 1.0f - value;
			value = std::pow(value, 1.0f / gamma);
			monoPixels[i] = (unsigned char)std::lround(value * 255.0f);
		}
	}
	sourceTexture.loadData(monoPixels);
}

void JPbox_kinect2::drawSourceTexture()
{
	if (!sourceTexture.isAllocated()) return;
	float width = fbo.getWidth();
	float height = fbo.getHeight();
	float drawX = 0.0f;
	float drawY = 0.0f;
	if (!parameters.getBoolValue(2))
	{
		const float scale = std::min(width / sourceTexture.getWidth(),
			height / sourceTexture.getHeight());
		width = sourceTexture.getWidth() * scale;
		height = sourceTexture.getHeight() * scale;
		drawX = (fbo.getWidth() - width) * 0.5f;
		drawY = (fbo.getHeight() - height) * 0.5f;
	}
	ofPushMatrix();
	if (parameters.getBoolValue(0))
	{
		ofTranslate(fbo.getWidth(), 0.0f);
		ofScale(-1.0f, 1.0f);
		drawX = fbo.getWidth() - drawX - width;
	}
	if (parameters.getBoolValue(7))
	{
		ofTranslate(0.0f, fbo.getHeight());
		ofScale(1.0f, -1.0f);
		drawY = fbo.getHeight() - drawY - height;
	}
	sourceTexture.draw(drawX, drawY, width, height);
	ofPopMatrix();
}

void JPbox_kinect2::updateFBO()
{
	if (!onoff.boolValue)
	{
		JPbox::updateFBO();
		return;
	}
	fbo.begin();
	ofClear(0, 0, 0, 255);
	ofSetColor(255);
	drawSourceTexture();
	fbo.end();
}

void JPbox_kinect2::draw()
{
	JPbox::draw();
	ofSetColor(255);
	fbo.draw(x, y + padding_top / 2.0f - 3.0f, fbowidth, fboheight);
	JPbox::draw_outlet();

	const string badge = getStreamLabel() + "  " + getCaptureStatus();
	string visible = badge;
	while (visible.size() > 4 &&
		jp_constants::p_font.stringWidth(visible) > fbowidth - 6.0f)
	{
		visible.pop_back();
	}
	if (visible != badge) visible += "..";
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_INPUT, 220);
	ofDrawRectangle(x - fbowidth / 2.0f,
		y + padding_top / 2.0f - 3.0f + fboheight - 17.0f,
		fbowidth, 17.0f);
	ofSetColor(getCaptureStatus() == "STREAMING" ?
		COL_ACCENT_GREEN : COL_ACCENT_GOLD);
	jp_constants::p_font.drawString(visible,
		x - fbowidth / 2.0f + 3.0f,
		y + padding_top / 2.0f - 3.0f + fboheight - 4.0f);
}

void JPbox_kinect2::clear()
{
	if (cleared) return;
	cleared = true;
	capture.reset();
	sourceTexture.clear();
	colorPixels.clear();
	monoPixels.clear();
	rawColor.clear();
	rawMono.clear();
	JPbox::clear();
}

void JPbox_kinect2::saveCustomState(ofXml &boxNode) const
{
	auto state = boxNode.appendChild("kinect2");
	state.appendChild("stream").set((int)stream);
}

void JPbox_kinect2::loadCustomState(const ofXml &boxNode)
{
	auto state = boxNode.getChild("kinect2");
	if (state)
	{
		setStream((Stream)ofClamp(state.getChild("stream").getIntValue(), 0, 2));
	}
}

void JPbox_kinect2::copyCustomStateFrom(const JPbox *source)
{
	const auto *kinect = dynamic_cast<const JPbox_kinect2 *>(source);
	if (kinect != nullptr) setStream(kinect->getStream());
}

JPbox_kinect2::Stream JPbox_kinect2::getStream() const
{
	return stream;
}

void JPbox_kinect2::setStream(Stream value)
{
	stream = (Stream)ofClamp((int)value, 0, 2);
	lastFrameVersion = 0;
}

string JPbox_kinect2::getStreamLabel() const
{
	if (stream == DEPTH) return "DEPTH";
	if (stream == INFRARED) return "IR";
	return "COLOR";
}

string JPbox_kinect2::getCaptureStatus() const
{
	return capture ? capture->status() : "NOT AVAILABLE";
}

void JPbox_kinect2::requestReconnect()
{
	if (capture) capture->reconnect();
}

bool JPbox_kinect2::isDriverAvailable()
{
#ifdef KINECT2
	return true;
#else
	return false;
#endif
}
