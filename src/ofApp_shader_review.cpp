#include "ofApp.h"
#include "JPutils/jp_storage.h"
#include "JPgui/jp_button.h"

int ofApp::curatedReviewRows() const { return shaderCuratedMode ? 5 : 0; }
int ofApp::previewInspectorRows() const {
    return curatedReviewRows() + std::max(1,int(previewUniformNames.size()+previewBoolNames.size()));
}
string ofApp::selectedShaderLoadPath() const {
    if (getSelectedShaderPath().empty()) return {};
    const auto& entry=shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
    if (!entry.metadata.groupPath.empty()) return entry.metadata.groupPath;
    return entry.standalone ? entry.path : string();
}
bool ofApp::saveShaderReview(const string& path, const ofJson& changes) {
    if (!shaderCuratedMode || shaderReviewFile.empty()) return false;
    try {
        auto root=ofJson::parse(jp::readBytes(shaderReviewFile));
        jp_shader_catalog::parseCurated(root); // Never replace a corrupt catalog.
        ofJson* target=nullptr;
        for (auto& entry:root["entries"]) if (entry["path"]==path) {target=&entry;break;}
        if (!target) {
            const ShaderEntry* source=nullptr;
            for (const auto& folder:shaderFolders) for (const auto& entry:folder.shaders) if(entry.path==path)source=&entry;
            if (!source) throw std::runtime_error("Shader no longer exists");
            const auto& m=source->metadata;
            root["entries"].push_back({{"path",path},{"category",m.category},{"name",{{"en",m.name.en},{"es",m.name.es}}},
                {"description",{{"en",m.description.en},{"es",m.description.es}}},{"tags",m.tags},{"inputs",m.inputs},
                {"user_visible",false},{"needs_parameters",false},{"needs_improvement",false},{"ask_pupper",false},{"group_path",""}});
            target=&root["entries"].back();
        }
        target->merge_patch(changes);
        const auto parsed=jp_shader_catalog::parseCurated(root);
        jp::atomicWrite(shaderReviewFile,root.dump(2)+"\n");
        for (const auto& metadata:parsed) if(metadata.path==path)
            for (auto& folder:shaderFolders) for(auto& entry:folder.shaders) if(entry.path==path)entry.metadata=metadata;
        return true;
    } catch (const std::exception& e) {
        ofLogError("shader-review")<<e.what();
        publishToast("curation",jp::ToastState::Error,language==0 ? "Could not save the curation changes." : "No se pudieron guardar los cambios de curado.");
        return false;
    }
}
bool ofApp::assignShaderGroup(const string& path, const string& group) {
    if (!group.empty() && (ofToLower(ofFilePath::getFileExt(group))!="xml" || !JPboxgroup::validateGroupFile(group))) {
        publishToast("curation",jp::ToastState::Error,language==0 ?
            "Choose a valid Guipper group or composition with all its source files." :
            "Elegí un grupo o composición de Guipper con sus archivos de origen disponibles.");
        return false;
    }
    ofJson changes={{"group_path",group}};
    if(group.empty()) for(const auto& folder:shaderFolders)for(const auto& entry:folder.shaders)
        if(entry.path==path && !entry.standalone)changes["user_visible"]=false;
    if (!saveShaderReview(path,changes)) return false;
    if(getSelectedShaderPath()==path){previewShaderPath.clear();selectShaderForPreview(selectedShaderFolder,selectedShaderIndex);}
    return true;
}
void ofApp::pressShaderReviewRow(int row, float x, const ofRectangle& bounds) {
    if(getSelectedShaderPath().empty())return;
    // Copy before changing metadata or opening a system dialog.
    const auto entry=shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
    if(row==0) {
        if(!entry.standalone && entry.metadata.groupPath.empty())return;
        saveShaderReview(entry.path,{{"user_visible",!entry.metadata.userVisible}});
    } else if(row==1) saveShaderReview(entry.path,{{"needs_parameters",!entry.metadata.needsParameters}});
    else if(row==2) saveShaderReview(entry.path,{{"needs_improvement",!entry.metadata.needsImprovement}});
    else if(row==3) saveShaderReview(entry.path,{{"ask_pupper",!entry.metadata.askPupper}});
    else if(row==4) {
        if(!entry.metadata.groupPath.empty() && x>=bounds.getRight()-30) {assignShaderGroup(entry.path,"");return;}
        auto result=ofSystemLoadDialog(language==0 ? "Choose a Guipper group/composition (.xml)" : "Elegí un grupo/composición de Guipper (.xml)",false);
        toastLastUpdate=ofGetElapsedTimef();
        if(result.bSuccess)assignShaderGroup(entry.path,ofFilePath::getAbsolutePath(result.getPath()));
    }
}
void ofApp::drawShaderReviewRow(int row, const ofRectangle& bounds) {
    const auto& entry=shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
    auto fit=[&](const string& text,float width){return jp_tooltip::fit(text,width,[&](const string& v){return font_p.stringWidth(v);});};
    if(row<4) {
        const bool on=row==0?entry.metadata.userVisible:row==1?entry.metadata.needsParameters:row==2?entry.metadata.needsImprovement:entry.metadata.askPupper;
        const bool enabled=row!=0 || entry.standalone || !entry.metadata.groupPath.empty();
        const string label=row==0 ? (language==0?"Show in user version":"Mostrar al usuario") : row==1 ? (language==0?"Add parameters (pending)":"Agregar parámetros (pendiente)") : row==2 ? (language==0?"Improve shader":"Mejorar shader") : (language==0?"Ask Pupper":"Preguntarle a Pupper");
        ofSetColor(COL_BG_INPUT);ofDrawRectRounded(bounds,3);
        ofRectangle check(bounds.x+5,bounds.y+5,15,15);
        ofNoFill();ofSetColor(enabled?COL_TEXT_SECONDARY:COL_TEXT_MUTED);ofDrawRectangle(check);ofFill();
        if(on){ofSetColor(row==0?COL_ACCENT_GREEN:COL_ACCENT_GOLD);ofDrawRectangle(check.x+3,check.y+3,9,9);}
        ofSetColor(enabled?COL_TEXT_PRIMARY:COL_TEXT_MUTED);font_p.drawString(fit(label,bounds.width-32),bounds.x+28,bounds.y+18);
        jp_tooltip::drawFor(label,bounds,bounds.inside(ofGetMouseX(),ofGetMouseY()),"review-flag-"+ofToString(row));
    } else {
        const bool assigned=!entry.metadata.groupPath.empty();
        ofRectangle choose=bounds;if(assigned)choose.width-=32;
        const string label=assigned?(language==0?"Group: ":"Grupo: ")+ofFilePath::getFileName(entry.metadata.groupPath):
            (language==0?"Assign group...":"Asociar grupo...");
        jp_button::draw(choose,fit(label,choose.width-12),false,true,COL_BORDER_MUTED);
        if(assigned)jp_button::draw(ofRectangle(bounds.getRight()-28,bounds.y,28,bounds.height),"x",false,true,COL_BORDER_MUTED);
        jp_tooltip::drawFor(assigned ? entry.metadata.groupPath : (language==0?"LOAD will insert the saved nodes and connections as a group.":"LOAD insertará los nodos y conexiones guardados como grupo."),
            choose,choose.inside(ofGetMouseX(),ofGetMouseY()),"review-group");
    }
}

