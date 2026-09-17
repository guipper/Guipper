#include "ofApp.h"
#include "JPutils/jp_text_input.h"
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
        shaderNameFocused=false;
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
    shaderNameFocused=false;
    rebuildShaderFolderOrder(); ensureShaderSelectionVisible();
    return true;
}
void ofApp::handleShaderNameKey(int key) { jp_text_input::controller().key(key); }
void ofApp::drawShaderNameField(const ofRectangle& field) {
    ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofFill();ofSetColor(COL_BG_INPUT);ofDrawRectRounded(field,3);
    ofNoFill();ofSetColor(shaderNameFocused?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(field,3);ofFill();ofPopStyle();
    auto f=jp_text_input::field("shader-name","curator",field,font_p,shaderNameText,shaderNameFocused);
    f.read=[this]{return shaderNameFocused?shaderNameText:shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].displayName(language!=0);};
    f.focus=[this]{
        const auto& entry=shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
        shaderNameText=entry.displayName(language!=0);shaderNamePath=entry.path;shaderNameLanguage=language;shaderNameFocused=true;
    };
    f.cancel=[this]{shaderNameFocused=false;};
    f.commit=[this]{return commitShaderName();};f.limit=512;
    f.validate=[](const string& v)->string{return v.find_first_not_of(" \t\r\n")==string::npos?jp_text_input::message("Enter a name","Ingresá un nombre"):"";};
    jp_text_input::controller().add(std::move(f));
}
