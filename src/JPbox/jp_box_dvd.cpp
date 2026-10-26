#include "jp_box_dvd.h"
#include "../JPgui/jp_gl_state.h"
#include <cmath>

float JPbox_dvd::value(const char* name) const {
    for(auto* p:parameters.parameters) if(p->name==name)
        return std::clamp(p->floatValue,p->nativeMin,p->nativeMax);
    return 0;
}
void JPbox_dvd::setup(string directory,string label) {
    JPbox::setup("dvdBounce",label);dir="dvdBounce";tipo=DVDBOX;
    auto slider=[&](const char* name,float initial,float lo,float hi){
        parameters.addFloatValue(initial,name);auto* p=parameters.getJParameter(parameters.getSize()-1);
        p->min=p->nativeMin=lo;p->max=p->nativeMax=hi;p->defaultFloatValue=initial;
    };
    slider("size",.25,.02,1);slider("speed x",.15,-1,1);slider("speed y",.11,-1,1);
    parameters.addBoolValue(false,"pause");
    fbohandlergroup.addFbohandler("imagen");fbohandlergroup.setupdragobjects(x,y,outlet_size,outlet_size);
    phaseX=.25;phaseY=.35;lastTime=ofGetElapsedTimef();updateFBO();
}
void JPbox_dvd::update() {
    JPbox::update();fbohandlergroup.setPos(x-width/2,y,0);
    if(shouldRenderThisFrame())updateFBO();
    else lastTime=ofGetElapsedTimef();
}
void JPbox_dvd::updateFBO() {
    const double now=ofGetElapsedTimef(),dt=std::clamp(now-lastTime,0.0,.1);lastTime=now;
    if(tryPassThroughFBO())return;
    const int w=jp_constants::renderWidth,h=jp_constants::renderHeight;
    if(w<1||h<1)return;
    jp_gl::ScopedNoScissor clip;ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofSetColor(255);ofDisableBlendMode();
    if(fbo.getWidth()!=w||fbo.getHeight()!=h)fbo.allocate(w,h,GL_RGBA);
    auto* input=fbohandlergroup.getisPointerSet(0)?fbohandlergroup.getFboPointerReference(0):nullptr;
    fbo.begin();ofClear(0,0,0,0);
    if(onoff.boolValue&&input&&input!=&fbo&&input->isAllocated()) {
        const float scale=std::min(w/input->getWidth(),h/input->getHeight())*value("size");
        const float iw=input->getWidth()*scale,ih=input->getHeight()*scale;
        const float travelX=std::max(0.f,w-iw),travelY=std::max(0.f,h-ih);
        if(!parameters.getBoolValue(parameters.indexOfName("pause"))) {
            // Speed is canvas lengths per second, independent of frame rate.
            if(travelX>0)phaseX=std::fmod(std::fmod(phaseX+dt*value("speed x")*w/travelX,2)+2,2);
            if(travelY>0)phaseY=std::fmod(std::fmod(phaseY+dt*value("speed y")*h/travelY,2)+2,2);
        }
        const auto bounce=[](double p){return float(1-std::abs(1-p));};
        input->draw(bounce(phaseX)*travelX,bounce(phaseY)*travelY,iw,ih);
    }
    fbo.end();ofPopStyle();
}
void JPbox_dvd::draw() {
    JPbox::draw();ofPushStyle();ofSetColor(255);fbo.draw(x,y+padding_top/2-3,fbowidth,fboheight);ofPopStyle();draw_outlet();draw_inlets();
}
