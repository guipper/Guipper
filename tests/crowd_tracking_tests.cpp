#include "../src/JPutils/jp_hand_tracking.h"
#include <cassert>
#include <iostream>
using namespace jp_crowd;
static std::shared_ptr<Grid> grid(double t,float depth=4){auto g=std::make_shared<Grid>();g->width=60;g->height=30;g->captured=t;g->meters.assign(1800,depth);return g;}
static void person(Grid &g,int start,int end,float z){for(int y=3;y<27;++y)for(int x=start;x<end;++x)g.meters[y*60+x]=z;}
int main(){
 Config c;c.nearM=.7f;c.farM=3.5f;Engine engine;auto g=grid(1);person(*g,3,15,2);auto s=engine.process(g,{c});auto a=s->results[0].zones;
 assert(a[0].valid&&a[0].positionValid&&!a[0].motionValid&&std::abs(a[0].occupancy-.16)<.001&&a[1].occupancy>.4&&a[2].occupancy==0&&a[3].occupancy==0);
 assert(std::abs(a[0].x-.15)<.001&&std::abs(a[0].y-.5)<.001&&a[0].meters==2);
 auto stable=grid(1.1);person(*stable,3,15,2.001);s=engine.process(stable,{c});assert(s->results[0].zones[0].motion==0&&s->results[0].zones[0].motionValid);
 auto moving=grid(1.2);person(*moving,20,35,1.9);s=engine.process(moving,{c});assert(s->results[0].zones[0].motion>.1&&s->results[0].zones[0].activityValid);
 Config roi=c;roi.left=.65f;roi.nearM=1;roi.farM=3;s=engine.process(grid(1.3),{roi});assert(s->results[0].zones[0].valid&&!s->results[0].zones[0].positionValid&&s->results[0].zones[0].occupancy==0);
 auto holes=grid(1.4,0);s=engine.process(holes,{c});assert(!s->results[0].zones[0].valid&&!s->results[0].zones[0].motionValid);
 s=engine.process(grid(2),{c});assert(!s->results[0].zones[0].motionValid);
 // Equivalent constant velocity at 15 and 30 FPS, including noise threshold.
 auto rate=[&](int fps){Engine e;std::shared_ptr<Snapshot> out;for(int i=0;i<=fps;++i){auto f=grid(10.+double(i)/fps,2.f-float(i)/fps);out=e.process(f,{c});}return out->results[0].zones[0].motion;};
 assert(std::abs(rate(15)-rate(30))<.001);
 Engine irregular;irregular.process(grid(1,2),{c});assert(irregular.process(grid(1.12,1.9),{c})->results[0].zones[0].motionValid);
 assert(irregular.process(grid(1.17,1.85),{c})->results[0].zones[0].motionValid);
 assert(!irregular.process(grid(1.5,1.5),{c})->results[0].zones[0].motionValid);

 // Background learning uses depth only. Fixed floor/wall disappears, foreground remains.
 Engine background;c.background=true;background.learn();
 for(int i=0;i<65;++i)s=background.process(grid(20.+i/30.,3),{c});
 assert(s->background==Background::Ready&&s->results[0].zones[0].occupancy==0);
 auto foreground=grid(22.2,3);person(*foreground,3,15,2);s=background.process(foreground,{c});assert(s->results[0].zones[0].positionValid&&s->results[0].zones[0].occupancy>.15);
 background.missing();s=background.process(grid(22.3),{c});assert(s->background==Background::None&&!s->results[0].zones[0].valid);
 background.clearBackground();
 background.learn();background.process(grid(23),{c});background.missing();s=background.process(grid(24),{c});assert(s->background==Background::Failed);
 background.learn();for(int i=0;i<65;++i)s=background.process(grid(30.+i/30.,0),{c});assert(s->background==Background::Failed);
 // Native preprocessing rejects invalid depth and isolated speckles, no RGB needed.
 std::vector<float> depth(32*24,2000);depth[2*32+2]=NAN;depth[2*32+6]=500;auto prepared=prepare(depth,32,24,1);
 assert(prepared&&prepared->width==8&&prepared->height==6&&prepared->meters[0]==0&&prepared->meters[1]==0&&prepared->meters[2]==2);
 assert(!prepare(depth,32,23,1));
 Config corrupt=c;corrupt.left=NAN;auto rejected=engine.process(grid(39),{corrupt});assert(!rejected->results[0].zones[0].valid);
 // Binding routes the selected zone/config and TTL; missing position is never fake zero.
 jp_hand::Binding b;b.source={jp_hand::SourceKind::KinectIR,{},jp_hand::Detector::Crowd};b.signal=jp_hand::CrowdOccupancy;b.crowd=c;b.crowd.background=false;
 jp_hand::Snapshot snap;snap.captured=1;snap.crowd=engine.process(grid(40),{b.crowd});assert(jp_hand::sample(snap,b,1).valid&&!jp_hand::sample(snap,b,1.251).valid);
 b.signal=jp_hand::CrowdX;assert(!jp_hand::sample(snap,b,1).valid);b.crowd.left=.2;assert(!jp_hand::sample(snap,b,1).valid);
 std::cout<<"PASS: audience zones, occupancy/centroids, motion/noise/FPS, gaps, depth filtering, background and binding expiry\n";
}
