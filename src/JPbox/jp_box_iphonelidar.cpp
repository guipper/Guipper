#include "jp_box_iphonelidar.h"
#include "jp_record3d_protocol.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using std::atomic;
using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::unique_lock;

#ifdef IPHONE_LIDAR
#include <lzfse.h>
#include <usbmuxd.h>
#endif

namespace
{
	constexpr uint64_t kRetryMillis = 2000;
	constexpr unsigned kReceiveSliceMillis = 250;
	// Record3D stops sending, without closing the socket, when recording is
	// paused in the app. Past this the status says so instead of STREAMING.
	constexpr double kStalledSeconds = 1.5;
	// ARKit confidence: 0 low, 1 medium, 2 high.
	constexpr uint8_t kMinConfidence = 1;

	mutex sharedCaptureMutex;
	weak_ptr<IPhoneLidarCaptureSource> sharedCapture;
}

class IPhoneLidarCaptureSource
{
public:
	std::atomic<unsigned> colorUsers{0}, audienceUsers{0};

	struct ConfidenceFrame
	{
		vector<uint8_t> values;
		int width = 0;
		int height = 0;
	};

	IPhoneLidarCaptureSource()
	{
#ifdef IPHONE_LIDAR
		worker = thread(&IPhoneLidarCaptureSource::run, this);
#else
		setStatus("NOT BUILT: install_iphone_lidar_linux.sh");
#endif
	}

	~IPhoneLidarCaptureSource()
	{
		stop();
	}

	void stop()
	{
		stopRequested = true;
		reconnectRequested = true;
		wake.notify_all();
		if (worker.joinable()) worker.join();
	}

