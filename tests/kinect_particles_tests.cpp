#include "../src/JPbox/jp_kinect_particles.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace jp_particles;
static Engine::Depth depth(float mm,int w=32,int h=24) {
    return std::make_shared<const std::vector<float>>(w*h,mm);
}
int main() {
    Settings s; s.nearM=1;s.farM=4;s.density=1;s.delay=0;s.turbulence=0;s.drift=0;s.spread=0;
    Intrinsics k{30,30,16,12};
    Engine e;
    e.submit(depth(2000),32,24,1,k);e.update(1,.1f,s);
    assert(!e.silhouette.empty() && e.particles.size()==6400);
    assert(e.silhouette.front().x<0 && e.silhouette.front().y>0);
    for(const auto& p:e.particles)assert(p.position.z==2 && std::isfinite(p.position.x));
    e.reset();e.submit(depth(0),32,24,1,k);e.update(1,.1f,s);assert(e.particles.empty());
    e.submit(depth(std::numeric_limits<float>::quiet_NaN()),32,24,2,k);e.update(2,.1f,s);assert(e.silhouette.empty());
    e.submit(depth(4500),32,24,3,k);e.update(3,.1f,s);assert(e.silhouette.empty());
    // History selection is timestamp based, even with irregular capture intervals.
    e.reset();e.submit(depth(1000),32,24,1,k);e.submit(depth(3000),32,24,1.2,k);
    s.delay=.2;s.stripes=1;e.update(1.2,.01f,s);
    bool old=false,recent=false;for(auto p:e.silhouette){old|=std::abs(p.z-1)<.001f;recent|=std::abs(p.z-3)<.001f;}assert(old&&recent);
    for(int i=0;i<100;++i)e.submit(depth(2000),32,24,2+i*.01,k);
    assert(e.history.size()==Engine::maxFrames);
    // Stale camera stops emission; existing particles expire instead of freezing.
    e.update(4,.1f,s);assert(e.history.empty()&&e.silhouette.empty());
    for(int i=0;i<100;++i)e.update(4+i*.1,.1f,s);
    assert(e.particles.empty());
    // Long running emission stays bounded, as does a stalled application's catch-up.
    s.life=8;
    for(int i=0;i<200;++i){e.submit(depth(2000),32,24,10+i*.1,k);e.update(10+i*.1,.1f,s);}
    assert(e.particles.size()<=Engine::maxParticles);
    e.reset();e.submit(depth(2000),32,24,100,k);e.update(100,30,s);assert(e.particles.size()==6400);
    // New dimensions/session reset history and particles; malformed frames ignored.
    e.submit(depth(2000,16,16),16,16,101,k);assert(e.particles.empty()&&e.history.size()==1);
    e.submit(depth(2000),16,16,102,k);assert(e.history.size()==1);
    e.submit(depth(2000,16,16),16,16,0,k);assert(e.history.size()==1);
    // Emission rate at 30/60 Hz agrees; no velocity tied to render frame count.
    Engine a,b;s.delay=0;s.life=8;
    for(int i=0;i<30;++i){a.submit(depth(2000),32,24,i/30.,k);a.update(i/30.,1.f/30,s);}
    for(int i=0;i<60;++i){b.submit(depth(2000),32,24,i/60.,k);b.update(i/60.,1.f/60,s);}
    assert(std::abs(int(a.particles.size())-int(b.particles.size()))<=1);
    s.density=0;e.update(0,.1f,s);assert(e.silhouette.empty());
    // Nearby particles move coherently. Seed is a lifetime property, never a
    // different force field for each particle.
    const auto v=Engine::flow({.3f,.2f,2.f},1);
    const auto n=Engine::flow({.3001f,.2001f,2.0001f},1);
    assert(std::abs(v.x-n.x)+std::abs(v.y-n.y)+std::abs(v.z-n.z)<.01f);
    Engine lighting;auto ramp=std::make_shared<std::vector<float>>(32*24);
    for(int y=0;y<24;++y)for(int x=0;x<32;++x)(*ramp)[y*32+x]=2000+x*5;
    s.density=1;s.nearM=1;s.farM=4;s.delay=0;
    lighting.submit(ramp,32,24,0,k);lighting.update(0,0,s);
    float darkest=1,brightest=0;
    for(const auto& p:lighting.silhouette){assert(std::isfinite(p.light)&&p.light>=0&&p.light<=1);darkest=std::min(darkest,p.light);brightest=std::max(brightest,p.light);}
    assert(brightest-darkest>.01f);
    // Detail uses the newest native pixels even with delayed, sparse or disabled plume.
    Engine detail;Settings detailed=s;detailed.detail=1;detailed.density=.1;detailed.delay=.2;
    detail.submit(depth(1500),32,24,1,k);detail.submit(depth(2500),32,24,1.2,k);detail.update(1.2,0,detailed);
    assert(detail.detailSurface.size()==32*24&&detail.silhouette.size()<detail.detailSurface.size());
    for(auto p:detail.detailSurface)assert(std::abs(p.z-2.5)<.001&&std::isfinite(p.light));
    detailed.density=0;detail.update(1.21,0,detailed);assert(detail.silhouette.empty()&&detail.detailSurface.size()==32*24);
    detail.update(2,0,detailed);assert(detail.detailSurface.empty());
    detailed.detail=0;detail.submit(depth(2000),32,24,3,k);detail.update(3,0,detailed);assert(detail.detailSurface.empty());
    e.reset();assert(e.history.empty()&&e.particles.empty());
    std::cout<<"Kinect particles core: PASS\n";
}
