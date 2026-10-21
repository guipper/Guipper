#include "../src/JPutils/jp_hand_tracking.h"
#include <cassert>
#include <iostream>
using namespace jp_hand;
static bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 Source hands{SourceKind::Camera,"same-device"},body=hands;body.detector=Detector::Body;
 assert(hands!=body&&(hands<body)!=(body<hands));
 Binding b;b.source=body;b.base=.2f;b.smoothingMs=0;
 auto s=simulatedSnapshot(10,false,Detector::Body);s.body.aspect=4.f/3;
 auto point=[&](int i,float x,float y){s.body.points[i].position={x,y};};
 point(11,.4,.3);point(12,.6,.3);point(23,.4,.7);point(24,.6,.7);point(15,.2,.3);point(16,.8,.3);
 setSignal(b,TorsoX);assert(near(sample(s,b,10).raw,.5));
 setSignal(b,TorsoY);assert(near(sample(s,b,10).raw,.5));
 setSignal(b,WristSpan);assert(near(sample(s,b,10).raw,3));
 setSignal(b,ArmHeight);assert(near(sample(s,b,10).raw,1));
 s.body.points[15].visibility=.2;assert(!sample(s,b,10).valid);
 setSignal(b,TorsoX);assert(sample(s,b,10).valid);
 setSignal(b,BodyPointY);b.point=15;s.body.points[15].visibility=1;
 assert(near(sample(s,b,10).raw,.7));assert(!sample(s,b,10.251).valid&&!sample(s,b,9).valid);
 s.body.points[15].presence=.1;assert(!sample(s,b,10).valid);s.body.points[15].presence=1;
 point(15,-.1,.3);assert(!sample(s,b,10).valid);point(15,.2,.3);
 b.point=33;assert(!sample(s,b,10).valid);b.point=15;
 b.source.kind=SourceKind::KinectIR;assert(!sample(s,b,10).valid);b.source=body;
 s.body.present=false;assert(!sample(s,b,10).valid);
 Filter hold;setSignal(b,TorsoX);assert(near(hold.step({.8,true},b,0,1,10),.8));assert(near(hold.step({},b,0,1,12),.8));
 b.loss=Loss::ReturnBase;
 auto decay=[&](int fps){Filter f;f.step({.8,true},b,0,1,10);for(int i=1;i<=fps*2;++i)f.step({},b,0,1,10.+double(i)/fps);return f.output;};
 assert(near(decay(15),decay(60))&&near(decay(15),.2));
 b.loss=Loss::Release;Filter release;release.step({},b,0,1,1);release.step({},b,0,1,5);assert(!release.release);
 release.step({.8,true},b,0,1,10);release.step({},b,0,1,10.1);release.step({},b,0,1,10.4);assert(!release.release);
 release.step({},b,0,1,10.7);assert(release.release);release.step({.4,true},b,0,1,10.8);assert(!release.release);
 Filter copy=release;assert(!copy.initialized&&!copy.seenValid);
 std::cout<<"PASS: body signals, confidence, aspect, expiry, source identity and loss policies\n";
}
