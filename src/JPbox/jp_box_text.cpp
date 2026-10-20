#include "jp_box_text.h"
#include "../JPutils/jp_text_input.h"
#include "../JPutils/jp_text_layout.h"
#include "../JPutils/jp_font.h"
#include "../JPgui/jp_gl_state.h"
#include <set>

void JPbox_text::setup(string directory,string label) {
    JPbox::setup("textSource",label);dir="textSource";tipo=TEXTBOX;
    auto slider=[&](const char *name,float value,float min,float max) {
        parameters.addFloatValue(value,name);auto *p=parameters.getJParameter(parameters.getSize()-1);
        p->nativeMin=p->min=min;p->nativeMax=p->max=max;p->defaultFloatValue=value;
    };
    slider("font size",96,8,512);slider("block width",.8f,.05f,1);slider("position x",.5f,0,1);slider("position y",.5f,0,1);
    slider("line spacing",1.2f,.5f,3);slider("letter spacing",0,-4,40);slider("padding",24,0,200);
    auto color=[&](const std::string &group,float value) {for(int i=0;i<3;++i){slider((group+" "+std::string(1,"rgb"[i])).c_str(),value,0,1);auto *p=parameters.getJParameter(parameters.getSize()-1);p->colorChannel=JPParameter::COLOR_R+i;p->colorGroup=group;}};
    color("text",1);slider("text opacity",1,0,1);slider("outline width",0,0,24);color("outline",0);
    slider("shadow opacity",0,0,1);slider("shadow x",4,-100,100);slider("shadow y",4,-100,100);slider("shadow softness",4,0,40);
    color("background",0);slider("background opacity",0,0,1);
    slider("font weight",400,100,900);parameters.addBoolValue(false,"italic");
    fontNames=jp_text::families();
    jp_gl::ScopedNoScissor clip;fbo.begin();ofClear(0,0,0,0);fbo.end();
}
jp_text::Style JPbox_text::style() {
    auto v=[&](const char *n){int i=parameters.indexOfName(n);auto *p=parameters.getJParameter(i);return std::clamp(p->floatValue,p->nativeMin,p->nativeMax);};
    jp_text::Style s;s.size=v("font size");s.width=v("block width");s.x=v("position x");s.y=v("position y");s.leading=v("line spacing");s.tracking=v("letter spacing");s.margin=v("padding");
    s.color={v("text r"),v("text g"),v("text b"),v("text opacity")};s.outline=v("outline width");s.edge={v("outline r"),v("outline g"),v("outline b"),1};
    s.shadow=v("shadow opacity");s.shadowX=v("shadow x");s.shadowY=v("shadow y");s.shadowBlur=v("shadow softness");
    s.background={v("background r"),v("background g"),v("background b"),v("background opacity")};s.align=alignment;s.anchor=anchor;s.weight=int(v("font weight"));s.italic=parameters.getBoolValue(parameters.indexOfName("italic"));return s;
}
void JPbox_text::setDraft(const std::string &value) {
    if(value.size()>8192||!jp_text_edit::validUTF8(value))return;
    draft=jp_text_edit::normalized(value,true);if(live)published=draft;
}
void JPbox_text::applyText(){published=draft;}
std::string JPbox_text::snapshot() const {
    return ofJson{{"draft",draft},{"published",published},{"live",live},{"family",family},{"font",fontFile},{"align",alignment},{"anchor",anchor}}.dump();
}
void JPbox_text::restore(const std::string &value) {
    try {
        auto j=ofJson::parse(value);
        auto d=j.value("draft",std::string()),p=j.value("published",d);
        if(d.size()>8192||p.size()>8192||!jp_text_edit::validUTF8(d)||!jp_text_edit::validUTF8(p))return;
        draft=d;published=p;live=j.value("live",true);family=j.value("family",std::string("Sans"));
        if(family.size()>256)family="Sans";
        fontFile=j.value("font",std::string());alignment=std::clamp(j.value("align",1),0,2);anchor=std::clamp(j.value("anchor",1),0,2);
        focused=filterFocused=false;signature.clear();status.clear();
        if(!fontFile.empty()) {
            // Imported fonts are profile-local assets; compositions cannot name arbitrary files.
            if(std::filesystem::path(fontFile).filename()!=fontFile){fontFile.clear();return;}
            const auto path=jp::AppPaths::current().data/"text-fonts"/fontFile;
            if(!std::filesystem::is_regular_file(path))status="Falta la fuente importada. Se usa una fuente de respaldo.";
#ifdef TARGET_LINUX
            else if(!jp_text::registerFont(path.string(),family))status="No se pudo cargar la fuente. Se usa una fuente de respaldo.";
#endif
        }
    } catch(...) {status="No se pudo leer la configuracion del texto";}
}
void JPbox_text::saveCustomState(ofXml &node) const {node.appendChild("textSource").set(snapshot());}
void JPbox_text::loadCustomState(const ofXml &node){auto text=node.getChild("textSource");if(text)restore(text.getValue());}
void JPbox_text::copyCustomStateFrom(const JPbox *source){if(auto *text=dynamic_cast<const JPbox_text*>(source))restore(text->snapshot());}
bool JPbox_text::importFont(const std::string &path) {
    try {
        auto source=std::filesystem::path(path);auto ext=ofToLower(source.extension().string());
        if((ext!=".ttf"&&ext!=".otf")||!std::filesystem::is_regular_file(source)||std::filesystem::file_size(source)>32*1024*1024){status="Elige una fuente .ttf u .otf (hasta 32 MB)";return false;}
        std::string selected=source.stem().string();
#ifdef TARGET_LINUX
        if(!jp_text::registerFont(path,selected)){status="La fuente no se pudo abrir";return false;}
#else
        ofTrueTypeFont probe;if(!probe.load(path,16)){status="La fuente no se pudo abrir";return false;}
#endif
        const auto root=jp::AppPaths::current().data/"text-fonts";std::filesystem::create_directories(root);
        const auto filename=jp_boxuid::mint()+ext;std::filesystem::copy_file(source,root/filename);
        family=selected;fontFile=filename;fontNames=jp_text::families();status.clear();return true;
    } catch(const std::exception &){status="No se pudo guardar la fuente en el perfil";return false;}
}
void JPbox_text::update() {
    JPbox::update();if(!onoff.boolValue||!shouldRenderThisFrame())return;
    std::string current=published+"\x1f"+family+"\x1f"+fontFile+ofToString(alignment)+ofToString(anchor);
    for(int i=0;i<parameters.getSize();++i)current+="|"+(parameters.getType(i)==JPParameter::BOOL?ofToString(parameters.getBoolValue(i)):ofToString(parameters.getFloatValue(i),6));
    const int w=jp_constants::renderWidth,h=jp_constants::renderHeight;
    current+="|"+ofToString(w)+"x"+ofToString(h);
    if(current==signature)return;
    signature=current;
    const auto s=style();jp_gl::ScopedNoScissor clip;ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);
    if(fbo.getWidth()!=w||fbo.getHeight()!=h)fbo.allocate(w,h,GL_RGBA);
