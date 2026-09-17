#pragma once
#include "jp_text_edit.h"
#include "ofMain.h"
#include "jp_constants.h"
#include "jp_pointer.h"
#include <set>

namespace jp_text_input {
inline bool& spanish() {static bool value=false;return value;}
inline std::string message(const char* en,const char* es) {return spanish()?es:en;}

struct Field {
    std::string id, panel;
    ofRectangle bounds, hitBounds;
    ofTrueTypeFont* font=nullptr;
    bool focused=false, multiline=false, restoreOnCancel=true;
    size_t limit=65536;
    int layer=jp_pointer::kFieldEdit;
    std::function<bool(const std::string&)> accepts;
    std::function<std::string()> read;
    std::function<void(const std::string&)> write;
    std::function<void()> focus, cancel, change;
    std::function<bool()> commit;
    std::function<std::string(const std::string&)> validate;
    std::function<bool(int)> special;
};
class Controller {
public:
    jp_text_edit::Model model;
    bool consumed=false;
    int currentModifiers=0;
    void defer(int key) {pending.emplace_back(key,ofGetWindowPtr()->events().getModifiers());consumed=true;}
    void beginFrame() {fields.clear();}
    void endFrame() {
        if(!active.empty() && !find(active)) {active.clear();dragging=false;}
        if(!active.empty() && !model.error.empty()) {
            auto& font=*current.font;
            const std::string error=model.error=="Invalid text or length limit exceeded"?message("Text exceeds the allowed length","El texto supera el límite permitido"):model.error;
            const float width=std::min((float)ofGetWidth()-16,font.stringWidth(error)+16);
            const float x=std::clamp(current.bounds.x,8.f,std::max(8.f,ofGetWidth()-width-8));
            const float y=std::min(current.bounds.getBottom()+3,ofGetHeight()-font.getSize()-18.f);
            ofPushStyle();ofFill();ofSetColor(COL_BG_INPUT);ofDrawRectRounded(x,y,width,font.getSize()+12,3);
            ofSetColor(COL_ACCENT_GOLD);font.drawString(error,x+8,y+font.getSize()+3);ofPopStyle();
        }
        auto queued=std::move(pending);pending.clear();
        for(const auto& event:queued)if(!key(event.first,event.second))break;
    }
    void reset() {active.clear();fields.clear();pending.clear();dragging=false;rejected=false;consumed=false;buttons.clear();}
    bool focused() const {return !active.empty();}
    bool gesture() const {return dragging;}
    void add(Field field) {
        field.hitBounds=field.bounds.getIntersection(ofRectangle(0,0,ofGetWidth(),ofGetHeight()));
        if(glIsEnabled(GL_SCISSOR_TEST)) {
            GLint clip[4],vp[4];glGetIntegerv(GL_SCISSOR_BOX,clip);glGetIntegerv(GL_VIEWPORT,vp);
            const float sx=std::max(1,vp[2])/(float)std::max(1,ofGetWidth()),sy=std::max(1,vp[3])/(float)std::max(1,ofGetHeight());
            field.hitBounds=field.hitBounds.getIntersection(ofRectangle((clip[0]-vp[0])/sx,ofGetHeight()-(clip[1]-vp[1]+clip[3])/sy,clip[2]/sx,clip[3]/sy));
        }
        if(field.hitBounds.width<=0 || field.hitBounds.height<=0)return;
        fields.push_back(std::move(field));auto& f=fields.back();
        if(f.focused && active!=f.id) start(f,false);
        if(active==f.id && !f.focused) active.clear();
        if(active==f.id) {
            current=f;
            const auto value=f.read();
            if(value!=model.text) {model.text=value;model.select(value.size());}
        }
        draw(f);
    }
    bool validate() {
        if(current.validate)model.error=current.validate(model.text);
        else model.error.clear();
        return model.error.empty();
    }
    bool finish(bool cancel=false) {
        if(active.empty())return true;
        if(cancel) {const auto before=model.text;if(current.restoreOnCancel)model.cancel();current.write(model.text);if(current.change&&before!=model.text)current.change();if(current.cancel)current.cancel();}
        else {
            if(!validate())return false;
            if(current.commit&&!current.commit()) {model.error=message("Could not apply value","No se pudo aplicar el valor");return false;}
        }
        active.clear();model.breakGroup();return true;
    }
    bool key(int key,int modifiers=-1) {
        if(active.empty())return false;
        consumed=true;lastInteraction=ofGetElapsedTimef();
        const std::string before=model.text;
        if(key==OF_KEY_ESC) {finish(true);return true;}
        if(modifiers<0)modifiers=ofGetWindowPtr()->events().getModifiers();
        currentModifiers=modifiers;
        const bool shift=modifiers & OF_KEY_SHIFT;
        const bool control=modifiers & OF_KEY_CONTROL;
        const bool alt=modifiers & OF_KEY_ALT;
#ifdef TARGET_OSX
        const bool command=modifiers & OF_KEY_COMMAND;
        const bool word=alt;
#else
        const bool command=control&&!alt; // Ctrl+Alt/AltGr produces text.
        const bool word=command;
#endif
        if(key==OF_KEY_TAB) {
            const auto id=active, panel=current.panel;
            if(!finish())return true;
            std::vector<Field> order;
            for(const auto& f:fields)if(f.panel==panel)order.push_back(f);
            std::stable_sort(order.begin(),order.end(),[](const Field&a,const Field&b){return a.bounds.y==b.bounds.y?a.bounds.x<b.bounds.x:a.bounds.y<b.bounds.y;});
            if(order.size()>1)for(size_t i=0;i<order.size();++i)if(order[i].id==id){start(order[(i+order.size()+(shift?-1:1))%order.size()]);break;}
            return true;
        }
        if(current.special && !shift && current.special(key)) {sync();return true;}
        if(key==OF_KEY_RETURN || key=='\r') {
            if(current.multiline&&!command)model.insert("\n",lastInteraction);
            else {finish();return true;}
        } else if(key==OF_KEY_LEFT || key==OF_KEY_RIGHT) {
#ifdef TARGET_OSX
            if(command)model.edge(key==OF_KEY_RIGHT,shift);else
#endif
            model.horizontal(key==OF_KEY_LEFT?-1:1,shift,word);
        } else if(key==OF_KEY_HOME || key==OF_KEY_END) model.edge(key==OF_KEY_END,shift,control||command);
        else if(key==OF_KEY_UP || key==OF_KEY_DOWN) {
#ifdef TARGET_OSX
            if(command)model.edge(key==OF_KEY_DOWN,shift,true);else
#endif
            model.vertical(key==OF_KEY_UP?-1:1,shift);
        }
        else if(key==OF_KEY_BACKSPACE || key==OF_KEY_DEL)model.erase(key==OF_KEY_BACKSPACE?-1:1,word,lastInteraction);
        else {
            int chord=key;
            if(chord>=1 && chord<=26)chord+='a'-1;
            if(chord>='A'&&chord<='Z')chord+='a'-'A';
            if(command) {
                if(chord=='a')model.selectAll();
                else if(chord=='c'||chord=='x') {model.breakGroup();if(model.selected()){ofGetWindowPtr()->setClipboardString(model.selection());if(chord=='x')model.erase(-1,false,lastInteraction);}}
                else if(chord=='v') {model.breakGroup();auto value=ofGetWindowPtr()->getClipboardString();if(!value.empty())model.insert(value,lastInteraction);}
                else if(chord=='z') {if(shift)model.redo();else model.undo();}
                else if(chord=='y')model.redo();
            } else if(key>=32 && !(key>=OF_KEY_F1 && key<=OF_KEY_INSERT) && !(key>=OF_KEY_LEFT_SHIFT && key<=OF_KEY_RIGHT_SUPER)) {
                std::string value;ofUTF8Append(value,key);model.insert(value,lastInteraction,true);
            }
        }
        current.write(model.text);if(current.change && before!=model.text)current.change();return true;
    }
    bool press(int x,int y,int button) {
        if(dragging) {buttons.insert(button);return true;}
        Field* target=nullptr;
        for(auto& f:fields) {jp_pointer::Scope scope(f.layer);if(f.hitBounds.inside(x,y) && jp_pointer::available(x,y))target=&f;}
        if(button!=OF_MOUSE_BUTTON_LEFT) {if(!target)return false;buttons.insert(button);dragging=true;rejected=true;return true;}
        buttons.insert(button);
        if(target && target->id==active) {point(x,y,ofGetKeyPressed(OF_KEY_SHIFT));dragging=true;return true;}
        if(!active.empty()&&!finish()) {dragging=true;rejected=true;return true;}
        if(!target){buttons.erase(button);return false;}
        start(*target);point(x,y,ofGetKeyPressed(OF_KEY_SHIFT));dragging=true;return true;
    }
    bool drag(int x,int y) {if(!dragging)return false;if(!rejected)point(x,y,true,false);return true;}
    bool release(int button=OF_MOUSE_BUTTON_LEFT) {bool was=buttons.erase(button)>0;if(buttons.empty()){dragging=false;rejected=false;}return was;}
private:
    std::vector<Field> fields;
    std::vector<std::pair<int,int>> pending;
    std::set<int> buttons;
    Field current;
    std::string active;
    bool dragging=false,rejected=false;
    float lastInteraction=0,lastClick=-1;
    ofVec2f clickPosition;
    size_t visibleStart=0;
    Field* find(const std::string& id) {for(auto&f:fields)if(f.id==id)return &f;return nullptr;}
    void start(const Field& f,bool initialize=true) {current=f;active=f.id;visibleStart=0;visibleLines.clear();lastClick=-1;model.multiline=f.multiline;model.limit=f.limit;model.accepts=f.accepts;model.begin(f.read());lastInteraction=ofGetElapsedTimef();if(initialize&&f.focus)f.focus();}
    void sync() {if(!active.empty() && current.read()!=model.text){auto value=current.read();model.selectAll();model.insert(value,ofGetElapsedTimef());}}
    std::vector<std::pair<size_t,size_t>> visibleLines;
    size_t hit(float x,float y) {
        auto& font=*current.font;
        size_t a=visibleStart,b=model.text.size();
        if(current.multiline && visibleLines.empty()) {
            size_t start=0;
            do {size_t end=start;
                while(end<model.text.size()&&model.text[end]!='\n'){size_t n=jp_text_edit::next(model.text,end);if(end>start&&font.stringWidth(model.text.substr(start,n-start))>current.bounds.width-12)break;end=n;}
                visibleLines.emplace_back(start,end);if(end==model.text.size())break;start=model.text[end]=='\n'?end+1:end;
            }while(start<=model.text.size());
        }
        if(current.multiline && !visibleLines.empty()) {
            int row=std::clamp((int)((y-current.bounds.y-4)/font.getLineHeight()),0,(int)visibleLines.size()-1);
            a=visibleLines[row].first;b=visibleLines[row].second;
        }
        float before=0;
        for(size_t p=a;p<b;) {size_t n=jp_text_edit::next(model.text,p);float after=font.stringWidth(model.text.substr(a,n-a));if(x-current.bounds.x-6<(before+after)*.5f)return p;p=n;before=after;}
        return b;
    }
    void point(float x,float y,bool extend,bool doubleClick=true) {
        const auto p=hit(x,y);const float now=ofGetElapsedTimef();
        if(doubleClick && !extend && now-lastClick<.35f && clickPosition.distance(ofVec2f(x,y))<5) {model.selectWord(p);lastClick=-1;}
        else {model.select(p,extend);if(doubleClick){lastClick=now;clickPosition={x,y};}}
        lastInteraction=now;
    }
    void draw(const Field& f) {
        auto& font=*f.font;const bool editing=f.id==active;
        const auto text=editing?model.text:f.read();
        const float width=std::max(1.f,f.bounds.width-12);
        std::vector<std::pair<size_t,size_t>> lines;
        size_t start=0;
        if(!f.multiline && editing) {
            start=jp_text_edit::boundary(text,std::min(visibleStart,model.cursor));
            if(font.stringWidth(text.substr(start,model.cursor-start))>width) {
                start=model.cursor;
                while(start>0) {const auto previous=jp_text_edit::previous(text,start);if(font.stringWidth(text.substr(previous,model.cursor-previous))>width)break;start=previous;}
            }
        }
        do {
            size_t end=start;
            while(end<text.size()&&text[end]!='\n') {size_t n=jp_text_edit::next(text,end);if(end>start&&font.stringWidth(text.substr(start,n-start))>width)break;end=n;}
            lines.emplace_back(start,end);
            if(!f.multiline || end==text.size())break;
            start=text[end]=='\n'?end+1:end;
        }while(start<=text.size());
        if(editing&&!f.multiline && lines.front().second==text.size()) {
            while(start>0){const auto previous=jp_text_edit::previous(text,start);if(font.stringWidth(text.substr(previous))>width)break;start=previous;}
            lines.front().first=start;
        }
        if(editing)visibleStart=start;
        size_t first=0,count=f.multiline?std::max(1,(int)((f.bounds.height-8)/font.getLineHeight())):1;
        if(editing&&f.multiline)for(size_t i=0;i<lines.size();++i)if(model.cursor>=lines[i].first&&model.cursor<=lines[i].second){if(i>=count)first=i-count+1;break;}
        if(editing)visibleLines.assign(lines.begin()+first,lines.begin()+std::min(lines.size(),first+count));
        ofPushStyle();ofFill();
        float y=f.multiline?f.bounds.y+4+font.getSize():f.bounds.y+f.bounds.height*.5f+font.getSize()*.35f;
        for(size_t i=first;i<lines.size()&&i<first+count;++i) {
            const size_t a=lines[i].first,b=lines[i].second;
            if(editing&&model.selected()) {
                size_t lo=std::max(a,model.low()),hi=std::min(b,model.high());
                if(hi>lo) {ofSetColor(ofColor(COL_ACCENT_CYAN,105));ofDrawRectangle(f.bounds.x+6+font.stringWidth(text.substr(a,lo-a)),y-font.getSize(),font.stringWidth(text.substr(lo,hi-lo)),font.getSize()+3);}
            }
            ofSetColor(COL_TEXT_PRIMARY);font.drawString(text.substr(a,b-a),f.bounds.x+6,y);
            if(editing&&model.cursor>=a&&model.cursor<=b && std::fmod(ofGetElapsedTimef()-lastInteraction,1.f)<.55f) {
                const float x=f.bounds.x+6+font.stringWidth(text.substr(a,model.cursor-a));ofSetColor(COL_ACCENT_CYAN);ofDrawLine(x,y-font.getSize(),x,y+3);
            }
            y+=font.getLineHeight();
        }
        ofPopStyle();
    }

};
inline Field field(const std::string& id,const std::string& panel,const ofRectangle& bounds,
    ofTrueTypeFont& font,std::string& text,bool focused) {
    Field f;f.id=id;f.panel=panel;f.bounds=bounds;f.font=&font;f.focused=focused;f.layer=std::max(jp_pointer::kFieldEdit,jp_pointer::layer());
    f.read=[&text]{return text;};f.write=[&text](const std::string& value){text=value;};return f;
}
inline std::string integer(const std::string& s,long long low,long long high) {
    try {size_t end=0;long long n=std::stoll(s,&end);
        if(end==s.size() && n>=low && n<=high)return {};
    }catch(...){}
    return message("Range: ","Rango: ")+std::to_string(low)+".."+std::to_string(high);
}
inline Controller& controller() {static Controller value;return value;}
}
