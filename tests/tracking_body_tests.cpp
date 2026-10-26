#include "../src/JPutils/jp_hand_tracking.h"
#include <cassert>
#include <iostream>
using namespace jp_hand;
static bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(){
 // Eye-cover gesture: dwell, anatomical side, scale/aspect, expiry and release.
 for(float aspect:{1.f,4.f/3,16.f/9})for(int fps:{15,30,60}){
  Body person;person.present=true;person.aspect=aspect;
  auto put=[&](int i,float x,float y){person.points[i]={{x/aspect,y},1,1};};
  put(11,.3,.5);put(12,.7,.5);put(2,.44,.25);put(5,.56,.25);
  for(int i:{15,17,19,21})put(i,.44,.25);
  EyeCoverGesture gesture;gesture.update(person,0);assert(person.eyeCoverValid&&person.leftEyeCover==0);
  for(int n=1;n<=fps;++n)gesture.update(person,double(n)/fps);
  assert(person.leftEyeCover==1);
  for(int i:{15,17,19,21})put(i,.2,.7);
  for(int i:{16,18,20,22})put(i,.44,.25);
  gesture.update(person,1.05);assert(person.eyeCoverValid&&person.leftEyeCover==0); // right hand alone
  for(int i:{15,17,19,21})put(i,.56,.25);
  gesture.update(person,1.1);gesture.update(person,1.31);assert(person.leftEyeCover==1); // either eye
  gesture.update(person,1.7);assert(person.leftEyeCover==0); // no dwell across a gap
  person.points[2].visibility=person.points[5].visibility=0;gesture.update(person,1.75);assert(!person.eyeCoverValid);
  person.points[2].visibility=person.points[5].visibility=1;
  gesture.update(person,2);gesture.update(person,2.1);assert(person.leftEyeCover==0);
  for(int i:{15,17,19,21})put(i,.2,.7);
  gesture.update(person,2.12);assert(person.leftEyeCover==0);
  for(int i:{15,17,19,21})put(i,.56,.25);
  gesture.update(person,2.15);gesture.update(person,2.3);assert(person.leftEyeCover==0);
  gesture.update(person,2.36);assert(person.leftEyeCover==1);
  Snapshot frame;person.player=1;frame.bodies={person};frame.captured=2.36;
  Binding gestureBinding;gestureBinding.source={SourceKind::KinectIR,{},Detector::Body};setSignal(gestureBinding,LeftEyeCover);
  assert(sample(frame,gestureBinding,2.36).raw==1&&gestureBinding.loss==Loss::ReturnBase);
  assert(!sample(frame,gestureBinding,2.7).valid);
  gesture.clear();assert(signalLabel(LeftEyeCover)=="Mano izq. sobre ojo");
 }
 Source hands{SourceKind::Camera,"same-device"},body=hands;body.detector=Detector::Body;
 assert(hands!=body&&(hands<body)!=(body<hands));
 Binding b;b.source=body;b.person=BodySelection::Player1;b.base=.2f;b.smoothingMs=0;
 auto s=simulatedSnapshot(10,false,Detector::Body);s.bodies.front().aspect=4.f/3;
 auto point=[&](int i,float x,float y){s.bodies.front().points[i].position={x,y};};
 point(11,.4,.3);point(12,.6,.3);point(23,.4,.7);point(24,.6,.7);point(15,.2,.3);point(16,.8,.3);
 setSignal(b,TorsoX);assert(near(sample(s,b,10).raw,.5));
 setSignal(b,TorsoY);assert(near(sample(s,b,10).raw,.5));
 setSignal(b,WristSpan);assert(near(sample(s,b,10).raw,3));
 setSignal(b,ArmHeight);assert(near(sample(s,b,10).raw,1));
 s.bodies.front().points[15].visibility=.2;assert(!sample(s,b,10).valid);
 setSignal(b,TorsoX);assert(sample(s,b,10).valid);
 setSignal(b,BodyPointY);b.point=15;s.bodies.front().points[15].visibility=1;
 assert(near(sample(s,b,10).raw,.7));assert(!sample(s,b,10.251).valid&&!sample(s,b,9).valid);
 s.bodies.front().points[15].presence=.1;assert(!sample(s,b,10).valid);s.bodies.front().points[15].presence=1;
 point(15,-.1,.3);assert(!sample(s,b,10).valid);point(15,.2,.3);
 b.point=33;assert(!sample(s,b,10).valid);b.point=15;
 b.source.kind=SourceKind::KinectIR;assert(sample(s,b,10).valid);
 assert(near(sample(s,b,10).raw,.7));assert(!sample(s,b,10.251).valid);
 Source rgbBody{SourceKind::KinectRGB,{},Detector::Body};assert(b.source!=rgbBody);
 b.source=body;
 s.bodies.front().present=false;assert(!sample(s,b,10).valid);
 Filter hold;setSignal(b,TorsoX);assert(near(hold.step({.8,true},b,0,1,10),.8));assert(near(hold.step({},b,0,1,12),.8));
 b.loss=Loss::ReturnBase;
 auto decay=[&](int fps){Filter f;f.step({.8,true},b,0,1,10);for(int i=1;i<=fps*2;++i)f.step({},b,0,1,10.+double(i)/fps);return f.output;};
 assert(near(decay(15),decay(60))&&near(decay(15),.2));
 b.loss=Loss::Release;Filter release;release.step({},b,0,1,1);release.step({},b,0,1,5);assert(!release.release);
 release.step({.8,true},b,0,1,10);release.step({},b,0,1,10.1);release.step({},b,0,1,10.4);assert(!release.release);
 release.step({},b,0,1,10.7);assert(release.release);release.step({.4,true},b,0,1,10.8);assert(!release.release);
 Filter copy=release;assert(!copy.initialized&&!copy.seenValid);
 // Two independently selectable people, unrelated to detector output order.
 auto person=[](float x,float meters=0.f){Body p=simulatedSnapshot(20,false,Detector::Body).bodies.front();
  bodyCenter(p);const float offset=x-p.center.x;for(auto &point:p.points)point.position.x+=offset;
  p.depthValid=meters>0;p.meters=meters;bodyCenter(p);return p;};
 BodyAssociator assoc;auto tracked=assoc.update({person(.7,4),person(.3,3)},20);
 assert(tracked.size()==2&&tracked[0].player==1&&tracked[1].player==2);
 tracked=assoc.update({person(.69,4),person(.31,3)},20.1);
 assert(tracked.size()==2&&tracked[0].player==1&&tracked[1].player==2);
 Snapshot two;two.captured=20.1;two.bodies=tracked;
 Binding a;a.source={SourceKind::KinectIR,{},Detector::Body};setSignal(a,TorsoX);a.person=BodySelection::Player1;
 Binding other=a;other.person=BodySelection::Player2;assert(!(a==other));
 assert(near(sample(two,a,20.1).raw,.31)&&near(sample(two,other,20.1).raw,.69));
 a.person=BodySelection::Nearest;assert(selectedBody(two,a)->player==1);
 two.bodies[0].depthValid=false;assert(selectedBody(two,a)->player==2);
 two.bodies[1].depthValid=false;assert(!sample(two,a,20.1).valid);
 a.source.kind=SourceKind::Camera;assert(!sample(two,a,20.1).valid);
 a.person=BodySelection::Centered;assert(sample(two,a,20.1).valid); // 2D survives absent depth.
 a.person=BodySelection::Player1;
 tracked=assoc.update({person(.68,4)},20.2);assert(tracked.size()==1&&tracked[0].player==2);
 two.bodies=tracked;two.captured=20.2;assert(!sample(two,a,20.2).valid);assert(sample(two,other,20.2).valid);
 tracked=assoc.update({person(.32,3),person(.67,4)},20.4);assert(tracked.size()==2&&tracked[0].player==1);
 // Ambiguity pauses assignments; after the reservation expires slots recover.
 BodyAssociator crossing;crossing.update({person(.3),person(.7)},30);
 assert(crossing.update({person(.48),person(.52)},30.1).empty()&&crossing.needsReassign);
 assert(crossing.update({person(.3),person(.7)},30.2).empty());
 assert(crossing.update({person(.3),person(.7)},32.3).size()==2);
 // Overlapping silhouettes can stay distinct when measured depths separate them.
 BodyAssociator depthCross;depthCross.update({person(.4,2),person(.6,4)},40);
 auto overlap=depthCross.update({person(.5,4),person(.5,2)},40.1);
 assert(overlap.size()==2&&!depthCross.needsReassign);
 for(auto &p:overlap)assert(p.player==(p.meters==2?1:2));
 // After a long absence slots become available automatically.
 BodyAssociator expired;expired.update({person(.3)},50);expired.update({},52.1);
 auto returned=expired.update({person(.8)},52.2);assert(returned.size()==1&&!expired.needsReassign&&!returned.front().motionValid);
 BodyAssociator independent;assert(independent.update({person(.3)},52.2).size()==1);
 // Depth uses aligned metric samples, rejects holes and mixed foreground/background.
 Frame depth;depth.width=512;depth.height=424;depth.depthWidth=512;depth.depthHeight=424;depth.depth.assign(512*424,3200);
 Body measured=person(.4);measureBodyDepth(measured,depth);assert(measured.depthValid&&near(measured.meters,3.2));
 std::fill(depth.depth.begin(),depth.depth.end(),0);measureBodyDepth(measured,depth);assert(!measured.depthValid);
 for(size_t i=0;i<depth.depth.size();++i)depth.depth[i]=i%2?1000:4000;
 measureBodyDepth(measured,depth);assert(!measured.depthValid);
 // Analytic translation has the same velocity and activity at different FPS.
 auto translate=[&](float x){return person(x);};
 auto motionAt=[&](int fps){BodyMotion motion;Body moving;
  for(int i=0;i<=fps;++i){moving=translate(.3f+.1f*float(i)/fps);motion.update(moving,60.+double(i)/fps);}
  assert(moving.motionValid&&moving.activityValid);return moving;};
 const auto m15=motionAt(15),m30=motionAt(30),m60=motionAt(60);
 assert(near(m15.velocityX,.1f)&&near(m60.velocityX,.1f)&&near(m15.velocityY,0));
 assert(near(m15.energy,m30.energy)&&near(m30.energy,m60.energy)&&m30.energy>.1f);
 BodyMotion still;Body stationary;
 for(int i=0;i<=30;++i){stationary=translate(.4f+(i%2?.0001f:-.0001f));still.update(stationary,70.+i/30.);}
 assert(stationary.motionValid&&stationary.energy==0&&!stationary.activityValid&&stationary.velocityX==0);
 stationary=translate(.4);still.update(stationary,72);assert(!stationary.motionValid); // No speed spike over a gap.
 stationary=translate(.4);still.update(stationary,72.1);assert(!stationary.motionValid);
 stationary=translate(.4);still.update(stationary,72.2);assert(stationary.motionValid);
 stationary.aspect=1;still.update(stationary,72.23);assert(!stationary.motionValid);
 BodyMotion lowConfidence;
 for(int i=0;i<10;++i){stationary=translate(.3f+i*.001f);for(int j:{13,14,15,16,23,24,25,26,27,28})stationary.points[j].visibility=0;lowConfidence.update(stationary,80.+i/30.);}
 assert(!stationary.motionValid);
 Snapshot group;group.captured=90;group.bodies={person(.2),person(.8)};
 group.bodies[1].player=2;
 for(auto &p:group.bodies){p.motionValid=true;p.activityValid=true;p.activityX=p.center.x;p.activityY=.5;}
 group.bodies[0].energy=.4;group.bodies[1].energy=1.2;group.bodies[0].velocityX=-.1;group.bodies[1].velocityX=.1;
 Binding groupLink;groupLink.source={SourceKind::KinectIR,{},Detector::Body};groupLink.person=BodySelection::Group;
 setSignal(groupLink,BodyEnergy);assert(near(sample(group,groupLink,90).raw,.8)&&groupLink.musical);
 setSignal(groupLink,BodyVelocityX);assert(near(sample(group,groupLink,90).raw,0)&&!groupLink.musical&&groupLink.inputMin<0);
 setSignal(groupLink,BodyActivityX);assert(near(sample(group,groupLink,90).raw,.65));
 groupLink.bodyZone=1;assert(near(sample(group,groupLink,90).raw,.2));
 groupLink.bodyZone=2;assert(!sample(group,groupLink,90).valid); // Empty region is not zero movement.
 groupLink.bodyZone=3;assert(near(sample(group,groupLink,90).raw,.8));
 groupLink.bodyZone=0;group.bodies[1].motionValid=false;setSignal(groupLink,BodyEnergy);assert(near(sample(group,groupLink,90).raw,.4));
 group.bodies[0].motionValid=false;assert(!sample(group,groupLink,90).valid);
 assert(!sample(group,groupLink,90.3).valid);
 // Arms below hips and fully raised contribute 0% and 100%, respectively.
 Snapshot arms=group;arms.captured=91;
 for(auto &p:arms.bodies){p.points[11].position.y=p.points[12].position.y=.5;p.points[23].position.y=p.points[24].position.y=.7;}
 arms.bodies[0].points[15].position.y=arms.bodies[0].points[16].position.y=.9;
 arms.bodies[1].points[15].position.y=arms.bodies[1].points[16].position.y=.1;
 setSignal(groupLink,ArmHeight);groupLink.smoothingMs=0;Filter average;
 assert(near(average.step(sample(arms,groupLink,91),groupLink,0,100,91),50));
 groupLink.players=1;assert(near(sample(arms,groupLink,91).raw,0));
 groupLink.players=2;assert(near(sample(arms,groupLink,91).raw,2));
 groupLink.players=0;assert(!sample(arms,groupLink,91).valid);
 groupLink.players=3;assert(near(sample(arms,groupLink,91).raw,1));
 groupLink.person=BodySelection::Player1;groupLink.players=2;assert(!sample(arms,groupLink,91).valid);
 BodyAssociator movingTracker;movingTracker.update({person(.3)},100);movingTracker.update({person(.31)},100.1);
 auto active=movingTracker.update({person(.32)},100.2);assert(active.size()==1&&active.front().motionValid);
 movingTracker.update({},100.25);active=movingTracker.update({person(.33)},100.3);assert(active.size()==1&&!active.front().motionValid);
 // Four slots survive all input permutations, losses and slot-specific masks.
 BodyAssociator four;std::vector<Body> crowd4={person(.12),person(.37),person(.62),person(.87)};
 auto quartet=four.update(crowd4,200);assert(quartet.size()==4);
 std::array<int,4> order{0,1,2,3};double stamp=200;
 do{std::vector<Body> shuffled;for(int i:order)shuffled.push_back(crowd4[i]);quartet=four.update(shuffled,stamp+=.03);
  assert(quartet.size()==4);for(auto &p:quartet)assert(near(p.center.x,crowd4[p.player-1].center.x));
 }while(std::next_permutation(order.begin(),order.end()));
 Snapshot fourSnapshot;fourSnapshot.bodies=quartet;fourSnapshot.captured=stamp;
 Binding fourLink;fourLink.source=groupLink.source;setSignal(fourLink,TorsoX);
 assert(fourLink.players==15&&near(sample(fourSnapshot,fourLink,stamp).raw,.495));
 fourLink.players=12;assert(near(sample(fourSnapshot,fourLink,stamp).raw,.745));
 for(int player=1;player<=MaxBodies;++player){fourLink.players=15;fourLink.person=playerSelection(player);assert(selectedPlayer(fourLink.person)==player);assert(near(sample(fourSnapshot,fourLink,stamp).raw,crowd4[player-1].center.x));}
 assert(four.update({},stamp+3).empty());assert(four.update(crowd4,stamp+3.1).size()==4);
 assert(int(BodySelection::Group)==4&&int(BodySelection::Player2)==3);
 std::cout<<"PASS: body signals, two-person association, selection, crossings, depth, motion FPS/noise/gaps, group/zone aggregation, expiry and loss policies\n";
}
