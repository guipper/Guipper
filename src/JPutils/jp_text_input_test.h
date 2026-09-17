#pragma once
#include "jp_text_input.h"
#include "../ofApp.h"
#include "ofAppGLFWWindow.h"

namespace jp_text_input_test {
inline bool run(ofApp& app) {
    auto& ui=jp_text_input::controller();ui.reset();
    app.registerSurfaces();app.pantallaActiva=app.NODOS;
    app.boxes.clear();app.boxes.addBox("shaders/generative/basic2.frag",100,100);app.boxes.selectOpenBoxByIndex(0);

    bool passed=true;
    auto check=[&](bool value,const char* label){if(!value)ofLogError("text-input")<<label;passed=value&&passed;};
    std::string a="12",b="second",committed=a;
    int focus=0,commits=0,cancels=0;
    auto frame=[&]{
        ui.beginFrame();
        for(int i=0;i<2;++i) {
            auto f=jp_text_input::field(i?"b":"a","test",ofRectangle(10,10+i*50,220,30),jp_constants::p_font,i?b:a,focus==i);
            f.focus=[&,i]{focus=i;};f.cancel=[&]{focus=-1;++cancels;};
            f.commit=[&,i]{if(!i)committed=a;focus=-1;++commits;return true;};
            if(!i)f.validate=[](const std::string& s){return jp_text_input::integer(s,1,100);};
            ui.add(std::move(f));
        }
        ui.endFrame();
    };
    frame();ui.model.selectAll();ui.key('x');
    check(a=="x","draft permits incomplete invalid values");
    check(ui.press(15,70,0)&&focus==0&&commits==0,"invalid blur retains focus and consumes target click");
    check(ui.release(),"rejected click owns release");
    check(ui.model.text=="x"&&!ui.model.error.empty(),"validation keeps draft and explains error");
    ui.key(OF_KEY_ESC);check(a=="12"&&focus==-1&&cancels==1,"escape restores initial value");
    frame();ui.press(60,20,0);ui.release();ui.model.selectAll();ui.key('4');ui.key('2');
    ui.key(OF_KEY_TAB);check(committed=="42"&&focus==1,"tab commits and advances");
    frame();ui.key(OF_KEY_ESC);frame();
    ui.press(20,70,0);ui.drag(80,70);check(ui.model.selected(),"drag selects");ui.release();
    b=std::string(120,'a');frame();ui.press(100,70,0);
    const auto clickedCursor=ui.model.cursor;frame();ui.drag(100,70);
    check(ui.model.cursor==clickedCursor,"click keeps a stable horizontal viewport");ui.release();
    ui.finish(true);frame();ui.press(20,70,0);ui.release();
    // Real OF event dispatch: text must consume both callbacks and preserve nodes/session.
    const auto originalPath=ofToDataPath("uishots/text-input/must-not-save.xml",true);app.savedirectory=originalPath;const auto count=app.boxes.boxes.size();
    auto event=[&](int key,int physical,int modifiers){ofKeyEventArgs e(ofKeyEventArgs::Pressed,key,physical,0,key,modifiers);ofGetWindowPtr()->events().notifyKeyEvent(e);ofKeyEventArgs up(ofKeyEventArgs::Released,key,physical,0,key,0);ofGetWindowPtr()->events().notifyKeyEvent(up);};
    event('a',65,OF_KEY_CONTROL);event('c',67,OF_KEY_CONTROL);
    check(ofGetWindowPtr()->getClipboardString()==b,"clipboard copies selection");
    ofGetWindowPtr()->setClipboardString("niño\r\ncanción");event('v',86,OF_KEY_CONTROL);
    check(b=="niño canción","paste once and normalize newlines");
    event('z',90,OF_KEY_CONTROL);check(b=="second","undo stays local");
    event('y',89,OF_KEY_CONTROL);check(b=="niño canción","redo stays local");
    event('s',83,OF_KEY_CONTROL);check(app.savedirectory==originalPath&&!app.saveModalActive&&!ofFile::doesFileExist(originalPath),"save shortcut consumed by text");
    event(OF_KEY_DEL,261,0);check(app.boxes.boxes.size()==count,"delete does not delete nodes");
    ui.finish(true);ui.reset();
    // Real settings fields share the same adapter and validators.
    app.pantallaActiva=app.OPCIONES;app.initOptionsFields();
    app.focusedOptionsField=app.FIELD_OSC_PORT_IN;
    app.draw();ui.model.selectAll();for(char c:std::string("65536"))ui.key(c);ui.key(OF_KEY_RETURN);
    check(app.focusedOptionsField==app.FIELD_OSC_PORT_IN,"settings rejects invalid port");
    ui.key(OF_KEY_ESC);check(app.focusedOptionsField==-1,"settings escape only unfocuses input");
    ui.reset();app.clearFieldFocus();
    // Register the real search and ensure Escape keeps the live query.
    app.pantallaActiva=app.SHADER_INDEX;app.shaderSearchText="niño";app.shaderSearchFocused=true;
    app.draw();ui.key('s');ui.key(OF_KEY_ESC);check(app.shaderSearchText=="niños"&&!app.shaderSearchFocused,"search escape retains query");
    // Save modal is above all other input surfaces, with a separate text focus.
    app.openSaveModal();app.saveModalName="test";
    ofGetWindowPtr()->setClipboardString("first-frame");event('a',65,OF_KEY_CONTROL);event('v',86,OF_KEY_CONTROL);
    app.draw();check(app.saveModalName=="first-frame","newly opened field preserves input before its first draw");ui.key(OF_KEY_ESC);
    check(app.saveModalActive&&!app.saveModalTextFocused,"first escape only exits modal input");
    app.draw();app.keyPressed(OF_KEY_ESC);check(!app.saveModalActive,"second escape closes modal");
    ui.reset();app.clearFieldFocus();
    ofDirectory::createDirectory(ofToDataPath("savefiles/text-input-blocked.xml",true),true,true);
    app.openSaveModal();app.saveModalName="text-input-blocked";app.draw();ui.model.selectAll();ui.key(OF_KEY_RETURN);
    check(app.saveModalActive&&app.saveModalTextFocused&&ui.model.selection()=="text-input-blocked","failed save keeps draft selection and focus");
    ui.key(OF_KEY_ESC);app.cancelSaveModal();ui.reset();app.clearFieldFocus();
    // One visual batch: both languages, wide and compact, long UTF-8 input.
    const string shots=ofToDataPath("uishots/text-input",true);ofDirectory::createDirectory(shots,true,true);
    for(int lang=0;lang<2;++lang)for(int compact=0;compact<2;++compact) {
        ui.reset();app.clearFieldFocus();app.language=lang;app.pantallaActiva=app.OPCIONES;
        ofSetWindowShape(compact?800:1440,compact?600:900);
        for(int poll=0;poll<10;++poll)ofAppGLFWWindow::pollEvents();
        app.initOptionsFields();app.optionsFieldText[app.FIELD_DEFAULT_COMPO]="/composiciones/área de trabajo/niño/canción con un nombre muy largo.xml";
        app.focusedOptionsField=app.FIELD_DEFAULT_COMPO;
        ofGetWindowPtr()->makeCurrent();ofGetWindowPtr()->swapBuffers();ofGetWindowPtr()->startRender();app.draw();ui.model.selectAll();app.draw();glReadBuffer(GL_BACK);
        ofImage shot;shot.grabScreen(0,0,ofGetWidth(),ofGetHeight());
        size_t nonBlack=0;for(const auto& pixel:shot.getPixels().getPixelsIter())if(pixel[0]||pixel[1]||pixel[2])++nonBlack;
        check(nonBlack>size_t(ofGetWidth()*ofGetHeight()/100),"capture contains the UI");shot.save(shots+"/settings-"+(lang?"es":"en")+(compact?"-small":"-wide")+".png");ofGetWindowPtr()->finishRender();
    }
    ui.reset();app.clearFieldFocus();app.boxes.clear();
    ofLogNotice("text-input")<<"passed="<<passed;return passed;
}
}
