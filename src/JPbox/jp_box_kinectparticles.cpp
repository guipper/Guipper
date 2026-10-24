#include "jp_box_kinectparticles.h"
#include "../JPgui/jp_gl_state.h"
#include "../JPutils/jp_app_paths.h"

void JPbox_kinectparticles::setup(string directory,string boxName) {
    JPbox::setup(directory,boxName);
    dir="kinectParticles"; name=boxName; tipo=KINECTPARTICLESBOX;
    auto add=[&](float value,const char* label,float lo,float hi) {
        const int i=parameters.getSize();
        parameters.addFloatValue(value,label);
        parameters.setMin(lo,i); parameters.setMax(hi,i);
    };
    add(1.f,"density",0,1); add(1.2f,"point size",1,16);
    add(2.4f,"life s",.1f,8); add(.22f,"turbulence",0,2);
    add(.45f,"rise m/s",-1,1); add(.08f,"spread",0,1.5f);
    add(.65f,"delay s",0,2); add(15,"time stripes",1,40);
    add(1.f,"silhouette",0,1); add(.5f,"near m",.5f,4.49f);
    add(4.5f,"far m",.51f,4.5f); add(1,"zoom",.25f,3);
    add(0,"color cycle",0,1); add(1,"brightness",0,3);
    parameters.addBoolValue(true,"mirror"); parameters.addBoolValue(false,"additive");
    add(1.f,"surface detail",0,1); // Append: preserve saved parameter indices.
    add(1.f,"surface contrast",0,2);
    add(.8f,"plume opacity",0,1);
    // Private renderer resources belong to this executable. A previously
    // migrated development profile does not receive new bundled files.
    const auto shaderPath = jp::AppPaths::current().bundle / "shaders/private/kinectparticles";
    if (!shader.load(shaderPath.string()))
        ofLogError("JPbox_kinectparticles") << "Could not load internal shader: " << shaderPath;
    mesh.setMode(OF_PRIMITIVE_POINTS); mesh.setUsage(GL_STREAM_DRAW);
    mesh.getVertices().reserve(565000); mesh.getColors().reserve(565000);
    cleared=false; lastUpdate=ofGetElapsedTimef(); ensureTarget();
    // Acquire lazily in update, so a deleted/undo-only or inactive box opens nothing.
}
void JPbox_kinectparticles::ensureTarget() {
    const int w=jp_constants::renderWidth,h=jp_constants::renderHeight;
    if(w<=0 || h<=0 || (fbo.isAllocated()&&w==targetWidth&&h==targetHeight))return;
    jp_gl::ScopedNoScissor unclipped;
    ofFbo::Settings s; s.width=w;s.height=h;s.internalformat=GL_RGBA;
    s.useDepth=true;s.useStencil=false;s.numSamples=0;fbo.allocate(s);
    targetWidth=w;targetHeight=h;
    fbo.begin();ofClear(0,0,0,255);fbo.end();
}
void JPbox_kinectparticles::submitDepth(jp_particles::Engine::Depth frame,int w,int h,
    double time,jp_particles::Intrinsics intrinsics) {
    depthFocalY=intrinsics.fy;
    simulation.submit(std::move(frame),w,h,time,intrinsics);
}
void JPbox_kinectparticles::update() {
    JPbox::update(); ensureTarget();
    const double now=ofGetElapsedTimef();
    const float dt=lastUpdate>0?float(now-lastUpdate):0;lastUpdate=now;
    if(!onoff.boolValue || !captureAllowed) {
        capture.reset();simulation.reset();mesh.clear();version=0;updateFBO();return;
    }
    if(!capture) capture=JPbox_kinect2::acquireSharedCapture();
    const auto intrinsics=JPbox_kinect2::depthIntrinsics(capture);
    if(intrinsics.valid) {
        auto frame=JPbox_kinect2::acquireRawDepth(capture,version);
        if(frame) submitDepth(frame,JPbox_kinect2::depthWidth(),JPbox_kinect2::depthHeight(),now,
            {intrinsics.fx,intrinsics.fy,intrinsics.cx,intrinsics.cy});
    }
    advanceSimulation(now,dt);
}
void JPbox_kinectparticles::advanceSimulation(double now,float dt) {
    ensureTarget();
    jp_particles::Settings s;
    s.density=parameters.getFloatValue(DENSITY);s.life=parameters.getFloatValue(LIFE);
    s.turbulence=parameters.getFloatValue(TURBULENCE);s.drift=parameters.getFloatValue(DRIFT);
    s.spread=parameters.getFloatValue(SPREAD);s.delay=parameters.getFloatValue(DELAY);
    s.stripes=parameters.getFloatValue(STRIPES);s.nearM=parameters.getFloatValue(NEAR_M);
    s.farM=std::max(s.nearM+.01f,parameters.getFloatValue(FAR_M));
    s.detail=parameters.getFloatValue(DETAIL);
    simulation.update(now,dt,s);
    mesh.clear();mesh.setMode(OF_PRIMITIVE_POINTS);
    // Draw current surface first so equal-depth delayed splats cannot overwrite
    // its fine features. Plume particles retain their own depth and appearance.
    for(const auto &p:simulation.detailSurface){
        mesh.addVertex({p.x,p.y,p.z});mesh.addColor(ofFloatColor(0,.75f,p.light,s.detail));
    }
    const float silhouette=parameters.getFloatValue(SILHOUETTE)*(1.f-s.detail);
    if(silhouette>0) for(const auto& p:simulation.silhouette) {
        mesh.addVertex({p.x,p.y,p.z});
        mesh.addColor(ofFloatColor(0,1,p.light,silhouette));
    }
    for(const auto& p:simulation.particles) {
        mesh.addVertex({p.position.x,p.position.y,p.position.z});
        const float age=std::clamp(p.age/p.lifetime,0.f,1.f);
        // Color attributes encode age and brightness; palette is evaluated on GPU.
        mesh.addColor(ofFloatColor(age,0,0,1-age));
    }
    updateFBO();
}
void JPbox_kinectparticles::updateFBO() {
    if(tryPassThroughFBO() || !fbo.isAllocated())return;
    jp_gl::ScopedNoScissor unclipped;
    fbo.begin();ofPushStyle();ofClear(0,0,0,255);glClear(GL_DEPTH_BUFFER_BIT);
    if(onoff.boolValue && shader.isLoaded() && mesh.getNumVertices()) {
        const bool additive=parameters.getBoolValue(ADDITIVE);
        ofEnableDepthTest();glDepthMask(GL_TRUE);
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        glEnable(GL_PROGRAM_POINT_SIZE);
        shader.begin();
        shader.setUniform2f("targetSize",fbo.getWidth(),fbo.getHeight());
        shader.setUniform1f("depthFocalY",depthFocalY);
        shader.setUniform1f("pointSize",parameters.getFloatValue(SIZE));
        shader.setUniform1f("zoom",parameters.getFloatValue(ZOOM));
        shader.setUniform1f("colorCycle",parameters.getFloatValue(PALETTE));
        shader.setUniform1f("brightness",parameters.getFloatValue(BRIGHTNESS));
        shader.setUniform1f("surfaceContrast",parameters.getFloatValue(parameters.indexOfName("surface contrast")));
        shader.setUniform1f("plumeOpacity",parameters.getFloatValue(parameters.indexOfName("plume opacity")));
        shader.setUniform1i("mirror",parameters.getBoolValue(MIRROR));
        // The surface owns depth. Transparent plume edges must not occlude
        // later particles; depth-test the plume against the body without writes.
        const size_t sourceCount=mesh.getNumVertices()-simulation.particles.size();
        auto &vbo=mesh.getVbo();
        if(sourceCount)vbo.draw(GL_POINTS,0,sourceCount);
        glDepthMask(GL_FALSE);
        ofEnableBlendMode(additive?OF_BLENDMODE_ADD:OF_BLENDMODE_ALPHA);
        if(simulation.particles.size())vbo.draw(GL_POINTS,sourceCount,simulation.particles.size());
        shader.end();
        glDisable(GL_PROGRAM_POINT_SIZE);glDepthMask(GL_TRUE);ofDisableDepthTest();
    }
    ofPopStyle();fbo.end();
}
string JPbox_kinectparticles::getCaptureStatus() const {
    if(!shader.isLoaded())return "Error de shader";
    if(!captureAllowed || !onoff.boolValue)return "Detenida";
    if(simulation.history.empty())return "Esperando profundidad - " + JPbox_kinect2::captureStatus(capture);
    if(simulation.silhouette.empty()&&simulation.detailSurface.empty())return "Sin profundidad en rango: ajustar near / far";
    return ofToString(simulation.particles.size())+" particulas | "+ofToString(simulation.history.size())+" cuadros";
}
void JPbox_kinectparticles::draw() {
    ofSetColor(255);JPbox::draw();
    fbo.draw(x,y+padding_top/2.f-3.f,fbowidth,fboheight);JPbox::draw_outlet();
}
void JPbox_kinectparticles::setCaptureActive(bool active) {
    captureAllowed=active;
    if(!active) { capture.reset();simulation.reset();mesh.clear();version=0;lastUpdate=0; }
}
void JPbox_kinectparticles::clear() {
    if(cleared)return;
    cleared=true;
    setCaptureActive(false);shader.unload();JPbox::clear();
}
