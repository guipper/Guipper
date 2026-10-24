#include "jp_box_sarv.h"
#include "../JPgui/jp_gl_state.h"
#include "../JPutils/jp_app_paths.h"

void JPbox_sarv::setup(string directory,string boxName) {
    JPbox::setup(directory,boxName); dir="sarv";name=boxName;tipo=SARVBOX;
    auto add=[&](float v,const char* n,float lo,float hi){int i=parameters.getSize();parameters.addFloatValue(v,n);parameters.setMin(lo,i);parameters.setMax(hi,i);};
    add(15,"time_slices",1,60); add(2,"kinect_depth",0,8);
    add(.0015f,"min_points_scale",.0001f,.02f);add(.0015f,"max_points_scale",.0001f,.02f);
    add(0,"spheres_cubes_points",0,2);add(2,"particles_res_mult",1,2);
    add(1,"particle_life_duration",.05f,10);add(0,"particle_life_variance",0,1);
    const auto base=jp::AppPaths::current().bundle/"shaders/private";
    evolve.load((base/"sarv_update").string());render.load((base/"sarv_render").string());
    cleared=false;previous=ofGetElapsedTimef();simulationTime=previous;allocate();
}
void JPbox_sarv::allocate() {
    jp_gl::ScopedNoScissor unclipped;
    if(!fbo.isAllocated()||fbo.getWidth()!=jp_constants::renderWidth||fbo.getHeight()!=jp_constants::renderHeight){
        ofFbo::Settings s;s.width=jp_constants::renderWidth;s.height=jp_constants::renderHeight;s.internalformat=GL_RGBA;s.useDepth=true;s.numSamples=0;fbo.allocate(s);
    }
    int m=std::clamp(int(std::round(parameters.getFloatValue(5))),1,2);
    if(sw==512*m&&state[0].isAllocated())return;
    sw=512*m;sh=424*m;initialized=false;current=0;
    ofFbo::Settings s;s.width=sw;s.height=sh;s.internalformat=GL_RGBA32F;s.textureTarget=GL_TEXTURE_RECTANGLE;s.minFilter=GL_NEAREST;s.maxFilter=GL_NEAREST;s.useDepth=false;s.numSamples=0;
    for(auto& f:state){f.allocate(s);f.begin();ofClear(999,999,999,0);f.end();}
    indices.clear();indices.setMode(OF_PRIMITIVE_POINTS);indices.setUsage(GL_STATIC_DRAW);
    for(int y=0;y<sh;++y)for(int x=0;x<sw;++x)indices.addVertex({x+.5f,y+.5f,0});
    surface.clear();surface.setMode(OF_PRIMITIVE_POINTS);surface.setUsage(GL_STATIC_DRAW);
    for(int y=0;y<424;++y)for(int x=0;x<512;++x)surface.addVertex({x+.5f,y+.5f,0});
    pixels.allocate(512,424,4);
    ofTextureData td;td.width=512;td.height=424;td.glInternalFormat=GL_RGBA32F;td.textureTarget=GL_TEXTURE_RECTANGLE;source.allocate(td);source.setTextureMinMagFilter(GL_NEAREST,GL_NEAREST);
}
void JPbox_sarv::buildSource(double now) {
    if(history.empty()||!intrinsics.valid)return;
    const float cutoff=parameters.getFloatValue(1);
    for(int y=0;y<424;++y)for(int x=0;x<512;++x){
        // Mirrored vertical ramp, displaced by a coherent animated field.
        float warp=.05f*ofNoise(x/1536.f,y/1272.f,float(now)*.1f);
        float phase=(1.f-(y+.5f)/424.f+warp)/.03f;
        float ramp=1.f-std::abs(std::fmod(std::abs(phase),2.f)-1.f);
        float delay=std::clamp((ramp-.41057938f)/(.59445846f-.41057938f),0.f,1.f);
        size_t age=size_t(std::round(delay*(history.size()-1)));
        float z=(*history[history.size()-1-age])[y*512+x]*.001f;
        float* p=&pixels[(y*512+x)*4];
        if(!std::isfinite(z)||z<=0||z>=cutoff||now-lastDepth>.5){p[0]=p[1]=p[2]=999;p[3]=0;continue;}
        // Match the TD depth point cloud, then translate and rotate Y by -180 degrees.
        p[0]=-(x+.5f-intrinsics.cx)/intrinsics.fx*z+.07f;
        p[1]=-(y+.5f-intrinsics.cy)/intrinsics.fy*z+.21f;p[2]=1.f-z;p[3]=1;
    }
    source.loadData(pixels);
}
void JPbox_sarv::step(double now) {
    if(!evolve.isLoaded())return;
    jp_gl::ScopedNoScissor unclipped;
    state[1-current].begin();ofPushStyle();ofDisableDepthTest();ofDisableAlphaBlending();ofSetColor(255);
    evolve.begin();evolve.setUniformTexture("previousState",state[current].getTexture(),0);evolve.setUniformTexture("emitter",source,1);
    evolve.setUniform2f("stateSize",sw,sh);evolve.setUniform1i("initialize",!initialized);evolve.setUniform1f("time",now);
    evolve.setUniform1f("duration",parameters.getFloatValue(6));evolve.setUniform1f("variance",parameters.getFloatValue(7));
    ofDrawRectangle(0,0,sw,sh);evolve.end();ofPopStyle();state[1-current].end();current=1-current;initialized=true;
}
void JPbox_sarv::update(){
    JPbox::update();allocate();double now=ofGetElapsedTimef();double dt=std::clamp(now-previous,0.,.1);previous=now;
    if(!active||!onoff.boolValue){setCaptureActive(active);updateFBO();return;}
    if(!capture)capture=JPbox_kinect2::acquireSharedCapture();
    intrinsics=JPbox_kinect2::depthIntrinsics(capture);
    if(auto frame=JPbox_kinect2::acquireRawDepth(capture,version)){latest=frame;lastDepth=now;}
    accumulator+=dt;
    while(accumulator>=1./60.){
        accumulator-=1./60.;simulationTime+=1./60.;
        if(latest&&latest->size()==512*424){history.push_back(latest);while(history.size()>size_t(std::clamp(int(std::round(parameters.getFloatValue(0))),1,60)))history.pop_front();buildSource(now);step(simulationTime);}
    }
    updateFBO();
}
void JPbox_sarv::updateFBO(){
    if(tryPassThroughFBO()||!fbo.isAllocated())return;
    jp_gl::ScopedNoScissor unclipped;fbo.begin();ofPushStyle();ofClear(0,0,0,255);glClear(GL_DEPTH_BUFFER_BIT);
    if(active&&onoff.boolValue&&initialized&&render.isLoaded()){
        ofEnableDepthTest();glDepthMask(GL_TRUE);ofDisableAlphaBlending();glEnable(GL_PROGRAM_POINT_SIZE);
        render.begin();render.setUniformTexture("positions",source,0);render.setUniform2f("targetSize",fbo.getWidth(),fbo.getHeight());
        render.setUniform1i("surface",1);render.setUniform1i("shape",inspectorChoice());render.setUniform2f("scaleRange",parameters.getFloatValue(2),parameters.getFloatValue(3));surface.draw();
        render.setUniformTexture("positions",state[current].getTexture(),0);render.setUniform1i("surface",0);indices.draw();render.end();
        glDisable(GL_PROGRAM_POINT_SIZE);ofDisableDepthTest();
    }
    ofPopStyle();fbo.end();
}
void JPbox_sarv::draw(){ofSetColor(255);JPbox::draw();fbo.draw(x,y+padding_top/2.f-3.f,fbowidth,fboheight);JPbox::draw_outlet();}
int JPbox_sarv::inspectorChoice() const{return parameters.parameters.size()>4?std::clamp(int(std::round(parameters.parameters[4]->floatValue)),0,2):0;}
void JPbox_sarv::setInspectorChoice(int choice){parameters.setFloatValue(std::clamp(choice,0,2),4);parameters.setFloatLerpValue(std::clamp(choice,0,2),4);}
string JPbox_sarv::inspectorChoiceStatus() const{
    if(!evolve.isLoaded()||!render.isLoaded())return "Error de shader SARV";
    if(!active||!onoff.boolValue)return "Detenida";
    if(!initialized||ofGetElapsedTimef()-lastDepth>.5)return JPbox_kinect2::captureStatus(capture);
    return ofToString(sw*sh)+" particles | "+ofToString(history.size())+" frames";
}
void JPbox_sarv::setCaptureActive(bool value){active=value;if(!active||!onoff.boolValue){capture.reset();history.clear();latest.reset();version=0;initialized=false;accumulator=0;}}
void JPbox_sarv::clear(){if(cleared)return;cleared=true;setCaptureActive(false);evolve.unload();render.unload();source.clear();for(auto& f:state)f.clear();JPbox::clear();}
