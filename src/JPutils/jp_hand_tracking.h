#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace jp_hand {
inline double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Point {float x=0,y=0;};
inline float distance(Point a,Point b){return std::hypot((a.x-b.x)*16.f/9.f,a.y-b.y);}
struct Hand {std::array<Point,21> points{};bool right=true;float sideScore=0;uint64_t id=0;float meters=0;bool depthValid=false;Point palm;};
struct Frame {uint64_t serial=0;double captured=0;std::vector<unsigned char> rgb;std::vector<float> depth;float captureFps=0;}; // RGB 960x540, registered depth 1920x1080 millimetres
struct Snapshot {std::vector<Hand> hands;std::shared_ptr<const Frame> frame;double captured=0,inferenceMs=0;float inferenceFps=0;uint64_t dropped=0;std::string status="Detenido";bool simulated=false;};
enum Signal {Pinch,Opening,PositionX,PositionY,Distance};
struct Binding {bool right=true;int signal=Pinch,point=-1;float inputMin=.15f,inputMax=1, smoothingMs=100,deadzone=.01f,base=0;bool invert=false;};
inline bool operator==(const Binding &a,const Binding &b){return a.right==b.right&&a.signal==b.signal&&a.point==b.point&&a.inputMin==b.inputMin&&a.inputMax==b.inputMax&&a.smoothingMs==b.smoothingMs&&a.deadzone==b.deadzone&&a.base==b.base&&a.invert==b.invert;}
struct Value {float raw=0;bool valid=false;};
inline Point palm(const Hand &h){Point p;for(int i:{0,5,9,13,17}){p.x+=h.points[i].x/5;p.y+=h.points[i].y/5;}return p;}
inline float opening(const Hand &h){float sum=0;for(int i:{6,10,14,18}){Point a{(h.points[i-1].x-h.points[i].x)*16.f/9.f,h.points[i-1].y-h.points[i].y},b{(h.points[i+2].x-h.points[i].x)*16.f/9.f,h.points[i+2].y-h.points[i].y};float den=std::hypot(a.x,a.y)*std::hypot(b.x,b.y);if(den<1e-7)return 0;float angle=std::acos(std::clamp((a.x*b.x+a.y*b.y)/den,-1.f,1.f));sum+=std::clamp((angle-1.5707963f)/1.5707963f,0.f,1.f)/4;}return sum;}
inline Value sample(const Snapshot &s,const Binding &b,double time){
 if(time-s.captured>.25 || time<s.captured)return {};
 for(const auto &h:s.hands)if(h.right==b.right){
  Point p=b.point>=0&&b.point<21?h.points[b.point]:h.palm;
  if(b.signal==PositionX)return {p.x,std::isfinite(p.x)};
  if(b.signal==PositionY)return {1-p.y,std::isfinite(p.y)};
  if(b.signal==Distance)return {h.meters,h.depthValid};
  if(b.signal==Opening)return {opening(h),true};
  float width=distance(h.points[5],h.points[17]);return {width>1e-5?distance(h.points[4],h.points[8])/width:0,width>1e-5};
 }return {};
}
struct Filter {
 float accepted=0,normalized=0,output=0;bool initialized=false,valid=false;double last=0;
 Filter()=default;Filter(const Filter&){} // Runtime state never follows clipboard/undo copies.
 Filter& operator=(const Filter&){reset();return *this;}
 void reset(){accepted=normalized=output=0;initialized=valid=false;last=0;}
 float step(Value v,const Binding &b,float lo,float hi,double time){
  float dt=last>0?float(std::max(0.,time-last)):0;last=time;
  if(!initialized){output=std::clamp(b.base,lo,hi);normalized=hi>lo?(output-lo)/(hi-lo):0;accepted=normalized;initialized=true;}
  valid=v.valid&&std::isfinite(v.raw)&&std::isfinite(b.inputMin)&&std::isfinite(b.inputMax)&&b.inputMax>b.inputMin;
  if(valid){float target=std::clamp((v.raw-b.inputMin)/(b.inputMax-b.inputMin),0.f,1.f);if(b.invert)target=1-target;
   if(std::abs(target-accepted)>=std::clamp(b.deadzone,0.f,.5f))accepted=target;
   float alpha=b.smoothingMs<=0?1:1-std::exp(-dt/(b.smoothingMs*.001f));normalized+=alpha*(accepted-normalized);output=lo+(hi-lo)*normalized;
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
  if(detected.size()==2&&distance(detected[0].palm,detected[1].palm)<.12f)return {};
  std::vector<Hand> out;std::vector<bool> used(tracks.size(),false);
  for(auto h:detected){h.palm=palm(h);int best=-1;float first=10,second=10;
   for(size_t i=0;i<tracks.size();++i)if(!used[i]){float d=distance(h.palm,tracks[i].hand.palm);if(d<first){second=first;first=d;best=int(i);}else second=std::min(second,d);}
   if(best>=0&&first>.3f)best=-1;
   if(best>=0&&second-first<.07f)continue;
   if(best>=0&&h.right!=tracks[best].hand.right)continue;
   if(best>=0){h.id=tracks[best].hand.id;h.right=tracks[best].hand.right;used[best]=true;tracks[best]={h,time};}
   else {if(h.sideScore<.6f)continue;bool occupied=false;for(auto &t:tracks)if(t.hand.right==h.right)occupied=true;if(occupied)continue;h.id=next++;tracks.push_back({h,time});used.push_back(true);}
   out.push_back(h);
  }
  if(out.size()==2&&distance(out[0].palm,out[1].palm)<.08f)out.clear();
  return out;
 }
};
inline void measureDepth(Hand &h,const Frame &f){
 h.palm=palm(h);h.depthValid=false;if(f.depth.size()!=1920*1080)return;
 int x=int(h.palm.x*1920),y=int(h.palm.y*1080);std::vector<float> values;
 for(int j=std::max(0,y-5);j<std::min(1080,y+6);++j)for(int i=std::max(0,x-5);i<std::min(1920,x+6);++i){float d=f.depth[j*1920+i];if(std::isfinite(d)&&d>=500&&d<=4500)values.push_back(d);}
 if(values.size()<9)return;
 std::sort(values.begin(),values.end());
 if(values[values.size()*9/10]-values[values.size()/10]>100)return;
 h.meters=values[values.size()/2]*.001f;h.depthValid=true;
}
Snapshot simulatedSnapshot(double time,bool lost=false);
void start(bool simulation=false);void stop();void reconnect();void shutdown();
uint64_t session();
void beginFrame();void demand();void setPanelVisible(bool value);
const Snapshot &snapshot();bool running();bool simulation();void simulateLoss(bool value);
} // namespace
