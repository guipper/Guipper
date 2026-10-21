#include "jp_box_analogcolor.h"
#include "jp_analogcolor_shaders.h"
#include "../JPgui/jp_gl_state.h"
#include "../JPutils/jp_text_layout.h"
float JPbox_analogcolor::value(const char *name)const{
    for(auto *p:parameters.parameters)if(p->name==name)return std::clamp(p->floatValue,p->nativeMin,p->nativeMax);
    return 0;
}
void JPbox_analogcolor::setup(string directory,string label){
    JPbox::setup("analogColor",label);dir="analogColor";tipo=ANALOGCOLORBOX;
    auto slider=[&](const char *name,float v,float lo,float hi){parameters.addFloatValue(v,name);auto *p=parameters.getJParameter(parameters.getSize()-1);p->min=p->nativeMin=lo;p->max=p->nativeMax=hi;p->defaultFloatValue=v;};
    slider("palette",0,0,2);slider("colors",8,2,16);slider("dither",.35,0,1);slider("grain",.12,0,1);
    slider("bleed",.7,0,4);slider("contrast",1,.5,2);slider("saturation",1,0,2);slider("mix",1,0,1);
    slider("pixel size",1,1,64);
    fbohandlergroup.addFbohandler("imagen");fbohandlergroup.setupdragobjects(x,y,outlet_size,outlet_size);
    reload();updateFBO();
}
void JPbox_analogcolor::reload(){
    shader.unload();
    if(!shader.setupShaderFromSource(GL_VERTEX_SHADER,jp_analogcolor::vertex)||!shader.setupShaderFromSource(GL_FRAGMENT_SHADER,jp_analogcolor::fragment)||!shader.bindDefaults()||!shader.linkProgram())shader.unload();
}
void JPbox_analogcolor::update(){JPbox::update();parameters.setFloatValue(std::round(value("colors")),parameters.indexOfName("colors"));fbohandlergroup.setPos(x-width/2,y,0);if(shouldRenderThisFrame())updateFBO();}
void JPbox_analogcolor::updateFBO(){
    if(tryPassThroughFBO())return;
    jp_gl::ScopedNoScissor clip;ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofSetColor(255);ofFill();ofDisableBlendMode();
    const int w=jp_constants::renderWidth,h=jp_constants::renderHeight;
    if(w<1||h<1){ofPopStyle();return;}
    if(fbo.getWidth()!=w||fbo.getHeight()!=h)fbo.allocate(w,h,GL_RGBA);
    auto *input=fbohandlergroup.getisPointerSet(0)?fbohandlergroup.getFboPointerReference(0):nullptr;
    fbo.begin();ofClear(0,0,0,0);
    if(onoff.boolValue&&ready()&&input&&input!=&fbo&&input->isAllocated()){
        shader.begin();shader.setUniformTexture("source",input->getTexture(),1);
        shader.setUniform2f("resolution",w,h);shader.setUniform2f("sourceSize",input->getWidth(),input->getHeight());
        shader.setUniform1i("paletteMode",int(std::round(value("palette"))));shader.setUniform1i("colorCount",int(std::round(value("colors"))));
        shader.setUniform1f("dithering",value("dither"));shader.setUniform1f("grain",value("grain"));shader.setUniform1f("bleed",value("bleed"));
        shader.setUniform1f("contrastAmount",value("contrast"));shader.setUniform1f("saturationAmount",value("saturation"));shader.setUniform1f("mixAmount",value("mix"));
        shader.setUniform1f("pixelSize",std::round(value("pixel size")));
        shader.setUniform1f("frameSeed",float(ofGetFrameNum()%10000));ofDrawRectangle(0,0,w,h);shader.end();
    }
    fbo.end();ofPopStyle();
}
void JPbox_analogcolor::draw(){JPbox::draw();ofPushStyle();ofSetColor(255);fbo.draw(x,y+padding_top/2-3,fbowidth,fboheight);ofPopStyle();draw_outlet();draw_inlets();}
void JPbox_analogcolor::clear(){shader.unload();JPbox::clear();}
void JPbox_analogcolor::layoutInspector(float x,float y,float w){for(int i=0;i<3;++i)palettes[i].set(x+i*(w+8)/3,y,(w-16)/3,30);status.set(x,y+42,w,75);}
void JPbox_analogcolor::drawInspector(){
    auto &font=jp_constants::inspector_media_font;ofPushStyle();const char *labels[]={"Analogica","Sepia","Grises"};
    for(int i=0;i<3;++i){ofFill();ofSetColor(COL_BG_INPUT);ofDrawRectRounded(palettes[i],3);ofNoFill();ofSetColor(int(std::round(value("palette")))==i?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(palettes[i],3);ofFill();ofSetColor(COL_TEXT_PRIMARY);font.drawString(labels[i],palettes[i].x+8,palettes[i].y+20);}
    const string text=!ready()?"No se pudo compilar el efecto. Recarga la caja.":!fbohandlergroup.getisPointerSet(0)?"Conecta una imagen, video o camara a imagen.":ofToString(int(std::round(value("colors"))))+" colores. Pixel size: 1 original; valores mayores pixelan. Dither: tramado; grain: grano; bleed: sangrado. Mix 0 conserva el original.";
    auto lines=jp_text::wrap(text,status.width,[&](const string &s){return font.stringWidth(s);});ofSetColor(COL_TEXT_SECONDARY);for(size_t i=0;i<std::min(size_t(4),lines.size());++i)font.drawString(lines[i],status.x,status.y+13+i*16);ofPopStyle();
}
bool JPbox_analogcolor::clickInspector(float x,float y){for(int i=0;i<3;++i)if(palettes[i].inside(x,y)){parameters.setFloatValue(i,0);parameters.setFloatLerpValue(i,0);return true;}return false;}
