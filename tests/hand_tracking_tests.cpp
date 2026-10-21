#include "../src/JPutils/jp_hand_tracking.h"
#include "../src/JPutils/jp_hand_resize.h"
#include <cassert>
#include <iostream>
using namespace jp_hand;
bool near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
 // The HD webcam path must contain actual RGB after downscaling, including
 // odd strides, channel order and top/bottom orientation.
 for(auto size:{std::pair<int,int>{1280,720},{1280,960},{1023,767}}){
  Frame input;input.width=input.nativeWidth=size.first;input.height=input.nativeHeight=size.second;
  input.captured=12;input.serial=31;input.rgb.resize(size_t(input.width)*input.height*3);
  for(int y=0;y<input.height;++y)for(int x=0;x<input.width;++x){auto i=(size_t(y)*input.width+x)*3;input.rgb[i]=x*255/(input.width-1);input.rgb[i+1]=y*255/(input.height-1);input.rgb[i+2]=73;}
  auto output=fitRgbFrame(input);assert(output.valid()&&output.width<=960&&output.height<=540);
  assert(output.nativeWidth==input.width&&output.nativeHeight==input.height&&output.serial==31&&output.captured==12);
  assert(std::abs(float(output.width)/output.height-float(input.width)/input.height)<.003f);
  assert(output.rgb[0]<2&&output.rgb[1]<2&&output.rgb[2]==73);
  assert(output.rgb[output.rgb.size()-3]>252&&output.rgb[output.rgb.size()-2]>252);
  for(size_t i=2;i<output.rgb.size();i+=3)assert(output.rgb[i]==73);
 }
 Binding b;b.inputMin=0;b.inputMax=1;b.base=8;b.smoothingMs=0;b.deadzone=0;
 Filter f;assert(near(f.step({.5,true},b,1,32,1),16.5));assert(f.step({-3,true},b,1,32,2)==1);assert(f.step({4,true},b,1,32,3)==32);
 b.invert=true;assert(f.step({0,true},b,1,32,4)==32);b.invert=false;
 assert(f.step({0,false},b,1,32,5)==32);assert(f.step({NAN,true},b,1,32,6)==32);
 b.deadzone=.01;f.step({.5,true},b,0,1,7);assert(near(f.step({.505,true},b,0,1,8),.5));
 b.base=0;b.smoothingMs=100;b.deadzone=0;
 auto evolve=[&](int fps){Filter a;a.step({0,true},b,0,1,1);for(int i=1;i<=fps;++i)a.step({1,true},b,0,1,1.+double(i)/fps);return a.output;};
 assert(near(evolve(5),evolve(15))&&near(evolve(15),evolve(120)));
 Filter resumed;resumed.step({0,true},b,0,1,1);resumed.step({1,true},b,0,1,1.1);
 float held=resumed.output;resumed.step({0,false},b,0,1,3);assert(resumed.output==held);
 float returning=resumed.step({1,true},b,0,1,3.02);assert(returning>held&&returning<.9f);
 Filter copy=f;assert(!copy.initialized);copy=f;assert(!copy.initialized);
 auto s=simulatedSnapshot(10);assert(s.simulated&&s.hands.size()==2);assert(!simulatedSnapshot(10,true).hands.size());
 b.signal=PositionX;assert(sample(s,b,10.1).valid);assert(!sample(s,b,10.251).valid);assert(!sample(s,b,9).valid);
 s.hands[1].depthValid=false;b.signal=Distance;assert(!sample(s,b,10.1).valid);b.signal=Pinch;assert(sample(s,b,10.1).valid);
 b.signal=PositionY;b.point=8;assert(near(sample(s,b,10.1).raw,1-s.hands[1].points[8].y));
 b.inputMin=1;b.inputMax=1;assert(!f.step({.5,true},b,0,1,10)||!f.valid);
 auto h=s.hands[0];Frame frame;frame.depth.assign(1920*1080,1100);measureDepth(h,frame);assert(h.depthValid&&near(h.meters,1.1));
 frame.depth.assign(1920*1080,NAN);measureDepth(h,frame);assert(!h.depthValid);
 for(size_t i=0;i<frame.depth.size();++i)frame.depth[i]=i%2?700:1500;
 measureDepth(h,frame);assert(!h.depthValid);
 Associator a;auto hands=a.update(s.hands,10);assert(hands.size()==2);auto original=hands;
 hands=a.update(s.hands,10.1);assert(hands.size()==2&&hands[0].id==original[0].id);
 auto crossed=s.hands;for(auto &p:crossed[1].points)p.x-=crossed[1].palm.x-crossed[0].palm.x;
 assert(a.update(crossed,10.2).empty());
 auto flipped=s.hands;flipped[0].right=!flipped[0].right;flipped[1].right=!flipped[1].right;assert(a.update(flipped,10.3).empty());
 hands=a.update(s.hands,10.4);assert(hands.size()==2&&hands[0].id==original[0].id);
 a.update({},10.5);hands=a.update(s.hands,10.8);assert(hands.size()==2&&hands[0].id==original[0].id);
 // Geometric signals must use the actual image aspect, including native IR.
 b={};auto originalHand=s.hands[1];s.hands={originalHand};s.captured=10;
 for(float aspect:{4.f/3.f,512.f/424.f}){
  auto adapted=originalHand;adapted.aspect=aspect;
  for(auto &point:adapted.points)point.x=.5f+(point.x-.5f)*originalHand.aspect/aspect;
  Snapshot alternative=s;alternative.hands={adapted};
  for(int signal:{int(Pinch),int(Opening)}){b.signal=signal;assert(near(sample(s,b,10.1).raw,sample(alternative,b,10.1).raw));}
 }
 // IR depth samples the native grid; missing Z never disables optical signals.
 Frame ir;ir.width=ir.depthWidth=512;ir.height=ir.depthHeight=424;ir.depth.assign(512*424,1700);
 h=originalHand;measureDepth(h,ir);assert(h.depthValid&&near(h.meters,1.7f)&&near(h.aspect,512.f/424.f));
 for(auto &point:h.points)point.y=-1;
 measureDepth(h,ir);assert(!h.depthValid);
 b.signal=Distance;b.source.kind=SourceKind::Camera;assert(!sample(s,b,10.1).valid);
 b.signal=Pinch;assert(sample(s,b,10.1).valid);
 // Robust normalization, invalid samples, monotonic intensity and bounded exposure.
 std::vector<float> intensities(512*424);for(size_t i=0;i<intensities.size();++i)intensities[i]=100+(i%512)*20;
 IRNormalizer normalizer;auto rgb=normalizer.convert(intensities.data(),intensities.size(),1);
 assert(rgb.size()==intensities.size()*3&&rgb[100*3]<rgb[400*3]);
 for(size_t i=0;i<intensities.size();++i)assert(rgb[i*3]==rgb[i*3+1]&&rgb[i*3]==rgb[i*3+2]);
 intensities[100]=NAN;intensities[101]=0;intensities[102]=INFINITY;
 rgb=normalizer.convert(intensities.data(),intensities.size(),1.03);assert(rgb[300]==0&&rgb[303]==0&&rgb[306]==0);
 auto adjust=[&](int fps){IRNormalizer n;std::vector<float> values(1024);for(int i=0;i<1024;++i)values[i]=10+i;n.convert(values.data(),values.size(),1);for(auto &v:values)v*=4;for(int i=1;i<=fps;++i)n.convert(values.data(),values.size(),1.+double(i)/fps);return n.high;};
 assert(std::abs(adjust(15)-adjust(60))<.0001f);
 std::fill(intensities.begin(),intensities.end(),0);rgb=normalizer.convert(intensities.data(),intensities.size(),3);assert(std::all_of(rgb.begin(),rgb.end(),[](auto v){return v==0;}));
 ir.rgb.assign(ir.width*ir.height*3,0);assert(ir.valid());ir.rgb.pop_back();assert(!ir.valid());
 std::cout<<"PASS: mapping, FPS independence, loss/TTL/Z, shared simulation, safe hand association, variable aspect, native IR depth and normalization\n";
}
