#include "jp_hand_panel.h"
#include "jp_button.h"
#include "../JPutils/jp_tooltip.h"
#include "../JPutils/jp_text_input.h"
#include <deque>
namespace jp_hand_ui {
namespace {
bool opened=false,moving=false,resizing=false,lost=false,choosingCamera=false;
bool diagnostics=false,calibrating=false;double calibrationEnd=0,calibrationStamp=0;float calibrationMin=0,calibrationMax=0;int calibrationSamples=0;
std::string calibrationMessage;jp_hand::Binding calibrationBinding;
struct LandmarkHit {ofVec2f position;int point;bool right;};std::vector<LandmarkHit> landmarks;
const char *bodyNames[]={"Nariz","Ojo izq. interior","Ojo izquierdo","Ojo izq. exterior","Ojo der. interior","Ojo derecho","Ojo der. exterior","Oreja izquierda","Oreja derecha","Boca izquierda","Boca derecha","Hombro izquierdo","Hombro derecho","Codo izquierdo","Codo derecho","Muneca izquierda","Muneca derecha","Menique izquierdo","Menique derecho","Indice izquierdo","Indice derecho","Pulgar izquierdo","Pulgar derecho","Cadera izquierda","Cadera derecha","Rodilla izquierda","Rodilla derecha","Tobillo izquierdo","Tobillo derecho","Talon izquierdo","Talon derecho","Pie izquierdo","Pie derecho"};
int cameraPage=0;size_t menuButtonStart=0;ofRectangle pickerBounds,deviceButton;
jp_hand::Source shownSource;uint64_t shownSession=0;
std::string lastCamera;
ofRectangle panel(40,65,590,674),settings;ofVec2f offset;
Target target;jp_hand::Binding local;jp_hand::Filter localFilter;
ofTexture texture;std::shared_ptr<const jp_hand::Frame> displayed;
struct Button {ofRectangle bounds;std::function<void()> action,backwards;};std::vector<Button> buttons;
struct Field {std::string text;bool focus=false;};std::array<Field,4> fields;
std::deque<std::array<float,3>> history;double lastHistory=0;
bool attached(){return target.binding&&target.valid&&target.valid();}
jp_hand::Binding &binding(){return attached()?*target.binding:local;}
void edit(const std::function<void()> &f){if(attached()&&target.edit)target.edit(f);else f();}
void resetMapping(){if(attached())target.filter->reset();else localFilter.reset();calibrating=false;calibrationMessage.clear();history.clear();}
void label(const std::string &text,float x,float y,ofColor color=COL_TEXT_SECONDARY){ofSetColor(color);jp_constants::control_font.drawString(text,x,y);}
void button(float x,float y,float w,const std::string &text,std::function<void()> action,bool on=false,bool enabled=true,std::function<void()> backwards={}){
 ofRectangle r(x,y,w,26);jp_button::draw(r,jp_tooltip::fit(text,w-12,[](const std::string &s){return jp_constants::control_font.stringWidth(s);}),on,enabled);if(enabled)buttons.push_back({r,std::move(action),std::move(backwards)});
}
void cycleButton(float x,float y,float w,const std::string &text,std::function<void(int)> change,bool enabled=true){
 button(x,y,w,text,[change]{change(1);},false,enabled,[change]{change(-1);});
 if(enabled&&!choosingCamera)jp_tooltip::draw(text+"\nClic izquierdo: siguiente · Clic derecho: anterior",ofRectangle(x,y,w,26));
}
void field(int i,float x,float y,const std::string &name,float value){
 auto &state=fields[i];if(!state.focus)state.text=ofToString(value,i==2?0:3);
 label(name,x,y-5);ofRectangle bounds(x,y,123,26);ofSetColor(COL_BG_INPUT);ofDrawRectRounded(bounds,4);
 jp_text_input::Field f;f.id="kinect-field-"+ofToString(i);f.panel="kinect";f.bounds=bounds;f.font=&jp_constants::control_font;f.focused=state.focus;f.layer=80;f.limit=16;
 f.read=[i]{return fields[i].text;};f.write=[i](const std::string&s){fields[i].text=s;};f.focus=[i]{fields[i].focus=true;};f.cancel=[i]{fields[i].focus=false;};
 f.validate=[i](const std::string &s){try{size_t end;float v=std::stof(s,&end);if(end!=s.size()||!std::isfinite(v))throw std::runtime_error("numero");auto &b=binding();
  if((i==0&&v>=b.inputMax)||(i==1&&v<=b.inputMin))return std::string("Minimo debe ser menor que maximo");
  if((i==2&&(v<0||v>5000))||(i==3&&(v<0||v>50)))return std::string("Fuera de rango");
  return std::string();}catch(...){return std::string("Ingresar un numero valido");}};
 f.commit=[i]{float v=std::stof(fields[i].text);edit([&]{auto &b=binding();if(i==0)b.inputMin=v;if(i==1)b.inputMax=v;if(i==2)b.smoothingMs=v;if(i==3)b.deadzone=v*.01f;});fields[i].focus=false;calibrating=false;return true;};
 jp_text_input::controller().add(std::move(f));
}
}
void open(Target t){jp_text_input::controller().finish();target=std::move(t);opened=true;choosingCamera=false;calibrating=false;calibrationMessage.clear();history.clear();displayed.reset();for(auto &f:fields)f.focus=false;jp_hand::setPanelVisible(true,binding().source);jp_hand::refreshSources();}
bool visible(){return opened;}bool contains(float x,float y){return opened&&panel.inside(x,y);}
void beginFrame(){settings={};if(target.binding&&!attached()){jp_text_input::controller().finish(true);target={};choosingCamera=false;calibrating=false;for(auto &f:fields)f.focus=false;history.clear();}jp_hand::setPanelVisible(opened,binding().source);}
bool editing(){for(auto &f:fields)if(f.focus)return true;return false;}
void settingsButton(const ofRectangle &r){settings=r;jp_button::draw(r,"Seguimiento",opened);}
void draw(){
 if(!opened)return;
 jp_pointer::Scope scope(80);ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofFill();
 panel.width=std::clamp(panel.width,570.f,std::max(570.f,float(ofGetWidth()-20)));panel.height=740;
 panel.x=std::clamp(panel.x,10.f,std::max(10.f,ofGetWidth()-panel.width-10));panel.y=std::clamp(panel.y,42.f,std::max(42.f,ofGetHeight()-panel.height-10));
 ofSetColor(COL_BG_PANEL);ofDrawRectRounded(panel,6);ofNoFill();ofSetColor(COL_BORDER_DEFAULT);ofDrawRectRounded(panel,6);ofFill();
 float x=panel.x+14,y=panel.y,w=panel.width-28;buttons.clear();landmarks.clear();
 label("SEGUIMIENTO",x,y+24,COL_TEXT_PRIMARY);label("Arrastrar para mover",x+195,y+24,COL_TEXT_MUTED);
 button(panel.getRight()-80,y+7,66,"Cerrar",[]{jp_text_input::controller().finish(true);opened=false;target={};jp_hand::setPanelVisible(false);});

 auto &source=binding().source;
 if(source!=shownSource||shownSession!=jp_hand::session(source)){shownSource=source;shownSession=jp_hand::session(source);resetMapping();displayed.reset();texture.clear();lost=false;}
 auto choose=[&](jp_hand::Source next){jp_text_input::controller().finish();next.detector=binding().source.detector;edit([&]{binding().source=next;});resetMapping();choosingCamera=false;jp_hand::setPanelVisible(true,next);};
 button(x,y+43,136,"Kinect RGB",[choose]{choose({jp_hand::SourceKind::KinectRGB,{}});},source.kind==jp_hand::SourceKind::KinectRGB);
 button(x+144,y+43,194,"Kinect infrarrojo",[choose]{choose({jp_hand::SourceKind::KinectIR,{}});},source.kind==jp_hand::SourceKind::KinectIR);
 button(x+346,y+43,w-346,"Camara de PC",[choose]{choose({jp_hand::SourceKind::Camera,lastCamera});},source.kind==jp_hand::SourceKind::Camera);
 auto available=jp_hand::sources();
 if(source.kind==jp_hand::SourceKind::Camera){
  lastCamera=source.device;std::string name=source.device.empty()?"Elegir camara...":"Camara guardada ausente";
  for(const auto &entry:available)if(entry.source.kind==source.kind&&entry.source.device==source.device)name=entry.label;
  deviceButton.set(x,y+76,w-154,26);
  button(x,y+76,w-154,name,[&]{choosingCamera=!choosingCamera;cameraPage=0;},choosingCamera);
  button(x+w-146,y+76,146,"Actualizar camaras",[]{jp_hand::refreshSources();});
 }else label(source.kind==jp_hand::SourceKind::KinectIR?"IR experimental · validar dedos y pinza a 1,2 a 2 m":"RGB + profundidad · necesita luz visible",x,y+94,source.kind==jp_hand::SourceKind::KinectIR?COL_ACCENT_GOLD:COL_TEXT_SECONDARY);
 button(x,y+109,152,"Iniciar esta fuente",[]{jp_hand::start(false,binding().source);},jp_hand::running(binding().source)&&!jp_hand::simulation(binding().source),source.kind!=jp_hand::SourceKind::Camera||!source.device.empty());
 button(x+160,y+109,78,"Detener",[]{jp_hand::stop(binding().source);},false,jp_hand::running(binding().source));
 button(x+246,y+109,100,"Reconectar",[]{jp_hand::reconnect(binding().source);},false,source.kind!=jp_hand::SourceKind::Camera||!source.device.empty());
 button(x+354,y+109,100,"Simulacion",[]{jp_hand::start(true,binding().source);},jp_hand::simulation(binding().source));
 if(jp_hand::simulation(binding().source))button(x+462,y+109,w-462,lost?"Volver":"Perder",[]{lost=!lost;jp_hand::simulateLoss(lost,binding().source);},lost);
 auto chooseDetector=[](jp_hand::Detector detector){jp_text_input::controller().finish();edit([detector]{auto &b=binding();
  b.source.detector=detector;
  b.point=-1;jp_hand::setSignal(b,detector==jp_hand::Detector::Body?jp_hand::TorsoX:jp_hand::Pinch);
 });resetMapping();jp_hand::setPanelVisible(true,binding().source);};
 button(x,y+145,100,"Manos",[chooseDetector]{chooseDetector(jp_hand::Detector::Hands);},source.detector==jp_hand::Detector::Hands);
 button(x+108,y+145,108,"Cuerpo RGB",[chooseDetector]{chooseDetector(jp_hand::Detector::Body);},source.detector==jp_hand::Detector::Body,source.kind!=jp_hand::SourceKind::KinectIR);
 button(x+340,y+145,w-340,diagnostics?"Ocultar diagnostico":"Diagnostico",[]{diagnostics=!diagnostics;},diagnostics);
 const auto &s=jp_hand::snapshot(binding().source);label(s.status+" · "+ofToString(jp_hand::consumers(binding().source))+" vinculos",x,y+181,jp_hand::simulation(binding().source)?COL_ACCENT_GOLD:COL_TEXT_PRIMARY);
 ofRectangle previewArea(x,y+195,276,155.25f),preview=previewArea;ofSetColor(12);ofDrawRectangle(previewArea);
 if(s.frame&&s.frame!=displayed){displayed=s.frame;if(displayed->valid()){
  // clear() retains OF's old texture dimensions. loadData() alone does not
  // reallocate a cleared texture when the next source is the same size or smaller.
  if(!texture.isAllocated()||texture.getWidth()!=displayed->width||texture.getHeight()!=displayed->height)texture.allocate(displayed->width,displayed->height,GL_RGB);
  texture.loadData(displayed->rgb.data(),displayed->width,displayed->height,GL_RGB);
 }}
 if(s.frame&&texture.isAllocated()){preview.set(0,0,s.frame->width,s.frame->height);preview.scaleTo(previewArea,OF_SCALEMODE_FIT);ofSetColor(255);texture.draw(preview);}
 auto &b=binding();
 for(auto &h:s.hands){ofColor c=h.right?ofColor(COL_ACCENT_CYAN):ofColor(COL_ACCENT_GOLD);ofSetColor(c);ofSetLineWidth(1.5);
  auto point=[&](int i){return ofVec2f(preview.x+h.points[i].x*preview.width,preview.y+h.points[i].y*preview.height);};
  for(int finger=0;finger<5;++finger){ofDrawLine(point(0),point(1+finger*4));for(int j=1;j<4;++j)ofDrawLine(point(finger*4+j),point(finger*4+j+1));}
  for(int i=0;i<21;++i){ofDrawCircle(point(i),h.right==b.right&&b.point==i?4:2);landmarks.push_back({point(i),i,h.right});}
  ofVec2f p(preview.x+h.palm.x*preview.width,preview.y+h.palm.y*preview.height);ofNoFill();ofDrawRectangle(p.x-h.depthRadiusX*preview.width,p.y-h.depthRadiusY*preview.height,2*h.depthRadiusX*preview.width,2*h.depthRadiusY*preview.height);ofFill();
  const std::string side=h.right?"Derecha":"Izquierda";float sideWidth=jp_constants::control_font.stringWidth(side);
  ofDrawCircle(p,h.right==b.right&&b.point<0?4:2);label(side,std::clamp(p.x-sideWidth*.5f,preview.x,preview.getRight()-sideWidth),std::min(p.y+22,preview.getBottom()-4),c);
  landmarks.push_back({p,-1,h.right});
 }
 if(s.body.present&&b.source.detector==jp_hand::Detector::Body){
  auto point=[&](int i){const auto p=s.body.points[i].position;return ofVec2f(preview.x+p.x*preview.width,preview.y+p.y*preview.height);};
  const int edges[][2]={{0,11},{0,12},{11,12},{11,13},{13,15},{12,14},{14,16},{11,23},{12,24},{23,24},{23,25},{25,27},{24,26},{26,28},{27,29},{29,31},{28,30},{30,32}};
  ofSetColor(COL_ACCENT_CYAN);ofSetLineWidth(2);
  for(auto &edge:edges)if(s.body.points[edge[0]].valid()&&s.body.points[edge[1]].valid())ofDrawLine(point(edge[0]),point(edge[1]));
  for(int i=0;i<33;++i)if(s.body.points[i].valid()){ofSetColor(i==b.point?COL_ACCENT_GOLD:COL_ACCENT_CYAN);ofDrawCircle(point(i),i==b.point?5:3);landmarks.push_back({point(i),i,true});}
 }
 auto v=jp_hand::sample(s,b,jp_hand::now());
 float tx=x+290,ty=y+212;
 auto metric=[&](const std::string &t){label(t,tx,ty);ty+=20;};
 if(diagnostics){metric("Captura: "+ofToString(s.frame?s.frame->captureFps:0,1)+" FPS");metric("Detector: "+ofToString(s.inferenceFps,1)+" FPS / "+ofToString(s.inferenceMs,0)+" ms");metric("Edad: "+(s.captured?ofToString((jp_hand::now()-s.captured)*1000,0)+" ms":"--"));metric("Descartes: "+ofToString(s.dropped));}
 else {metric(b.source.detector==jp_hand::Detector::Body?"Una persona · luz visible":"Hasta dos manos");metric(s.frame?(s.frame->infrared?"IR ":"RGB ")+ofToString(s.frame->width)+" x "+ofToString(s.frame->height):"Sin imagen");metric("Imagen sin espejo");metric("Clic en un punto para elegirlo");metric(jp_hand::signalLabel(b.signal));metric("Entrada: "+(v.valid?ofToString(v.raw,3):"sin seguimiento"));}
 label(attached()?"Parametro: "+target.name:"Diagnostico · elegir una senal para explorar",x,y+372,COL_TEXT_PRIMARY);
 if(attached())button(x,y+385,158,target.active()?"Vinculo activo":"Activar vinculo",[]{edit([]{target.activate(!target.active());});},target.active());
 else label("Abrir desde el icono de seguimiento de un parametro.",x,y+405);
 button(x+170,y+385,128,b.invert?"Invertir: si":"Invertir: no",[]{edit([]{binding().invert=!binding().invert;});},b.invert);
 if(b.source.detector==jp_hand::Detector::Body){
  cycleButton(x,y+423,270,jp_hand::signalLabel(b.signal),[](int step){edit([step]{auto &b=binding();jp_hand::setSignal(b,jp_hand::TorsoX+(b.signal-jp_hand::TorsoX+step+6)%6);if(b.signal>=jp_hand::BodyPointX&&b.point<0)b.point=0;});resetMapping();});
  cycleButton(x+280,y+423,w-280,b.point<0?"Elegir punto en imagen":bodyNames[std::clamp(b.point,0,32)],[](int step){edit([step]{auto &b=binding();b.point=(b.point+step+33)%33;});resetMapping();},b.signal==jp_hand::BodyPointX||b.signal==jp_hand::BodyPointY);
 }else{
 cycleButton(x,y+423,136,b.right?"Mano derecha":"Mano izquierda",[](int){edit([]{binding().right=!binding().right;});resetMapping();});
 cycleButton(x+146,y+423,154,jp_hand::signalLabel(b.signal),[](int step){edit([step]{auto &b=binding();const int count=b.source.hasDepth()?5:4;jp_hand::setSignal(b,b.signal>=count?(step>0?0:count-1):(b.signal+step+count)%count);});resetMapping();});
 const char *points[]={"Muneca","Pulgar base","Pulgar nudillo","Pulgar medio","Pulgar punta","Indice nudillo","Indice medio","Indice distal","Indice punta","Medio nudillo","Medio medio","Medio distal","Medio punta","Anular nudillo","Anular medio","Anular distal","Anular punta","Menique nudillo","Menique medio","Menique distal","Menique punta"};
 cycleButton(x+310,y+423,w-310,b.point<0?"Punto: palma":std::string(points[std::clamp(b.point,0,20)])+" ("+ofToString(b.point)+")",[](int step){edit([step]{binding().point=(binding().point+1+step+22)%22-1;});resetMapping();},b.signal==jp_hand::PositionX||b.signal==jp_hand::PositionY);
 }
 label(b.source.detector==jp_hand::Detector::Body?"Torso: centro · Munecas: ancho de hombros · Brazos: altura torso":"X: derecha · Y: arriba | Pinza relativa al ancho de palma",x,y+466);
 if(calibrating&&!(calibrationBinding==b))calibrating=false;
 if(calibrating){if(v.valid&&s.captured!=calibrationStamp){calibrationStamp=s.captured;if(calibrationSamples++==0)calibrationMin=calibrationMax=v.raw;else{calibrationMin=std::min(calibrationMin,v.raw);calibrationMax=std::max(calibrationMax,v.raw);}}if(jp_hand::now()>=calibrationEnd){calibrating=false;if(calibrationSamples>=12&&calibrationMax-calibrationMin>.01f){edit([]{binding().inputMin=calibrationMin;binding().inputMax=calibrationMax;});calibrationMessage="Recorrido aplicado";}else calibrationMessage="Sin recorrido suficiente. Repetir.";}}
 button(x,y+478,178,"Centrar posicion actual",[v]{edit([v]{auto &b=binding();float half=(b.inputMax-b.inputMin)*.5f;b.inputMin=v.raw-half;b.inputMax=v.raw+half;});resetMapping();},false,v.valid&&!calibrating);
 button(x+186,y+478,w-186,calibrating?"Cancelar medicion ("+ofToString(int(std::ceil(calibrationEnd-jp_hand::now())))+" s)":"Medir mi recorrido · 5 s",[]{calibrating=!calibrating;calibrationEnd=jp_hand::now()+5;calibrationSamples=0;calibrationStamp=0;calibrationBinding=binding();calibrationMessage.clear();},calibrating);
 field(0,x,y+532,"Entrada minima",b.inputMin);field(1,x+146,y+532,"Entrada maxima",b.inputMax);
 button(x+285,y+532,122,"Tomar minimo",[v]{edit([v]{binding().inputMin=v.raw;});},false,!calibrating&&v.valid&&v.raw<b.inputMax);
 button(x+415,y+532,w-415,"Tomar maximo",[v]{edit([v]{binding().inputMax=v.raw;});},false,!calibrating&&v.valid&&v.raw>b.inputMin);
 field(2,x,y+584,"Suavizado (ms)",b.smoothingMs);field(3,x+146,y+584,"Zona muerta (%)",b.deadzone*100);
 label(attached()?"Salida: "+ofToString(target.low(),2)+" a "+ofToString(target.high(),2):"Salida de prueba: 0 a 1",x+285,y+601);

 jp_hand::Filter &f=attached()?*target.filter:localFilter;if(!attached())f.step(v,b,0,1,jp_hand::now());
 float normalized=v.valid&&b.inputMax>b.inputMin?std::clamp((v.raw-b.inputMin)/(b.inputMax-b.inputMin),0.f,1.f):NAN;if(b.invert)normalized=1-normalized;
 const char *lossNames[]={"Al perder: mantener valor","Al perder: volver suavemente al valor base","Al perder: desactivar vinculo"};
 cycleButton(x,y+626,w,lossNames[std::clamp(int(b.loss),0,2)],[](int step){edit([step]{binding().loss=jp_hand::Loss((int(binding().loss)+step+3)%3);});});
 jp_tooltip::draw("Se aplica tras 0,5 s sin una senal valida. El vinculo se conserva al detener la fuente.",ofRectangle(x,y+626,w,26));
 ofSetColor(COL_BG_INPUT);ofDrawRectangle(x,y+665,w,7);ofSetColor(v.valid?COL_ACCENT_CYAN:COL_ACCENT_GOLD);if(std::isfinite(normalized))ofDrawRectangle(x,y+665,w*normalized,7);
 label("Entrada: "+(v.valid?ofToString(v.raw,3):"--")+"   Mapeada: "+(std::isfinite(normalized)?ofToString(normalized*100,0)+"%":"--")+"   Salida: "+ofToString(attached()?target.value():f.output,3),x,y+690);
 const std::string state=!b.source.hasDepth()&&b.signal==jp_hand::Distance?"Distancia no disponible en esta camara":v.valid?"Seguimiento valido":jp_hand::running(binding().source)?(b.source.detector==jp_hand::Detector::Body?"Sin seguimiento de los puntos elegidos":"Sin seguimiento"):"Detenido · conserva el valor base";
 label(calibrating?"Move el cuerpo o la mano por todo el recorrido":!calibrationMessage.empty()?calibrationMessage:state,x,y+711,v.valid?COL_ACCENT_CYAN:COL_ACCENT_GOLD);
 if(jp_hand::now()-lastHistory>.033){history.push_back({v.valid?v.raw:NAN,normalized,attached()?target.value():f.output});if(history.size()>240)history.pop_front();lastHistory=jp_hand::now();}
 if(diagnostics){ofRectangle plot(tx,y+288,w-290,62);ofSetColor(COL_BG_INPUT);ofDrawRectangle(plot);const ofColor colors[]={ofColor(COL_ACCENT_GOLD),ofColor(COL_ACCENT_CYAN),ofColor(220)};
 for(int series=0;series<3;++series){ofSetColor(colors[series]);float lo=series==0?b.inputMin:series==2&&attached()?target.low():0,hi=series==0?b.inputMax:series==2&&attached()?target.high():1;
  for(size_t i=1;i<history.size();++i)if(std::isfinite(history[i-1][series])&&std::isfinite(history[i][series])&&hi>lo){auto yy=[&](float z){return plot.getBottom()-std::clamp((z-lo)/(hi-lo),0.f,1.f)*plot.height;};ofDrawLine(plot.x+(i-1)*plot.width/239,yy(history[i-1][series]),plot.x+i*plot.width/239,yy(history[i][series]));}}
 label("Cruda",tx,y+363,colors[0]);label("Mapeada",tx+64,y+363,colors[1]);label("Salida",tx+153,y+363,colors[2]);}
 if(choosingCamera){
  // Draw last and hit-test first: the anchored picker covers only this panel.
  std::vector<jp_hand::SourceInfo> cameras;for(auto &entry:available)if(entry.source.kind==jp_hand::SourceKind::Camera)cameras.push_back(entry);
  ofRectangle menu(x,y+106,w,34*std::min(6,int(cameras.size()))+42);ofSetColor(COL_BG_PANEL);ofDrawRectangle(menu);ofNoFill();ofSetColor(COL_BORDER_DEFAULT);ofDrawRectangle(menu);ofFill();
  pickerBounds=menu;menuButtonStart=buttons.size();
  const int first=cameraPage*6;
  for(int i=first;i<std::min(first+6,int(cameras.size()));++i){auto entry=cameras[i];button(x+6,y+112+(i-first)*34,w-12,entry.label,[choose,entry]{choose(entry.source);},source.kind==entry.source.kind&&source.device==entry.source.device);}
  if(cameras.empty())label("No hay camaras disponibles. Actualizar camaras.",x+10,y+132);
  if(cameras.size()>6)cycleButton(x+6,menu.getBottom()-33,w-12,"Mas camaras",[count=int(cameras.size())](int step){const int pages=(count+5)/6;cameraPage=(cameraPage+step+pages)%pages;});
 }
 ofPopStyle();
}
bool press(int x,int y,int mouseButton){
 if(settings.inside(x,y)){open();return true;}if(!contains(x,y))return false;
 if(mouseButton!=OF_MOUSE_BUTTON_LEFT&&mouseButton!=OF_MOUSE_BUTTON_RIGHT)return true;
 auto activate=[mouseButton](const Button &b){auto f=mouseButton==OF_MOUSE_BUTTON_LEFT?b.action:b.backwards;if(f)f();};
 if(choosingCamera&&!deviceButton.inside(x,y)){
  if(pickerBounds.inside(x,y)){for(size_t i=buttons.size();i>menuButtonStart;--i)if(buttons[i-1].bounds.inside(x,y)){activate(buttons[i-1]);return true;}return true;}
  if(mouseButton==OF_MOUSE_BUTTON_LEFT)choosingCamera=false;return true;
 }
 for(auto it=buttons.rbegin();it!=buttons.rend();++it)if(it->bounds.inside(x,y)){activate(*it);return true;}
 if(mouseButton!=OF_MOUSE_BUTTON_LEFT)return true;

 const auto &snap=jp_hand::snapshot(binding().source);
 if(snap.captured>0&&jp_hand::now()-snap.captured<=.25){const LandmarkHit *nearest=nullptr;float best=12;
  for(const auto &hit:landmarks){float d=hit.position.distance(ofVec2f(x,y));if(d<best){nearest=&hit;best=d;}}
  if(nearest){const auto hit=*nearest;edit([hit]{auto &b=binding();b.point=hit.point;b.right=hit.right;const bool body=b.source.detector==jp_hand::Detector::Body;const bool vertical=b.signal==jp_hand::PositionY||b.signal==jp_hand::BodyPointY||b.signal==jp_hand::TorsoY;jp_hand::setSignal(b,body?(vertical?jp_hand::BodyPointY:jp_hand::BodyPointX):(vertical?jp_hand::PositionY:jp_hand::PositionX));});resetMapping();return true;}
 }
 if(y<panel.y+36){moving=true;offset={x-panel.x,y-panel.y};}
 if(x>panel.getRight()-12){resizing=true;offset.x=panel.width-x;}
 return true;
}
bool drag(int x,int y){if(moving){panel.x=x-offset.x;panel.y=y-offset.y;return true;}if(resizing){panel.width=x+offset.x;return true;}return contains(x,y);}
bool release(){bool used=moving||resizing;moving=resizing=false;return used;}
bool key(int key){if(opened&&choosingCamera&&key==OF_KEY_ESC){choosingCamera=false;return true;}if(!opened||key!=OF_KEY_ESC||editing())return false;opened=false;target={};jp_hand::setPanelVisible(false);return true;}
}
