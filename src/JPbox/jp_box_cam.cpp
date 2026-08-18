#include "jp_box_cam.h"
#include <limits>
#include <map>

JPCameraCaptureSource::JPCameraCaptureSource(int deviceId, int width, int height)
	: deviceId(deviceId), lastUpdateFrame(std::numeric_limits<uint64_t>::max())
{
	grabber.setDeviceID(deviceId);
	grabber.setDesiredFrameRate(60);
	initialized = grabber.setup(width, height);
	if (!initialized)
	{
		ofLogWarning("CAMARITA")
			<< "Unable to open camera device " << deviceId;
	}
}

JPCameraCaptureSource::~JPCameraCaptureSource()
{
	grabber.close();
}

void JPCameraCaptureSource::updateOnce()
{
	if (!initialized)
	{
		return;
	}
	const uint64_t frame = ofGetFrameNum();
	if (lastUpdateFrame == frame)
	{
		return;
	}
	lastUpdateFrame = frame;
	grabber.update();
}

bool JPCameraCaptureSource::isInitialized() const
{
	return initialized && grabber.isInitialized();
}

void JPCameraCaptureSource::draw(float x, float y, float width, float height) const
{
	if (isInitialized())
	{
		grabber.draw(x, y, width, height);
	}
}

const ofTexture &JPCameraCaptureSource::getTexture() const
{
	return grabber.getTexture();
}

bool JPCameraCaptureSource::hasTexture() const
{
	return isInitialized() && grabber.getTexture().isAllocated();
}

namespace
{
	struct CameraDeviceInfo
	{
		int id = -1;
		string name;
	};

	vector<CameraDeviceInfo> &cameraDevices()
	{
		static vector<CameraDeviceInfo> devices;
		return devices;
	}

	bool &cameraDevicesScanned()
	{
		static bool scanned = false;
		return scanned;
	}

	// Bumped by a rescan. Each box compares it against its own copy so one
	// refresh reaches every camera box, not only the one that was clicked.
	uint64_t &cameraRescanGeneration()
	{
		static uint64_t generation = 0;
		return generation;
	}

	map<int, weak_ptr<JPCameraCaptureSource>> &cameraSources()
	{
		static map<int, weak_ptr<JPCameraCaptureSource>> sources;
		return sources;
	}

