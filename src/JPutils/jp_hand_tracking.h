#pragma once
#include "jp_crowd_tracking.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <tuple>
#include <functional>
#include <string>
#include <vector>
namespace jp_hand {
inline double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// Saved by name, not number. IPhoneLidar is depth only: it feeds Publico.
enum class SourceKind { KinectRGB=0, KinectIR=1, Camera=2, IPhoneLidar=3 };
enum class Detector { Hands=0, Body=1, Crowd=2 };
struct Source {
 SourceKind kind=SourceKind::KinectRGB;
 std::string device;
 Detector detector=Detector::Hands;
 bool operator==(const Source &b)const{return kind==b.kind&&device==b.device&&detector==b.detector;}
 bool operator!=(const Source &b)const{return !(*this==b);}
 bool operator<(const Source &b)const{return std::tie(kind,device,detector)<std::tie(b.kind,b.device,b.detector);}
 bool hasDepth()const{return kind!=SourceKind::Camera;}
};
struct SourceInfo {Source source;std::string label;bool available=true;};
struct Point {float x=0,y=0;};
inline float distance(Point a,Point b,float aspect=16.f/9.f){return std::hypot((a.x-b.x)*aspect,a.y-b.y);}
struct Hand {std::array<Point,21> points{};bool right=true;float sideScore=0,aspect=16.f/9.f;uint64_t id=0;float meters=0;bool depthValid=false;Point palm;float depthRadiusX=0,depthRadiusY=0;};
struct Frame {
 uint64_t serial=0;double captured=0;
 int width=960,height=540,depthWidth=1920,depthHeight=1080;
 int nativeWidth=1920,nativeHeight=1080;
 std::vector<unsigned char> rgb;std::vector<float> depth;float captureFps=0;
 bool infrared=false;
 bool valid()const{return width>0&&height>0&&width<=4096&&height<=4096&&rgb.size()==size_t(width)*height*3;}
}; // Packed RGB8 (including normalized IR); optional metric depth shares its field of view.
// Percentile clipping avoids bright IR reflections controlling exposure. Smooth
// limits in time, not frames, to prevent brightness pumping at variable FPS.
struct IRNormalizer {
 float low=0,high=1;double previous=0;
 std::vector<unsigned char> convert(const float *values,size_t size,double time){
  std::vector<float> samples;samples.reserve(size/8+1);
  for(size_t i=0;i<size;i+=8)if(std::isfinite(values[i])&&values[i]>0)samples.push_back(std::log1p(values[i]));
  std::vector<unsigned char> out(size*3,0);
  if(samples.size()<16)return out;
  std::sort(samples.begin(),samples.end());float lo=samples[samples.size()/100],hi=samples[samples.size()*99/100];
  if(hi-lo<.01f)return out;
  float alpha=previous>0?1-std::exp(-float(std::max(0.,time-previous))/.5f):1;previous=time;
  low+=(lo-low)*alpha;high+=(hi-high)*alpha;
  for(size_t i=0;i<size;++i){float v=values[i];unsigned char c=std::isfinite(v)&&v>0?static_cast<unsigned char>(255*std::clamp((std::log1p(v)-low)/std::max(.01f,high-low),0.f,1.f)):0;out[i*3]=out[i*3+1]=out[i*3+2]=c;}
  return out;
 }
};
struct BodyPoint {Point position;float visibility=0,presence=0;bool valid()const{return visibility>=.6f&&presence>=.6f&&std::isfinite(position.x)&&std::isfinite(position.y)&&position.x>=0&&position.x<=1&&position.y>=0&&position.y<=1;}};
struct Body {
 std::array<BodyPoint,33> points{};bool present=false;float aspect=16.f/9.f;
 int player=0;Point center;float meters=0;bool depthValid=false;
 float energy=0,velocityX=0,velocityY=0,activityX=0,activityY=0;bool motionValid=false,activityValid=false;
};
constexpr int MaxBodies=4;
enum class BodySelection { Centered=0, Nearest=1, Player1=2, Player2=3, Group=4, Player3=5, Player4=6 };
inline BodySelection playerSelection(int player){return player<=2?BodySelection(player+1):BodySelection(player+2);}
inline int selectedPlayer(BodySelection selection){return selection==BodySelection::Player1?1:selection==BodySelection::Player2?2:selection==BodySelection::Player3?3:selection==BodySelection::Player4?4:0;}
inline std::string bodySelectionLabel(BodySelection selection){
 const char *names[]={"Auto: mas centrada","Auto: mas cercana","Persona 1","Persona 2","Promedio del grupo","Persona 3","Persona 4"};return names[std::clamp(int(selection),0,6)];
}
inline bool bodyCenter(Body &b){
 Point sum;int count=0;
 // Shoulders are required; hips improve stability when visible.
 for(int i:{11,12})if(!b.points[i].valid())return false;
 for(int i:{11,12,23,24})if(b.points[i].valid()){sum.x+=b.points[i].position.x;sum.y+=b.points[i].position.y;++count;}
 b.center={sum.x/count,sum.y/count};return true;
}
inline void measureBodyDepth(Body &b,const Frame &f){
 b.depthValid=false;b.meters=0;
 if(!bodyCenter(b)||f.depthWidth<=0||f.depthHeight<=0||f.depth.size()!=size_t(f.depthWidth)*f.depthHeight)return;
 // Interior torso patch in the same field of view as the detector input.
 int x=int(b.center.x*f.depthWidth),y=int(b.center.y*f.depthHeight);
 float width=std::abs(b.points[11].position.x-b.points[12].position.x)*f.depthWidth;
 int radius=std::clamp(int(width*.15f),2,12);std::vector<float> values;
 for(int j=std::max(0,y-radius);j<std::min(f.depthHeight,y+radius+1);++j)
  for(int i=std::max(0,x-radius);i<std::min(f.depthWidth,x+radius+1);++i){float z=f.depth[j*f.depthWidth+i];if(std::isfinite(z)&&z>=500&&z<=4500)values.push_back(z);}
 if(values.size()<9)return;
 std::sort(values.begin(),values.end());
 if(values[values.size()*9/10]-values[values.size()/10]>.3f*1000)return;
 b.meters=values[values.size()/2]*.001f;b.depthValid=true;
}
// Per-person temporal analysis, independent of render FPS and detector array
// order. Compare with an interpolated frame 150 ms ago; no derivative across
// losses, identity resets, resolution/aspect changes or implausible pose jumps.
class BodyMotion {
 struct Timed {Body body;double time;};std::deque<Timed> history;
public:
 void clear(){history.clear();}
 void update(Body &body,double time){
  body.energy=body.velocityX=body.velocityY=0;body.motionValid=body.activityValid=false;
  if(!history.empty()&&(time<=history.back().time||time-history.back().time>.25||std::abs(body.aspect-history.back().body.aspect)>.001f))history.clear();
  history.push_back({body,time});while(history.size()>32)history.pop_front();
  const double target=time-.15;
  while(history.size()>2&&history[1].time<=target)history.pop_front();
  if(history.size()<2||history.front().time>target+1e-6)return;
  const auto &a=history[0],&b=history[1];float t=std::clamp(float((target-a.time)/(b.time-a.time)),0.f,1.f);
  Body past=a.body;
  for(int i=0;i<33;++i){auto &p=past.points[i];const auto &q=b.body.points[i];
   if(!p.valid()||!q.valid()){p.visibility=0;continue;}
   p.position={p.position.x+(q.position.x-p.position.x)*t,p.position.y+(q.position.y-p.position.y)*t};
  }
  for(int i:{11,12})if(!body.points[i].valid()||!past.points[i].valid())return;
  const float oldWidth=distance(past.points[11].position,past.points[12].position,body.aspect);
  const float newWidth=distance(body.points[11].position,body.points[12].position,body.aspect);
  if(oldWidth<.03f||newWidth<.03f||std::max(oldWidth,newWidth)>std::min(oldWidth,newWidth)*1.5f)return;
  const float scale=(oldWidth+newWidth)*.5f;
  int count=0;float total=0,weightedX=0,weightedY=0;Point delta;int torsoCount=0;
  for(int i:{11,12,13,14,15,16,23,24,25,26,27,28}){
   if(!body.points[i].valid()||!past.points[i].valid())continue;
   const Point p=body.points[i].position,q=past.points[i].position;
   const float displacement=distance(p,q,body.aspect)/scale;
   if(displacement>1.2f){history.clear();return;} // Rewarm after a pose outlier.
   const float speed=std::max(0.f,displacement-.015f)/.15f;
   total+=speed;weightedX+=p.x*speed;weightedY+=(1-p.y)*speed;++count;
   if(i==11||i==12||i==23||i==24){delta.x+=p.x-q.x;delta.y+=q.y-p.y;++torsoCount;}
  }
  if(count<6||torsoCount<2)return;
  body.energy=total/count;body.motionValid=true;
  const float dx=delta.x/torsoCount,dy=delta.y/torsoCount;
  if(std::hypot(dx*body.aspect,dy)/scale>.015f){body.velocityX=dx/.15f;body.velocityY=dy/.15f;}
  if(total>.01f){body.activityX=weightedX/total;body.activityY=weightedY/total;body.activityValid=true;}
 }
};
// Slots are local to each source. Reserve briefly, then reacquire automatically.
class BodyAssociator {
 struct Track {Body body;Point velocity;double seen=0;bool occupied=false,blocked=false;};
 std::array<Track,MaxBodies> tracks{};std::array<BodyMotion,MaxBodies> motion;
 static float cost(const Body &b,const Track &t,double time){
  const float dt=std::clamp(float(time-t.seen),0.f,.25f);
  Point predicted{t.body.center.x+t.velocity.x*dt,t.body.center.y+t.velocity.y*dt};
  float d=distance(b.center,predicted,b.aspect);
  if(d>.45f)return 100;
  if(b.depthValid&&t.body.depthValid){float dz=std::abs(b.meters-t.body.meters);if(dz>.65f)return 100;d+=dz*.3f;}
  return d;
 }
public:
 bool needsReassign=false;
 std::vector<Body> update(std::vector<Body> detected,double time){
  detected.erase(std::remove_if(detected.begin(),detected.end(),[](Body &b){return !b.present||!bodyCenter(b);}),detected.end());
  if(detected.size()>MaxBodies)detected.resize(MaxBodies);
  std::sort(detected.begin(),detected.end(),[](const Body &a,const Body &b){return a.center.x<b.center.x;});
  // Reserve slots briefly, then allow automatic reacquisition (not biometric identity).
  for(int i=0;i<MaxBodies;++i)if(tracks[i].occupied&&time-tracks[i].seen>2){tracks[i]={};motion[i].clear();}
  bool ambiguous=false;
  for(size_t a=0;a<detected.size();++a)for(size_t b=a+1;b<detected.size();++b)
   if(distance(detected[a].center,detected[b].center,detected[a].aspect)<.16f&&
      !(detected[a].depthValid&&detected[b].depthValid&&std::abs(detected[a].meters-detected[b].meters)>.35f))ambiguous=true;
  float costs[MaxBodies][MaxBodies];
  for(int d=0;d<MaxBodies;++d)for(int t=0;t<MaxBodies;++t)
   costs[d][t]=d<int(detected.size())&&tracks[t].occupied&&!tracks[t].blocked?cost(detected[d],tracks[t],time):100;
  auto tied=[](std::array<float,MaxBodies> alternatives){std::sort(alternatives.begin(),alternatives.end());return alternatives[0]<.45f&&alternatives[1]-alternatives[0]<.08f;};
  for(size_t d=0;d<detected.size();++d){std::array<float,MaxBodies> row;for(int t=0;t<MaxBodies;++t)row[t]=costs[d][t];ambiguous|=tied(row);}
  for(int t=0;t<MaxBodies;++t){std::array<float,MaxBodies> column;for(int d=0;d<MaxBodies;++d)column[d]=costs[d][t];ambiguous|=tied(column);}
  if(ambiguous){for(auto &t:tracks)if(t.occupied)t.blocked=true;needsReassign=true;return {};}
  // Exhaustive bounded assignment: maximize matches, then minimize total cost.
  // Four slots require at most 5^4 branches; no detector-order greedy matching.
  std::array<int,MaxBodies> assignment,current;assignment.fill(-1);current.fill(-1);
  float best=1e9f;
  auto search=[&](auto &&self,size_t d,unsigned used,float score)->void{
   if(d==detected.size()){if(score<best){best=score;assignment=current;}return;}
   current[d]=-1;self(self,d+1,used,score+10);
   for(int t=0;t<MaxBodies;++t)if(!(used&(1u<<t))&&costs[d][t]<.45f){current[d]=t;self(self,d+1,used|(1u<<t),score+costs[d][t]);}
   current[d]=-1;
  };search(search,0,0,0);
  std::array<bool,MaxBodies> used{};for(int t:assignment)if(t>=0)used[t]=true;
  std::vector<Body> out;
  for(size_t d=0;d<detected.size();++d){int t=assignment[d];
   if(t<0){
    // An unmatched old person might be this detection. Do not assign a new
    // number until all existing slots have an unambiguous match this frame.
    bool missing=false;for(int i=0;i<MaxBodies;++i)if(tracks[i].occupied&&!used[i])missing=true;
    if(missing)continue;
    for(int i=0;i<MaxBodies;++i)if(!tracks[i].occupied&&!used[i]){t=i;used[i]=true;break;}
   }
   if(t<0)continue;
   auto &track=tracks[t];Body b=detected[d];b.player=t+1;
   Point velocity;const double dt=time-track.seen;
   if(track.occupied&&dt>0&&dt<.3){velocity={(b.center.x-track.body.center.x)/float(dt),(b.center.y-track.body.center.y)/float(dt)};velocity.x=std::clamp(velocity.x,-1.f,1.f);velocity.y=std::clamp(velocity.y,-1.f,1.f);}
   track={b,velocity,time,true,false};out.push_back(b);
  }
  std::array<bool,MaxBodies> visible{};
  for(auto &body:out){visible[body.player-1]=true;motion[body.player-1].update(body,time);}
  for(int i=0;i<MaxBodies;++i)if(!visible[i])motion[i].clear();
  needsReassign=false;for(const auto &track:tracks)needsReassign|=track.blocked;
  return out;
 }
};
struct Snapshot {std::vector<Hand> hands;std::vector<Body> bodies;bool bodyNeedsReassign=false;std::shared_ptr<const jp_crowd::Snapshot> crowd;std::shared_ptr<const Frame> frame;double captured=0,inferenceMs=0;float inferenceFps=0;uint64_t dropped=0;std::string status="Detenido";bool simulated=false;};
enum Signal {Pinch,Opening,PositionX,PositionY,Distance,TorsoX,TorsoY,WristSpan,ArmHeight,BodyPointX,BodyPointY,CrowdMotion,CrowdOccupancy,CrowdX,CrowdY,CrowdDistance,CrowdActivityX,CrowdActivityY,BodyEnergy,BodyVelocityX,BodyVelocityY,BodyActivityX,BodyActivityY};
enum class Loss {Hold=0,ReturnBase=1,Release=2};
struct Binding {Source source;BodySelection person=BodySelection::Group;int players=(1<<MaxBodies)-1;int bodyZone=0;bool right=true;int signal=Pinch,point=-1;float inputMin=.15f,inputMax=1, smoothingMs=100,attackMs=80,releaseMs=600,deadzone=.01f,base=0;bool invert=false,musical=false;Loss loss=Loss::Hold;jp_crowd::Config crowd;int zone=0;};
inline bool operator==(const Binding &a,const Binding &b){return a.source==b.source&&a.person==b.person&&a.players==b.players&&a.bodyZone==b.bodyZone&&a.right==b.right&&a.signal==b.signal&&a.point==b.point&&a.inputMin==b.inputMin&&a.inputMax==b.inputMax&&a.smoothingMs==b.smoothingMs&&a.musical==b.musical&&a.attackMs==b.attackMs&&a.releaseMs==b.releaseMs&&a.deadzone==b.deadzone&&a.base==b.base&&a.invert==b.invert&&a.loss==b.loss&&a.crowd==b.crowd&&a.zone==b.zone;}
inline std::string signalLabel(int signal){const char *names[]={"Pinza","Apertura","Posicion X","Posicion Y","Distancia (m)","Torso X","Torso Y","Separacion munecas","Altura brazos","Punto X","Punto Y","Movimiento","Ocupacion","Centro X","Centro Y","Distancia mediana (m)","Actividad X","Actividad Y","Energia corporal","Desplazamiento X/s","Desplazamiento Y/s","Centro de actividad X","Centro de actividad Y"};return names[std::clamp(signal,0,int(BodyActivityY))];}
inline bool bodyMotionSignal(int signal){return signal>=BodyEnergy&&signal<=BodyActivityY;}
inline void setSignal(Binding &b,int signal){b.signal=signal;b.inputMin=signal==CrowdDistance?3:signal==Pinch?.15f:signal==Distance?.7f:0;b.inputMax=signal==CrowdDistance?4.5f:signal==Distance?1.5f:signal==WristSpan?3:signal==ArmHeight?2:1;
 if((signal>=TorsoX&&signal<=BodyPointY)||bodyMotionSignal(signal))b.musical=false;
 if(signal==BodyEnergy){b.inputMax=1.5f;b.musical=true;b.attackMs=100;b.releaseMs=800;}
 if(signal==BodyVelocityX||signal==BodyVelocityY){b.inputMin=-.5f;b.inputMax=.5f;}
}
// Presets alter only mapping; keep area, device, output range and background.
inline void crowdPreset(Binding &b,int preset){
 setSignal(b,preset==1?CrowdOccupancy:preset==2?CrowdActivityX:CrowdMotion);
 b.invert=false;b.deadzone=.01f;b.loss=Loss::Hold;b.musical=preset!=2;
 b.attackMs=preset==1?250:80;b.releaseMs=preset==1?1200:600;b.smoothingMs=250;
 if(preset==0)b.inputMax=.15f;
}
struct Value {float raw=0;bool valid=false;};
inline Point palm(const Hand &h){Point p;for(int i:{0,5,9,13,17}){p.x+=h.points[i].x/5;p.y+=h.points[i].y/5;}return p;}
inline float opening(const Hand &h){float sum=0;for(int i:{6,10,14,18}){Point a{(h.points[i-1].x-h.points[i].x)*h.aspect,h.points[i-1].y-h.points[i].y},b{(h.points[i+2].x-h.points[i].x)*h.aspect,h.points[i+2].y-h.points[i].y};float den=std::hypot(a.x,a.y)*std::hypot(b.x,b.y);if(den<1e-7)return 0;float angle=std::acos(std::clamp((a.x*b.x+a.y*b.y)/den,-1.f,1.f));sum+=std::clamp((angle-1.5707963f)/1.5707963f,0.f,1.f)/4;}return sum;}
inline bool bodyEnabled(const Body &body,const Binding &b){return body.player>=1&&body.player<=MaxBodies&&(b.players&(1<<(body.player-1)));}
inline bool bodyInZone(const Body &body,const Binding &b){
 if(b.signal==BodyPointX||b.signal==BodyPointY||b.bodyZone==0)return true;
 return b.bodyZone==1+std::clamp(int(body.center.x*3),0,2);
}
inline Value bodyValue(const Body &body,const Binding &b){
 auto valid=[&](std::initializer_list<int> ids){for(int i:ids)if(!body.points[i].valid())return false;return true;};
 auto p=[&](int i){return body.points[i].position;};
 if(b.signal==BodyEnergy)return {body.energy,body.motionValid};
 if(b.signal==BodyVelocityX)return {body.velocityX,body.motionValid};
 if(b.signal==BodyVelocityY)return {body.velocityY,body.motionValid};
 if(b.signal==BodyActivityX)return {body.activityX,body.motionValid&&body.activityValid};
 if(b.signal==BodyActivityY)return {body.activityY,body.motionValid&&body.activityValid};
 if(b.signal==BodyPointX||b.signal==BodyPointY){if(b.point<0||b.point>=33||!body.points[b.point].valid())return {};return {b.signal==BodyPointX?p(b.point).x:1-p(b.point).y,true};}
 if(b.signal==TorsoX||b.signal==TorsoY){if(!valid({11,12,23,24}))return {};Point center{(p(11).x+p(12).x+p(23).x+p(24).x)/4,(p(11).y+p(12).y+p(23).y+p(24).y)/4};return {b.signal==TorsoX?center.x:1-center.y,true};}
 if(b.signal==WristSpan){if(!valid({11,12,15,16}))return {};float width=distance(p(11),p(12),body.aspect);return {width>.02f?distance(p(15),p(16),body.aspect)/width:0,width>.02f};}
 if(b.signal==ArmHeight){if(!valid({11,12,15,16,23,24}))return {};float hip=(p(23).y+p(24).y)*.5f,shoulder=(p(11).y+p(12).y)*.5f,span=hip-shoulder;return {span>.03f?(hip-(p(15).y+p(16).y)*.5f)/span:0,span>.03f};}
 return {};
}
inline const Body *selectedBody(const Snapshot &s,const Binding &b){
 if(b.person==BodySelection::Group)return nullptr;
 const Body *selected=nullptr;float best=100;
 for(const auto &body:s.bodies){if(!body.present||!bodyEnabled(body,b)||!bodyInZone(body,b))continue;
  if(selectedPlayer(b.person)){if(body.player==selectedPlayer(b.person))return &body;continue;}
  if(b.person==BodySelection::Nearest&&(!b.source.hasDepth()||!body.depthValid))continue;
  float score=b.person==BodySelection::Nearest?body.meters:std::abs(body.center.x-.5f);
  if(score<best){best=score;selected=&body;}
 }return selected;
}
inline Value sample(const Snapshot &s,const Binding &b,double time){
 if(time-s.captured>.25 || time<s.captured)return {};
 if(b.source.detector==Detector::Crowd){
  if((b.source.kind!=SourceKind::KinectIR&&b.source.kind!=SourceKind::IPhoneLidar)||!s.crowd)return {};
  const auto *result=s.crowd->find(b.crowd);if(!result)return {};
  const auto &z=result->zones[std::clamp(b.zone,0,3)];
  switch(b.signal){
   case CrowdMotion:return {z.motion,z.motionValid};case CrowdOccupancy:return {z.occupancy,z.valid};
   case CrowdX:return {z.x,z.positionValid};case CrowdY:return {z.y,z.positionValid};
   case CrowdDistance:return {z.meters,z.positionValid};
   case CrowdActivityX:return {z.activityX,z.activityValid};case CrowdActivityY:return {z.activityY,z.activityValid};default:return {};
  }
 }
 if(b.source.detector==Detector::Body){
  if(b.person!=BodySelection::Group){const auto *selected=selectedBody(s,b);return selected?bodyValue(*selected,b):Value{};}
  float total=0,weights=0;
  for(const auto &body:s.bodies){if(!body.present||!bodyEnabled(body,b)||!bodyInZone(body,b))continue;const auto v=bodyValue(body,b);if(!v.valid)continue;
   const float weight=b.signal==BodyActivityX||b.signal==BodyActivityY?body.energy:1.f;
   // Average bounded contributions so an overextended arm cannot cancel another person.
   const float contribution=b.inputMax>b.inputMin?std::clamp(v.raw,b.inputMin,b.inputMax):v.raw;
   total+=contribution*weight;weights+=weight;
  }
  return {weights>0?total/weights:0,weights>0};
 }

 if(b.signal<Pinch||b.signal>Distance)return {};
 for(const auto &h:s.hands)if(h.right==b.right){
  Point p=b.point>=0&&b.point<21?h.points[b.point]:h.palm;
  if(b.signal==PositionX)return {p.x,std::isfinite(p.x)};
  if(b.signal==PositionY)return {1-p.y,std::isfinite(p.y)};
  if(b.signal==Distance)return {h.meters,b.source.hasDepth()&&h.depthValid};
  if(b.signal==Opening)return {opening(h),true};
  float width=distance(h.points[5],h.points[17],h.aspect);return {width>1e-5?distance(h.points[4],h.points[8],h.aspect)/width:0,width>1e-5};
 }return {};
}
struct Filter {
 float accepted=0,normalized=0,output=0;bool initialized=false,valid=false,seenValid=false,release=false;double last=0,missingSince=0;
 Filter()=default;Filter(const Filter&){} // Runtime state never follows clipboard/undo copies.
 Filter& operator=(const Filter&){reset();return *this;}
 void reset(){accepted=normalized=output=0;initialized=valid=seenValid=release=false;last=missingSince=0;}
 float step(Value v,const Binding &b,float lo,float hi,double time){
  float dt=last>0?float(std::max(0.,time-last)):0;last=time;
  if(!initialized){output=std::clamp(b.base,lo,hi);normalized=hi>lo?(output-lo)/(hi-lo):0;accepted=normalized;initialized=true;}
  valid=v.valid&&std::isfinite(v.raw)&&std::isfinite(b.inputMin)&&std::isfinite(b.inputMax)&&b.inputMax>b.inputMin;
  if(valid){seenValid=true;missingSince=0;release=false;float target=std::clamp((v.raw-b.inputMin)/(b.inputMax-b.inputMin),0.f,1.f);if(b.invert)target=1-target;
   if(std::abs(target-accepted)>=std::clamp(b.deadzone,0.f,.5f))accepted=target;
   float tau=b.musical?(accepted>normalized?b.attackMs:b.releaseMs):b.smoothingMs;float alpha=tau<=0?1:1-std::exp(-dt/(tau*.001f));normalized+=alpha*(accepted-normalized);output=lo+(hi-lo)*normalized;
  }else if(seenValid){
   if(missingSince==0)missingSince=time;
   if(time-missingSince>=.5){
    if(b.loss==Loss::Release)release=true;
    if(b.loss==Loss::ReturnBase){float target=hi>lo?std::clamp((b.base-lo)/(hi-lo),0.f,1.f):0;float alpha=1-std::exp(-dt/std::max(.1f,b.smoothingMs*.001f));normalized+=alpha*(target-normalized);accepted=normalized;output=lo+(hi-lo)*normalized;}
   }
  }return std::clamp(output,lo,hi);
 }
};
// Preserve handedness through brief classification flips. Reject ambiguous
// crossings and duplicate side assignments, rather than handing off a control.
class Associator {
 struct Track {Hand hand;double seen=0;};std::vector<Track> tracks;uint64_t next=1;
public:
 std::vector<Hand> update(std::vector<Hand> detected,double time){
  tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[&](auto &t){return time-t.seen>1.;}),tracks.end());
  for(auto &h:detected)h.palm=palm(h);
  if(detected.size()==2&&distance(detected[0].palm,detected[1].palm,detected[0].aspect)<.12f)return {};
  std::vector<Hand> out;std::vector<bool> used(tracks.size(),false);
  for(auto h:detected){h.palm=palm(h);int best=-1;float first=10,second=10;
   for(size_t i=0;i<tracks.size();++i)if(!used[i]){float d=distance(h.palm,tracks[i].hand.palm,h.aspect);if(d<first){second=first;first=d;best=int(i);}else second=std::min(second,d);}
   if(best>=0&&first>.3f)best=-1;
   if(best>=0&&second-first<.07f)continue;
   if(best>=0&&h.right!=tracks[best].hand.right)continue;
   if(best>=0){h.id=tracks[best].hand.id;h.right=tracks[best].hand.right;used[best]=true;tracks[best]={h,time};}
   else {if(h.sideScore<.6f)continue;bool occupied=false;for(auto &t:tracks)if(t.hand.right==h.right)occupied=true;if(occupied)continue;h.id=next++;tracks.push_back({h,time});used.push_back(true);}
   out.push_back(h);
  }
  if(out.size()==2&&distance(out[0].palm,out[1].palm,out[0].aspect)<.08f)out.clear();
  return out;
 }
};
inline void measureDepth(Hand &h,const Frame &f){
 h.palm=palm(h);h.aspect=float(f.width)/std::max(1,f.height);h.depthValid=false;
 h.depthRadiusX=h.depthRadiusY=0;
 int w=f.depthWidth,ht=f.depthHeight;
 if(w<=0||ht<=0||f.depth.size()!=size_t(w)*ht||h.palm.x<0||h.palm.x>=1||h.palm.y<0||h.palm.y>=1)return;
 int x=int(h.palm.x*w),y=int(h.palm.y*ht);
 float palmWidth=std::hypot((h.points[5].x-h.points[17].x)*w,(h.points[5].y-h.points[17].y)*ht);
 int radius=std::clamp(int(palmWidth*.12f),1,5);
 h.depthRadiusX=float(radius+.5f)/w;h.depthRadiusY=float(radius+.5f)/ht;
 std::vector<float> values;
 for(int j=std::max(0,y-radius);j<std::min(ht,y+radius+1);++j)for(int i=std::max(0,x-radius);i<std::min(w,x+radius+1);++i){float d=f.depth[j*w+i];if(std::isfinite(d)&&d>=500&&d<=4500)values.push_back(d);}
 if(values.size()<9)return;
 std::sort(values.begin(),values.end());
 if(values[values.size()*9/10]-values[values.size()/10]>100)return;
 h.meters=values[values.size()/2]*.001f;h.depthValid=true;
}
inline Snapshot simulatedSnapshot(double time,bool lost=false,Detector detector=Detector::Hands,int bodyCount=2){
 Snapshot s;s.simulated=true;s.captured=time;s.inferenceFps=30;s.inferenceMs=0;
 auto f=std::make_shared<Frame>();f->serial=uint64_t(time*30);f->captured=time;f->captureFps=30;f->rgb.resize(960*540*3,24);s.frame=f;
 if(!lost&&detector==Detector::Body){Body b;b.present=true;for(auto &p:b.points){p.position={.5f,.2f};p.visibility=p.presence=1;}
  float x=.5f+.1f*std::sin(time),lift=.25f*std::sin(time*2);
  b.points[0].position={x,.15f};b.points[11].position={x+.1f,.35f};b.points[12].position={x-.1f,.35f};
  b.points[13].position={x+.15f,.45f-lift*.5f};b.points[14].position={x-.15f,.45f-lift*.5f};b.points[15].position={x+.2f,.55f-lift};b.points[16].position={x-.2f,.55f-lift};
  b.points[23].position={x+.08f,.6f};b.points[24].position={x-.08f,.6f};b.points[25].position={x+.08f,.75f};b.points[26].position={x-.08f,.75f};b.points[27].position={x+.08f,.9f};b.points[28].position={x-.08f,.9f};
  for(int player=1;player<=std::clamp(bodyCount,1,MaxBodies);++player){auto person=b;person.player=player;person.depthValid=true;person.meters=player==1?3.2f:4.f;
   for(auto &p:person.points)p.position.x=bodyCount<=2?.5f+(p.position.x-.5f)*.65f+(player==1?-.22f:.22f):.14f+.24f*(player-1)+(p.position.x-.5f)*.35f;
   bodyCenter(person);s.bodies.push_back(person);
  }
 }
 if(!lost&&detector==Detector::Hands)for(bool right:{false,true}){Hand h;h.right=right;h.sideScore=1;h.id=right?2:1;h.meters=1.1f+.3f*std::sin(time*.5);h.depthValid=true;
  float x=(right?.65f:.3f)+.08f*std::sin(time),y=.65f;h.points[0]={x,y};
  for(int finger=0;finger<5;++finger)for(int joint=0;joint<4;++joint)h.points[1+finger*4+joint]={x+(finger-2)*.035f,y-.045f*(joint+1)};
  h.points[4]={h.points[8].x-.015f-.08f*float((1+std::sin(time*2))/2),h.points[8].y};h.palm=palm(h);s.hands.push_back(h);
 }s.status=lost?"Simulacion: sin seguimiento":"Simulacion: siguiendo";return s;
}

using FrameProvider=std::function<std::shared_ptr<const Frame>()>;
// Main-thread API; capture/process workers only publish immutable snapshots.
void startWithFrameProvider(FrameProvider provider,const Source &source={});
void start(bool simulation=false,const Source &source={});
void stop(const Source &source={});void stopAll();void reconnect(const Source &source={});void shutdown();
uint64_t session(const Source &source={});
void beginFrame();void demand(const Source &source={},const void *consumer=nullptr);
void demandCrowd(const Source &source,const jp_crowd::Config &config);
void learnCrowdBackground(const Source &source);void clearCrowdBackground(const Source &source);
void setPanelVisible(bool value,const Source &source={});
const Snapshot &snapshot(const Source &source={});
bool running(const Source &source={});bool simulation(const Source &source={});
void simulateLoss(bool value,const Source &source={});
size_t consumers(const Source &source={});
std::vector<SourceInfo> sources();void refreshSources();
} // namespace