#ifdef TARGET_LINUX
    auto pixels=jp_text::rasterize(published,family,s,w,h);
    if(!pixels.error.empty()){status=pixels.error;fbo.begin();ofClear(0,0,0,0);fbo.end();ofPopStyle();return;}
    texture.loadData(pixels.rgba.data(),w,h,GL_RGBA);
    fbo.begin();ofClear(0,0,0,0);ofSetColor(255);ofDisableBlendMode();texture.draw(0,0);fbo.end();
    if(status.empty()||status=="El texto excede el lienzo. Reduce tamano o ajusta posicion.")status=pixels.clipped?"El texto excede el lienzo. Reduce tamano o ajusta posicion.":"";
#else
    fbo.begin();ofClear(0,0,0,0);drawFallback(s);fbo.end();
#endif
    ofPopStyle();++renderCount;
}
void JPbox_text::drawFallback(const jp_text::Style &s) {
    // FreeType backend for OF platforms without Pango. Latin/Greek/Cyrillic and
    // imported outlines; complex-script shaping uses the Linux Pango backend.
    auto path=fontFile.empty()?jp::AppPaths::current().bundle/jp_font::bodyFace:jp::AppPaths::current().data/"text-fonts"/fontFile;
    if(fontFile.empty())path=family=="Serif"?OF_TTF_SERIF:family=="Monospace"?OF_TTF_MONO:OF_TTF_SANS;
    ofTrueTypeFont font;ofTrueTypeFontSettings settings(path,int(s.size));settings.antialiased=true;settings.contours=true;
    settings.addRanges(ofAlphabet::Latin);settings.addRanges(ofAlphabet::Greek);settings.addRanges(ofAlphabet::Cyrillic);
    if(!font.load(settings)){jp_font::loadLatin(font,jp_font::bodyFace,int(s.size));status="Fuente de respaldo en uso";}
    font.setLetterSpacing(1+s.tracking/std::max(1.f,s.size*.5f));font.setLineHeight(s.size*s.leading);
    const float width=std::max(1.f,fbo.getWidth()*s.width-2*s.margin);
    const auto lines=jp_text::wrap(published,width,[&](const auto &v){return font.stringWidth(v);});
    float height=lines.size()*font.getLineHeight(),left=fbo.getWidth()*s.x-width*.5f,top=fbo.getHeight()*s.y-(s.anchor==0?0:s.anchor==2?height:height*.5f);
    auto color=[](const auto &c){ofSetColor(ofFloatColor(c[0],c[1],c[2],c[3]));};
    ofEnableAlphaBlending();if(!published.empty()){color(s.background);ofDrawRectangle(left-s.margin,top-s.margin,width+2*s.margin,height+2*s.margin);}
    float y=top+font.getAscenderHeight();for(const auto &line:lines){float x=left+(s.align==0?0:s.align==2?width-font.stringWidth(line):(width-font.stringWidth(line))*.5f);
        if(s.shadow>0){ofSetColor(ofFloatColor(0,0,0,s.shadow));font.drawString(line,x+s.shadowX,y+s.shadowY);}
        auto paths=font.getStringAsPoints(line);for(auto &p:paths){p.setFillColor(ofFloatColor(s.color[0],s.color[1],s.color[2],s.color[3]));p.setStrokeColor(ofFloatColor(s.edge[0],s.edge[1],s.edge[2],1));p.setStrokeWidth(s.outline*2);p.draw(x,y);}y+=font.getLineHeight();
    }
}
void JPbox_text::clear(){focused=filterFocused=false;texture.clear();JPbox::clear();}

