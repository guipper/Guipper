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

	// The frame buffers are never mutated once published, so consumers get a
	// refcounted handle instead of their own copy. Null means "nothing new".
	JPbox_kinect2::ColorFrame acquireColorFrame(int &format,
		uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return nullptr;
		format = colorFrameFormat;
		version = frameVersion;
		return colorFrame;
	}

	JPbox_kinect2::MonoFrame acquireMonoFrame(bool infrared,
		uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return nullptr;
		version = frameVersion;
		return infrared ? infraredFrame : depthFrame;
	}

	// Same contract, but always the depth buffer and never the infrared one -
	// consumers of this want metric millimetres.
	JPbox_kinect2::MonoFrame acquireDepthFrame(uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return nullptr;
		version = frameVersion;
		return depthFrame;
	}

	JPbox_kinect2::DepthIntrinsics intrinsics() const
	{
		lock_guard<mutex> lock(dataMutex);
		return irParams;
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
			{
				// A different device may come back on reconnect, so the old
				// intrinsics must not be handed out in the meantime.
				lock_guard<mutex> lock(dataMutex);
				irParams = JPbox_kinect2::DepthIntrinsics();
			}
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

			{
				// Pinhole parameters of the IR camera, needed by anything that
				// wants to unproject the depth buffer into camera-space XYZ.
				const libfreenect2::Freenect2Device::IrCameraParams ir =
					device->getIrCameraParams();
				lock_guard<mutex> lock(dataMutex);
				irParams.fx = ir.fx;
				irParams.fy = ir.fy;
				irParams.cx = ir.cx;
				irParams.cy = ir.cy;
				irParams.valid = ir.fx > 0.0f && ir.fy > 0.0f;
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
					const float *depthData =
						reinterpret_cast<const float *>(depth->data);
					const float *irData =
						reinterpret_cast<const float *>(infrared->data);
					// Built outside the lock, then published by pointer swap.
					auto nextColor = std::make_shared<vector<unsigned char>>(
						color->data,
						color->data + color->width * color->height * 4);
					auto nextDepth = std::make_shared<vector<float>>(depthData,
						depthData + depth->width * depth->height);
					auto nextInfrared = std::make_shared<vector<float>>(irData,
						irData + infrared->width * infrared->height);
					{
						lock_guard<mutex> lock(dataMutex);
						colorFrame = std::move(nextColor);
						depthFrame = std::move(nextDepth);
						infraredFrame = std::move(nextInfrared);
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
	// shared_ptr, not vector: published once, then read by refcount.
	std::shared_ptr<const vector<unsigned char>> colorFrame;
	std::shared_ptr<const vector<float>> depthFrame;
	std::shared_ptr<const vector<float>> infraredFrame;
	JPbox_kinect2::DepthIntrinsics irParams;
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

	// Anything the grey mapping reads. A change here has to repaint even if
	// the device never sends another frame.
	const uint64_t settingsHash =
		(uint64_t)(nearMm) * 1000003u +
		(uint64_t)(farMm) * 10007u +
		(uint64_t)(irGain * 1000.0f) * 101u +
		(uint64_t)(gamma * 1000.0f) * 13u +
		(parameters.getBoolValue(1) ? 2u : 0u) +
		(parameters.getBoolValue(6) ? 4u : 0u);
	if (settingsHash != settingsVersion) settingsVersion = settingsHash;

	updateSourceTexture();
	updateFBO();
}

void JPbox_kinect2::refreshToneCurve()
{
	const float gamma = ofClamp(parameters.getFloatValue(8), 0.1f, 4.0f);
	const bool invert = parameters.getBoolValue(1);
	if (gamma == toneCurveGamma && invert == toneCurveInvert) return;
	toneCurveGamma = gamma;
	toneCurveInvert = invert;

	const float exponent = 1.0f / gamma;
	for (int i = 0; i < kToneCurveSize; ++i)
	{
		float value = (float)i / (float)(kToneCurveSize - 1);
		if (invert) value = 1.0f - value;
		toneCurve[i] = (unsigned char)std::lround(
			std::pow(value, exponent) * 255.0f);
	}
}

void JPbox_kinect2::updateSourceTexture()
{
	if (!capture) return;
	refreshToneCurve();

	if (stream == COLOR)
	{
		if (auto frame = capture->acquireColorFrame(colorFormat,
			lastFrameVersion))
		{
			rawColor = std::move(frame);
		}
		if (!rawColor ||
			rawColor->size() != (size_t)kColorWidth * kColorHeight * 4)
		{
			return;
		}
		if (lastFrameVersion == builtFrameVersion) return;
		builtFrameVersion = lastFrameVersion;

		// The device hands over BGRX or RGBX. Swizzling that on the CPU was
		// two million iterations a frame; GL does it for free on upload, and
		// an RGB internal format drops the undefined X byte.
		if (textureStream != (int)COLOR)
		{
			sourceTexture.clear();
			textureStream = (int)COLOR;
		}
		if (!sourceTexture.isAllocated())
		{
			sourceTexture.allocate(kColorWidth, kColorHeight, GL_RGB);
		}
		int glFormat = GL_RGBA;
#ifdef KINECT2
		if (colorFormat == (int)libfreenect2::Frame::BGRX) glFormat = GL_BGRA;
#endif
		sourceTexture.loadData(rawColor->data(), kColorWidth, kColorHeight,
			glFormat);
		return;
	}

	if (auto frame = capture->acquireMonoFrame(stream == INFRARED,
		lastFrameVersion))
	{
		rawMono = std::move(frame);
	}
	if (!rawMono || rawMono->size() != (size_t)kDepthWidth * kDepthHeight)
	{
		return;
	}
	// Rebuild on a new frame OR on a settings change, so dragging near/far or
	// gamma repaints immediately instead of waiting on the next device frame -
	// which never arrives at all once the sensor drops out.
	if (lastFrameVersion == builtFrameVersion &&
		settingsVersion == builtSettingsVersion)
	{
		return;
	}
	builtFrameVersion = lastFrameVersion;
	builtSettingsVersion = settingsVersion;

	const vector<float> &source = *rawMono;
	monoPixels.allocate(kDepthWidth, kDepthHeight, OF_PIXELS_GRAY);
	// loadData only reallocates when the incoming image is LARGER, so coming
	// back from the 1920x1080 colour texture it would happily blit 512x424 of
	// grey into a corner of the old RGB one. Drop it and let loadData rebuild.
	if (textureStream == (int)COLOR)
	{
		sourceTexture.clear();
	}
	textureStream = (int)stream;
	constexpr float lastEntry = (float)(kToneCurveSize - 1);
	if (stream == DEPTH)
	{
		const float nearMm = parameters.getFloatValue(3);
		const float farMm = std::max(nearMm + 1.0f,
			parameters.getFloatValue(4));
		const bool clipDepth = parameters.getBoolValue(6);
		const float scale = 1.0f / (farMm - nearMm);
		for (size_t i = 0; i < source.size(); ++i)
		{
			const float mm = source[i];
			if (!std::isfinite(mm) || mm <= 0.0f ||
				(clipDepth && (mm < nearMm || mm > farMm)))
			{
				monoPixels[i] = 0;
				continue;
			}
			const float value =
				1.0f - ofClamp((mm - nearMm) * scale, 0.0f, 1.0f);
			monoPixels[i] = toneCurve[(int)(value * lastEntry + 0.5f)];
		}
	}
	else
	{
		const float gain = parameters.getFloatValue(5) / 65535.0f;
		for (size_t i = 0; i < source.size(); ++i)
		{
			const float ir = source[i];
			const float value = std::isfinite(ir) ?
				ofClamp(ir * gain, 0.0f, 1.0f) : 0.0f;
			monoPixels[i] = toneCurve[(int)(value * lastEntry + 0.5f)];
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
	// Rect mode is global and this runs during update(), so it inherits
	// whatever the last draw left behind. drawSourceTexture computes drawX/drawY
	// as a TOP-LEFT corner, and ofTexture::draw honours the rect mode - under
	// OF_RECTMODE_CENTER the frame is centred on that corner instead, leaving
	// only its bottom-right quarter inside the FBO.
	//
	// Invisible in this box's own thumbnail, but when the kinect IS the active
	// render the node background is drawn from this FBO, so a quarter-filled
	// FBO paints a quarter-filled canvas. It reads as intermittent because it
	// depends on what drew last: clicking any box changes the order and the
	// next composite comes out right.
	//
	// Same defect 7f59ec0 fixed in JPbox_preset::renderActiveRender. An audit
	// of every fbo.begin() in the box types found this as the only remaining
	// one - the rest already set it (jp_box_cam.cpp:335 is the closest twin).
	ofSetRectMode(OF_RECTMODE_CORNER);
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
	// Push/pop: the rect mode is global GL-ish state that every later drawer
	// inherits, and leaving it on CORNER moved anything that draws centred.
	ofPushStyle();
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
	ofPopStyle();
}

void JPbox_kinect2::clear()
{
	if (cleared) return;
	cleared = true;
	capture.reset();
	sourceTexture.clear();
	monoPixels.clear();
	rawColor.reset();
	rawMono.reset();
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
	// Force a refetch and a rebuild: the cached picture belongs to the stream
	// we are leaving.
	lastFrameVersion = 0;
	builtFrameVersion = 0;
	rawColor.reset();
	rawMono.reset();
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

// --- Raw depth tap ---------------------------------------------------------
// KinectV2CaptureSource is file local, so boxes that are not a JPbox_kinect2
// reach the device through these instead of constructing their own.

shared_ptr<KinectV2CaptureSource> JPbox_kinect2::acquireSharedCapture()
{
	return KinectV2CaptureSource::acquire();
}

JPbox_kinect2::MonoFrame JPbox_kinect2::acquireRawDepth(
	const shared_ptr<KinectV2CaptureSource> &source, uint64_t &version)
{
	if (!source) return nullptr;
	return source->acquireDepthFrame(version);
}

JPbox_kinect2::DepthIntrinsics JPbox_kinect2::depthIntrinsics(
	const shared_ptr<KinectV2CaptureSource> &source)
{
	if (!source) return DepthIntrinsics();
	return source->intrinsics();
}

string JPbox_kinect2::captureStatus(
	const shared_ptr<KinectV2CaptureSource> &source)
{
	return source ? source->status() : "NOT AVAILABLE";
}

int JPbox_kinect2::depthWidth()
{
	return kDepthWidth;
}

int JPbox_kinect2::depthHeight()
{
	return kDepthHeight;
}
