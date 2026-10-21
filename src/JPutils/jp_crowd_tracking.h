#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

// Depth-only audience measurements. No person count, identities or inferred joints.
// This engine is owned by the capture worker; the render thread reads snapshots.
namespace jp_crowd {
struct Config {
 float left=0,top=0,right=1,bottom=1,nearM=3.f,farM=4.5f,speedMps=.5f,noiseM=.03f;
 bool background=false;
 bool operator==(const Config &b)const{return left==b.left&&top==b.top&&right==b.right&&bottom==b.bottom&&nearM==b.nearM&&farM==b.farM&&speedMps==b.speedMps&&noiseM==b.noiseM&&background==b.background;}
 bool valid()const{return std::isfinite(left)&&std::isfinite(top)&&std::isfinite(right)&&std::isfinite(bottom)&&left>=0&&top>=0&&right<=1&&bottom<=1&&right-left>=.03f&&bottom-top>=.03f&&std::isfinite(nearM)&&std::isfinite(farM)&&nearM>=.5f&&farM<=4.5f&&farM-nearM>=.1f&&std::isfinite(speedMps)&&speedMps>=.05f&&speedMps<=5&&std::isfinite(noiseM)&&noiseM>=.005f&&noiseM<=.2f;}
};
struct Zone {
 float occupancy=0,motion=0,x=0,y=0,meters=0,activityX=0,activityY=0,coverage=0;
 bool valid=false,positionValid=false,motionValid=false,activityValid=false;
};
struct Result {Config config;std::array<Zone,4> zones{};std::vector<uint8_t> mask;}; // 0 excluded, 1 occupied, 2 moving
struct Grid {int width=0,height=0;double captured=0;std::vector<float> meters;};
enum class Background {None,Learning,Ready,Failed};
struct Snapshot {
 std::shared_ptr<const Grid> grid;std::vector<Result> results;
 Background background=Background::None;float progress=0;
 const Result *find(const Config &config)const{for(const auto &r:results)if(r.config==config)return &r;return nullptr;}
};
inline std::string backgroundLabel(Background state){switch(state){case Background::Learning:return "Aprendiendo fondo: mantener la pista vacia";case Background::Ready:return "Fondo listo";case Background::Failed:return "Fondo incompleto: repetir con la pista vacia";default:return "Sin fondo aprendido";}}
inline bool metricDepth(float mm){return std::isfinite(mm)&&mm>=500&&mm<=4500;}
inline std::shared_ptr<Grid> prepare(const std::vector<float> &depth,int width,int height,double time){
 if(width<8||height<8||width>4096||height>4096||depth.size()!=size_t(width)*height||!std::isfinite(time))return {};
 auto g=std::make_shared<Grid>();g->width=(width+3)/4;g->height=(height+3)/4;g->captured=time;g->meters.resize(g->width*g->height);
 for(int y=0;y<g->height;++y)for(int x=0;x<g->width;++x){
  int xx=std::min(width-2,x*4+2),yy=std::min(height-2,y*4+2);float center=depth[yy*width+xx];
  if(!metricDepth(center))continue;
  std::array<float,9> local{};int count=0;
  for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){float d=depth[(yy+j)*width+xx+i];if(metricDepth(d)&&std::abs(d-center)<80)local[count++]=d;}
  if(count<5)continue;
  std::sort(local.begin(),local.begin()+count);g->meters[y*g->width+x]=local[count/2]*.001f;
 }return g;
}
// Two deliberate phases, bounded samples and robust percentiles. No camera state.
struct Calibration {
 double started=0,last=0;bool active=false;std::vector<float> quiet,moving;
 void start(double time){started=time;last=0;active=true;quiet.clear();moving.clear();}
 void add(double time,double captured,float value,bool valid){
  if(!active||!valid||!std::isfinite(value)||captured==last||time-captured>.25||captured<started)return;
  last=captured;auto &values=time-started<3?quiet:moving;if(values.size()<300)values.push_back(value);
 }
 bool finish(float &lo,float &hi){
  active=false;if(quiet.size()<12||moving.size()<20)return false;
  std::sort(quiet.begin(),quiet.end());std::sort(moving.begin(),moving.end());
  float floor=quiet[quiet.size()*95/100],peak=moving[moving.size()*90/100];
  if(peak-floor<.005f)return false;
  lo=floor;hi=peak;return true;
 }
};
inline std::string quality(bool running,double age,float coverage){
 if(!running)return "Detenido";
 if(age<0||age>.25)return "Captura interrumpida";
 if(coverage<.5f)return "Poca profundidad valida";
 return "Recibiendo bien";
}
class Engine {
 std::shared_ptr<const Grid> previous;
 std::deque<std::shared_ptr<const Grid>> recent;
 std::vector<float> background;
 std::vector<std::vector<float>> learning;
 Background state=Background::None;double learnStart=0,lastLearn=0;
 int width=0,height=0;
public:
 void learn(){state=Background::Learning;learning.clear();background.clear();learnStart=lastLearn=0;}
 void clearBackground(){state=Background::None;background.clear();learning.clear();learnStart=lastLearn=0;}
 void missing(){previous.reset();recent.clear();if(state==Background::Ready)clearBackground();if(state==Background::Learning){state=Background::Failed;learning.clear();}}
 std::shared_ptr<Snapshot> process(std::shared_ptr<const Grid> grid,const std::vector<Config> &configs){
  auto out=std::make_shared<Snapshot>();if(!grid)return out;
  if(width!=grid->width||height!=grid->height){width=grid->width;height=grid->height;previous.reset();recent.clear();if(state!=Background::Learning)clearBackground();else{learning.clear();learnStart=lastLearn=0;}}
  out->grid=grid;const auto &depth=grid->meters;const auto size=depth.size();
  if(state==Background::Learning){
   if(learnStart==0)learnStart=grid->captured;
   if(grid->captured-lastLearn>=.03&&learning.size()<64){learning.push_back(depth);lastLearn=grid->captured;}
   out->progress=std::clamp(float((grid->captured-learnStart)/2),0.f,1.f);
   if(grid->captured-learnStart>=2){
    if(learning.size()>=12){background.assign(size,0);size_t usable=0;
     for(size_t i=0;i<size;++i){std::vector<float> values;for(const auto &frame:learning)if(frame[i]>0)values.push_back(frame[i]);if(values.size()*4<learning.size()*3)continue;std::sort(values.begin(),values.end());if(values[values.size()*9/10]-values[values.size()/10]>.1f)continue;background[i]=values[values.size()/2];++usable;}
     state=usable>=size/4?Background::Ready:Background::Failed;
    }else state=Background::Failed;
    learning.clear();if(state!=Background::Ready)background.clear();
   }
  }
  out->background=state;
  if(previous&&(grid->captured<=previous->captured||grid->captured-previous->captured>.25))recent.clear();
  while(!recent.empty()&&grid->captured-recent.front()->captured>.25)recent.pop_front();
  // Choose the sample nearest 100 ms, allowing slower/irregular USB cadence.
  // A fast frame following a slow one must not invalidate otherwise good depth.
  std::shared_ptr<const Grid> reference;double closest=1;
  for(const auto &candidate:recent){double age=grid->captured-candidate->captured;if(age>=.08&&std::abs(age-.1)<closest){reference=candidate;closest=std::abs(age-.1);}}
  double dt=reference?grid->captured-reference->captured:0;
  bool temporal=reference&&dt>=.08&&dt<=.25&&reference->meters.size()==size;
  for(const auto &config:configs){
   Result result;result.config=config;result.mask.resize(size);if(!config.valid()){out->results.push_back(std::move(result));continue;}
   struct Sum{int area=0,valid=0,occupied=0,compared=0;float x=0,y=0,motion=0,mx=0,my=0;std::vector<float> distances;};std::array<Sum,4> sums{};
   bool ready=!config.background||state==Background::Ready;
   for(int y=0;y<height;++y)for(int x=0;x<width;++x){
    float px=(x+.5f)/width,py=(y+.5f)/height;if(px<config.left||px>=config.right||py<config.top||py>=config.bottom)continue;
    int zone=1+std::clamp(int(3*(px-config.left)/(config.right-config.left)),0,2);size_t i=y*width+x;
    float z=depth[i];bool valid=z>0&&(!config.background||(state==Background::Ready&&background[i]>0));
    auto occupied=[&](float value){return value>=config.nearM&&value<config.farM&&(!config.background||(state==Background::Ready&&background[i]>0&&value<background[i]-.15f));};
    bool present=valid&&occupied(z),comparable=valid&&temporal&&reference->meters[i]>0;
    float motion=0;
    if(comparable&&(present||occupied(reference->meters[i])))motion=std::clamp(float((std::abs(z-reference->meters[i])/dt-config.noiseM/.1f)/config.speedMps),0.f,1.f);
    result.mask[i]=motion>.05f?2:present?1:0;
    for(int k:{0,zone}){auto &sum=sums[k];++sum.area;if(valid)++sum.valid;if(comparable)++sum.compared;
     if(present){++sum.occupied;sum.x+=px;sum.y+=1-py;sum.distances.push_back(z);}
     sum.motion+=motion;sum.mx+=motion*px;sum.my+=motion*(1-py);
    }
   }
   for(int i=0;i<4;++i){auto &sum=sums[i];auto &z=result.zones[i];if(!sum.area)continue;
    z.coverage=float(sum.valid)/sum.area;z.valid=ready&&sum.area>=16&&z.coverage>=.5f;z.motionValid=z.valid&&temporal&&sum.compared>=sum.area*.5f;
    // Denominators stay fixed to the selected image area; holes never inflate occupancy.
    z.occupancy=float(sum.occupied)/sum.area;z.motion=sum.motion/sum.area;
    z.positionValid=z.valid&&sum.occupied>=4;
    if(z.positionValid){z.x=sum.x/sum.occupied;z.y=sum.y/sum.occupied;auto &v=sum.distances;std::nth_element(v.begin(),v.begin()+v.size()/2,v.end());z.meters=v[v.size()/2];}
    z.activityValid=z.motionValid&&sum.motion>=1;
    if(z.activityValid){z.activityX=sum.mx/sum.motion;z.activityY=sum.my/sum.motion;}
   }
   out->results.push_back(std::move(result));
  }
  previous=std::move(grid);recent.push_back(previous);while(recent.size()>64)recent.pop_front();return out;
 }
};
} // namespace jp_crowd
