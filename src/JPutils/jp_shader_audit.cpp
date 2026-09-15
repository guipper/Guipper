#include "jp_shader_audit.h"
#include "../ofApp.h"
#include "../JPbox/jp_box_shader.h"
#include "jp_shader_globals.h"
#include "jp_app_paths.h"
#include "jp_audio.h"
#include <cmath>

// Opt-in offline audit. The launcher supplies a copied resource tree and an
// isolated user root; no project, source shader or update feed is modified.
bool jp_shader_audit::run(ofApp& app,const char* path) {
    const auto output=jp::AppPaths::current().cache/"shader-audit";
    std::filesystem::create_directories(output);
    ofJson report={{"path",path},{"seed",20260915},{"width",320},{"height",180},
        {"gpu",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
        {"openGL",reinterpret_cast<const char*>(glGetString(GL_VERSION))},
        {"samples",ofJson::array()},{"passed",false}};
    auto save=[&](){jp::atomicWrite(output/"result.json",report.dump(2));};
    try {
        jp_audio::setEnabled(false);ofSeedRandom(20260915);
        jp_constants::renderWidth=320;jp_constants::renderHeight=180;
        ofDisableArbTex();
        ofImage inputs[2];ofFbo sources[2];std::string names[2]={"audit-A","audit-B"};
        for(int n=0;n<2;++n){
            ofPixels pixels;pixels.allocate(320,180,OF_PIXELS_RGBA);
            for(int y=0;y<180;++y)for(int x=0;x<320;++x)
                pixels.setColor(x,y,n==0?ofColor(x*255/319,y*255/179,80,255):
                    ofColor(((x/20+y/20)%2)*220,80,255-x*255/319,255));
            inputs[n].setFromPixels(pixels);
            sources[n].allocate(320,180,GL_RGBA);
            sources[n].begin();ofClear(0,0,0,255);ofSetColor(255);inputs[n].draw(0,0);sources[n].end();
        }
        JPbox_shader box;box.setup(path,"audit");
        box.fbo.allocate(320,180,GL_RGBA32F);box.onoff.boolValue=true;
        GLint linked=0;glGetProgramiv(box.shader.getProgram(),GL_LINK_STATUS,&linked);
        report["node_compiles"]=box.shader.isLoaded()&&linked;
        if(!report["node_compiles"].get<bool>()){save();return false;}
        for(const auto& d:box.uniformDiagnostics)if(d.severity==jp_uniform_parser::Severity::Error){report["parser_error"]=d.message;save();return false;}
        for(int i=0;i<box.fbohandlergroup.getSize();++i)box.fbohandlergroup.setFboPointer(&sources[i%2],&names[i%2],i);
        box.updateFBO(); // Production node path, including global/parameter/input binding.
        report["node_render_error"]=glGetError();
        if (!std::getenv("GUIPPER_SHADER_PREVIEW_INPUTS")) {
            app.previewImg1=inputs[0];app.previewImg2=inputs[1];
        }
        report["preview_input_sizes"]={{app.previewImg1.getWidth(),app.previewImg1.getHeight()},
            {app.previewImg2.getWidth(),app.previewImg2.getHeight()}};
        app.shaderFolders.clear();ofApp::ShaderFolder folder;ofApp::ShaderEntry entry;
        entry.path=path;entry.name="audit";folder.shaders.push_back(entry);app.shaderFolders.push_back(folder);
        app.selectShaderForPreview(0,0); // Production preview loader and render path.
        report["preview_compiles"]=app.previewShaderLoaded;
        if(app.previewFbo.isAllocated()){ofPixels p;app.previewFbo.readToPixels(p);ofSaveImage(p,(output/"preview.png").string());}
        if(std::getenv("GUIPPER_SHADER_BROWSER_AUDIT")) {
            // Synthetic metadata exercises layout without approving any source.
            auto& visualEntry=app.shaderFolders[0].shaders[0];
            visualEntry.catalogued=!std::getenv("GUIPPER_SHADER_PREVIEW_INPUTS");visualEntry.official=visualEntry.catalogued;
            visualEntry.metadata.name={"A deliberately long shader name for layout verification", "Un nombre de shader muy largo para comprobar la ventana"};
            visualEntry.metadata.description={"Two reproducible image inputs, blended using the amount control.", "Dos imágenes reproducibles que se mezclan mediante el control amount."};
            visualEntry.metadata.inputs={"texture1", "texture2"};
            app.shaderFolders[0].category="mixers";app.shaderFolders[0].expanded=true;
            app.rebuildShaderFolderOrder();
            for(int width:{1080,400})for(int lang:{0,1}) {
                ofSetWindowShape(width,width==400?430:780);
                app.language=lang;
                ofFbo shot;shot.allocate(ofGetWidth(),ofGetHeight(),GL_RGBA);
                shot.begin();ofClear(12,15,20,255);app.draw_shaderindex();shot.end();
                ofPixels pixels;shot.readToPixels(pixels);
                ofSaveImage(pixels,(output/("browser-"+ofToString(width)+"-"+ofToString(lang)+".png")).string());
                const auto layout=app.getShaderBrowserLayout();
                if(layout.details.getBottom()>layout.preview.y || layout.search.getBottom()>layout.details.y)
                    throw std::runtime_error("Browser sections overlap");
            }
        }
        ofFbo rendered;rendered.allocate(320,180,GL_RGBA32F);
        std::vector<float> initial;
        for(int i=0;i<box.parameters.getSize();++i)initial.push_back(box.parameters.getJParameter(i)->variabletype==JPParameter::FLOAT?box.parameters.getJParameter(i)->floatValue:0);
        bool finite=true;int sampleIndex=0;
        auto render=[&](const std::string& label,float time,bool capture){
            GLuint query=0;const bool timer=GLEW_VERSION_3_3||GLEW_ARB_timer_query;
            if(timer)glGenQueries(1,&query);
            rendered.begin();ofClear(0,0,0,255);ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofDisableAlphaBlending();
            if(timer)glBeginQuery(GL_TIME_ELAPSED,query);
            box.shader.begin();JPShaderGlobalsCtx ctx;ctx.width=320;ctx.height=180;ctx.liveMouse=false;
            jp_shader_globals::apply(box.shader,ctx);box.update_NonglobalUniforms();
            box.shader.setUniform1f("time",time);
            // Deterministic empty feedback: feedback-dependent shaders are marked
            // for separate history testing, not certified by this sweep.
            box.shader.setUniformTexture("feedback",sources[0].getTexture(),0);
            ofSetColor(255);ofDrawRectangle(0,0,320,180);box.shader.end();
            if(timer)glEndQuery(GL_TIME_ELAPSED);
            ofPopStyle();rendered.end();
            GLuint64 nanos=0;if(timer){glGetQueryObjectui64v(query,GL_QUERY_RESULT,&nanos);glDeleteQueries(1,&query);}
            ofFloatPixels pixels;rendered.readToPixels(pixels);size_t nonfinite=0;double sum=0;
            ofPixels image;image.allocate(320,180,OF_PIXELS_RGBA);
            for(size_t i=0;i<pixels.size();++i){float v=pixels[i];if(!std::isfinite(v)){++nonfinite;v=0;}if(i%4!=3)sum+=ofClamp(v,0,1);image[i]=i%4==3?255:static_cast<unsigned char>(ofClamp(v,0,1)*255);}
            const auto error=glGetError();finite=finite&&nonfinite==0&&error==GL_NO_ERROR;
            ofJson sample={{"label",label},{"nonfinite_components",nonfinite},{"gl_error",error},{"mean_rgb",sum/(320*180*3)}};
            sample["gpu_ms"]=timer?ofJson(double(nanos)/1000000):ofJson(nullptr);
            if(capture){const auto file="frame-"+ofToString(sampleIndex)+".png";ofSaveImage(image,(output/file).string());sample["image"]=file;}
            report["samples"].push_back(sample);++sampleIndex;
        };
        for(int frame=0;frame<24;++frame)render("initial/t="+ofToString(frame/6.0f),frame/6.0f,true);
        for(int i=0;i<box.parameters.getSize();++i){
            auto* parameter=box.parameters.getJParameter(i);
            if(parameter->variabletype==JPParameter::BOOL) {
                const bool before=parameter->boolValue;
                for(bool v:{false,true}){parameter->boolValue=v;render(parameter->name+"="+(v?"true":"false"),1,false);}
                parameter->boolValue=before;continue;
            }
            if(parameter->variabletype!=JPParameter::FLOAT)continue;
            for(float v:{parameter->effectiveMin(),parameter->effectiveMax()}){
                parameter->floatValue=parameter->floatLerpValue=v;render(parameter->name+"="+ofToString(v),1,false);
            }
            parameter->floatValue=parameter->floatLerpValue=initial[i];
        }
        for(int seed=0;seed<4;++seed){
            ofSeedRandom(20260915+seed);
            for(int i=0;i<box.parameters.getSize();++i){auto*p=box.parameters.getJParameter(i);if(p->variabletype==JPParameter::FLOAT&&!p->randomLocked)p->floatValue=p->floatLerpValue=ofRandom(p->effectiveMin(),p->effectiveMax());}
            render("RDM/seed="+ofToString(20260915+seed),1,true);
        }
        report["passed"]=finite&&app.previewShaderLoaded&&report["node_render_error"]==0;
        report["limitations"]="Finite-pixel sweep at 320x180, 24 animation frames and four RDM seeds. No live audio, feedback history or two-hour stability certification.";
        save();return report["passed"].get<bool>();
    } catch(const std::exception& e){report["error"]=e.what();save();return false;}
}
