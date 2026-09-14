#include "ofApp.h"
#include "JPutils/jp_storage.h"
#include "JPutils/jp_version.h"
#include "JPutils/jp_textwrap.h"
#include <ctime>
#include "JPgui/jp_gl_state.h"

void ofApp::loadReleasePreferences() {
    try {
        const auto file = jp::preferencePath("updates.json");
        if (!ofFile::doesFileExist(file)) return;
        const auto value = ofJson::parse(jp::readBytes(file));
        updates.automatic = value.value("automatic", false);
        updates.lastCheck = value.value("lastCheck", std::int64_t(0));
        updates.channel = value.value("channel", std::string("stable"));
        if (updates.channel != "beta") updates.channel = "stable";
    } catch (const std::exception& error) { ofLogWarning("updates") << error.what(); }
}
void ofApp::saveReleasePreferences() {
    try {
        ofJson value={{"automatic",updates.automatic},{"lastCheck",updates.lastCheck},{"channel",updates.channel}};
        jp::atomicWrite(jp::preferencePath("updates.json"), value.dump(2));
    } catch (const std::exception& error) { releaseMessage = error.what(); }
}
bool ofApp::releaseActionEnabled(int action) {
    const auto state=updates.status().state;
    const bool busy=state==jp::UpdateState::Checking || state==jp::UpdateState::Downloading || state==jp::UpdateState::Ready;
    switch (action) {
    case 0: return !busy && state!=jp::UpdateState::Disabled;
    case 1: return state==jp::UpdateState::Available;
    case 2: return state==jp::UpdateState::Ready;
    case 3: return state==jp::UpdateState::Checking || state==jp::UpdateState::Available || state==jp::UpdateState::Downloading || state==jp::UpdateState::Ready;
    case 4: return true;
    case 5: return !busy;
    case 6: return true;
    default: return false;
    }
}
void ofApp::releaseAction(int action) {
    if (!releaseActionEnabled(action)) return;
    releaseMessage.clear();
    jp::recordEvent("release_action_" + std::to_string(action));
    switch (action) {
    case 0: updates.check(true, std::time(nullptr)); saveReleasePreferences(); break;
    case 1: updates.download(); break;
    case 2:
        if (updates.status().state != jp::UpdateState::Ready) break;
        // The checked save is the final gate; editors also have independent
        // dirty tabs and must be saved explicitly before an install.
        if (shaderEditor.hasUnsavedChanges()) {
            releaseMessage = language == 0 ? "Save shader tabs before installing." : "Guardá las pestañas de shaders antes de instalar.";
            break;
        }
        if (saveSession(savedirectory) && updates.install(true,true)) ofExit();
        break;
    case 3: updates.cancel(); break;
    case 4: updates.automatic=!updates.automatic; saveReleasePreferences(); break;
    case 5:
        if (updates.status().state == jp::UpdateState::Checking || updates.status().state == jp::UpdateState::Downloading || updates.status().state == jp::UpdateState::Ready) break;
        updates.channel=updates.channel=="stable"?"beta":"stable"; saveReleasePreferences(); break;
    case 6: {
        auto destination=ofSystemSaveDialog("guipper-diagnostics.json", language==0?"Export diagnostics":"Exportar diagnóstico");
        if (!destination.bSuccess) break;
        try {
            ofJson report={{"version",jp::version},{"gpuVendor",gpuVendor},{"gpuRenderer",gpuRenderer},{"openGL",gpuGlVersion},
                {"updateChannel",updates.channel},{"updateState",int(updates.status().state)}};
#ifdef TARGET_WIN32
            report["platform"]="Windows";
#elif defined(TARGET_LINUX)
            report["platform"]="Linux";
#else
            report["platform"]="Other";
#endif
            // Controlled event records; no raw compiler logs, source text,
            // project serialization, media paths or user assets attached.
            jp::recordEvent("diagnostics_requested");
            const auto events=jp::AppPaths::current().state / "logs/events.log";
            report["events"] = std::filesystem::is_regular_file(events) ? jp::readBytes(events) : "";
            jp::atomicWrite(destination.getPath(), report.dump(2));
            releaseMessage=language==0?"Diagnostics exported.":"Diagnóstico exportado.";
        } catch (const std::exception& error) { releaseMessage=error.what(); }
        break;
    }
    default: break;
    }
}
void ofApp::drawReleasePanel() {
    if (!releasePanelOpen) return;
    const float width=std::max(1.0f,std::min(600.0f,float(ofGetWidth())-24.0f));
    const float x=(ofGetWidth()-width)*0.5f;
    const float height=std::max(1.0f,std::min(430.0f,float(ofGetHeight())-24.0f));
    const float top=std::max(12.0f,(ofGetHeight()-height)*0.5f);
    releaseViewport=ofRectangle(x,top,width,height);
    releaseScroll=ofClamp(releaseScroll,0.0f,430.0f-height);
    const float y=top-releaseScroll;
    auto fit=[&](std::string text) {
        if (font_p.stringWidth(text)<=width-60) return text;
        while (!text.empty() && font_p.stringWidth(text+"...")>width-60) {
            // Remove a complete UTF-8 codepoint.
            size_t at=text.size()-1;
            while (at>0 && (static_cast<unsigned char>(text[at])&0xc0)==0x80) --at;
            text.resize(at);
        }
        return text+"...";
    };
    ofPushStyle();
    // Canvas nodes may leave CENTER mode active. Panel geometry and hit targets
    // are window-space top-left rectangles, regardless of the preceding view.
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofEnableAlphaBlending();
    ofSetLineWidth(1.0f);
    ofFill(); ofSetColor(0,0,0,180); ofDrawRectangle(0,0,ofGetWidth(),ofGetHeight());
    ofSetColor(COL_BG_PANEL); ofDrawRectangle(releaseViewport);
    jp_gl::ScopedScissor clip(releaseViewport);
    ofSetColor(COL_TEXT_PRIMARY);
    modalFont.drawString("Guipper " + std::string(jp::version),x+20,y+32);
    const auto status=updates.status();
    const bool es=language!=0;
    const char* english[]={"Updates unavailable in this build", "No update selected", "Checking for updates...", "A new version is available", "Downloading...", "Verified update ready", "Update postponed", "Update could not complete"};
    const char* spanish[]={"Actualizaciones no disponibles en esta compilación", "Sin actualización seleccionada", "Buscando actualizaciones...", "Hay una nueva versión", "Descargando...", "Actualización verificada y lista", "Actualización pospuesta", "No se pudo completar la actualización"};
    font_p.drawString(fit((es?spanish:english)[int(status.state)]),x+20,y+61);
    font_p.drawString(fit((es?"Canal: ":"Channel: ")+updates.channel+(es?" | Consulta diaria: ":" | Daily checks: ")+(updates.automatic?(es?"sí":"on"):(es?"no":"off"))),x+20,y+86);
    const std::vector<std::string> labels=es?
        std::vector<std::string>{"Buscar actualizaciones", "Descargar", "Guardar e instalar", "Posponer / cancelar", "Activar / desactivar consulta diaria", "Cambiar canal stable / beta", "Exportar diagnóstico"}:
        std::vector<std::string>{"Check for updates", "Download", "Save and install", "Postpone / cancel", "Enable / disable daily checks", "Switch stable / beta channel", "Export diagnostics"};
    for (int i=0;i<7;++i) {
        releaseButtons[i]=ofRectangle(x+20,y+101+i*35,width-40,29);
        const bool enabled=releaseActionEnabled(i);
        ofSetColor(enabled?COL_TEXT_PRIMARY:COL_TEXT_SECONDARY);
        ofNoFill(); ofDrawRectangle(releaseButtons[i]); ofFill();
        font_p.drawString(fit(ofToString(i+1)+". "+labels[i]),x+30,y+121+i*35);
    }
    ofSetColor(COL_TEXT_PRIMARY);
    auto message=releaseMessage;
    if (status.state==jp::UpdateState::Error && !status.message.empty()) message=status.message;
    if (status.message=="Cancelling check...") message=es?"Cancelando consulta...":"Cancelling check...";
    if (status.message=="Stopping download...") message=es?"Deteniendo descarga...":"Stopping download...";
    if (message.empty()) message=es?"Esc: cerrar. Nunca se instala ni reinicia automáticamente.":"Esc: close. Installation and restart are always your choice.";
    const auto lines=jp_textwrap::wrap([this](const string& text){return font_p.stringWidth(text);},message,width-40);
    for (size_t i=0;i<std::min<size_t>(lines.size(),3);++i) font_p.drawString(lines[i],x+20,y+370+i*17);
    if (height<430) {
        ofSetColor(COL_TEXT_SECONDARY);
        const float thumb=height*height/430.0f;
        ofDrawRectangle(x+width-6,top+(height-thumb)*releaseScroll/(430.0f-height),3,thumb);
    }
    ofPopStyle();
}
