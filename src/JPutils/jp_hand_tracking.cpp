#include "jp_hand_tracking.h"
#include "jp_hand_resize.h"
#include "../JPbox/jp_box_kinect2.h"
#include "../JPbox/jp_box_iphonelidar.h"
#include "../JPbox/jp_box_cam.h"
#include <map>
#include <set>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef __linux__
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace jp_hand {
namespace {
struct Service {
 Source key;std::set<const void*> users,previousUsers;bool dormant=false;
 std::mutex mutex; std::condition_variable wake; std::thread thread;
 std::atomic<uint64_t> generation{0}; std::atomic<int> mode{0};
 std::atomic<bool> closing{false},panel{false},loss{false},reconnecting{false};std::atomic<double> wanted{0};
 Snapshot published,current;FrameProvider provider;
 std::vector<std::pair<jp_crowd::Config,double>> crowdRequests;int backgroundCommand=0;uint64_t backgroundGeneration=0;
 ~Service(){close();}
 void close(){closing=true;++generation;wake.notify_all();if(thread.joinable())thread.join();}
 bool cancelled(uint64_t g){return closing||generation!=g;}
 void publish(Snapshot s,uint64_t g){std::lock_guard<std::mutex> l(mutex);if(!cancelled(g))published=std::move(s);}
 void status(const std::string &text,uint64_t g,bool sim=false){Snapshot s;s.status=text;s.simulated=sim;publish(s,g);}
 void pause(int ms){std::unique_lock<std::mutex> l(mutex);wake.wait_for(l,std::chrono::milliseconds(ms));}
 std::vector<jp_crowd::Config> crowdConfigs(){
  std::lock_guard<std::mutex> lock(mutex);std::vector<jp_crowd::Config> configs;
  const double time=now();
  crowdRequests.erase(std::remove_if(crowdRequests.begin(),crowdRequests.end(),[&](const auto &r){return time-r.second>1;}),crowdRequests.end());
  for(const auto &r:crowdRequests)configs.push_back(r.first);
  return configs;
 }
 void crowd(uint64_t g,bool simulated){
  const bool phone=key.kind==SourceKind::IPhoneLidar;
  if(key.kind!=SourceKind::KinectIR&&!phone){status("Publico requiere profundidad: Kinect o iPhone LiDAR",g,simulated);return;}
  jp_crowd::Engine engine;FrameProvider frames;{std::lock_guard<std::mutex> l(mutex);frames=provider;}
  std::shared_ptr<KinectV2CaptureSource> source;std::shared_ptr<IPhoneLidarCaptureSource> phoneSource;std::shared_ptr<void> lease;
  uint64_t serial=0,dropped=0;double last=0,started=now();
  while(!cancelled(g)){
   if(!panel&&now()-wanted>.5){dormant=true;status("En pausa: sin vinculos activos",g,simulated);return;}
   std::shared_ptr<const Frame> f;
   if(simulated){
    auto mock=std::make_shared<Frame>();mock->width=mock->depthWidth=mock->nativeWidth=512;mock->height=mock->depthHeight=mock->nativeHeight=424;mock->captured=now();mock->serial=serial+1;mock->captureFps=30;mock->infrared=true;mock->depth.resize(512*424,4500);
    if(!loss){float center=.5f+.25f*std::sin(mock->captured);for(int y=90;y<380;++y)for(int x=0;x<512;++x)if(std::abs(x/512.f-center)<.12f)mock->depth[y*512+x]=3600+150*std::sin(mock->captured*2);}
    else std::fill(mock->depth.begin(),mock->depth.end(),0);f=mock;
   }else if(frames)f=frames();
   else if(phone){
    if(!phoneSource){phoneSource=JPbox_iphonelidar::acquireSharedCapture();if(reconnecting.exchange(false))JPbox_iphonelidar::reconnectShared(phoneSource);lease=JPbox_iphonelidar::audienceLease(phoneSource);}
    f=JPbox_iphonelidar::audienceFrame(phoneSource);
   }else{
    if(!source){source=JPbox_kinect2::acquireSharedCapture();if(reconnecting.exchange(false))JPbox_kinect2::reconnectShared(source);lease=JPbox_kinect2::audienceLease(source);}
    f=JPbox_kinect2::audienceFrame(source);
   }
   if(!f||now()-f->captured>.25){engine.missing();status(simulated?"Simulacion: sin profundidad":"Sin profundidad: "+(phone?JPbox_iphonelidar::captureStatus(phoneSource):JPbox_kinect2::captureStatus(source)),g,simulated);pause(30);continue;}
   if(f->captured<started||f->serial==serial){pause(3);continue;}
   if(serial&&f->serial>serial+1)dropped+=f->serial-serial-1;serial=f->serial;
   const auto began=now();auto grid=jp_crowd::prepare(f->depth,f->depthWidth,f->depthHeight,f->captured);
   if(!grid){engine.missing();status("Sin profundidad valida",g,simulated);pause(30);continue;}
   int command=0;{std::lock_guard<std::mutex> lock(mutex);if(backgroundGeneration==g){command=backgroundCommand;backgroundCommand=0;}}if(command==1)engine.learn();if(command==2)engine.clearBackground();
   auto analysis=engine.process(grid,crowdConfigs());
   // Preview is generated from depth, never color or a model's estimated Z.
   auto preview=std::make_shared<Frame>();preview->width=preview->nativeWidth=f->depthWidth;preview->height=preview->nativeHeight=f->depthHeight;preview->depthWidth=preview->depthHeight=0;preview->captured=f->captured;preview->serial=f->serial;preview->captureFps=f->captureFps;preview->infrared=true;preview->rgb.resize(size_t(preview->width)*preview->height*3);
   for(int y=0;y<preview->height;++y)for(int x=0;x<preview->width;++x){float z=grid->meters[(y/4)*grid->width+x/4];auto c=z>0?static_cast<unsigned char>(35+200*std::clamp((4.5f-z)/4,0.f,1.f)):0;size_t i=(y*preview->width+x)*3;preview->rgb[i]=preview->rgb[i+1]=preview->rgb[i+2]=c;}
   Snapshot s;s.frame=preview;s.captured=f->captured;s.crowd=std::move(analysis);s.simulated=simulated;s.dropped=dropped;
   double time=now();s.inferenceMs=(time-began)*1000;s.inferenceFps=last>0?1/(time-last):0;last=time;
   s.status=simulated?"Simulacion: publico":phone?"Publico: LiDAR del iPhone activo":"Publico: profundidad activa";
   if(s.crowd->background==jp_crowd::Background::Learning)s.status="Aprendiendo fondo · pista vacia";
   publish(std::move(s),g);pause(simulated?33:2);
  }
 }

 // The phone as a controller: no image and no model, only the pose Record3D
 // sends with every frame. Holding the source keeps the USB stream open.
 void pose(uint64_t g,bool simulated){
  if(key.kind!=SourceKind::IPhoneLidar){status("Movimiento requiere el iPhone",g,simulated);return;}
  std::shared_ptr<IPhoneLidarCaptureSource> phoneSource;uint64_t serial=0;double last=0;
  while(!cancelled(g)){
   if(!panel&&now()-wanted>.5){dormant=true;status("En pausa: sin vinculos activos",g,simulated);return;}
   Snapshot s;s.simulated=simulated;
   if(simulated){
    const double t=now();s.captured=t;auto &p=s.phone;p.valid=true;
    p.tiltX=float(20*std::sin(t*1.1));p.tiltY=float(25*std::sin(t*.7));p.turn=float(40*std::sin(t*.3));
    p.x=float(.2*std::sin(t*.5));p.y=float(1.2+.1*std::sin(t*.9));p.z=float(.2*std::cos(t*.5));
    s.status="Simulacion: movimiento del telefono";publish(std::move(s),g);pause(16);continue;
   }
   if(!phoneSource){phoneSource=JPbox_iphonelidar::acquireSharedCapture();if(reconnecting.exchange(false))JPbox_iphonelidar::reconnectShared(phoneSource);}
   const auto sample=JPbox_iphonelidar::latestPose(phoneSource);
   if(!sample.serial||now()-sample.captured>.25){status("Sin movimiento: "+JPbox_iphonelidar::captureStatus(phoneSource),g);pause(30);continue;}
   if(sample.serial==serial){pause(3);continue;}serial=sample.serial;
   s.phone=phonePose(sample.qx,sample.qy,sample.qz,sample.qw,sample.tx,sample.ty,sample.tz);s.captured=sample.captured;
   const double time=now();s.inferenceFps=last>0?float(1/(time-last)):0;last=time;
   s.status=s.phone.valid?"Telefono: movimiento activo":"Pose invalida";
   publish(std::move(s),g);pause(2);
  }
 }

#ifdef __linux__
 struct Process {
  pid_t pid=-1;int input=-1,output=-1;std::string pending;
  ~Process(){if(input>=0)::close(input);if(output>=0)::close(output);if(pid>0){kill(pid,SIGKILL);int status;while(waitpid(pid,&status,0)<0&&errno==EINTR){}}}
  bool launch(const std::string &dir,bool body){
   int in[2],out[2];if(pipe2(in,O_CLOEXEC))return false;
   if(pipe2(out,O_CLOEXEC)){::close(in[0]);::close(in[1]);return false;}
   posix_spawn_file_actions_t a;posix_spawn_file_actions_init(&a);
   posix_spawn_file_actions_adddup2(&a,in[0],STDIN_FILENO);
   posix_spawn_file_actions_adddup2(&a,out[1],STDOUT_FILENO);
   // stderr remains attached to Guipper's log; never an unread bounded pipe.
   std::string python=dir+"/venv/bin/python",script=dir+"/worker.py",model=dir+(body?"/pose_landmarker_lite.task":"/hand_landmarker.task"),detector=body?"body":"hands";
   char *args[]={const_cast<char*>(python.c_str()),const_cast<char*>(script.c_str()),const_cast<char*>(model.c_str()),const_cast<char*>(detector.c_str()),nullptr};
   int error=posix_spawn(&pid,python.c_str(),&a,nullptr,args,environ);
   posix_spawn_file_actions_destroy(&a);::close(in[0]);::close(out[1]);
   if(error){pid=-1;::close(in[1]);::close(out[0]);return false;}
   input=in[1];output=out[0];fcntl(input,F_SETFL,O_NONBLOCK);fcntl(output,F_SETFL,O_NONBLOCK);return true;
  }
  bool transfer(Service &s,uint64_t g,int fd,void *data,size_t size,bool writing,double deadline){
   auto *bytes=static_cast<unsigned char*>(data);size_t done=0;
   while(done<size&&!s.cancelled(g)&&now()<deadline){pollfd p{fd,short(writing?POLLOUT:POLLIN),0};int r=poll(&p,1,25);
    if(r<0&&errno==EINTR)continue;if(r<0)return false;if(!r)continue;
    ssize_t n=writing?::write(fd,bytes+done,size-done) : ::read(fd,bytes+done,size-done);
    if(n>0)done+=n;else if(n==0)return false;else if(errno!=EINTR&&errno!=EAGAIN)return false;
   }return done==size;
  }
  bool line(Service &s,uint64_t g,std::string &line,double deadline){
   while(!s.cancelled(g)&&now()<deadline){auto e=pending.find('\n');if(e!=std::string::npos){line=pending.substr(0,e);pending.erase(0,e+1);return true;}
    if(pending.size()>65536)return false;pollfd p{output,POLLIN,0};int r=poll(&p,1,25);if(r<0&&errno==EINTR)continue;if(r<0)return false;if(!r)continue;
    char b[4096];ssize_t n=::read(output,b,sizeof(b));if(n>0)pending.append(b,n);else if(n==0)return false;else if(errno!=EINTR&&errno!=EAGAIN)return false;
   }return false;
  }
 };
 void real(uint64_t g){
  if(key.kind!=SourceKind::KinectRGB&&key.kind!=SourceKind::KinectIR&&key.kind!=SourceKind::Camera){status(key.kind==SourceKind::IPhoneLidar?"iPhone LiDAR solo alimenta Publico":"Fuente no compatible. Seleccionar otra fuente.",g);return;}
  const bool body=key.detector==Detector::Body;
  FrameProvider frames;{std::lock_guard<std::mutex> l(mutex);frames=provider;}
  const char *custom=getenv("GUIPPER_HAND_RUNTIME"),*xdg=getenv("XDG_DATA_HOME"),*home=getenv("HOME");
  std::string dir=custom?custom:std::string(xdg?xdg:((std::string(home?home:".")+"/.local/share").c_str()))+"/Guipper/hand-tracking";
  for(auto file:{"/venv/bin/python","/worker.py",body?"/pose_landmarker_lite.task":"/hand_landmarker.task"})if(access((dir+file).c_str(),R_OK)){status("Runtime ausente: scripts/install_hand_tracking_linux.sh",g);return;}
  Process process;if(!process.launch(dir,body)){status("Error al iniciar Python",g);return;}
  std::string line;if(!process.line(*this,g,line,now()+30)){status("Error: runtime no responde al iniciar",g);return;}
  try{auto ready=ofJson::parse(line);if(ready.value("protocol",0)!=2||ready.value("detector",std::string("hands"))!=(body?"body":"hands")||(body&&ready.value("max_bodies",0)!=MaxBodies))throw std::runtime_error("protocolo");}catch(...){status("Actualizar runtime: scripts/install_hand_tracking_linux.sh",g);return;}
  std::shared_ptr<KinectV2CaptureSource> source;std::shared_ptr<void> lease;
  std::shared_ptr<JPCameraCaptureSource> camera;
  Associator assoc;BodyAssociator bodyAssoc;uint64_t serial=0,dropped=0;double last=0;const double started=now();
  const bool infrared=key.kind==SourceKind::KinectIR;
  while(!cancelled(g)){
   if(!panel&&now()-wanted>.5){
    if(body){{std::lock_guard<std::mutex> l(mutex);if(!cancelled(g))mode=0;}status("Detenido: sin vinculos activos. Iniciar para reasignar.",g);}
    else{dormant=true;status("En pausa: sin vinculos activos",g);}
    return;
   }
   if(!frames){
    if(key.kind==SourceKind::Camera){
     if(!camera){camera=JPbox_cam::acquireCameraByKey(key.device);if(camera&&reconnecting.exchange(false))camera->reconnect();}
    }else if(!source){source=JPbox_kinect2::acquireSharedCapture();if(reconnecting.exchange(false))JPbox_kinect2::reconnectShared(source);lease=JPbox_kinect2::handLease(source,infrared);}
   }
   auto f=frames?frames():key.kind==SourceKind::Camera?(camera?camera->frame():nullptr):JPbox_kinect2::handFrame(source,infrared);
   if(!f||now()-f->captured>1){status(key.kind==SourceKind::Camera?(camera?camera->status():"Seleccionar una camara"):"Sin camara: "+JPbox_kinect2::captureStatus(source),g);pause(40);continue;}
   if(f->captured<started){pause(5);continue;}
   if(f->serial==serial){pause(3);continue;}
   if(serial&&f->serial>serial+1)dropped+=f->serial-serial-1;serial=f->serial;
   if(!f->valid()){status("Error: formato de captura invalido",g);return;}
   if(key.kind==SourceKind::Camera&&(f->width>960||f->height>540)){
    f=std::make_shared<Frame>(fitRgbFrame(*f));
   }
   unsigned char header[32];uint32_t size=f->rgb.size();uint64_t stamp=uint64_t(f->captured*1000);
   auto put=[&](int offset,uint64_t value,int bytes){for(int i=0;i<bytes;++i)header[offset+i]=(value>>(i*8))&255;};
   put(0,size,4);put(4,f->width,4);put(8,f->height,4);put(12,1,4);put(16,stamp,8);put(24,f->serial,8);
   double deadline=now()+3;
   if(!process.transfer(*this,g,process.input,header,32,true,deadline)||!process.transfer(*this,g,process.input,const_cast<unsigned char*>(f->rgb.data()),size,true,deadline)||!process.line(*this,g,line,deadline)){
    status("Error: detector desconectado o bloqueado. Reconectar.",g);return;
   }
   try{
    auto data=ofJson::parse(line);if(data.at("serial").get<uint64_t>()!=serial)throw std::runtime_error("cuadro anterior");std::vector<Hand> detected;
    Snapshot s;s.frame=f;s.captured=f->captured;s.inferenceMs=data.at("ms");s.dropped=dropped;
    if(body){
     const auto &poses=data.at("bodies");if(!poses.is_array()||poses.size()>MaxBodies)throw std::runtime_error("cuerpos");
     std::vector<Body> bodies;
     for(const auto &points:poses){
      if(!points.is_array()||points.size()!=33)throw std::runtime_error("cuerpo");Body b;b.present=true;b.aspect=float(f->width)/f->height;
      for(size_t i=0;i<points.size();++i){if(points[i].size()!=4)throw std::runtime_error("punto corporal");auto &p=b.points[i];p.position={points[i][0].get<float>(),points[i][1].get<float>()};p.visibility=points[i][2];p.presence=points[i][3];if(!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||!std::isfinite(p.visibility)||!std::isfinite(p.presence))throw std::runtime_error("coordenadas");}
      measureBodyDepth(b,*f);bodies.push_back(b);
     }
     s.bodies=bodyAssoc.update(std::move(bodies),f->captured);s.bodyNeedsReassign=bodyAssoc.needsReassign;
    }else{
    if(!data.at("hands").is_array()||data.at("hands").size()>2)throw std::runtime_error("manos");
    for(const auto &item:data.at("hands")){Hand h;h.right=item.at("right");
     // The runtime's existing label convention is calibrated for Kinect's
     // native image. V4L2 webcams need the opposite label, not mirrored pixels.
     if(key.kind==SourceKind::Camera)h.right=!h.right;
     h.sideScore=item.at("score");const auto &pts=item.at("points");if(pts.size()!=21)throw std::runtime_error("puntos");
     for(int i=0;i<21;++i){h.points[i]={pts[i][0].get<float>(),pts[i][1].get<float>()};if(!std::isfinite(h.points[i].x)||!std::isfinite(h.points[i].y)||std::abs(h.points[i].x)>2||std::abs(h.points[i].y)>2)throw std::runtime_error("coordenadas");}
     measureDepth(h,*f);detected.push_back(h);
    }
    s.hands=assoc.update(detected,f->captured);
    }
    double time=now();s.inferenceFps=last>0?float(1/(time-last)):0;last=time;
    s.status=time-s.captured>.25?"Resultado vencido":body?(s.bodyNeedsReassign?"Cruce ambiguo: recuperando personas":s.bodies.empty()?"Buscando personas":"Siguiendo "+std::to_string(s.bodies.size())+" persona(s)"):(s.hands.empty()?"Buscando mano":"Siguiendo");publish(std::move(s),g);
   }catch(const std::exception &){status("Error: respuesta invalida del detector",g);return;}
  }
 }
#endif
 void run(){
#ifdef __linux__
  sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
#endif
  uint64_t handled=0;
  while(!closing){uint64_t g=generation;if(!panel&&now()-wanted>.5){pause(30);continue;}if(g==handled||mode==0){pause(30);continue;}handled=g;
   if(key.detector==Detector::Crowd){dormant=false;crowd(g,mode==2);if(dormant)handled=0;continue;}
   if(key.detector==Detector::Pose){dormant=false;pose(g,mode==2);if(dormant)handled=0;continue;}
   if(mode==2){std::array<BodyMotion,MaxBodies> simulatedMotion;while(!cancelled(g)){if(!panel&&now()-wanted>.5){pause(50);continue;}Snapshot s=simulatedSnapshot(now(),loss,key.detector,MaxBodies);
     if(key.kind!=SourceKind::KinectRGB){auto f=std::make_shared<Frame>(*s.frame);f->width=f->nativeWidth=key.kind==SourceKind::KinectIR?512:640;f->height=f->nativeHeight=key.kind==SourceKind::KinectIR?424:480;f->rgb.assign(f->width*f->height*3,24);f->infrared=key.kind==SourceKind::KinectIR;f->depthWidth=f->depthHeight=0;s.frame=f;for(auto &b:s.bodies){b.aspect=float(f->width)/f->height;if(key.kind==SourceKind::Camera)b.depthValid=false;}for(auto &h:s.hands){h.aspect=float(f->width)/f->height;if(key.kind==SourceKind::Camera)h.depthValid=false;}}
     for(auto &b:s.bodies)simulatedMotion[b.player-1].update(b,s.captured);
     if(s.bodies.empty())for(auto &motion:simulatedMotion)motion.clear();
     publish(std::move(s),g);pause(33);}continue;}
#ifdef __linux__
   dormant=false;real(g);if(dormant)handled=0;
#else
   status("Captura no disponible en esta plataforma",g);
#endif
  }
 }
};
// Only main-thread entry points access this registry. Each service owns one
// process and immutable mailbox; no registry lock ever surrounds USB or waitpid.
std::map<Source,std::unique_ptr<Service>> &registry(){static std::map<Source,std::unique_ptr<Service>> entries;return entries;}
uint64_t nextGeneration(){static uint64_t serial=0;return ++serial;}
Service &service(const Source &key){auto &entry=registry()[key];if(!entry){entry=std::make_unique<Service>();entry->key=key;entry->generation=nextGeneration();}return *entry;}

}
void startWithFrameProvider(FrameProvider provider,const Source &source){auto &s=service(source);{std::lock_guard<std::mutex> l(s.mutex);s.provider=std::move(provider);}start(false,source);}
void start(bool sim,const Source &source){auto &s=service(source);{std::lock_guard<std::mutex> l(s.mutex);s.generation=nextGeneration();s.mode=sim?2:1;s.backgroundCommand=0;s.published={};s.current={};s.published.status="Iniciando";s.published.simulated=sim;}if(!s.thread.joinable())s.thread=std::thread([&s]{s.run();});s.wanted=now();s.wake.notify_all();}
void stop(const Source &source){auto &s=service(source);std::lock_guard<std::mutex> l(s.mutex);s.generation=nextGeneration();s.mode=0;s.backgroundCommand=0;s.provider={};s.published={};s.current={};s.wake.notify_all();}
void stopAll(){for(auto &entry:registry())stop(entry.first);}
void reconnect(const Source &source){
 if(source.kind==SourceKind::Camera){std::vector<Source> keys;for(auto &entry:registry())if(entry.first.kind==SourceKind::Camera&&entry.first.device==source.device&&entry.second->mode!=0)keys.push_back(entry.first);if(std::find(keys.begin(),keys.end(),source)==keys.end())keys.push_back(source);for(auto &key:keys){service(key).reconnecting=(key==source);start(simulation(key),key);}return;}
 // Both Kinect modalities share one device reset; the iPhone is a device of
 // its own. Every participating session changes generation first, so no
 // pre-reconnect response can reach a binding.
 const bool phone=source.kind==SourceKind::IPhoneLidar;
 std::vector<Source> keys;for(auto &entry:registry())if(entry.first.kind!=SourceKind::Camera&&(entry.first.kind==SourceKind::IPhoneLidar)==phone&&entry.second->mode!=0)keys.push_back(entry.first);
 if(std::find(keys.begin(),keys.end(),source)==keys.end())keys.push_back(source);
 for(auto &key:keys){service(key).reconnecting=(key==source);start(simulation(key),key);}
}
void shutdown(){for(auto &entry:registry())entry.second->close();}
uint64_t session(const Source &source){return service(source).generation;}
void beginFrame(){
#ifdef TARGET_LINUX
 JPbox_cam::collectCameraTextures();
#endif
 for(auto &entry:registry()){auto &s=*entry.second;std::lock_guard<std::mutex> l(s.mutex);s.current=s.published;s.previousUsers.swap(s.users);s.users.clear();if(s.current.captured>0&&now()-s.current.captured>.25)s.current.status="Resultado vencido";}
}
void demand(const Source &source,const void *consumer){auto &s=service(source);s.wanted=now();if(consumer)s.users.insert(consumer);}
void demandCrowd(const Source &source,const jp_crowd::Config &config){
 if(source.detector!=Detector::Crowd||!config.valid())return;auto &s=service(source);std::lock_guard<std::mutex> lock(s.mutex);
 for(auto &request:s.crowdRequests)if(request.first==config){request.second=now();return;}
 if(s.crowdRequests.size()<64)s.crowdRequests.push_back({config,now()});
}
void learnCrowdBackground(const Source &source){if(source.detector==Detector::Crowd&&running(source)){auto &s=service(source);std::lock_guard<std::mutex> lock(s.mutex);s.backgroundGeneration=s.generation;s.backgroundCommand=1;}}
void clearCrowdBackground(const Source &source){if(source.detector==Detector::Crowd){auto &s=service(source);std::lock_guard<std::mutex> lock(s.mutex);s.backgroundGeneration=s.generation;s.backgroundCommand=2;}}
void setPanelVisible(bool value,const Source &source){for(auto &entry:registry())entry.second->panel=false;if(value)service(source).panel=true;}
const Snapshot &snapshot(const Source &source){return service(source).current;}
bool running(const Source &source){return service(source).mode!=0;}
bool simulation(const Source &source){return service(source).mode==2;}
void simulateLoss(bool v,const Source &source){service(source).loss=v;}
size_t consumers(const Source &source){return service(source).previousUsers.size();}
std::vector<SourceInfo> sources(){
 std::vector<SourceInfo> result={{{SourceKind::KinectRGB,{}} ,"Kinect RGB",JPbox_kinect2::isDriverAvailable()},{{SourceKind::KinectIR,{}} ,"Kinect infrarrojo (experimental)",JPbox_kinect2::isDriverAvailable()},{{SourceKind::IPhoneLidar,{}},"iPhone LiDAR (Record3D)",JPbox_iphonelidar::isDriverAvailable()}};
#ifdef TARGET_LINUX
 auto cameras=JPbox_cam::handCameraSources();result.insert(result.end(),cameras.begin(),cameras.end());
#endif
 return result;
}
void refreshSources(){JPbox_cam::rescanCameraDevices();}
}
