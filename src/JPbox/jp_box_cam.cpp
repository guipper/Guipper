#include "jp_box_cam.h"
#include <limits>
#include <map>

#ifdef TARGET_LINUX
// All device operations run on this worker. Render-only textures live in a
// separate weak cache, so releasing the last tracker never destroys GL objects.
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
struct JPCameraCaptureSource::State {
 std::mutex mutex;std::shared_ptr<const jp_hand::Frame> frame;
 std::string key,status="Iniciando camara";int width=1280,height=720;
 std::atomic<bool> reconnect{false};
};
namespace {
struct CameraDeviceInfo {int id=-1;string name,key,path;};
struct CameraHub {
 static uint64_t &ephemeralEpoch(){static uint64_t epoch=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());return epoch;}
 std::mutex mutex;std::condition_variable wake;std::thread thread;
 bool closing=false,scan=true;uint64_t generation=0;
 vector<CameraDeviceInfo> devices;
 map<string,weak_ptr<JPCameraCaptureSource>> sources;
 vector<weak_ptr<JPCameraCaptureSource::State>> pending;
 struct Active {weak_ptr<JPCameraCaptureSource::State> state;std::unique_ptr<ofVideoGrabber> grabber;double retry=0,last=0;uint64_t serial=0;};
 ~CameraHub(){stop();}
 void stop(){{std::lock_guard<std::mutex> l(mutex);closing=true;}wake.notify_all();if(thread.joinable())thread.join();}
 void ensure(){if(!thread.joinable())thread=std::thread([this]{run();});}
 static string stableKey(const ofVideoDevice &d){
  namespace fs=std::filesystem;std::error_code ec;
  for(const char *directory:{"/dev/v4l/by-id","/dev/v4l/by-path"}){
   fs::directory_iterator it(directory,ec),end;
   if(ec){ec.clear();continue;}
   for(;it!=end;it.increment(ec)){if(ec)break;if(fs::equivalent(it->path(),d.hardwareName,ec))return it->path().string();ec.clear();}
  }
  if(!d.serialID.empty())return "serial:"+d.serialID+":"+d.deviceName;
  // No persistent identity: explicit session-only key, never match a different
  // physical device after re-enumeration by its ordinal number.
  return "session:"+std::to_string(ephemeralEpoch())+":"+d.hardwareName+":"+d.deviceName;
 }
 void run(){
  vector<Active> active;
  while(true){
   bool rescan=false;{std::lock_guard<std::mutex> l(mutex);if(closing)break;rescan=scan;scan=false;for(auto &s:pending)active.push_back({s,{}});pending.clear();}
   if(rescan){
    ++ephemeralEpoch();
    ofVideoGrabber probe;auto found=probe.listDevices();vector<CameraDeviceInfo> next;
    for(const auto &d:found)if(d.bAvailable)next.push_back({d.id,d.deviceName,stableKey(d),d.hardwareName});
    {std::lock_guard<std::mutex> l(mutex);devices=std::move(next);++generation;}
   }
   for(auto it=active.begin();it!=active.end();){
    auto state=it->state.lock();if(!state){it=active.erase(it);continue;}
    double time=jp_hand::now();
    auto status=[&](const string &s){std::lock_guard<std::mutex> l(state->mutex);state->status=s;state->frame.reset();};
    if(state->reconnect.exchange(false)){it->grabber.reset();it->retry=0;it->last=0;status("Reconectando camara");std::lock_guard<std::mutex> l(mutex);scan=true;}
    if(!it->grabber&&time>=it->retry){
     CameraDeviceInfo selected;{std::lock_guard<std::mutex> l(mutex);for(const auto &d:devices)if(d.key==state->key)selected=d;}
     it->retry=time+2;
     if(selected.id<0){status("Camara ausente. Actualizar camaras.");std::lock_guard<std::mutex> l(mutex);scan=true;}
     else {
      // Resolve the stable hardware path again in this grabber's own enumeration.
      auto grabber=std::make_unique<ofVideoGrabber>();int id=-1;
      for(auto &d:grabber->listDevices())if(stableKey(d)==state->key){id=d.id;break;}
      if(id>=0){grabber->setDeviceID(id);grabber->setPixelFormat(OF_PIXELS_RGB);grabber->setDesiredFrameRate(30);}
      if(id>=0&&grabber->setup(state->width,state->height,false)){it->grabber=std::move(grabber);it->last=time;}
      else status("Camara ocupada o sin acceso. Reconectar.");
     }
    }
    if(it->grabber){
     it->grabber->update();
     if(it->grabber->isFrameNew()){
      const auto &pixels=it->grabber->getPixels();
      if(pixels.getNumChannels()==3&&pixels.getWidth()>0&&pixels.getHeight()>0){
       auto frame=std::make_shared<jp_hand::Frame>();frame->serial=++it->serial;frame->captured=jp_hand::now();
       frame->width=frame->nativeWidth=pixels.getWidth();frame->height=frame->nativeHeight=pixels.getHeight();frame->depthWidth=frame->depthHeight=0;
       frame->captureFps=it->last>0?float(1/(frame->captured-it->last)):0;frame->rgb.assign(pixels.begin(),pixels.end());it->last=frame->captured;
       {std::lock_guard<std::mutex> l(state->mutex);state->frame=std::move(frame);state->status="Capturando";}
      }
     }else if(time-it->last>2){it->grabber.reset();status("Camara desconectada. Reconectando...");std::lock_guard<std::mutex> l(mutex);scan=true;}
    }
    ++it;
   }
   std::unique_lock<std::mutex> l(mutex);wake.wait_for(l,std::chrono::milliseconds(5),[this]{return closing||scan||!pending.empty();});
  }
  active.clear(); // ofVideoGrabber closes on its owning worker.
 }
};
CameraHub &cameraHub(){static CameraHub hub;return hub;}
struct CameraTexture {weak_ptr<JPCameraCaptureSource::State> state;ofTexture texture;uint64_t serial=0;};
map<const JPCameraCaptureSource::State*,CameraTexture> &cameraTextures(){static map<const JPCameraCaptureSource::State*,CameraTexture> values;return values;}
vector<CameraDeviceInfo> discoverCameraDevices(){auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);h.ensure();return h.devices;}
uint64_t cameraRescanGeneration(){auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);return h.generation;}
shared_ptr<JPCameraCaptureSource> acquireCameraSource(int deviceId,int width,int height){
 for(auto &d:discoverCameraDevices())if(d.id==deviceId)return JPbox_cam::acquireCameraByKey(d.key,width,height);
 return {};
}
int findDefaultCameraListIndex(const vector<int> &ids){
 auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);
 for(size_t i=0;i<ids.size();++i)for(const auto &d:h.devices)if(d.id==ids[i]){auto existing=h.sources.find(d.key);if(existing==h.sources.end()||existing->second.expired())return int(i);}
 return ids.empty()?-1:0;
}
}
JPCameraCaptureSource::JPCameraCaptureSource(int deviceId,int width,int height):JPCameraCaptureSource([deviceId]{for(const auto &d:discoverCameraDevices())if(d.id==deviceId)return d.key;return string();}(),width,height){}