bool ofApp::selectedShaderHasInputs() const {
    if (getSelectedShaderPath().empty()) return false;
    const auto& entry=shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
    return !entry.metadata.inputs.empty() || (previewShaderPath==entry.path && !previewInputNames.empty());
}
bool ofApp::matchesShaderReviewFilter(const ShaderEntry& entry) const {
    if (!shaderCuratedMode) return true;
    if (shaderReviewFilter==1) return entry.metadata.userVisible;
    if (shaderReviewFilter==2) return !entry.metadata.userVisible;
    if (shaderReviewFilter==3) return entry.metadata.needsParameters;
    if (shaderReviewFilter==4) return entry.metadata.needsImprovement;
    if (shaderReviewFilter==5) return entry.metadata.askPupper;
    return true;
}

string ofApp::folderTabLabel(int index) const {
    int count=0;
    const string query=ofToLower(shaderSearchText);
    for(int f=0;f<int(shaderFolders.size());++f) {
        const auto& folder=shaderFolders[f];
        if(index==0 ? folder.isFavorites : f!=index-1)continue;
        const bool folderMatch=ofToLower(folder.name+" "+jp_shader_catalog::categoryName(folder.category,true)).find(query)!=string::npos;
        for(const auto& entry:folder.shaders) if(matchesShaderReviewFilter(entry) &&
            (query.empty() || folderMatch || ofToLower(entry.name+" "+entry.path).find(query)!=string::npos ||
            (entry.catalogued && jp_shader_catalog::matches(entry.metadata,shaderSearchText))))++count;
    }
    string label=language==0?"All":"Todas";
    if(index>0) {
        const auto& folder=shaderFolders[index-1];
        string category=folder.category.empty()?folder.name:folder.category;
        if(folder.isFavorites)label=language==0?"Favorites":"Favoritos";
        else if(category=="generative")label=language==0?"Generative":"Generativos";
        else if(category=="effects" || category=="imageprocessing")label=language==0?"Effects":"Efectos";
        else if(category=="mixers" || category=="blending")label=language==0?"Mixers":"Mezclas";
        else if(category=="contrib")label="Contrib";
        else if(category=="combo")label="Combo";
        else if(category=="internal")label=language==0?"Helpers":"Auxiliares";
        else label=folder.name;
    }
    return label+" ("+ofToString(count)+")";
}