	void pruneExpiredCameraSources()
	{
		auto &sources = cameraSources();
		for (auto it = sources.begin(); it != sources.end();)
		{
			if (it->second.expired())
			{
				it = sources.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	const vector<CameraDeviceInfo> &discoverCameraDevices()
	{
		if (cameraDevicesScanned())
		{
			return cameraDevices();
		}

		cameraDevicesScanned() = true;
		cameraDevices().clear();
		ofVideoGrabber probe;
		const vector<ofVideoDevice> devices = probe.listDevices();
		for (const ofVideoDevice &device : devices)
		{
			if (device.bAvailable)
			{
				cameraDevices().push_back({device.id, device.deviceName});
				ofLogNotice("CAMARITA")
					<< device.id << ": " << device.deviceName;
			}
			else
			{
				ofLogNotice("CAMARITA")
					<< device.id << ": " << device.deviceName
					<< " - unavailable";
			}
		}
		if (cameraDevices().empty())
		{
			ofLogWarning("CAMARITA")
				<< "No available camera capture devices found";
		}
		return cameraDevices();
	}

	int findDefaultCameraListIndex(const vector<int> &deviceIds)
	{
		pruneExpiredCameraSources();
		const auto &sources = cameraSources();
		for (int i = 0; i < int(deviceIds.size()); i++)
		{
			auto source = sources.find(deviceIds[i]);
			if (source == sources.end() || source->second.expired())
			{
				return i;
			}
		}
		return deviceIds.empty() ? -1 : 0;
	}

	shared_ptr<JPCameraCaptureSource> acquireCameraSource(
		int deviceId, int width, int height)
	{
		pruneExpiredCameraSources();
		auto &sources = cameraSources();
		auto existing = sources.find(deviceId);
		if (existing != sources.end())
		{
			shared_ptr<JPCameraCaptureSource> source =
				existing->second.lock();
			if (source)
			{
				return source;
			}
		}

		shared_ptr<JPCameraCaptureSource> source =
			make_shared<JPCameraCaptureSource>(
				deviceId, width, height);
		sources[deviceId] = source;
		return source;
	}
}

JPbox_cam::JPbox_cam() {}
JPbox_cam::~JPbox_cam()
{
	releaseCameraSource();
}

void JPbox_cam::setup(string _dir, string _name)
{

	name = "CAMARITA";
	dir = "cam";
	JPbox::setup(_dir, _name);

	camsize = 0;
	// COMO YA CALCULAMOS EL TIPO DE ARCHIVO QUE ES POR SU EXTENSION, ONDA LAS IMAGENES Y VIDEOS Y SHADERS.
	// PERO COMO LA CAMARA NO LO TIENE ENTONCES VAMOS A USAR LA VARIABLE DIR PARA QUE GUARDE QUE ES UNA CAMARA
	// Y AS� CUANDO LO INICIALIZA QUE CREE UNA CAM. MESPLICO. //LO MISMO EN SPOUT BOX

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addFloatValue(0.0, "camaraindex");
	parameters.addBoolValue(true, "strech");
	// Appended LAST on purpose: JPboxgroup::load fills parameters positionally,
	// so inserting anywhere else would shift every parameter in every saved
	// composition. Files written before this existed simply stop short and the
	// zoom keeps its neutral 1.0.
	parameters.addFloatValue(1.0, "scale ratio");
	if (JPParameter *ratio = parameters.getJParameter(parameters.getSize()-1))
	{
		ratio->nativeMin = ratio->min = 0.1f;
		ratio->nativeMax = ratio->max = 4.0f;
		ratio->defaultFloatValue = 1.0f;
	}
	scaleRatioIndex = parameters.getSize()-1;

	refreshCameraDevices();
	const int defaultListIndex =
		findDefaultCameraListIndex(availableDeviceIds);
	const float defaultValue =
		defaultListIndex > 0 && availableDeviceIds.size() > 1
			? float(defaultListIndex) /
				float(availableDeviceIds.size() - 1)
			: 0.0f;
	parameters.setFloatValue(defaultValue, 4);
	parameters.setFloatLerpValue(defaultValue, 4);

	tipo = CAMBOX;
}

std::shared_ptr<JPCameraCaptureSource> JPbox_cam::acquireSharedCamera(
	int deviceId, int width, int height)
{
	// Straight through to the same refcounted map CAMARITA uses, so a depth box
	// and a camera box pointed at one device share a single open grabber
	// instead of the second one failing to open it.
	return acquireCameraSource(deviceId, width, height);
}

vector<int> JPbox_cam::availableCameraIds()
{
	vector<int> ids;
	for (const CameraDeviceInfo &device : discoverCameraDevices())
	{
		ids.push_back(device.id);
	}
	return ids;
}

string JPbox_cam::cameraLabel(int deviceId)
{
	for (const CameraDeviceInfo &device : discoverCameraDevices())
	{
		if (device.id == deviceId) return device.name;
	}
	return "no camera";
}

vector<pair<int, int>> JPbox_cam::openCameraSources()
{
	vector<pair<int, int>> open;
	for (auto &entry : cameraSources())
	{
		// use_count on the LOCKED shared_ptr, minus the temporary this lock
		// created, is how many boxes actually hold the device.
		if (auto locked = entry.second.lock())
		{
			open.push_back({entry.first, (int)locked.use_count() - 1});
		}
	}
	return open;
}

uint64_t JPbox_cam::cameraRescanCount()
{
	return cameraRescanGeneration();
}

void JPbox_cam::rescanCameraDevices()
{
	// Only the list is invalidated here. Boxes still hold their sources, so
	// nothing goes black; each one drops and reopens its device on its next
	// update, which is what picks up a device id that has moved.
	cameraDevicesScanned() = false;
	cameraDevices().clear();
	pruneExpiredCameraSources();
	cameraRescanGeneration()++;
	ofLogNotice("CAMARITA") << "Rescanning capture devices";
}

void JPbox_cam::refreshCameraDevices()
{
	availableDeviceIds.clear();
	const vector<CameraDeviceInfo> &devices = discoverCameraDevices();
	for (const CameraDeviceInfo &device : devices)
	{
		availableDeviceIds.push_back(device.id);
	}
	camsize = availableDeviceIds.size();
}

void JPbox_cam::applyCameraIndexFromParameter(bool force)
{
	if (parameters.getSize() <= 4)
	{
		return;
	}
	if (availableDeviceIds.empty())
	{
		refreshCameraDevices();
	}
	if (availableDeviceIds.empty())
	{
		return;
	}

	int targetListIndex = int(std::round(ofMap(parameters.getFloatValue(4),
											 0.0f,
											 1.0f,
											 0.0f,
											 float(availableDeviceIds.size() - 1),
											 true)));
	targetListIndex = ofClamp(targetListIndex, 0, int(availableDeviceIds.size()) - 1);
	const int targetDeviceId = availableDeviceIds[targetListIndex];
	if (!force &&
		targetListIndex == currentCameraListIndex &&
		targetDeviceId == currentDeviceId &&
		cameraSource)
	{
		return;
	}

	releaseCameraSource();
	currentCameraListIndex = targetListIndex;
	currentDeviceId = targetDeviceId;
	cameraSource =
		acquireCameraSource(currentDeviceId, camWidth, camHeight);
}

void JPbox_cam::releaseCameraSource()
{
	cameraSource.reset();
	currentCameraListIndex = -1;
	currentDeviceId = -1;
}

void JPbox_cam::update()
{
	JPbox::update();

	const bool rescanned = appliedRescanGeneration != cameraRescanGeneration();
	if (rescanned)
	{
		appliedRescanGeneration = cameraRescanGeneration();
		refreshCameraDevices();
	}
	applyCameraIndexFromParameter(rescanned);
	if (cameraSource)
	{
		cameraSource->updateOnce();
	}

	// The scheduler drops us to the staggered preview rate when nothing on
	// screen depends on this box. The source above is pumped either way - only
	// the render is skipped, and the FBO keeps its last frame.
	if (shouldRenderThisFrame()) updateFBO();
}
void JPbox_cam::updateFBO()
{
	if (onoff.boolValue)
	{
		// Shared with the NDI and Spout boxes, which carry the identical
		// transform. At ratio 1.0 this reproduces the previous ofMap maths
		// exactly, so existing compositions are unaffected.
		const float zoom = scaleRatioIndex >= 0 ?
			parameters.getFloatValue(scaleRatioIndex) : 1.0f;
		const jp_media::JPMediaRect r = jp_media::legacyTransformRect(
			parameters.getFloatValue(0), parameters.getFloatValue(1),
			parameters.getFloatValue(2), parameters.getFloatValue(3),
			zoom, jp_constants::renderWidth, jp_constants::renderHeight);
		ofSetRectMode(OF_RECTMODE_CORNER);
		fbo.begin();
		ofClear(0, 255);
		// ofClear does not touch the draw colour, and the colour is global GL
		// state shared with every other box updated this frame. Without this
		// the camera comes out tinted by whatever ran before it.
		ofSetColor(255, 255);
		if (cameraSource && cameraSource->isInitialized() &&
			!parameters.getBoolValue(5))
		{
			cameraSource->draw(r.x, r.y, r.width, r.height);
		}
		else if (cameraSource && cameraSource->isInitialized())
		{
			// Stretch fills the canvas, but the uniform zoom still applies -
			// same as Stretch in the media boxes' transformedRect.
			const float sw = jp_constants::renderWidth * zoom;
			const float sh = jp_constants::renderHeight * zoom;
			cameraSource->draw(
				(jp_constants::renderWidth - sw) * 0.5f,
				(jp_constants::renderHeight - sh) * 0.5f, sw, sh);
		}
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}
void JPbox_cam::draw()
{
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}

void JPbox_cam::clear()
{
	releaseCameraSource();
	JPbox::clear();
	cout << "CORRE CLEAR CAMARITA " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