JPCameraCaptureSource::JPCameraCaptureSource(const string &key,int width,int height){
 state=std::make_shared<State>();state->key=key;state->width=width;state->height=height;
 auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);h.pending.push_back(state);h.ensure();h.wake.notify_all();
}
JPCameraCaptureSource::~JPCameraCaptureSource()=default;
std::shared_ptr<const jp_hand::Frame> JPCameraCaptureSource::frame()const{std::lock_guard<std::mutex> l(state->mutex);return state->frame;}
string JPCameraCaptureSource::status()const{std::lock_guard<std::mutex> l(state->mutex);return state->status;}
void JPCameraCaptureSource::reconnect(){state->reconnect=true;}
bool JPCameraCaptureSource::isInitialized()const{auto f=frame();return f&&jp_hand::now()-f->captured<2;}
void JPCameraCaptureSource::updateOnce(){
 auto &textures=cameraTextures();for(auto it=textures.begin();it!=textures.end();)if(it->second.state.expired())it=textures.erase(it);else ++it;
 auto f=frame();if(!f)return;auto &entry=textures[state.get()];if(entry.state.expired()){entry.texture.clear();entry.serial=0;}entry.state=state;
 if(entry.serial!=f->serial){entry.serial=f->serial;if(!entry.texture.isAllocated()||entry.texture.getWidth()!=f->width||entry.texture.getHeight()!=f->height)entry.texture.allocate(f->width,f->height,GL_RGB);entry.texture.loadData(f->rgb.data(),f->width,f->height,GL_RGB);}
}
const ofTexture &JPCameraCaptureSource::getTexture()const{return cameraTextures()[state.get()].texture;}
bool JPCameraCaptureSource::hasTexture()const{auto it=cameraTextures().find(state.get());return isInitialized()&&it!=cameraTextures().end()&&it->second.texture.isAllocated();}
void JPCameraCaptureSource::draw(float x,float y,float width,float height)const{if(hasTexture())getTexture().draw(x,y,width,height);}
shared_ptr<JPCameraCaptureSource> JPbox_cam::acquireCameraByKey(const string &key,int width,int height){
 if(key.empty())return {};auto &h=cameraHub();
 // Construction queues the state, so do not hold the hub mutex across it.
 static std::mutex poolMutex;std::lock_guard<std::mutex> pool(poolMutex);
 {std::lock_guard<std::mutex> l(h.mutex);if(auto existing=h.sources[key].lock())return existing;}
 auto source=std::make_shared<JPCameraCaptureSource>(key,width,height);
 {std::lock_guard<std::mutex> l(h.mutex);h.sources[key]=source;}return source;
}
vector<jp_hand::SourceInfo> JPbox_cam::handCameraSources(){vector<jp_hand::SourceInfo> out;for(auto &d:discoverCameraDevices())out.push_back({{jp_hand::SourceKind::Camera,d.key},d.name+" ("+d.path+")",true});return out;}
void JPbox_cam::collectCameraTextures(){auto &textures=cameraTextures();for(auto it=textures.begin();it!=textures.end();)if(it->second.state.expired())it=textures.erase(it);else ++it;}
void JPbox_cam::shutdownCameraSources(){cameraHub().stop();cameraTextures().clear();}

#else
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


#endif
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

#ifdef TARGET_LINUX
vector<pair<int,int>> JPbox_cam::openCameraSources(){auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);vector<pair<int,int>> out;for(auto &d:h.devices)if(auto source=h.sources[d.key].lock())out.push_back({d.id,int(source.use_count())-1});return out;}
#else
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

#endif

uint64_t JPbox_cam::cameraRescanCount()
{
	return cameraRescanGeneration();
}

void JPbox_cam::rescanCameraDevices()
{
#ifdef TARGET_LINUX
 auto &h=cameraHub();std::lock_guard<std::mutex> l(h.mutex);h.scan=true;h.ensure();h.wake.notify_all();
#else
	// Only the list is invalidated here. Boxes still hold their sources, so
	// nothing goes black; each one drops and reopens its device on its next
	// update, which is what picks up a device id that has moved.
	cameraDevicesScanned() = false;
	cameraDevices().clear();
	pruneExpiredCameraSources();
	cameraRescanGeneration()++;
	ofLogNotice("CAMARITA") << "Rescanning capture devices";
#endif
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
