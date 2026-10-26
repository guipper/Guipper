#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

// CPU simulation independent of OF/USB/GL. No per-frame allocation after warmup
// except the bounded, immutable depth snapshots owned by the capture service.
namespace jp_particles {
struct Point { float x=0, y=0, z=0, light=1, sampleSeed=0; };
struct Particle { Point position; float age=0, lifetime=1, seed=0; };
struct Settings {
    float density=.55f, life=1.6f, turbulence=.35f, drift=.36f, spread=.35f;
    float delay=.65f, stripes=15, nearM=3, farM=4.5f;
    float sourceDensity=-1; // Negative keeps legacy sampling; boxes can decouple emission.
    float detail=0; // Full-resolution current surface, independent of plume density/history.
};
struct Intrinsics { float fx=365, fy=365, cx=255.5f, cy=211.5f; };
class Engine {
public:
    static constexpr size_t maxParticles=128000, maxFrames=32;
    using Depth = std::shared_ptr<const std::vector<float>>;
    struct Frame { Depth depth; double time; };
    std::deque<Frame> history;
    std::vector<Point> silhouette, detailSurface;
    std::vector<Particle> particles;
    // A spatial field shared by all particles: nearby material follows the
    // same flow, instead of independent random phases tearing a plume apart.
    static Point flow(Point p,float time) {
        // Curl of a smooth vector potential, stretched vertically so nearby
        // grains travel together in rising filaments rather than a wide spray.
        const float x=p.x*4.f+time*.18f,y=p.y*2.f-time*.32f,z=p.z*4.f;
        return {.5f*std::sin(x)*std::cos(y)-std::cos(z)*std::sin(x),
                std::sin(y)*std::cos(z)-std::cos(x)*std::sin(y),
                std::sin(z)*std::cos(x)-.5f*std::cos(y)*std::sin(z)};
    }
    void reset() { history.clear(); silhouette.clear(); detailSurface.clear(); particles.clear(); credit=0; clock=0; sourceBuilt=false; }
    void submit(Depth frame, int w, int h, double time, Intrinsics camera) {
        if(!frame || w<=0 || h<=0 || frame->size()!=size_t(w)*h ||
           !std::isfinite(time) || !(camera.fx>0) || !(camera.fy>0)) return;
        if(w!=width || h!=height || (!history.empty() && time<history.back().time)) reset();
        width=w; height=h; intrinsics=camera;
        history.push_back({std::move(frame),time});
        ++sourceVersion;
        while(history.size()>maxFrames) history.pop_front();
    }
    void update(double now, float dt, const Settings& s) {
        // Catch-up is bounded: resuming after a stall never emits a giant burst.
        dt=std::clamp(dt,0.f,.1f); clock+=dt;
        const float density=std::clamp(s.density,0.f,1.f);
        const float sourceDensity=s.sourceDensity<0?density:std::clamp(s.sourceDensity,0.f,1.f);
        const int step=std::max(1,int(std::round(5-4*sourceDensity)));
        const bool fresh=!history.empty() && now-history.back().time<=.5;
        const bool rebuild=!sourceBuilt || builtVersion!=sourceVersion ||
            cached.density!=s.density || cached.sourceDensity!=s.sourceDensity || cached.delay!=s.delay || cached.stripes!=s.stripes ||
            cached.nearM!=s.nearM || cached.farM!=s.farM || cached.detail!=s.detail;
        if(!fresh || sourceDensity<=0) { silhouette.clear();if(!fresh||s.detail<=0)sourceBuilt=false; }
        if(fresh && sourceDensity>0 && rebuild) {
            silhouette.clear();cached=s;builtVersion=sourceVersion;sourceBuilt=true;
            const float nearM=std::max(.5f,s.nearM), farM=std::max(nearM+.01f,s.farM);
            for(int y=0;y<height;y+=step) {
                // Triangle-wave bands sample different times, analogous to the
                // ramp + Time Machine TOP in the supplied TouchDesigner network.
                const float phase=float(y)/height*std::max(1.f,s.stripes);
                const float band=std::abs(2*(phase-std::floor(phase)) - 1);
                const double target=history.back().time-std::clamp(s.delay,0.f,2.f)*band;
                const Frame* selected=&history.front();
                for(const auto& frame:history) { if(frame.time>target) break; selected=&frame; }
                for(int x=0;x<width;x+=step) {
                    const float z=(*selected->depth)[size_t(y)*width+x]*.001f;
                    if(!std::isfinite(z) || z<nearM || z>farM) continue;
                    // Estimate the actual depth surface normal, not the normal
                    // of an identical sphere sprite. Reject jumps/holes so a
                    // background edge cannot shade the foreground as a slope.
                    auto sample=[&](int px,int py) {
                        px=std::clamp(px,0,width-1);py=std::clamp(py,0,height-1);
                        const float value=(*selected->depth)[size_t(py)*width+px]*.001f;
                        return std::isfinite(value)&&value>=nearM&&value<=farM&&std::abs(value-z)<.12f?value:z;
                    };
                    const float nx=(sample(x+2,y)-sample(x-2,y))*intrinsics.fx/(4*z);
                    const float ny=-(sample(x,y+2)-sample(x,y-2))*intrinsics.fy/(4*z);
                    const float length=std::sqrt(nx*nx+ny*ny+1);
                    const float diffuse=std::max(0.f,(-.58f*nx+.58f*ny+.57f)/length);
                    const float light=.12f+.88f*diffuse;
                    silhouette.push_back({(x+.5f-intrinsics.cx)/intrinsics.fx*z,
                        -(y+.5f-intrinsics.cy)/intrinsics.fy*z,z,light,float(y*width+x)/float(width*height)});
                }
            }
        }
        if(!fresh || s.detail<=0)detailSurface.clear();
        else if(rebuild){
            detailSurface.clear();
            const auto &depth=*history.back().depth;
            const float nearM=std::max(.5f,s.nearM),farM=std::max(nearM+.01f,s.farM);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const float z=depth[size_t(y)*width+x]*.001f;
                if(!std::isfinite(z)||z<nearM||z>farM)continue;
                // Small bilateral stencil: retain fine facial slopes without
                // treating holes or background jumps as nose/cheek normals.
                auto sample=[&](int px,int py){
                    int xx=std::clamp(px,0,width-1),yy=std::clamp(py,0,height-1);
                    float v=depth[size_t(yy)*width+xx]*.001f,d=std::abs(v-z);
                    if(!std::isfinite(v)||v<nearM||v>farM||d>.04f)return z;
                    return z+(v-z)/(1.f+d*100.f);
                };
                const float tl=sample(x-1,y-1),tc=sample(x,y-1),tr=sample(x+1,y-1);
                const float ml=sample(x-1,y),mr=sample(x+1,y);
                const float bl=sample(x-1,y+1),bc=sample(x,y+1),br=sample(x+1,y+1);
                float nx=(tr+2*mr+br-tl-2*ml-bl)*intrinsics.fx/(8*z);
                float ny=-(bl+2*bc+br-tl-2*tc-tr)*intrinsics.fy/(8*z);
                float normalLength=std::sqrt(nx*nx+ny*ny+1);
                float diffuse=std::max(0.f,(-.58f*nx+.58f*ny+.57f)/normalLength);
                float light=std::clamp(.2f+.8f*diffuse,0.f,1.f);
                detailSurface.push_back({(x+.5f-intrinsics.cx)/intrinsics.fx*z,-(y+.5f-intrinsics.cy)/intrinsics.fy*z,z,light,float(y*width+x)/float(width*height)});
            }
            // With zero plume density, still cache the separate detail surface.
            cached=s;builtVersion=sourceVersion;sourceBuilt=true;
        }
        if(!fresh) history.clear(); // Do not keep emitting from a disconnected camera.
        for(auto& p:particles) {
            p.age+=dt;
            auto velocity=[&](Point at,float time) {
                const Point f=flow(at,time);
                return Point{f.x*s.turbulence+at.x*s.spread*.2f,
                    f.y*s.turbulence+s.drift,
                    -std::abs(f.z)*s.turbulence*.35f-.3f*std::abs(s.drift)};
            };
            // Midpoint integration follows curved streamlines without expanding
            // the entire source surface away from the camera origin.
            const Point v=velocity(p.position,clock-dt);
            Point middle=p.position;
            middle.x+=v.x*dt*.5f;middle.y+=v.y*dt*.5f;middle.z+=v.z*dt*.5f;
            const Point next=velocity(middle,clock-dt*.5f);
            p.position.x+=next.x*dt;p.position.y+=next.y*dt;p.position.z+=next.z*dt;
        }
        particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle& p){
            return p.age>=p.lifetime;
        }),particles.end());
        if(!silhouette.empty()) {
            credit+=64000*density*dt;
            const size_t births=std::min(size_t(credit),maxParticles-particles.size());
            credit-=std::floor(credit); // Capacity overflow is discarded, never queued.
            for(size_t i=0;i<births;++i) {
                const size_t index=std::min(size_t(random()*silhouette.size()),silhouette.size()-1);
                const float seed=random();
                particles.push_back({silhouette[index],0,std::clamp(s.life,.1f,8.f)*( .8f+.4f*seed),seed});
            }
        } else credit=0;
    }
private:
    int width=0,height=0;
    Intrinsics intrinsics;
    float credit=0,clock=0;
    bool sourceBuilt=false;
    uint64_t sourceVersion=0,builtVersion=0;
    Settings cached;
    uint32_t rng=0x94b13f2u;
    float random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return float(rng>>8)/16777216.f; }
};
}
