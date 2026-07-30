#include "jp_box_cam.h"
#include <limits>
#include <map>

class JPCameraCaptureSource
{
public:
	JPCameraCaptureSource(int deviceId, int width, int height)
		: deviceId(deviceId)
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

	~JPCameraCaptureSource()
	{
		grabber.close();
	}

	void updateOnce()
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

	bool isInitialized() const
	{
		return initialized && grabber.isInitialized();
	}

	void draw(float x, float y, float width, float height) const
	{
		if (isInitialized())
		{
			grabber.draw(x, y, width, height);
		}
	}

private:
	int deviceId = -1;
	ofVideoGrabber grabber;
	bool initialized = false;
	uint64_t lastUpdateFrame = std::numeric_limits<uint64_t>::max();
};

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

	applyCameraIndexFromParameter();
	if (cameraSource)
	{
		cameraSource->updateOnce();
	}

	updateFBO();
}
void JPbox_cam::updateFBO()
{
	if (onoff.boolValue)
	{
		float mscalex = ofMap(parameters.getFloatValue(0), 0.0, 1.0, 0.0, jp_constants::renderWidth);
		float mscaley = ofMap(parameters.getFloatValue(1), 0.0, 1.0, 0.0, jp_constants::renderHeight);
		float moffsetx = ofMap(parameters.getFloatValue(2), 0.0, 1.0,
							   -jp_constants::renderWidth / 2 - mscalex / 2,
							   jp_constants::renderWidth / 2 + mscalex / 2);

		float moffsety = ofMap(parameters.getFloatValue(3), 0.0, 1.0,
							   -jp_constants::renderHeight / 2 - mscaley / 2,
							   jp_constants::renderHeight / 2 + mscaley / 2);
		ofSetRectMode(OF_RECTMODE_CORNER);
		fbo.begin();
		ofClear(0, 255);
		if (cameraSource && cameraSource->isInitialized() &&
			!parameters.getBoolValue(5))
		{
			cameraSource->draw(
				jp_constants::renderWidth / 2 - mscalex / 2 + moffsetx,
				jp_constants::renderHeight / 2 - mscaley / 2 + moffsety,
				mscalex,
				mscaley);
		}
		else if (cameraSource && cameraSource->isInitialized())
		{
			cameraSource->draw(
				0, 0,
				jp_constants::renderWidth,
				jp_constants::renderHeight);
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