	static shared_ptr<IPhoneLidarCaptureSource> acquire()
	{
		lock_guard<mutex> lock(sharedCaptureMutex);
		shared_ptr<IPhoneLidarCaptureSource> source = sharedCapture.lock();
		if (!source)
		{
			source = make_shared<IPhoneLidarCaptureSource>();
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

	JPbox_iphonelidar::DepthHandle acquireDepth(uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return nullptr;
		version = frameVersion;
		return depthFrame;
	}

	shared_ptr<const ConfidenceFrame> acquireConfidence(uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (frameVersion == 0 || frameVersion == version) return nullptr;
		version = frameVersion;
		return confidenceFrame;
	}

	shared_ptr<const ofPixels> acquireColor(uint64_t &version) const
	{
		lock_guard<mutex> lock(dataMutex);
		if (colorVersion == 0 || colorVersion == version) return nullptr;
		version = colorVersion;
		return colorFrame;
	}

	std::shared_ptr<const jp_hand::Frame> audienceSnapshot() const
	{
		lock_guard<mutex> lock(dataMutex);
		return audienceTrackingFrame;
	}

private:
	void setStatus(const string &value)
	{
		lock_guard<mutex> lock(dataMutex);
		statusText = value;
	}

#ifdef IPHONE_LIDAR
	enum class Receive
	{
		Done,
		Interrupted,
		Failed
	};

	void run()
	{
		vector<uint8_t> scratch(lzfse_decode_scratch_size());
		while (!stopRequested)
		{
			reconnectRequested = false;
			{
				// A different phone may come back, so nothing from the last
				// one may be handed out in the meantime.
				lock_guard<mutex> lock(dataMutex);
				audienceTrackingFrame.reset();
			}
			setStatus("SEARCHING IPHONE");

			usbmuxd_device_info_t *devices = nullptr;
			const int count = usbmuxd_get_device_list(&devices);
			if (count < 0)
			{
				// Distributions start usbmuxd only when an Apple device is
				// plugged in, so a missing daemon usually means no phone.
				setStatus("NO IPHONE / USBMUXD NOT RUNNING");
				waitBeforeRetry();
				continue;
			}
			bool found = false;
			uint32_t handle = 0;
			for (int i = 0; i < count && !found; ++i)
			{
				// Wi-Fi sync shows up here too; Record3D only listens on USB.
				if (devices[i].conn_type != CONNECTION_TYPE_USB) continue;
				handle = devices[i].handle;
				found = true;
			}
			usbmuxd_device_list_free(&devices);
			if (!found)
			{
				setStatus("IPHONE NOT FOUND / TRUST THIS COMPUTER");
				waitBeforeRetry();
				continue;
			}

			const int socket = usbmuxd_connect(handle, jp_record3d::kDevicePort);
			if (socket < 0)
			{
				setStatus("OPEN RECORD3D / ENABLE USB STREAMING");
				waitBeforeRetry();
				continue;
			}
			setStatus("PRESS RECORD IN RECORD3D");
			streamFrames(socket, scratch);
			usbmuxd_disconnect(socket);
			if (!stopRequested && !reconnectRequested) waitBeforeRetry();
		}
	}

	// Reads exactly `size` bytes in short slices so stop and reconnect are
	// noticed within a slice even while the app is not sending anything.
	Receive receive(int socket, uint8_t *destination, uint32_t size,
		bool &stalled)
	{
		uint32_t done = 0;
		while (done < size)
		{
			if (stopRequested || reconnectRequested) return Receive::Interrupted;
			uint32_t received = 0;
			const int result = usbmuxd_recv_timeout(socket,
				reinterpret_cast<char *>(destination + done), size - done,
				&received, kReceiveSliceMillis);
			if (result == -ETIMEDOUT || result == -EAGAIN)
			{
				if (!stalled && lastFrameTime > 0.0 &&
					jp_hand::now() - lastFrameTime > kStalledSeconds)
				{
					stalled = true;
					setStatus("PAUSED: PRESS RECORD IN RECORD3D");
				}
				continue;
			}
			if (result < 0 || received == 0) return Receive::Failed;
			done += received;
		}
		return Receive::Done;
	}

	void streamFrames(int socket, vector<uint8_t> &scratch)
	{
		vector<uint8_t> body;
		uint8_t peerTalk[jp_record3d::kPeerTalkHeaderSize];
		bool stalled = false;
		lastFrameTime = 0.0;
		double previousCapture = 0.0;
		while (!stopRequested && !reconnectRequested)
		{
			if (receive(socket, peerTalk, sizeof(peerTalk), stalled) !=
				Receive::Done)
			{
				break;
			}
			const uint32_t size = jp_record3d::bodySize(peerTalk);
			if (size == 0)
			{
				// Out of step with the stream; only a fresh connection
				// re-synchronises it.
				setStatus("PROTOCOL ERROR / UPDATE RECORD3D");
				break;
			}
			body.resize(size);
			if (receive(socket, body.data(), size, stalled) != Receive::Done)
			{
				break;
			}

			jp_record3d::Message message;
			if (!jp_record3d::parse(body.data(), body.size(), message))
			{
				setStatus("PROTOCOL ERROR / UPDATE RECORD3D");
				break;
			}
			const double captured = jp_hand::now();
			if (!publish(message, scratch, captured, previousCapture)) continue;
			previousCapture = captured;
			lastFrameTime = captured;
			stalled = false;
		}
	}

	bool publish(const jp_record3d::Message &message, vector<uint8_t> &scratch,
		double captured, double previousCapture)
	{
		const int width = (int)message.header.depthWidth;
		const int height = (int)message.header.depthHeight;
		const size_t pixels = (size_t)width * height;

		vector<float> metres(pixels);
		if (lzfse_decode_buffer(reinterpret_cast<uint8_t *>(metres.data()),
				message.depthBytes(), message.depth, message.header.depthSize,
				scratch.data()) != message.depthBytes())
		{
			return false;
		}

		auto confidence = make_shared<ConfidenceFrame>();
		if (message.hasConfidence())
		{
			confidence->values.resize(message.confidenceBytes());
			if (lzfse_decode_buffer(confidence->values.data(),
					confidence->values.size(), message.confidence,
					message.header.confidenceSize, scratch.data()) !=
				confidence->values.size())
			{
				confidence->values.clear();
			}
			else
			{
				confidence->width = (int)message.header.confidenceWidth;
				confidence->height = (int)message.header.confidenceHeight;
			}
		}
		// Confidence is only usable as a mask when it lines up pixel for
		// pixel; older app versions send none at all.
		const bool masked = confidence->width == width &&
			confidence->height == height;

		auto depth = make_shared<JPbox_iphonelidar::DepthFrame>();
		depth->width = width;
		depth->height = height;
		depth->millimetres.resize(pixels);
		for (size_t i = 0; i < pixels; ++i)
		{
			const float m = metres[i];
			const bool usable = std::isfinite(m) && m > 0.0f &&
				(!masked || confidence->values[i] >= kMinConfidence);
			depth->millimetres[i] = usable ? m * 1000.0f : 0.0f;
		}
		const jp_record3d::Intrinsics k = jp_record3d::scaleToDepth(message);
		depth->intrinsics.fx = k.fx;
		depth->intrinsics.fy = k.fy;
		depth->intrinsics.cx = k.cx;
		depth->intrinsics.cy = k.cy;
		depth->intrinsics.valid = k.fx > 0.0f && k.fy > 0.0f;

		shared_ptr<ofPixels> color;
		if (colorUsers.load() > 0 && message.rgb != nullptr)
		{
			ofBuffer jpeg(reinterpret_cast<const char *>(message.rgb),
				message.header.rgbSize);
			auto decoded = make_shared<ofPixels>();
			if (ofLoadImage(*decoded, jpeg)) color = std::move(decoded);
		}

		std::shared_ptr<jp_hand::Frame> audience;
		if (audienceUsers.load() > 0)
		{
			audience = make_shared<jp_hand::Frame>();
			audience->captured = captured;
			audience->width = audience->nativeWidth = audience->depthWidth = width;
			audience->height = audience->nativeHeight = audience->depthHeight = height;
			audience->captureFps = previousCapture > 0.0 ?
				float(1.0 / (captured - previousCapture)) : 0.0f;
			audience->depth = depth->millimetres;
		}

		lock_guard<mutex> lock(dataMutex);
		++frameVersion;
		if (audience) audience->serial = frameVersion;
		audienceTrackingFrame = std::move(audience);
		depthFrame = std::move(depth);
		confidenceFrame = std::move(confidence);
		if (color)
		{
			colorFrame = std::move(color);
			colorVersion = frameVersion;
		}
		statusText = message.header.deviceType == jp_record3d::LIDAR ?
			"STREAMING" : "STREAMING (TRUEDEPTH, NOT LIDAR)";
		return true;
	}

	void waitBeforeRetry()
	{
		unique_lock<mutex> lock(waitMutex);
		wake.wait_for(lock, std::chrono::milliseconds(kRetryMillis), [this]() {
			return stopRequested.load() || reconnectRequested.load();
		});
	}
#endif

	mutable mutex dataMutex;
	mutex waitMutex;
	condition_variable wake;
	thread worker;
	atomic<bool> stopRequested{false};
	atomic<bool> reconnectRequested{false};
	double lastFrameTime = 0.0; // capture thread only
	string statusText = "STARTING";
	JPbox_iphonelidar::DepthHandle depthFrame;
	shared_ptr<const ConfidenceFrame> confidenceFrame;
	shared_ptr<const ofPixels> colorFrame;
	std::shared_ptr<const jp_hand::Frame> audienceTrackingFrame;
	uint64_t frameVersion = 0;
	uint64_t colorVersion = 0;
};

JPbox_iphonelidar::JPbox_iphonelidar() = default;

JPbox_iphonelidar::~JPbox_iphonelidar()
{
	clear();
}

void JPbox_iphonelidar::setup(string directory, string boxName)
{
	JPbox::setup(directory, boxName);
	dir = "iphoneLidar";
	name = boxName;
	tipo = IPHONELIDARBOX;
	parameters.addBoolValue(false, "mirror");
	parameters.addBoolValue(false, "invert");
	parameters.addBoolValue(false, "stretch");
	parameters.addFloatValue(kMinRangeMm, "near mm");
	parameters.setMin(kMinRangeMm, NEAR_MM);
	parameters.setMax(kMaxRangeMm - 1.0f, NEAR_MM);
	parameters.addFloatValue(kMaxRangeMm, "far mm");
	parameters.setMin(kMinRangeMm + 1.0f, FAR_MM);
	parameters.setMax(kMaxRangeMm, FAR_MM);
	parameters.addBoolValue(true, "clip depth");
	parameters.addBoolValue(false, "flip vertical");
	parameters.addFloatValue(1.0f, "depth gamma");
	parameters.setMin(0.1f, GAMMA);
	parameters.setMax(4.0f, GAMMA);
	// The phone streams in the sensor's orientation; held the other way the
	// picture arrives on its side.
	parameters.addBoolValue(false, "rotate 90");
	capture = IPhoneLidarCaptureSource::acquire();
	cleared = false;
	updateColorLease();
}

void JPbox_iphonelidar::update()
{
	JPbox::update();
	const float nearMm = std::round(ofClamp(parameters.getFloatValue(NEAR_MM),
		kMinRangeMm, kMaxRangeMm - 1.0f));
	const float farMm = std::round(ofClamp(parameters.getFloatValue(FAR_MM),
		nearMm + 1.0f, kMaxRangeMm));
	const float gamma = ofClamp(parameters.getFloatValue(GAMMA), 0.1f, 4.0f);
	parameters.setFloatValue(nearMm, NEAR_MM);
	parameters.setFloatLerpValue(nearMm, NEAR_MM);
	parameters.setFloatValue(farMm, FAR_MM);
	parameters.setFloatLerpValue(farMm, FAR_MM);
	parameters.setFloatValue(gamma, GAMMA);
	parameters.setFloatLerpValue(gamma, GAMMA);

	// Same reasoning as the Kinect box: a range or gamma change repaints even
	// when the phone sends nothing new.
	const uint64_t settingsHash =
		(uint64_t)(nearMm) * 1000003u +
		(uint64_t)(farMm) * 10007u +
		(uint64_t)(gamma * 1000.0f) * 13u +
		(parameters.getBoolValue(INVERT) ? 2u : 0u) +
		(parameters.getBoolValue(CLIP_DEPTH) ? 4u : 0u);
	if (settingsHash != settingsVersion) settingsVersion = settingsHash;

	updateSourceTexture();
	updateFBO();
}

void JPbox_iphonelidar::refreshToneCurve()
{
	const float gamma = ofClamp(parameters.getFloatValue(GAMMA), 0.1f, 4.0f);
	const bool invert = parameters.getBoolValue(INVERT);
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

void JPbox_iphonelidar::updateSourceTexture()
{
	if (!capture) return;
	refreshToneCurve();

	if (stream == COLOR)
	{
		auto frame = capture->acquireColor(lastFrameVersion);
		if (!frame || !frame->isAllocated()) return;
		if (textureStream != (int)COLOR) sourceTexture.clear();
		textureStream = (int)COLOR;
		sourceTexture.loadData(*frame);
		return;
	}

	if (stream == CONFIDENCE)
	{
		auto frame = capture->acquireConfidence(lastFrameVersion);
		if (!frame || frame->values.empty()) return;
		monoPixels.allocate(frame->width, frame->height, OF_PIXELS_GRAY);
		// 0/1/2 spread over the grey range so the three levels read apart.
		for (size_t i = 0; i < frame->values.size(); ++i)
		{
			monoPixels[i] = (unsigned char)(std::min<int>(frame->values[i], 2) * 127);
		}
		sourceTexture.clear();
		textureStream = (int)CONFIDENCE;
		sourceTexture.loadData(monoPixels);
		return;
	}

	if (auto frame = capture->acquireDepth(lastFrameVersion))
	{
		rawDepth = std::move(frame);
	}
	if (!rawDepth || rawDepth->millimetres.empty()) return;
	if (lastFrameVersion == builtFrameVersion &&
		settingsVersion == builtSettingsVersion &&
		textureStream == (int)DEPTH)
	{
		return;
	}
	builtFrameVersion = lastFrameVersion;
	builtSettingsVersion = settingsVersion;

	// loadData only reallocates for a LARGER image; coming from the colour
	// texture it would blit the small depth image into a corner of it.
	if (textureStream != (int)DEPTH) sourceTexture.clear();
	textureStream = (int)DEPTH;

	monoPixels.allocate(rawDepth->width, rawDepth->height, OF_PIXELS_GRAY);
	const float nearMm = parameters.getFloatValue(NEAR_MM);
	const float farMm = std::max(nearMm + 1.0f, parameters.getFloatValue(FAR_MM));
	const bool clipDepth = parameters.getBoolValue(CLIP_DEPTH);
	const float scale = 1.0f / (farMm - nearMm);
	constexpr float lastEntry = (float)(kToneCurveSize - 1);
	const vector<float> &source = rawDepth->millimetres;
	for (size_t i = 0; i < source.size(); ++i)
	{
		const float mm = source[i];
		if (mm <= 0.0f || (clipDepth && (mm < nearMm || mm > farMm)))
		{
			monoPixels[i] = 0;
			continue;
		}
		// Near = bright, like the Kinect DEPTH stream, so the two boxes are
		// interchangeable in front of a displacement shader.
		const float value = 1.0f - ofClamp((mm - nearMm) * scale, 0.0f, 1.0f);
		monoPixels[i] = toneCurve[(int)(value * lastEntry + 0.5f)];
	}
	sourceTexture.loadData(monoPixels);
}

void JPbox_iphonelidar::drawSourceTexture()
{
	if (!sourceTexture.isAllocated()) return;
	const bool rotated = parameters.getBoolValue(ROTATE_90);
	// Fit what will be on screen: a quarter turn swaps the picture's sides.
	const float sourceWidth = rotated ?
		sourceTexture.getHeight() : sourceTexture.getWidth();
	const float sourceHeight = rotated ?
		sourceTexture.getWidth() : sourceTexture.getHeight();
	float width = fbo.getWidth();
	float height = fbo.getHeight();
	if (!parameters.getBoolValue(STRETCH))
	{
		const float scale = std::min(width / sourceWidth, height / sourceHeight);
		width = sourceWidth * scale;
		height = sourceHeight * scale;
	}
	// Everything about the centre of the fbo, so rotate, mirror and flip
	// compose without tracking corners.
	ofPushMatrix();
	ofTranslate(fbo.getWidth() * 0.5f, fbo.getHeight() * 0.5f);
	if (parameters.getBoolValue(MIRROR)) ofScale(-1.0f, 1.0f);
	if (parameters.getBoolValue(FLIP_VERTICAL)) ofScale(1.0f, -1.0f);
	if (rotated)
	{
		ofRotateDeg(90.0f);
		std::swap(width, height);
	}
	sourceTexture.draw(-width * 0.5f, -height * 0.5f, width, height);
	ofPopMatrix();
}

void JPbox_iphonelidar::updateFBO()
{
	if (!onoff.boolValue)
	{
		JPbox::updateFBO();
		return;
	}
	// drawSourceTexture computes a top-left corner; see JPbox_kinect2.
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();
	ofClear(0, 0, 0, 255);
	ofSetColor(255);
	drawSourceTexture();
	fbo.end();
}

void JPbox_iphonelidar::draw()
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

void JPbox_iphonelidar::updateColorLease()
{
	const bool wanted = capture && stream == COLOR;
	if (wanted && !colorLease)
	{
		auto source = capture;
		++source->colorUsers;
		colorLease = std::shared_ptr<void>(source.get(),
			[source](void *) { --source->colorUsers; });
	}
	else if (!wanted)
	{
		colorLease.reset();
	}
}

void JPbox_iphonelidar::setCaptureActive(bool active)
{
	if (active)
	{
		if (capture || cleared) return;
		lastFrameVersion = builtFrameVersion = 0;
		builtSettingsVersion = 0;
		capture = acquireSharedCapture();
	}
	else
	{
		colorLease.reset();
		capture.reset();
		rawDepth.reset();
	}
	updateColorLease();
}

void JPbox_iphonelidar::clear()
{
	if (cleared) return;
	cleared = true;
	colorLease.reset();
	capture.reset();
	sourceTexture.clear();
	monoPixels.clear();
	rawDepth.reset();
	JPbox::clear();
}

void JPbox_iphonelidar::saveCustomState(ofXml &boxNode) const
{
	auto state = boxNode.appendChild("iphoneLidar");
	state.appendChild("stream").set((int)stream);
}

void JPbox_iphonelidar::loadCustomState(const ofXml &boxNode)
{
	auto state = boxNode.getChild("iphoneLidar");
	if (state)
	{
		setStream((Stream)ofClamp(state.getChild("stream").getIntValue(), 0, 2));
	}
}

void JPbox_iphonelidar::copyCustomStateFrom(const JPbox *source)
{
	const auto *phone = dynamic_cast<const JPbox_iphonelidar *>(source);
	if (phone != nullptr) setStream(phone->getStream());
}

vector<string> JPbox_iphonelidar::inspectorChoiceLabels() const
{
	return {"COLOR", "DEPTH", "CONFIDENCE"};
}

int JPbox_iphonelidar::inspectorChoice() const
{
	return (int)stream;
}

void JPbox_iphonelidar::setInspectorChoice(int choice)
{
	setStream((Stream)choice);
}

string JPbox_iphonelidar::inspectorChoiceStatus() const
{
	return getCaptureStatus();
}

JPbox_iphonelidar::Stream JPbox_iphonelidar::getStream() const
{
	return stream;
}

void JPbox_iphonelidar::setStream(Stream value)
{
	stream = (Stream)ofClamp((int)value, 0, 2);
	lastFrameVersion = 0;
	builtFrameVersion = 0;
	rawDepth.reset();
	updateColorLease();
}

string JPbox_iphonelidar::getStreamLabel() const
{
	if (stream == COLOR) return "COLOR";
	if (stream == CONFIDENCE) return "CONF";
	return "DEPTH";
}

string JPbox_iphonelidar::getCaptureStatus() const
{
	return capture ? capture->status() : "NOT AVAILABLE";
}

void JPbox_iphonelidar::requestReconnect()
{
	if (capture) capture->reconnect();
}

bool JPbox_iphonelidar::isDriverAvailable()
{
#ifdef IPHONE_LIDAR
	return true;
#else
	return false;
#endif
}

void JPbox_iphonelidar::shutdownSharedCapture()
{
	shared_ptr<IPhoneLidarCaptureSource> source;
	{
		lock_guard<mutex> lock(sharedCaptureMutex);
		source = sharedCapture.lock();
	}
	if (source) source->stop();
}

shared_ptr<IPhoneLidarCaptureSource> JPbox_iphonelidar::acquireSharedCapture()
{
	return IPhoneLidarCaptureSource::acquire();
}

JPbox_iphonelidar::DepthHandle JPbox_iphonelidar::acquireDepth(
	const shared_ptr<IPhoneLidarCaptureSource> &source, uint64_t &version)
{
	return source ? source->acquireDepth(version) : nullptr;
}

string JPbox_iphonelidar::captureStatus(
	const shared_ptr<IPhoneLidarCaptureSource> &source)
{
	return source ? source->status() : "NOT AVAILABLE";
}

void JPbox_iphonelidar::reconnectShared(
	const shared_ptr<IPhoneLidarCaptureSource> &source)
{
	if (source) source->reconnect();
}

std::shared_ptr<void> JPbox_iphonelidar::audienceLease(
	const shared_ptr<IPhoneLidarCaptureSource> &source)
{
	if (!source) return {};
	++source->audienceUsers;
	return std::shared_ptr<void>(source.get(),
		[source](void *) { --source->audienceUsers; });
}

std::shared_ptr<const jp_hand::Frame> JPbox_iphonelidar::audienceFrame(
	const shared_ptr<IPhoneLidarCaptureSource> &source)
{
	return source ? source->audienceSnapshot() : nullptr;
}
