#include "ofApp.h"
#include "JPutils/jp_storage.h"
#include "JPutils/jp_textwrap.h"
#include "JPgui/jp_button.h"
#include <ctime>
namespace {
std::vector<std::string> reviewLines(const ofTrueTypeFont& font,const std::string& text,float width) {
    std::vector<std::string> lines;std::string line;
    for(size_t p=0;p<text.size();) {
        size_t end=p+1;while(end<text.size() && (static_cast<unsigned char>(text[end])&0xc0)==0x80)++end;
        auto character=text.substr(p,end-p);p=end;if(character=="\r")continue;
        if(character=="\n"){lines.push_back(line);line.clear();continue;}
        if(!line.empty() && font.stringWidth(line+character)>width){
            const auto space=line.find_last_of(" ");
            if(space!=std::string::npos && space>0){lines.push_back(line.substr(0,space));line=line.substr(space+1);}
            else {lines.push_back(line);line.clear();}
        }
        if(character!=" " || !line.empty())line+=character;
    }
    lines.push_back(line);return lines;
}
std::string eventDate(const ofJson& e) {
    const std::time_t time=e.at("time").get<int64_t>()/1000;std::tm date{};
#ifdef _WIN32
    gmtime_s(&date,&time);
#else
    gmtime_r(&time,&date);
#endif
    char text[32];std::strftime(text,sizeof text,"%Y-%m-%d %H:%M UTC",&date);return text;
}
std::string fieldLabel(const std::string& field,bool es) {
    if(field=="user_visible")return es?"Mostrar al usuario":"Show to users";
    if(field=="needs_parameters")return es?"Agregar parámetros":"Add parameters";
    if(field=="needs_improvement")return es?"Mejorar shader":"Improve shader";
    if(field=="ask_pupper")return es?"Preguntarle a Pupper":"Ask Pupper";
    if(field=="preview_defaults")return es?"Defaults de preview":"Preview defaults";
    return field=="name.es"?"Nombre (ES)":"Name (EN)";
}
std::string valueText(const ofJson& value){return value.is_string()?value.get<std::string>():value.dump();}
}
void ofApp::persistReviewPreferences() {
    ofJson settings={{"enabled",reviewEnabled},{"shared",reviewConfig.shared.string()},{"local",reviewConfig.local.string()},{"author",reviewConfig.author}};
    jp::atomicWrite(jp::preferencePath("review-collaboration.json"),settings.dump(2)+"\n");
}
void ofApp::updateReview() {
    if(!shaderCuratedMode)return;
    if(!reviewInitialized) {
        reviewInitialized=true;
        try {
            const auto prefs=jp::preferencePath("review-collaboration.json");
            if(std::filesystem::exists(prefs)) {
                const auto j=ofJson::parse(jp::readBytes(prefs));reviewEnabled=j.value("enabled",false);
                reviewConfig.shared=j.value("shared",string());reviewConfig.local=j.value("local",string());reviewConfig.author=j.value("author",string());
            }
            for(const auto& name:{"review-read.json","review-comment-drafts.json"}) {
                const auto file=jp::preferencePath(name);if(!std::filesystem::exists(file))continue;
                auto j=ofJson::parse(jp::readBytes(file));if(!j.is_object())continue;
                if(string(name)=="review-read.json")reviewRead=j;else reviewDrafts=j;
            }
        }catch(const std::exception& e){reviewStatus=e.what();reviewEnabled=false;}
    }
    if(reviewTask.valid() && reviewTask.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto result=reviewTask.get();reviewPendingApply=false;
        if(result.connected){
            const auto selected=getSelectedShaderPath();reviewConfig=result.config;reviewEnabled=true;
            try{persistReviewPreferences();}catch(const std::exception& e){reviewStatus=e.what();}
            scanShaders();for(int f=0;f<int(shaderFolders.size());++f)if(!shaderFolders[f].isFavorites)for(int i=0;i<int(shaderFolders[f].shaders.size());++i)if(shaderFolders[f].shaders[i].path==selected)selectShaderForPreview(f,i);
        }
        if(result.snapshot.project.is_object()) {
            reviewSnapshot=std::move(result.snapshot);reviewSourcePath.clear();applyReviewSnapshot();
            reviewStatus=reviewSnapshot.folderAvailable ? (language==0?"Shared folder read locally":"Carpeta compartida leída localmente") : (language==0?"Folder unavailable · using local copy":"Carpeta no disponible · usando copia local");
            reviewStatus+=" · "+ofToString(reviewSnapshot.pending)+(language==0?" pending":" pendientes");
            if(reviewSnapshot.deferred)reviewStatus+=" · "+ofToString(reviewSnapshot.deferred)+(language==0?" awaiting revisions":" esperando revisiones");
            if(!reviewSnapshot.errors.empty())reviewStatus+=" · "+ofToString(reviewSnapshot.errors.size())+(language==0?" file errors":" errores de archivos");
        }
        if(!result.error.empty())reviewStatus=result.error;
    }
    const float now=ofGetElapsedTimef();
    if(reviewEnabled && !reviewTask.valid() && (reviewForceRefresh || now-reviewLastPoll>=2.f)) {
        reviewForceRefresh=false;reviewLastPoll=now;auto config=reviewConfig;config.catalog=shaderReviewFile;
        reviewTask=std::async(std::launch::async,[config]{ReviewResult result;result.config=config;try{result.snapshot=jp_review::synchronize(config);jp_review::materialize(result.snapshot,config.catalog);}catch(const std::exception& e){result.error=e.what();}return result;});
    }
    if(reviewLocalDirty && !reviewTask.valid()) {
        try {jp::atomicWrite(jp::preferencePath("review-read.json"),reviewRead.dump()+"\n");jp::atomicWrite(jp::preferencePath("review-comment-drafts.json"),reviewDrafts.dump()+"\n");reviewLocalDirty=false;}
        catch(const std::exception& e){reviewStatus=e.what();}
    }
}
void ofApp::connectReview(bool creating) {
    if(reviewTask.valid())return;
    if(reviewConfig.author.empty()) {
        auto author=ofTrim(ofSystemTextBoxDialog(language==0?"Reviewer name":"Nombre del revisor",reviewConfig.author));
        toastLastUpdate=ofGetElapsedTimef();if(author.empty() || author.size()>128)return;reviewConfig.author=author;
    }
    auto choice=ofSystemLoadDialog(creating?(language==0?"Choose an empty shared review folder":"Elegí una carpeta vacía para la revisión compartida"):(language==0?"Choose the shared review folder":"Elegí la carpeta de revisión compartida"),true);
    toastLastUpdate=ofGetElapsedTimef();if(!choice.bSuccess)return;
    auto config=reviewConfig;config.shared=choice.getPath();config.catalog=shaderReviewFile;
    const auto cache=jp::preferencePath("review-cache");
    reviewStatus=language==0?"Opening review...":"Abriendo revisión...";
    reviewTask=std::async(std::launch::async,[config,cache,creating]()mutable {
        ReviewResult result;try {
            const auto project=creating?jp_review::create(config.shared,ofJson::parse(jp::readBytes(config.catalog))):ofJson::parse(jp::readBytes(config.shared/"review-project.json"));
            const string id=project.at("id");if(id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=string::npos)throw std::runtime_error("Invalid review identity");
            config.local=std::filesystem::path(cache)/id;jp_review::open(config);
            result.config=config;result.snapshot=jp_review::synchronize(config);result.connected=true;jp_review::materialize(result.snapshot,config.catalog);
        }catch(const std::exception& e){result.error=e.what();}return result;
    });
}
void ofApp::applyReviewSnapshot() {
    if(!reviewSnapshot.catalog.is_object())return;
    try {
        const auto metadata=jp_shader_catalog::parseCurated(reviewSnapshot.catalog);
        for(auto& folder:shaderFolders)for(auto& entry:folder.shaders)for(const auto& shared:metadata)if(entry.path==shared.path) {
            const auto localGroup=entry.metadata.groupPath;entry.metadata=shared;entry.metadata.groupPath=localGroup;entry.catalogued=true;
        }
        rebuildShaderFolderOrder();clampShaderScroll(getShaderBrowserLayout());
    }catch(const std::exception& e){reviewStatus=e.what();}
}
bool ofApp::queueReviewEvent(const ofJson& event) {
    if(reviewTask.valid() || reviewPendingApply){reviewStatus=language==0?"Updating review; try again shortly":"Actualizando revisión; intentá nuevamente";return false;}
    try {
        jp_review::enqueue(reviewConfig,event);reviewPendingApply=true;reviewForceRefresh=true;
        reviewStatus=language==0?"Saved locally · pending folder write":"Guardado localmente · pendiente de escritura en carpeta";
        return true;
    }catch(const std::exception& e){reviewStatus=e.what();publishToast("shared-review",jp::ToastState::Error,language==0?"Could not save the review change. Your text is retained.":"No se pudo guardar el cambio. Tu texto se conserva.");return false;}
}
bool ofApp::submitReviewChanges(const string& path,const ofJson& changes) {
    if(!reviewEnabled)return false;
    if(reviewTask.valid() || reviewPendingApply || !reviewSnapshot.fields.count(path)) {
        publishToast("shared-review",jp::ToastState::Info,language==0?"Review updating or shader missing from shared project. Refresh and try again.":"Revisión actualizándose o shader fuera del proyecto compartido. Actualizá e intentá nuevamente.");return false;
    }
    std::vector<std::pair<string,ofJson>> fields;
    for(const auto& item:changes.items())if(item.key()=="name") {for(const auto& name:item.value().items())fields.push_back({"name."+name.key(),name.value()});}
        else if(item.key()!="group_path")fields.push_back({item.key(),item.value()});
    for(const auto& field:fields)if(reviewSnapshot.fields.at(path).at(field.first).conflict) {
        reviewPanelOpen=true;reviewHistory=true;reviewScroll=0;
        publishToast("shared-review",jp::ToastState::Warning,language==0?"Resolve the pending conflict first":"Resolvé primero el conflicto pendiente");return false;
    }
    try {
        for(const auto& field:fields)jp_review::enqueue(reviewConfig,jp_review::change(reviewSnapshot,path,field.first,field.second,reviewConfig.author,jp_review::fingerprint(ofToDataPath(path,true))));
        reviewPendingApply=true;reviewForceRefresh=true;return true;
    }catch(const std::exception& e){reviewStatus=e.what();publishToast("shared-review",jp::ToastState::Error,language==0?"Could not save shared review changes":"No se pudieron guardar los cambios de revisión");return false;}
}
string ofApp::reviewSummary(const string& path) const {
    size_t comments=0,unread=0,conflicts=0;
    if(reviewSnapshot.project.is_object()) {
        const string project=reviewSnapshot.project.at("id");
        const auto read=reviewRead.value(reviewConfig.author,ofJson::object()).value(project,ofJson::object());
        for(const auto& item:reviewSnapshot.events)if(item.second.at("shader")==path) {
            if(item.second.at("type")=="comment")++comments;
            if(item.second.at("author")!=reviewConfig.author && !read.value(item.first,false))++unread;
        }
        const auto fields=reviewSnapshot.fields.find(path);if(fields!=reviewSnapshot.fields.end())for(const auto& field:fields->second)if(field.second.conflict)++conflicts;
    }
    return (language==0?"Comments / review":"Comentarios / revisión")+string(" (")+ofToString(comments)+")"+
        (unread?" · "+ofToString(unread)+(language==0?" new":" nuevos"):"")+(conflicts?" · ! "+ofToString(conflicts):"");
}
ofApp::ReviewLayout ofApp::getReviewLayout() const {
    ReviewLayout l;l.panel=getPreviewInspectorLayout().panel;const float x=l.panel.x+8,w=std::max(1.f,l.panel.width-16),y=l.panel.y;
    l.close.set(l.panel.getRight()-34,y+6,26,25);
    const float bw=(w-12)/4; l.reviewer.set(x,y+38,bw,25);l.create.set(x+bw+4,y+38,bw,25);l.open.set(x+2*(bw+4),y+38,bw,25);l.disconnect.set(x+3*(bw+4),y+38,bw,25);
    l.refresh.set(x+w-90,y+70,90,25);l.comments.set(x,y+114,(w-4)/2,25);l.history.set(x+(w+4)/2,y+114,(w-4)/2,25);
    const float compose=reviewHistory?0:132;
    l.body.set(x,y+146,std::max(1.f,w-18),std::max(1.f,l.panel.height-154-compose));l.track.set(l.body.getRight()+5,l.body.y,12,l.body.height);
    if(reviewContentHeight>l.body.height){const float h=std::min(l.body.height,std::max(22.f,l.body.height*l.body.height/reviewContentHeight));const float max=reviewContentHeight-l.body.height;l.thumb.set(l.track.x,l.track.y+(l.track.height-h)*ofClamp(reviewScroll/max,0.f,1.f),12,h);}
    const float editorY=l.panel.getBottom()-132;
    for(int i=0;i<4;++i)l.kinds[i].set(x+i*(bw+4),editorY,bw,25);
    l.editor.set(x,editorY+31,w,59);l.publish.set(x,editorY+97,w,27);return l;
}
void ofApp::drawReviewPanel() {
    auto l=getReviewLayout();const auto path=getSelectedShaderPath();
    if(reviewCommentPath!=path) {
        if(!reviewCommentPath.empty())reviewDrafts[reviewCommentPath]=reviewCommentText;
        reviewCommentPath=path;reviewCommentText=reviewDrafts.value(path,string());reviewCommentCursor=reviewCommentText.size();reviewCommentFocused=false;reviewScroll=0;
    }
    ofPushStyle();ofFill();ofSetColor(COL_BG_PANEL);ofDrawRectRounded(l.panel,4);
    auto fit=[&](const string& text,float width){if(font_p.stringWidth(text)<=width)return text;auto lines=reviewLines(font_p,text,std::max(1.f,width-font_p.stringWidth("…")));return lines.front()+"…";};
    auto button=[&](const ofRectangle& r,const string& text,bool active=false,bool enabled=true){jp_button::draw(r,fit(text,r.width-8),active,enabled,COL_BORDER_MUTED);};
    ofSetColor(COL_TEXT_PRIMARY);font_p.drawString(fit(language==0?"Comments & review":"Comentarios y revisión",l.panel.width-55),l.panel.x+8,l.panel.y+24);button(l.close,"x");
    const bool busy=reviewTask.valid() || reviewPendingApply;
    button(l.reviewer,reviewConfig.author.empty()?(language==0?"Reviewer":"Revisor"):reviewConfig.author,false,!busy);
    button(l.create,language==0?"Create":"Crear",false,!busy);button(l.open,language==0?"Open":"Abrir",false,!busy);button(l.disconnect,language==0?"Local":"Local",!reviewEnabled,!busy);
    button(l.refresh,language==0?"Refresh":"Actualizar",false,reviewEnabled && !busy);
    const string status=busy?(language==0?"Reading / saving locally...":"Leyendo / guardando localmente..."):reviewStatus.empty()?(language==0?"Create or open a shared review folder":"Creá o abrí una carpeta de revisión compartida"):reviewStatus;
    ofSetColor(COL_TEXT_SECONDARY);auto statusLines=reviewLines(font_p,status,l.panel.width-120);for(size_t i=0;i<std::min(size_t(2),statusLines.size());++i)font_p.drawString(statusLines[i],l.panel.x+8,l.panel.y+83+i*16);
    jp_tooltip::drawFor(status,l.panel,l.panel.inside(ofGetMouseX(),ofGetMouseY()) && ofGetMouseY()<l.panel.y+112,"review-status");
    button(l.comments,language==0?"Comments":"Comentarios",!reviewHistory);button(l.history,language==0?"History / conflicts":"Historial / conflictos",reviewHistory);
    reviewChoices.clear();float y=l.body.y-reviewScroll;
    auto paragraph=[&](const string& text,ofColor color){ofSetColor(color);for(const auto& line:reviewLines(font_p,text,l.body.width-12)){if(y+18>l.body.y && y<l.body.getBottom())font_p.drawString(line,l.body.x+4,y+16);y+=19;} };
    if(reviewSourcePath!=path){reviewSourcePath=path;reviewSourceHash=jp_review::fingerprint(ofToDataPath(path,true));}
    const auto& hash=reviewSourceHash;
    {
        jp_gl::ScopedScissor clip(l.body);
        auto fieldSet=reviewSnapshot.fields.find(path);
        if(reviewHistory && fieldSet!=reviewSnapshot.fields.end())for(const auto& field:fieldSet->second)if(field.second.conflict) {
            paragraph((language==0?"CONFLICT: ":"CONFLICTO: ")+fieldLabel(field.first,language!=0),COL_ACCENT_GOLD);
            paragraph((language==0?"Current common value: ":"Valor común actual: ")+valueText(field.second.value),COL_TEXT_SECONDARY);
            for(const auto& id:field.second.heads) {
                const auto& e=reviewSnapshot.events.at(id);paragraph(e.at("author").get<string>()+" · "+eventDate(e),COL_ACCENT_CYAN);
                paragraph(valueText(e.at("value")),COL_TEXT_PRIMARY);
                if(e.value("source_hash",string())!=hash)paragraph(language==0?"Different shader source version":"Versión de shader diferente",COL_ACCENT_GOLD);
                ofRectangle choose(l.body.x+4,y+2,l.body.width-8,25);button(choose,language==0?"Use this version":"Usar esta versión",false,!busy);
                if(choose.y>=l.body.y && choose.getBottom()<=l.body.getBottom())reviewChoices.push_back({choose,e});
                y+=34;
            }
        }
        std::vector<ofJson> events;for(const auto& item:reviewSnapshot.events)if(item.second.at("shader")==path && (reviewHistory || item.second.at("type")=="comment"))events.push_back(item.second);
        std::sort(events.begin(),events.end(),[](const ofJson& a,const ofJson& b){return std::make_pair(a.at("time").get<int64_t>(),a.at("id").get<string>())<std::make_pair(b.at("time").get<int64_t>(),b.at("id").get<string>());});
        if(events.empty())paragraph(language==0?"No entries yet. Comments are shared when you publish them.":"Todavía no hay entradas. Los comentarios se comparten al publicarlos.",COL_TEXT_MUTED);
        for(const auto& e:events) {
            if(y>=l.body.y && y+19<=l.body.getBottom() && reviewSnapshot.project.is_object()) {
                auto& read=reviewRead[reviewConfig.author][reviewSnapshot.project.at("id").get<string>()][e.at("id").get<string>()];
                if(read!=true){read=true;reviewLocalDirty=true;}
            }
            paragraph(e.at("author").get<string>()+" · "+eventDate(e),COL_ACCENT_CYAN);
            if(e.at("type")=="comment") {
                const string kind=e.at("kind");const string label=kind=="keep"?(language==0?"PROPOSE KEEP":"PROPONE CONSERVAR"):kind=="modify"?(language==0?"PROPOSE MODIFY":"PROPONE MODIFICAR"):kind=="remove"?(language==0?"PROPOSE REMOVE":"PROPONE ELIMINAR"):"";
                if(!label.empty())paragraph(label,COL_ACCENT_GOLD);
                paragraph(e.at("text"),COL_TEXT_PRIMARY);
            } else {
                paragraph(fieldLabel(e.at("field"),language!=0)+": "+valueText(e.at("value")),COL_TEXT_PRIMARY);
                if(e.at("parents").size()>1)paragraph(language==0?"Combines / resolves previous revisions":"Combina / resuelve revisiones anteriores",COL_TEXT_MUTED);
            }
            if(e.value("source_hash",string())!=hash)paragraph(language==0?"Different shader source version":"Versión de shader diferente",COL_ACCENT_GOLD);
            y+=14;
        }
    }
    reviewContentHeight=y-l.body.y+reviewScroll;reviewScroll=ofClamp(reviewScroll,0.f,std::max(0.f,reviewContentHeight-l.body.height));
    l=getReviewLayout();if(l.thumb.height>0){ofSetColor(COL_BG_INPUT);ofDrawRectangle(l.track);ofSetColor(COL_TEXT_MUTED);ofDrawRectRounded(l.thumb,3);}
    if(!reviewHistory) {
        const vector<string> labels=language==0?vector<string>{"Comment","Keep","Modify","Remove"}:vector<string>{"Comentar","Conservar","Modificar","Eliminar"};
        for(int i=0;i<4;++i)button(l.kinds[i],labels[i],i==reviewCommentKind);
        ofSetColor(COL_BG_INPUT);ofDrawRectRounded(l.editor,3);ofNoFill();ofSetColor(reviewCommentFocused?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(l.editor,3);ofFill();
        {jp_gl::ScopedScissor clip(l.editor);ofSetColor(COL_TEXT_PRIMARY);
            string shown=reviewCommentText;if(reviewCommentFocused)shown.insert(std::min(size_t(reviewCommentCursor),shown.size()),"|");
            if(shown.empty())shown=language==0?"Write a comment… Enter: new line":"Escribí un comentario… Enter: nueva línea";
            const auto lines=reviewLines(font_p,shown,l.editor.width-12);const auto before=reviewLines(font_p,reviewCommentText.substr(0,reviewCommentCursor),l.editor.width-12);
            size_t start=reviewCommentFocused && before.size()>3?before.size()-3:0;
            for(size_t i=start;i<lines.size() && i<start+3;++i)font_p.drawString(lines[i],l.editor.x+6,l.editor.y+17+(i-start)*18);
        }
        button(l.publish,language==0?"Publish comment · Ctrl+Enter":"Publicar comentario · Ctrl+Enter",false,reviewEnabled && !busy && !ofTrim(reviewCommentText).empty());
    }
    ofPopStyle();
}
void ofApp::publishReviewComment() {
    const auto text=ofTrim(reviewCommentText);if(!reviewEnabled || text.empty() || text.size()>16384 || !reviewSnapshot.project.is_object())return;
    static const vector<string> kinds={"comment","keep","modify","remove"};
    try {if(queueReviewEvent(jp_review::comment(reviewSnapshot,getSelectedShaderPath(),kinds[reviewCommentKind],text,reviewConfig.author,jp_review::fingerprint(ofToDataPath(getSelectedShaderPath(),true))))) {
        reviewCommentText.clear();reviewCommentCursor=0;reviewDrafts[reviewCommentPath]="";reviewLocalDirty=true;
    }}catch(const std::exception& e){reviewStatus=e.what();}
}
bool ofApp::pressReviewPanel(int x,int y) {
    auto l=getReviewLayout();if(!l.panel.inside(x,y))return false;
    if(l.close.inside(x,y)){reviewPanelOpen=false;reviewCommentFocused=false;return true;}
    const bool busy=reviewTask.valid() || reviewPendingApply;
    if(l.reviewer.inside(x,y) && !busy){auto author=ofTrim(ofSystemTextBoxDialog(language==0?"Reviewer name":"Nombre del revisor",reviewConfig.author));toastLastUpdate=ofGetElapsedTimef();if(!author.empty() && author.size()<=128){reviewConfig.author=author;try{persistReviewPreferences();}catch(const std::exception& e){reviewStatus=e.what();}}return true;}
    if(l.create.inside(x,y)){connectReview(true);return true;}if(l.open.inside(x,y)){connectReview(false);return true;}
    if(l.disconnect.inside(x,y) && !busy){reviewEnabled=false;try{persistReviewPreferences();}catch(const std::exception& e){reviewStatus=e.what();}reviewStatus=language==0?"Local mode; shared history preserved":"Modo local; historial compartido conservado";return true;}
    if(l.refresh.inside(x,y)){reviewForceRefresh=true;return true;}
    if(l.comments.inside(x,y) || l.history.inside(x,y)){reviewHistory=l.history.inside(x,y);reviewScroll=0;reviewCommentFocused=false;return true;}
    if(l.track.inside(x,y) && l.thumb.height>0){reviewScrollbarDragging=true;reviewScrollGrab=l.thumb.inside(x,y)?y-l.thumb.y:l.thumb.height/2;dragReviewScrollbar(y);return true;}
    for(const auto& choice:reviewChoices)if(choice.first.inside(x,y) && !busy) {
        try{const auto& e=choice.second;queueReviewEvent(jp_review::change(reviewSnapshot,e.at("shader"),e.at("field"),e.at("value"),reviewConfig.author,jp_review::fingerprint(ofToDataPath(e.at("shader").get<string>(),true))));}catch(const std::exception& e){reviewStatus=e.what();}return true;
    }
    if(!reviewHistory) {
        for(int i=0;i<4;++i)if(l.kinds[i].inside(x,y)){reviewCommentKind=i;return true;}
        if(l.publish.inside(x,y)){publishReviewComment();return true;}
        reviewCommentFocused=l.editor.inside(x,y);if(reviewCommentFocused){reviewCommentCursor=reviewCommentText.size();reviewCommentSelectAll=false;}
    }
    return true;
}
void ofApp::dragReviewScrollbar(float y) {const auto l=getReviewLayout();const float travel=l.track.height-l.thumb.height;if(travel>0)reviewScroll=ofClamp((y-reviewScrollGrab-l.track.y)/travel,0.f,1.f)*std::max(0.f,reviewContentHeight-l.body.height);}
void ofApp::reviewKeyPressed(int key) {
    if(key==OF_KEY_ESC){if(reviewCommentFocused)reviewCommentFocused=false;else reviewPanelOpen=false;return;}
    if(!reviewCommentFocused){if(key==OF_KEY_DOWN)reviewScroll+=40;if(key==OF_KEY_UP)reviewScroll=std::max(0.f,reviewScroll-40);return;}
    const bool modifier=ofGetKeyPressed(OF_KEY_CONTROL)||ofGetKeyPressed(OF_KEY_COMMAND);
    if(modifier && (key==OF_KEY_RETURN)){publishReviewComment();return;}
    auto prev=[&](int p){if(p>0)--p;while(p>0 && (static_cast<unsigned char>(reviewCommentText[p])&0xc0)==0x80)--p;return p;};
    auto next=[&](int p){if(p<int(reviewCommentText.size()))++p;while(p<int(reviewCommentText.size()) && (static_cast<unsigned char>(reviewCommentText[p])&0xc0)==0x80)++p;return p;};
    if(key==1 || (modifier && (key=='a'||key=='A'))){reviewCommentSelectAll=true;return;}
    if(key==3 || (modifier && (key=='c'||key=='C'))){if(reviewCommentSelectAll)ofGetWindowPtr()->setClipboardString(reviewCommentText);return;}
    if(key==OF_KEY_LEFT || key==OF_KEY_RIGHT){reviewCommentCursor=key==OF_KEY_LEFT?prev(reviewCommentCursor):next(reviewCommentCursor);reviewCommentSelectAll=false;return;}
    if(key==OF_KEY_HOME || key==OF_KEY_END){reviewCommentCursor=key==OF_KEY_HOME?0:reviewCommentText.size();reviewCommentSelectAll=false;return;}
    auto erase=[&]{if(reviewCommentSelectAll){reviewCommentText.clear();reviewCommentCursor=0;reviewCommentSelectAll=false;}};
    if(key==OF_KEY_BACKSPACE || key==OF_KEY_DEL){if(reviewCommentSelectAll)erase();else if(key==OF_KEY_BACKSPACE){const int start=prev(reviewCommentCursor);reviewCommentText.erase(start,reviewCommentCursor-start);reviewCommentCursor=start;}else reviewCommentText.erase(reviewCommentCursor,next(reviewCommentCursor)-reviewCommentCursor);}
    else {
        string inserted;if(key==22 || (modifier && (key=='v'||key=='V')))inserted=ofGetWindowPtr()->getClipboardString();
        else if(key==OF_KEY_RETURN)inserted="\n";
        else if(!modifier && ((key>=32 && key<=126)||(key>=160 && key<=0x24f)))ofUTF8Append(inserted,key);
        if(inserted.empty() || (reviewCommentSelectAll?0:reviewCommentText.size())+inserted.size()>16384)return;
        erase();reviewCommentText.insert(reviewCommentCursor,inserted);reviewCommentCursor+=inserted.size();
    }
    reviewDrafts[reviewCommentPath]=reviewCommentText;reviewLocalDirty=true;
}
