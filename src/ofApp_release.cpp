#include "ofApp.h"
#include "JPutils/jp_storage.h"
#include "JPutils/jp_version.h"
#include "JPutils/jp_textwrap.h"
#include <ctime>
#include "JPgui/jp_gl_state.h"
#include "JPgui/jp_button.h"

void ofApp::loadReleasePreferences() {
    try {
        const auto file = jp::preferencePath("updates.json");
        if (!ofFile::doesFileExist(file)) return;
        const auto value = ofJson::parse(jp::readBytes(file));
        updates.skippedVersion=value.value("skippedVersion",std::string());
        updates.automatic = value.value("automatic", false);
        updates.lastCheck = value.value("lastCheck", std::int64_t(0));
        updates.channel = value.value("channel", std::string("stable"));
        if (updates.channel != "beta") updates.channel = "stable";
    } catch (const std::exception& error) { ofLogWarning("updates") << error.what(); }
}
void ofApp::saveReleasePreferences() {
    try {
        ofJson value={{"automatic",updates.automatic},{"lastCheck",updates.lastCheck},{"channel",updates.channel},{"skippedVersion",updates.skippedVersion}};
        jp::atomicWrite(jp::preferencePath("updates.json"), value.dump(2));
    } catch (const std::exception& error) { releaseMessage = error.what(); }
}
bool ofApp::releaseActionEnabled(int action) {
    const auto state=updates.status().state;
    if (state==jp::UpdateState::Installing) return action==3;
    const bool busy=state==jp::UpdateState::Checking || state==jp::UpdateState::Downloading || state==jp::UpdateState::Ready;
    switch (action) {
    case 0: return !busy && state!=jp::UpdateState::Disabled;
    case 1: return state==jp::UpdateState::Available;
    case 2: return state==jp::UpdateState::Ready;
    case 3: return state==jp::UpdateState::Checking || state==jp::UpdateState::Available || state==jp::UpdateState::Downloading || state==jp::UpdateState::Ready;
    case 4: return true;
    case 5: return !busy;
    case 6: return true;
    case 7: return state==jp::UpdateState::Available && !updates.status().version.empty();
    case 8: return !updates.status().notesUrl.empty();
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
        if (saveSession(savedirectory)) {
            updates.install(true,true);
            if (updates.exitRequested()) ofExit();
        }
        break;
    case 3: updates.cancel(); break;
    case 4: updates.automatic=!updates.automatic; saveReleasePreferences(); break;
    case 5:
        if (updates.status().state == jp::UpdateState::Checking || updates.status().state == jp::UpdateState::Downloading || updates.status().state == jp::UpdateState::Ready) break;
        updates.cancel(); updates.channel=updates.channel=="stable"?"beta":"stable"; saveReleasePreferences(); break;
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
    case 7: updates.skip(); saveReleasePreferences(); break;
    case 8: ofLaunchBrowser(updates.status().notesUrl); break;
    default: break;
    }
}
void ofApp::drawReleasePanel() {
    if (!releasePanelOpen) return;
    const auto status=updates.status();
    const bool es=language!=0;
    const float width=std::max(1.0f,std::min(640.0f,float(ofGetWidth())-24.0f));
    const float inset=std::min(24.0f,width*0.08f), inner=width-inset*2;
    const bool compact=inner<540;
    const float gap=jp_button::kGap, buttonHeight=32;
    auto measure=[this](const string& value){return font_p.stringWidth(value);};
    auto wrap=[&](const std::string& text) {
        std::vector<std::string> result;
        for (const auto& line:jp_textwrap::wrap(measure,text,std::max(1.0f,inner))) {
            std::string part;
            for (size_t at=0;at<line.size();) {
                size_t end=at+1;
                while (end<line.size() && (static_cast<unsigned char>(line[end])&0xc0)==0x80) ++end;
                const auto codepoint=line.substr(at,end-at);
                if (!part.empty() && font_p.stringWidth(part+codepoint)>inner) {result.push_back(part);part.clear();}
                part+=codepoint;at=end;
            }
            result.push_back(part);
        }
        return result;
    };
    auto fit=[&](std::string text,float limit) {
        if (jp_constants::p_font.stringWidth(text)<=limit) return text;
        while (!text.empty() && jp_constants::p_font.stringWidth(text+"...")>limit) {
            size_t at=text.size()-1;
            while (at>0 && (static_cast<unsigned char>(text[at])&0xc0)==0x80) --at;
            text.resize(at);
        }
        return text+"...";
    };
    const char* english[]={"Updates unavailable in this build", "Ready to check for updates", "Checking for updates...", "A new version is available", "Downloading...", "Verified update ready", "Update postponed", "Update could not complete", "Preparing installation..."};
    const char* spanish[]={"Actualizaciones no disponibles en esta compilación", "Listo para buscar actualizaciones", "Buscando actualizaciones...", "Hay una nueva versión", "Descargando...", "Actualización verificada y lista", "Actualización pospuesta", "No se pudo completar la actualización", "Preparando instalación..."};
    std::string statusText=(es?spanish:english)[int(status.state)];
    const bool downloading=status.state==jp::UpdateState::Downloading;
    if (downloading) statusText=status.message=="Verifying signature..." ?
        (es?"Verificando firma...":"Verifying signature...") : statusText+" "+ofToString(int(ofClamp(status.progress,0.0,1.0)*100))+"%";
    std::string message=releaseMessage;
    if (status.state==jp::UpdateState::Error && !status.message.empty()) message=status.message;
    if (status.state==jp::UpdateState::Disabled && message.empty())
        message=es?"Abrí un AppImage firmado con actualizaciones habilitadas.":"Open a signed AppImage with updates enabled.";
    if (status.message=="Cancelling check...") message=es?"Cancelando consulta...":"Cancelling check...";
    if (status.message=="Stopping download...") message=es?"Deteniendo descarga...":"Stopping download...";
    if (status.state==jp::UpdateState::Installing) message=es?"Verificando antes de instalar. Podés cancelar.":"Verifying before installation. You can cancel.";
    const auto statusLines=wrap(statusText);
    const auto versionLines=wrap(status.version);
    const auto messageLines=message.empty()?std::vector<std::string>{}:wrap(message);
    const auto footerLines=wrap(es?"Guardá tu trabajo antes de instalar. Nunca se reinicia automáticamente.":"Save your work before installing. Guipper never restarts automatically.");
    const float rowBlock=compact?3*(buttonHeight+gap):buttonHeight+gap;
    const float statusHeight=statusLines.size()*20+(status.version.empty()?0:versionLines.size()*18+8)+(downloading?16:0);
    const float contentHeight=100+statusHeight+rowBlock*2+30+2*(buttonHeight+gap)+30+buttonHeight+24+messageLines.size()*18+(messageLines.empty()?0:12)+footerLines.size()*18+24;
    const float height=std::max(1.0f,std::min(contentHeight,float(ofGetHeight())-24.0f));
    const float x=(ofGetWidth()-width)*0.5f, top=(ofGetHeight()-height)*0.5f;
    releaseViewport=ofRectangle(x,top,width,height);
    releaseScrollMax=std::max(0.0f,contentHeight-height);
    releaseScroll=ofClamp(releaseScroll,0.0f,releaseScrollMax);
    float y=top-releaseScroll;
    const float left=x+inset;
    ofPushStyle();
    ofSetRectMode(OF_RECTMODE_CORNER);
    ofEnableAlphaBlending(); ofSetLineWidth(1); ofFill();
    ofSetColor(0,0,0,180); ofDrawRectangle(0,0,ofGetWidth(),ofGetHeight());
    ofSetColor(COL_BG_PANEL); ofDrawRectRounded(releaseViewport,4);
    ofNoFill(); ofSetColor(COL_BORDER_DEFAULT); ofDrawRectRounded(releaseViewport,4); ofFill();
    jp_gl::ScopedScissor clip(releaseViewport);
    jp_pointer::Scope pointer(jp_pointer::kModal);
    ofSetColor(COL_TEXT_PRIMARY);
    modalFont.drawString(es?"Actualizaciones":"Updates",left,y+32);
    releaseCloseButton=ofRectangle(x+width-inset-64,y+16,64,28);
    jp_button::draw(releaseCloseButton,"Esc",false,status.state!=jp::UpdateState::Installing);
    ofSetColor(COL_TEXT_SECONDARY);
    font_p.drawString("Guipper "+std::string(jp::version),left,y+57);
    ofSetColor(COL_BORDER_MUTED); ofDrawLine(left,y+72,left+inner,y+72);
    y+=100;
    ofSetColor(status.state==jp::UpdateState::Error?COL_ACCENT_RED_DIM:
        (status.state==jp::UpdateState::Ready?COL_ACCENT_GREEN:COL_TEXT_PRIMARY));
    for (const auto& line:statusLines) {font_p.drawString(line,left,y);y+=20;}
    if (!status.version.empty()) {
        y+=8; ofSetColor(COL_TEXT_SECONDARY);
        for (const auto& line:versionLines) {font_p.drawString(line,left,y);y+=18;}
    }
    if (downloading) {
        ofSetColor(COL_BG_INPUT); ofDrawRectangle(left,y-5,inner,4);
        ofSetColor(COL_ACCENT_CYAN); ofDrawRectangle(left,y-5,inner*ofClamp(status.progress,0.0,1.0),4); y+=16;
    }
    if (!messageLines.empty()) {
        ofSetColor(status.state==jp::UpdateState::Error?COL_ACCENT_RED_DIM:COL_TEXT_SECONDARY);
        for (const auto& line:messageLines) {font_p.drawString(line,left,y);y+=18;}
        y+=12;
    }
    const std::vector<std::string> labels=es?
        std::vector<std::string>{"Buscar", "Descargar", "Guardar e instalar", "Posponer / cancelar", std::string("Consulta diaria: ")+(updates.automatic?"activada":"desactivada"), "Canal: "+updates.channel+" (cambiar)", "Exportar diagnóstico", "Omitir versión", "Ver novedades"}:
        std::vector<std::string>{"Check for updates", "Download", "Save and install", "Postpone / cancel", std::string("Daily checks: ")+(updates.automatic?"on":"off"), "Channel: "+updates.channel+" (switch)", "Export diagnostics", "Skip version", "Release notes"};
    const int primary=status.state==jp::UpdateState::Available?1:(status.state==jp::UpdateState::Ready?2:0);
    auto button=[&](int action,float bx,float by,float bw) {
        releaseButtons[action]=ofRectangle(bx,by,bw,buttonHeight);
        jp_button::draw(releaseButtons[action],fit(ofToString(action+1)+"  "+labels[action],bw-16),
            (action==primary && releaseActionEnabled(action)) || (action==4 && updates.automatic),releaseActionEnabled(action),
            action==4?COL_ACCENT_GREEN:COL_ACCENT_CYAN);
    };
    auto row=[&](std::array<int,3> actions) {
        const float bw=compact?inner:(inner-gap*2)/3;
        for (int i=0;i<3;++i) button(actions[i],left+(compact?0:i*(bw+gap)),y+(compact?i*(buttonHeight+gap):0),bw);
        y+=rowBlock;
    };
    row({0,1,2}); row({3,7,8});
    auto section=[&](const std::string& title) {
        y+=12; ofSetColor(COL_TEXT_DIM); font_p.drawString(title,left,y); y+=18;
    };
    section(es?"PREFERENCIAS":"PREFERENCES");
    button(5,left,y,inner); y+=buttonHeight+gap;
    button(4,left,y,inner); y+=buttonHeight+gap;
    section(es?"SOPORTE":"SUPPORT");
    button(6,left,y,inner); y+=buttonHeight+24;
    ofSetColor(COL_TEXT_DIM);
    for (const auto& line:footerLines) {font_p.drawString(line,left,y);y+=18;}
    if (releaseScrollMax>0) {
        ofSetColor(COL_BG_SCROLLBAR);
        const float thumb=height*height/contentHeight;
        ofDrawRectangle(x+width-6,top+(height-thumb)*releaseScroll/releaseScrollMax,3,thumb);
    }
    ofPopStyle();
}