void JPbox_text::layoutEditor(float left,float top,float width) {
    float half=(width-8)/2,y=top;
    modeLive.set(left,y,half,28);modeApply.set(left+half+8,y,half,28);y+=38;
    editor.set(left,y,width,132);y+=140;
    applyButton.set(left,y,half,28);discardButton.set(left+half+8,y,half,28);y+=40;
    fontButton.set(left,y,width-112,30);importButton.set(left+width-104,y,104,30);y+=38;
    if(choosingFont){
        filterField.set(left,y,width,28);y+=34;fontRows.clear();
        std::vector<std::string> matched;for(const auto &f:fontNames)if(ofToLower(f).find(ofToLower(fontFilter))!=std::string::npos)matched.push_back(f);
        int pages=std::max(1,(int(matched.size())+4)/5);fontPage=std::clamp(fontPage,0,pages-1);
        for(int i=fontPage*5;i<std::min(int(matched.size()),fontPage*5+5);++i)fontRows.push_back({ofRectangle(left,y+(i-fontPage*5)*28,width,26),matched[i]});
        y+=140;fontPrev.set(left,y,half,28);fontNext.set(left+half+8,y,half,28);y+=46;
    }
    float third=(width-16)/3;for(int i=0;i<3;++i)alignButtons[i].set(left+i*(third+8),y,third,28);y+=36;
    for(int i=0;i<3;++i)anchorButtons[i].set(left+i*(third+8),y,third,28);
    y+=36;
    messageRect.set(left,y,width,44);
}
void JPbox_text::drawEditor(const std::function<void(const std::string&,const std::string&)> &record) {
    recordEdit=record;auto &font=jp_constants::inspector_media_font;ofPushStyle();
    auto button=[&](const ofRectangle &r,const std::string &text,bool selected=false,bool enabled=true){
        ofFill();ofSetColor(COL_BG_INPUT);ofDrawRectRounded(r,3);ofNoFill();ofSetColor(selected?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(r,3);ofFill();
        ofSetColor(enabled?COL_TEXT_PRIMARY:COL_TEXT_SECONDARY);
        auto label=text;while(!label.empty()&&font.stringWidth(label)>r.width-16)label.erase(jp_text_edit::previous(label,label.size()));
        font.drawString(label,r.x+8,r.y+19);
    };
    button(modeLive,"En vivo",live);button(modeApply,"Preparar texto",!live);
    ofSetColor(COL_BG_INPUT);ofDrawRectRounded(editor,3);ofNoFill();ofSetColor(focused?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(editor,3);ofFill();
    auto field=jp_text_input::field("text-editor-"+uid,"text",editor,font,draft,focused);
    field.multiline=true;field.limit=8192;
    field.focus=[this]{focused=true;editBefore=snapshot();};
    field.change=[this]{setDraft(draft);};
    field.cancel=[this]{focused=false;};
    field.commit=[this]{focused=false;if(recordEdit)recordEdit(editBefore,snapshot());return true;};
    jp_text_input::controller().add(std::move(field));
    bool pending=draft!=published;
    button(applyButton,pending?"Aplicar cambios":"Texto aplicado",pending,pending);
    button(discardButton,"Descartar borrador",false,pending);
    button(fontButton,family,choosingFont);button(importButton,"Importar fuente");
    if(choosingFont){
        ofSetColor(COL_BG_INPUT);ofDrawRectRounded(filterField,3);
        auto search=jp_text_input::field("text-font-"+uid,"text-fonts",filterField,font,fontFilter,filterFocused);
        search.limit=128;search.focus=[this]{filterFocused=true;};search.commit=[this]{filterFocused=false;return true;};search.cancel=[this]{filterFocused=false;};search.change=[this]{fontPage=0;};
        jp_text_input::controller().add(std::move(search));
        if(fontFilter.empty()&&!filterFocused){ofSetColor(COL_TEXT_SECONDARY);font.drawString("Buscar fuente...",filterField.x+8,filterField.y+19);}
        if(fontRows.empty()){ofSetColor(COL_TEXT_SECONDARY);font.drawString("Sin fuentes para esta busqueda",filterField.x+8,filterField.y+52);}
        for(const auto &row:fontRows)button(row.first,row.second,family==row.second);
        button(fontPrev,"Anterior");button(fontNext,"Siguiente");
    }
    const char *alignLabels[]={"Izquierda","Centro","Derecha"};const char *anchors[]={"Arriba","Medio","Abajo"};
    for(int i=0;i<3;++i){button(alignButtons[i],alignLabels[i],alignment==i);button(anchorButtons[i],anchors[i],anchor==i);}
    ofSetColor(status.empty()?COL_TEXT_SECONDARY:COL_ACCENT_GOLD);
    std::string message=status.empty()?(pending?"Borrador pendiente: la salida conserva el texto anterior.":live?"Edicion en vivo. Enter: nueva linea. Ctrl+Enter: salir.":"Prepara la siguiente frase y pulsa Aplicar cambios."):status;
    auto lines=jp_text::wrap(message,messageRect.width,[&](const auto &s){return font.stringWidth(s);});
    for(size_t i=0;i<std::min(size_t(2),lines.size());++i)font.drawString(lines[i],messageRect.x,messageRect.y+13+i*16);
    ofPopStyle();
}
bool JPbox_text::clickEditor(float x,float y) {
    auto before=snapshot();bool handled=true;
    if(modeLive.inside(x,y)){live=true;applyText();}
    else if(modeApply.inside(x,y))live=false;
    else if(applyButton.inside(x,y))applyText();
    else if(discardButton.inside(x,y))draft=published;
    else if(fontButton.inside(x,y)){choosingFont=!choosingFont;filterFocused=false;}
    else if(importButton.inside(x,y)){
        auto dialog=ofSystemLoadDialog("Importar fuente TTF u OTF",false);if(dialog.bSuccess)importFont(dialog.getPath());
    } else if(choosingFont&&fontPrev.inside(x,y))fontPage=std::max(0,fontPage-1);
    else if(choosingFont&&fontNext.inside(x,y))++fontPage;
    else {handled=false;
        if(choosingFont)for(const auto &row:fontRows)if(row.first.inside(x,y)){family=row.second;fontFile.clear();choosingFont=false;filterFocused=false;status.clear();handled=true;break;}
        for(int i=0;i<3;++i){if(alignButtons[i].inside(x,y)){alignment=i;handled=true;}if(anchorButtons[i].inside(x,y)){anchor=i;handled=true;}}
    }
    if(handled&&recordEdit)recordEdit(before,snapshot());
    return handled;
}

void JPbox_text::draw() {
    JPbox::draw();ofPushStyle();ofSetColor(255);
    fbo.draw(x,y+padding_top/2-3,fbowidth,fboheight);
    ofPopStyle();JPbox::draw_outlet();
}
