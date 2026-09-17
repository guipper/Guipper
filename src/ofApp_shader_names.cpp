#include "ofApp.h"
#include "JPutils/jp_storage.h"

void ofApp::loadCuratedShaderNames() {
    curatedShaderNames = ofJson::object();
    if (!shaderCuratedMode) return;
    try {
        const auto file = jp::preferencePath("curated-names.json");
        if (!std::filesystem::exists(file)) return;
        auto names=ofJson::parse(jp::readBytes(file));
        if (!names.is_object()) throw std::runtime_error("Invalid name preferences");
        curatedShaderNames=std::move(names);
    } catch (const std::exception& e) {
        ofLogWarning("curated-names") << e.what();
        publishToast("shader-name", jp::ToastState::Error, language==0 ?
            "Could not read shader names." : "No se pudieron leer los nombres de shaders.");
    }
}
bool ofApp::commitShaderName() {
    if (!shaderNameFocused) return true;
    const auto first=shaderNameText.find_first_not_of(" \t\r\n");
    const string name=first==string::npos ? "" : shaderNameText.substr(first,shaderNameText.find_last_not_of(" \t\r\n")-first+1);
    if (name.empty() || name.size()>512) {
        publishToast("shader-name",jp::ToastState::Warning,language==0 ?
            "Enter a name to continue." : "Escribí un nombre para continuar.");
        return false;
    }
    if (!shaderReviewFile.empty()) {
        if (!saveShaderReview(shaderNamePath,{{"name",{{shaderNameLanguage==0?"en":"es",name}}}})) return false;
        // Remove the old profile override so the shared catalog remains authoritative.
        if (curatedShaderNames.contains(shaderNamePath) && curatedShaderNames[shaderNamePath].is_object())
            curatedShaderNames[shaderNamePath].erase(shaderNameLanguage==0?"en":"es");
        try {jp::atomicWrite(jp::preferencePath("curated-names.json"),curatedShaderNames.dump(2)+"\n");}
        catch(const std::exception& e){ofLogWarning("curated-names")<<e.what();}
        shaderNameFocused=false;shaderNameSelectAll=false;
        rebuildShaderFolderOrder();ensureShaderSelectionVisible();return true;
    }
    try {
        auto updated=curatedShaderNames;
        const auto file=jp::preferencePath("curated-names.json");
        if (std::filesystem::exists(file)) updated=ofJson::parse(jp::readBytes(file));
        if (!updated.is_object()) throw std::runtime_error("Invalid name preferences");
        updated[shaderNamePath][shaderNameLanguage==0 ? "en" : "es"]=name;
        jp::atomicWrite(jp::preferencePath("curated-names.json"),updated.dump(2)+"\n");
        curatedShaderNames=std::move(updated);
    } catch (const std::exception& e) {
        ofLogError("curated-names") << e.what();
        publishToast("shader-name",jp::ToastState::Error,language==0 ?
            "Could not save the shader name. Try again." : "No se pudo guardar el nombre. Intentá de nuevo.");
        return false;
    }
    for (auto& folder : shaderFolders) for (auto& entry : folder.shaders)
        if (entry.path==shaderNamePath) {
            if (shaderNameLanguage==0) entry.metadata.name.en=name;
            else entry.metadata.name.es=name;
        }
    shaderNameFocused=false; shaderNameSelectAll=false;
    rebuildShaderFolderOrder(); ensureShaderSelectionVisible();
    return true;
}
void ofApp::handleShaderNameKey(int key) {
    if (key==OF_KEY_ESC) { shaderNameFocused=false; shaderNameSelectAll=false; return; }
    if (key==OF_KEY_RETURN || key=='\r' || key==OF_KEY_TAB) { commitShaderName(); return; }
    const bool modifier=ofGetKeyPressed(OF_KEY_CONTROL)||ofGetKeyPressed(OF_KEY_COMMAND);
    if (key==1 || (modifier && (key=='a'||key=='A'))) {shaderNameSelectAll=true;return;}
    if (key==3 || (modifier && (key=='c'||key=='C'))) {
        if (shaderNameSelectAll) ofGetWindowPtr()->setClipboardString(shaderNameText);
        return;
    }
    auto prev=[&](int pos){if(pos>0)--pos;while(pos>0 && (static_cast<unsigned char>(shaderNameText[pos])&0xc0)==0x80)--pos;return pos;};
    auto next=[&](int pos){if(pos<(int)shaderNameText.size())++pos;while(pos<(int)shaderNameText.size() && (static_cast<unsigned char>(shaderNameText[pos])&0xc0)==0x80)++pos;return pos;};
    auto eraseSelection=[&](){if(shaderNameSelectAll){shaderNameText.clear();shaderNameCursor=0;shaderNameSelectAll=false;}};
    if (key==OF_KEY_LEFT) {shaderNameCursor=shaderNameSelectAll?0:prev(shaderNameCursor);shaderNameSelectAll=false;return;}
    if (key==OF_KEY_RIGHT) {shaderNameCursor=shaderNameSelectAll?shaderNameText.size():next(shaderNameCursor);shaderNameSelectAll=false;return;}
    if (key==OF_KEY_HOME || key==OF_KEY_END) {shaderNameCursor=key==OF_KEY_HOME?0:shaderNameText.size();shaderNameSelectAll=false;return;}
    if (key==OF_KEY_BACKSPACE || key==OF_KEY_DEL) {
        if(shaderNameSelectAll)eraseSelection();
        else if(key==OF_KEY_BACKSPACE){int start=prev(shaderNameCursor);shaderNameText.erase(start,shaderNameCursor-start);shaderNameCursor=start;}
        else shaderNameText.erase(shaderNameCursor,next(shaderNameCursor)-shaderNameCursor);
        return;
    }
    string inserted;
    if (key==22 || (modifier && (key=='v'||key=='V'))) {
        inserted=ofGetWindowPtr()->getClipboardString();
        for(char& c:inserted)if(c=='\n'||c=='\r'||c=='\t')c=' ';
    } else if (!modifier && ((key>=32 && key<=126)||(key>=160 && key<=0x24f))) ofUTF8Append(inserted,key);
    if (inserted.empty()) return;
    if ((shaderNameSelectAll?0:shaderNameText.size())+inserted.size()>512) return;
    eraseSelection();shaderNameText.insert(shaderNameCursor,inserted);shaderNameCursor+=inserted.size();
}
void ofApp::drawShaderNameField(const ofRectangle& field) {
    string text=shaderNameFocused ? shaderNameText :
        shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].displayName(language!=0);
    int cursor=shaderNameFocused?shaderNameCursor:0;
    int start=0;
    auto advance=[&](int p){++p;while(p<(int)text.size() && (static_cast<unsigned char>(text[p])&0xc0)==0x80)++p;return p;};
    while(start<cursor && font_p.stringWidth(text.substr(start,cursor-start))>field.width-18)start=advance(start);
    int end=start;
    while(end<(int)text.size()) {int n=advance(end);if(font_p.stringWidth(text.substr(start,n-start))>field.width-18)break;end=n;}
    const string shown=text.substr(start,end-start);
    ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofFill();ofSetColor(COL_BG_INPUT);ofDrawRectRounded(field,3);
    ofNoFill();ofSetColor(shaderNameFocused?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(field,3);ofFill();
    if(shaderNameFocused && shaderNameSelectAll){ofSetColor(ofColor(COL_ACCENT_CYAN_DIM,120));ofDrawRectangle(field.x+5,field.y+3,font_p.stringWidth(shown),field.height-6);}
    ofSetColor(COL_TEXT_PRIMARY);font_p.drawString(shown,field.x+7,field.y+18);
    if(shaderNameFocused && !shaderNameSelectAll) {
        float x=field.x+7+font_p.stringWidth(text.substr(start,cursor-start));ofDrawLine(x,field.y+5,x,field.getBottom()-5);
    }
    ofPopStyle();
    jp_tooltip::drawFor(language==0 ? "Enter to save · Esc to cancel" : "Enter para guardar · Esc para cancelar",
        field,field.inside(ofGetMouseX(),ofGetMouseY()),"shader-visible-name");
}
