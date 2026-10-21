#include "jp_hand_tracking.h"
#include "../JPbox/jp_box_kinect2.h"
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
 std::mutex mutex; std::condition_variable wake; std::thread thread;
 std::atomic<uint64_t> generation{0}; std::atomic<int> mode{0};
 std::atomic<bool> closing{false},panel{false},loss{false};std::atomic<double> wanted{0};
 Snapshot published,current;
 ~Service(){close();}
 void close(){closing=true;++generation;wake.notify_all();if(thread.joinable())thread.join();}
 bool cancelled(uint64_t g){return closing||generation!=g;}
 void publish(Snapshot s,uint64_t g){std::lock_guard<std::mutex> l(mutex);if(!cancelled(g))published=std::move(s);}
 void status(const std::string &text,uint64_t g,bool sim=false){Snapshot s;s.status=text;s.simulated=sim;publish(s,g);}
 void pause(int ms){std::unique_lock<std::mutex> l(mutex);wake.wait_for(l,std::chrono::milliseconds(ms));}
#ifdef __linux__
 struct Process {
  pid_t pid=-1;int input=-1,output=-1;std::string pending;
  ~Process(){if(input>=0)::close(input);if(output>=0)::close(output);if(pid>0){kill(pid,SIGKILL);int status;while(waitpid(pid,&status,0)<0&&errno==EINTR){}}}
  bool launch(const std::string &dir){
   int in[2],out[2];if(pipe2(in,O_CLOEXEC))return false;
   if(pipe2(out,O_CLOEXEC)){::close(in[0]);::close(in[1]);return false;}
   posix_spawn_file_actions_t a;posix_spawn_file_actions_init(&a);
   posix_spawn_file_actions_adddup2(&a,in[0],STDIN_FILENO);
   posix_spawn_file_actions_adddup2(&a,out[1],STDOUT_FILENO);
   // stderr remains attached to Guipper's log; never an unread bounded pipe.
   std::string python=dir+"/venv/bin/python",script=dir+"/worker.py",model=dir+"/hand_landmarker.task";
   char *args[]={const_cast<char*>(python.c_str()),const_cast<char*>(script.c_str()),const_cast<char*>(model.c_str()),nullptr};
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
  const char *custom=getenv("GUIPPER_HAND_RUNTIME"),*xdg=getenv("XDG_DATA_HOME"),*home=getenv("HOME");
  std::string dir=custom?custom:std::string(xdg?xdg:((std::string(home?home:".")+"/.local/share").c_str()))+"/Guipper/hand-tracking";
  for(auto file:{"/venv/bin/python","/worker.py","/hand_landmarker.task"})if(access((dir+file).c_str(),R_OK)){status("Runtime ausente: scripts/install_hand_tracking_linux.sh",g);return;}
  Process process;if(!process.launch(dir)){status("Error al iniciar Python",g);return;}
  std::string line;if(!process.line(*this,g,line,now()+30)){status("Error: runtime no responde al iniciar",g);return;}
  try{auto ready=ofJson::parse(line);if(ready.value("protocol",0)!=1)throw std::runtime_error("protocolo");}catch(...){status("Error: protocolo de runtime incompatible",g);return;}
  std::shared_ptr<KinectV2CaptureSource> source;std::shared_ptr<void> lease;
  Associator assoc;uint64_t serial=0,dropped=0;double last=0;
  while(!cancelled(g)){
   if(!panel&&now()-wanted>.5){lease.reset();source.reset();pause(50);continue;}
   if(!source){source=JPbox_kinect2::acquireSharedCapture();lease=JPbox_kinect2::handLease(source);serial=0;}
   auto f=JPbox_kinect2::handFrame(source);
   if(!f||now()-f->captured>1){status("Sin camara: "+JPbox_kinect2::captureStatus(source),g);pause(40);continue;}
   if(f->serial==serial){pause(3);continue;}
   if(serial&&f->serial>serial+1)dropped+=f->serial-serial-1;serial=f->serial;
   unsigned char header[12];uint32_t size=f->rgb.size();uint64_t stamp=uint64_t(f->captured*1000);
   for(int i=0;i<4;++i)header[i]=(size>>(i*8))&255;for(int i=0;i<8;++i)header[4+i]=(stamp>>(i*8))&255;
   double deadline=now()+3;
   if(size!=960*540*3||!process.transfer(*this,g,process.input,header,12,true,deadline)||!process.transfer(*this,g,process.input,const_cast<unsigned char*>(f->rgb.data()),size,true,deadline)||!process.line(*this,g,line,deadline)){
    status("Error: detector desconectado o bloqueado. Reconectar.",g);return;
   }
   try{
    auto data=ofJson::parse(line);std::vector<Hand> detected;
    if(!data.at("hands").is_array()||data.at("hands").size()>2)throw std::runtime_error("manos");
    for(const auto &item:data.at("hands")){Hand h;h.right=item.at("right");h.sideScore=item.at("score");const auto &pts=item.at("points");if(pts.size()!=21)throw std::runtime_error("puntos");
     for(int i=0;i<21;++i){h.points[i]={pts[i][0].get<float>(),pts[i][1].get<float>()};if(!std::isfinite(h.points[i].x)||!std::isfinite(h.points[i].y)||std::abs(h.points[i].x)>2||std::abs(h.points[i].y)>2)throw std::runtime_error("coordenadas");}
     measureDepth(h,*f);detected.push_back(h);
    }
    Snapshot s;s.frame=f;s.captured=f->captured;s.inferenceMs=data.at("ms");s.hands=assoc.update(detected,f->captured);s.dropped=dropped;
    double time=now();s.inferenceFps=last>0?float(1/(time-last)):0;last=time;
    s.status=time-s.captured>.25?"Resultado vencido":s.hands.empty()?"Buscando mano":"Siguiendo";publish(std::move(s),g);
   }catch(const std::exception &){status("Error: respuesta invalida del detector",g);return;}
  }
 }
#endif
 void run(){
#ifdef __linux__
  sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
#endif
  uint64_t handled=0;
  while(!closing){uint64_t g=generation;if(g==handled||mode==0){pause(30);continue;}handled=g;
   if(mode==2){while(!cancelled(g)){Snapshot s=simulatedSnapshot(now(),loss);publish(std::move(s),g);pause(33);}continue;}
#ifdef __linux__
   real(g);
#else
   status("Captura no disponible en esta plataforma",g);
#endif
  }
 }
};
Service &service(){static Service s;return s;}
}
Snapshot simulatedSnapshot(double time,bool lost){
 Snapshot s;s.simulated=true;s.captured=time;s.inferenceFps=30;s.inferenceMs=0;
 auto f=std::make_shared<Frame>();f->serial=uint64_t(time*30);f->captured=time;f->captureFps=30;f->rgb.resize(960*540*3,24);s.frame=f;
 if(!lost)for(bool right:{false,true}){Hand h;h.right=right;h.sideScore=1;h.id=right?2:1;h.meters=1.1f+.3f*std::sin(time*.5);h.depthValid=true;
  float x=(right?.65f:.3f)+.08f*std::sin(time),y=.65f;h.points[0]={x,y};
  for(int finger=0;finger<5;++finger)for(int joint=0;joint<4;++joint)h.points[1+finger*4+joint]={x+(finger-2)*.035f,y-.045f*(joint+1)};
  h.points[4]={h.points[8].x-.015f-.08f*float((1+std::sin(time*2))/2),h.points[8].y};h.palm=palm(h);s.hands.push_back(h);
 }s.status=lost?"Simulacion: sin seguimiento":"Simulacion: siguiendo";return s;
}
void start(bool sim){auto &s=service();{std::lock_guard<std::mutex> l(s.mutex);++s.generation;s.mode=sim?2:1;s.published={};s.published.status="Iniciando";s.published.simulated=sim;}if(!s.thread.joinable())s.thread=std::thread([&s]{s.run();});s.wanted=now();s.wake.notify_all();}
void stop(){auto &s=service();std::lock_guard<std::mutex> l(s.mutex);++s.generation;s.mode=0;s.published={};s.current={};s.wake.notify_all();}
void reconnect(){start(simulation());}
void shutdown(){service().close();}
void beginFrame(){auto &s=service();std::lock_guard<std::mutex> l(s.mutex);s.current=s.published;if(s.current.captured>0&&now()-s.current.captured>.25)s.current.status="Resultado vencido";}
void demand(){service().wanted=now();}
void setPanelVisible(bool value){service().panel=value;}
const Snapshot &snapshot(){return service().current;}
bool running(){return service().mode!=0;}bool simulation(){return service().mode==2;}void simulateLoss(bool v){service().loss=v;}
}
