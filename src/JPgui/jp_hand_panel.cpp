#include "jp_hand_panel.h"
#include "jp_button.h"
#include "../JPutils/jp_text_input.h"
#include <deque>
namespace jp_hand_ui {
namespace {
bool opened=false,moving=false,resizing=false,lost=false;
ofRectangle panel(40,65,590,674),settings;ofVec2f offset;
Target target;jp_hand::Binding local;jp_hand::Filter localFilter;
ofTexture texture;std::shared_ptr<const jp_hand::Frame> displayed;
struct Button {ofRectangle bounds;std::function<void()> action;};std::vector<Button> buttons;
struct Field {std::string text;bool focus=false;};std::array<Field,4> fields;
std::deque<std::array<float,3>> history;double lastHistory=0;
bool attached(){return target.binding&&target.valid&&target.valid();}
jp_hand::Binding &binding(){return attached()?*target.binding:local;}
void edit(const std::function<void()> &f){if(attached()&&target.edit)target.edit(f);else f();}
void label(const std::string &text,float x,float y,ofColor color=COL_TEXT_SECONDARY){ofSetColor(color);jp_constants::control_font.drawString(text,x,y);}
void button(float x,float y,float w,const std::string &text,std::function<void()> action,bool on=false,bool enabled=true){
 ofRectangle r(x,y,w,26);jp_button::draw(r,text,on,enabled);if(enabled)buttons.push_back({r,std::move(action)});
}
void field(int i,float x,float y,const std::string &name,float value){
 auto &state=fields[i];if(!state.focus)state.text=ofToString(value,i==2?0:3);
 label(name,x,y-5);ofRectangle bounds(x,y,123,26);ofSetColor(COL_BG_INPUT);ofDrawRectRounded(bounds,4);
 jp_text_input::Field f;f.id="kinect-field-"+ofToString(i);f.panel="kinect";f.bounds=bounds;f.font=&jp_constants::control_font;f.focused=state.focus;f.layer=80;f.limit=16;
 f.read=[i]{return fields[i].text;};f.write=[i](const std::string&s){fields[i].text=s;};f.focus=[i]{fields[i].focus=true;};f.cancel=[i]{fields[i].focus=false;};
 f.validate=[i](const std::string &s){try{size_t end;float v=std::stof(s,&end);if(end!=s.size()||!std::isfinite(v))throw std::runtime_error("numero");auto &b=binding();
  if((i==0&&v>=b.inputMax)||(i==1&&v<=b.inputMin))return std::string("Minimo debe ser menor que maximo");
  if((i==2&&(v<0||v>5000))||(i==3&&(v<0||v>50)))return std::string("Fuera de rango");return std::string();}catch(...){return std::string("Ingresar un numero valido");}};
 f.commit=[i]{float v=std::stof(fields[i].text);edit([&]{auto &b=binding();if(i==0)b.inputMin=v;if(i==1)b.inputMax=v;if(i==2)b.smoothingMs=v;if(i==3)b.deadzone=v*.01f;});fields[i].focus=false;return true;};
 jp_text_input::controller().add(std::move(f));
}
}
void open(Target t){jp_text_input::controller().finish();target=std::move(t);opened=true;history.clear();displayed.reset();for(auto &f:fields)f.focus=false;jp_hand::setPanelVisible(true);}
bool visible(){return opened;}bool contains(float x,float y){return opened&&panel.inside(x,y);}
void beginFrame(){settings={};if(target.binding&&!attached()){jp_text_input::controller().finish(true);target={};for(auto &f:fields)f.focus=false;history.clear();}jp_hand::setPanelVisible(opened);}
bool editing(){for(auto &f:fields)if(f.focus)return true;return false;}
void settingsButton(const ofRectangle &r){settings=r;jp_button::draw(r,"Kinect",opened);}
void draw(){
 if(!opened)return;
 jp_pointer::Scope scope(80);ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofFill();
 panel.width=std::clamp(panel.width,570.f,std::max(570.f,float(ofGetWidth()-20)));panel.height=674;
 panel.x=std::clamp(panel.x,10.f,std::max(10.f,ofGetWidth()-panel.width-10));panel.y=std::clamp(panel.y,42.f,std::max(42.f,ofGetHeight()-panel.height-10));
 ofSetColor(COL_BG_PANEL);ofDrawRectRounded(panel,6);ofNoFill();ofSetColor(COL_BORDER_DEFAULT);ofDrawRectRounded(panel,6);ofFill();
 float x=panel.x+14,y=panel.y,w=panel.width-28;buttons.clear();
 label("KINECT · MANOS",x,y+24,COL_TEXT_PRIMARY);label("Arrastrar para mover",x+195,y+24,COL_TEXT_MUTED);
 button(panel.getRight()-65,y+7,50,"Cerrar",[]{jp_text_input::controller().finish(true);opened=false;target={};jp_hand::setPanelVisible(false);});
 button(x,y+43,152,"Iniciar seguimiento",[]{jp_hand::start(false);},jp_hand::running()&&!jp_hand::simulation());
 button(x+160,y+43,78,"Detener",[]{jp_hand::stop();},false,jp_hand::running());
 button(x+246,y+43,100,"Reconectar",[]{jp_hand::reconnect();});
 button(x+354,y+43,100,"Simulacion",[]{jp_hand::start(true);},jp_hand::simulation());
 if(jp_hand::simulation())button(x+462,y+43,w-462,lost?"Volver":"Perder",[]{lost=!lost;jp_hand::simulateLoss(lost);},lost);
 const auto &s=jp_hand::snapshot();label(s.status,x,y+89,jp_hand::simulation()?COL_ACCENT_GOLD:COL_TEXT_PRIMARY);
 ofRectangle preview(x,y+104,276,155.25f);ofSetColor(12);ofDrawRectangle(preview);
 if(s.frame&&s.frame!=displayed){displayed=s.frame;if(displayed->rgb.size()==960*540*3)texture.loadData(displayed->rgb.data(),960,540,GL_RGB);}
 if(s.frame&&texture.isAllocated()){ofSetColor(255);texture.draw(preview);}
 auto &b=binding();
 for(auto &h:s.hands){ofColor c=h.right?ofColor(COL_ACCENT_CYAN):ofColor(COL_ACCENT_GOLD);ofSetColor(c);ofSetLineWidth(1.5);
  auto point=[&](int i){return ofVec2f(preview.x+h.points[i].x*preview.width,preview.y+h.points[i].y*preview.height);};
  for(int finger=0;finger<5;++finger){ofDrawLine(point(0),point(1+finger*4));for(int j=1;j<4;++j)ofDrawLine(point(finger*4+j),point(finger*4+j+1));}
  for(int i=0;i<21;++i)ofDrawCircle(point(i),h.right==b.right&&b.point==i?4:2);
  ofVec2f p(preview.x+h.palm.x*preview.width,preview.y+h.palm.y*preview.height);ofNoFill();ofDrawRectangle(p.x-2,p.y-2,4,4);ofFill();
  label(h.right?"Derecha":"Izquierda",p.x+6,p.y,c);
 }
 float tx=x+290,ty=y+121;
 auto metric=[&](const std::string &t){label(t,tx,ty);ty+=20;};
 metric("RGB 960 x 540 · sin espejo");metric("Captura: "+ofToString(s.frame?s.frame->captureFps:0,1)+" FPS");metric("Inferencia: "+ofToString(s.inferenceFps,1)+" FPS / "+ofToString(s.inferenceMs,0)+" ms");
 metric("Edad: "+(s.captured?ofToString((jp_hand::now()-s.captured)*1000,0)+" ms":"--"));metric("Descartados: "+ofToString(s.dropped));
 for(auto &h:s.hands)if(h.right==b.right)metric(h.depthValid?"Palma Z: "+ofToString(h.meters,2)+" m":"Palma Z: sin dato");
 label(attached()?"Parametro: "+target.name:"Diagnostico · elegir una senal para explorar",x,y+281,COL_TEXT_PRIMARY);
 if(attached())button(x,y+292,158,target.active()?"Vinculo activo":"Activar vinculo",[]{edit([]{target.activate(!target.active());});},target.active());
 else label("Abrir desde el boton K de un parametro para vincularlo.",x,y+312);
 button(x+170,y+292,128,b.invert?"Invertir: si":"Invertir: no",[]{edit([]{binding().invert=!binding().invert;});},b.invert);
 const char *names[]={"Pinza","Apertura","Posicion X","Posicion Y","Distancia (m)"};
 button(x,y+330,136,b.right?"Mano derecha":"Mano izquierda",[]{edit([]{binding().right=!binding().right;});});
 button(x+146,y+330,154,names[std::clamp(b.signal,0,4)],[]{edit([]{auto &b=binding();b.signal=(b.signal+1)%5;b.inputMin=b.signal==jp_hand::Pinch?.15f:b.signal==jp_hand::Distance?.7f:0;b.inputMax=b.signal==jp_hand::Distance?1.5f:1;});});
 button(x+310,y+330,w-310,b.point<0?"Punto: palma":"Punto: "+ofToString(b.point),[]{edit([]{binding().point=(binding().point+2)%22-1;});},false,b.signal==jp_hand::PositionX||b.signal==jp_hand::PositionY);
 label("X: derecha · Y: arriba | Pinza relativa al ancho de palma",x,y+378);
 auto v=jp_hand::sample(s,b,jp_hand::now());
 field(0,x,y+403,"Entrada minima",b.inputMin);field(1,x+146,y+403,"Entrada maxima",b.inputMax);
 button(x+285,y+403,122,"Capturar minimo",[v]{edit([v]{binding().inputMin=v.raw;});},false,v.valid&&v.raw<b.inputMax);
 button(x+415,y+403,w-415,"Capturar maximo",[v]{edit([v]{binding().inputMax=v.raw;});},false,v.valid&&v.raw>b.inputMin);
 field(2,x,y+456,"Suavizado (ms)",b.smoothingMs);field(3,x+146,y+456,"Zona muerta (%)",b.deadzone*100);
 label(attached()?"Salida: "+ofToString(target.low(),2)+" a "+ofToString(target.high(),2):"Salida de prueba: 0 a 1",x+285,y+473);
 jp_hand::Filter &f=attached()?*target.filter:localFilter;if(!attached())f.step(v,b,0,1,jp_hand::now());
 label("Cruda: "+(v.valid?ofToString(v.raw,3):"--")+"   Normalizada: "+ofToString(f.normalized,3)+"   Final: "+ofToString(attached()?target.value():f.output,3),x,y+511);
 label(v.valid?"Seguimiento valido":jp_hand::running()?"Sin seguimiento · mantiene el ultimo valor":"Detenido · conserva el valor base",x,y+532,v.valid?COL_ACCENT_CYAN:COL_ACCENT_GOLD);
 if(jp_hand::now()-lastHistory>.033){history.push_back({v.valid?v.raw:NAN,f.normalized,attached()?target.value():f.output});if(history.size()>240)history.pop_front();lastHistory=jp_hand::now();}
 ofRectangle plot(x,y+549,w,76);ofSetColor(COL_BG_INPUT);ofDrawRectangle(plot);const ofColor colors[]={ofColor(COL_ACCENT_GOLD),ofColor(COL_ACCENT_CYAN),ofColor(220)};
 for(int series=0;series<3;++series){ofSetColor(colors[series]);float lo=series==0?b.inputMin:series==2&&attached()?target.low():0,hi=series==0?b.inputMax:series==2&&attached()?target.high():1;
  for(size_t i=1;i<history.size();++i)if(std::isfinite(history[i-1][series])&&std::isfinite(history[i][series])&&hi>lo){auto yy=[&](float z){return plot.getBottom()-std::clamp((z-lo)/(hi-lo),0.f,1.f)*plot.height;};ofDrawLine(plot.x+(i-1)*plot.width/239,yy(history[i-1][series]),plot.x+i*plot.width/239,yy(history[i][series]));}}
 label("Cruda",x,y+645,colors[0]);label("Normalizada",x+70,y+645,colors[1]);label("Final",x+188,y+645,colors[2]);label("8 s · cada curva en su rango",x+260,y+645);
 ofPopStyle();
}
bool press(int x,int y,int mouseButton){
 if(settings.inside(x,y)){open();return true;}if(!contains(x,y))return false;
 if(mouseButton!=OF_MOUSE_BUTTON_LEFT)return true;
 for(auto &button:buttons)if(button.bounds.inside(x,y)){auto f=button.action;f();return true;}
 if(y<panel.y+36){moving=true;offset={x-panel.x,y-panel.y};}
 if(x>panel.getRight()-12){resizing=true;offset.x=panel.width-x;}
 return true;
}
bool drag(int x,int y){if(moving){panel.x=x-offset.x;panel.y=y-offset.y;return true;}if(resizing){panel.width=x+offset.x;return true;}return contains(x,y);}
bool release(){bool used=moving||resizing;moving=resizing=false;return used;}
bool key(int key){if(!opened||key!=OF_KEY_ESC||editing())return false;opened=false;target={};jp_hand::setPanelVisible(false);return true;}
}
