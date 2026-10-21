#include "../src/JPutils/jp_hand_tracking.h"
#include <cassert>
#include <iostream>
using namespace jp_hand;
bool near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){
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
 std::cout<<"PASS: mapping, FPS independence, loss/TTL/Z, shared simulation, safe hand association\n";
}
