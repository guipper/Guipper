#include "jp_box_screen.h"
#include "../JPutils/jp_constants.h"
#include "../JPgui/jp_gl_state.h"
#if defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
#include "ofAppGLFWWindow.h"
#include <sstream>
#endif

void JPbox_screen::setup(string directory, string label) {
    JPbox::setup(directory,label);dir="screenCapture";name=label;tipo=SCREENBOX;
    parameters.addFloatValue(.5f,"scalex");parameters.addFloatValue(.5f,"scaley");
    parameters.addFloatValue(.5f,"offsetx");parameters.addFloatValue(.5f,"offsety");
    parameters.addBoolValue(false,"strech");
    parameters.addFloatValue(1.f,"scale ratio");
    parameters.addBoolValue(false,"capture window");
    auto *ratio=parameters.getJParameter(5);
    ratio->nativeMin=ratio->min=.1f;ratio->nativeMax=ratio->max=4.f;ratio->defaultFloatValue=1.f;
}
void JPbox_screen::setWindowMode(bool window) {
    if(windowMode()==window)return;
    stopCapture();monitorChoices.clear();parameters.setBoolValue(window,6);
}
void JPbox_screen::selectScreen() {
    if(retained)return;
    if(capture.snapshot().state==jp_screen::State::Selecting){capture.cancelSelection();return;}
    setonoff(true);
#ifdef _WIN32
    monitorChoices=windowMode()?jp_screen::windows():jp_screen::monitors();choosingMonitor=true;
#elif defined(TARGET_LINUX) && !defined(TARGET_RASPBERRY_PI_LEGACY)
    std::string parent;
    if(auto *window=dynamic_cast<ofAppGLFWWindow *>(ofGetWindowPtr())) {
        std::ostringstream id;id<<"x11:"<<std::hex<<window->getX11Window();parent=id.str();
    }
    capture.select(parent);
#else
    capture.select();
#endif
}
void JPbox_screen::selectMonitor(size_t index) {
    if(index>=monitorChoices.size())return;
    choosingMonitor=false;capture.select(monitorChoices[index].id);
}
void JPbox_screen::stopCapture() {
    choosingMonitor=false;capture.stop();uploaded.reset();texture.clear();
    if(fbo.isAllocated()){jp_gl::ScopedNoScissor unclipped;fbo.begin();ofClear(0,0,0,0);fbo.end();}
}
void JPbox_screen::setCaptureActive(bool active) {
    retained=!active;
    if(!active)stopCapture();
}
void JPbox_screen::update() {
    JPbox::update();
    if(!onoff.boolValue || retained){
        if(uploaded || choosingMonitor || capture.snapshot().state==jp_screen::State::Selecting)stopCapture();
        return;
    }
    auto state=capture.update();
    if(state.frame && state.frame!=uploaded){
        if(!uploaded || uploaded->width!=state.frame->width || uploaded->height!=state.frame->height) {
            texture.clear();
            texture.allocate(state.frame->width,state.frame->height,GL_RGBA);
        }
        texture.loadData(state.frame->rgba.data(),state.frame->width,state.frame->height,GL_RGBA);
        uploaded=state.frame;
    }else if(!state.frame && uploaded){
        uploaded.reset();texture.clear();
        jp_gl::ScopedNoScissor unclipped;
        fbo.begin();ofClear(0,0,0,0);fbo.end();
    }
    if(shouldRenderThisFrame())updateFBO();
}
void JPbox_screen::updateFBO() {
    jp_gl::ScopedNoScissor unclipped;
    ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofSetColor(255);
    ofEnableBlendMode(OF_BLENDMODE_DISABLED);
    fbo.begin();ofClear(0,0,0,0);
    if(uploaded && texture.isAllocated() && onoff.boolValue && !retained) {
        float w=fbo.getWidth(),h=fbo.getHeight();
        if(!parameters.getBoolValue(4)) {
            const float fit=std::min(w/uploaded->width,h/uploaded->height);
            w=uploaded->width*fit;h=uploaded->height*fit;
        }
        const float zoom=parameters.getFloatValue(5);
        w*=parameters.getFloatValue(0)*2.f*zoom;h*=parameters.getFloatValue(1)*2.f*zoom;
        const float x=(fbo.getWidth()-w)*.5f+(parameters.getFloatValue(2)-.5f)*2.f*fbo.getWidth();
        const float y=(fbo.getHeight()-h)*.5f+(parameters.getFloatValue(3)-.5f)*2.f*fbo.getHeight();
        texture.draw(x,y,w,h);
    }
    fbo.end();ofPopStyle();
}
void JPbox_screen::draw() {
    JPbox::draw();ofPushStyle();ofSetColor(255);
    fbo.draw(x,y+padding_top/2-3,fbowidth,fboheight);ofPopStyle();JPbox::draw_outlet();
}
void JPbox_screen::clear(){stopCapture();JPbox::clear();}
string JPbox_screen::statusLabel() const {
    if(choosingMonitor)return monitorChoices.empty()?"No hay fuentes disponibles":windowMode()?"Elegi una ventana":"Elegi un monitor";
    const auto s=capture.snapshot();
    if(!s.message.empty())return s.message;
    switch(s.state){
    case jp_screen::State::Selecting:return "Esperando seleccion o permiso";
    case jp_screen::State::Streaming:return uploaded?"Capturando - "+ofToString(uploaded->width)+" x "+ofToString(uploaded->height):"Esperando imagen";
    case jp_screen::State::Stopped:return "Captura detenida";
    case jp_screen::State::Error:return "No se pudo capturar. Selecciona nuevamente.";
    case jp_screen::State::Unavailable:return "Captura no disponible";
    default:return windowMode()?"Sin ventana seleccionada":"Sin pantalla seleccionada";
    }
}
