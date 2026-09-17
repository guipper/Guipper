#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// UTF-8 byte offsets, always on code point boundaries. No graphics or OS state.
namespace jp_text_edit {
inline bool continuation(unsigned char c) { return (c & 0xc0) == 0x80; }
inline size_t boundary(const std::string& s, size_t p) {
    p = std::min(p, s.size());
    while (p && p < s.size() && continuation(s[p])) --p;
    return p;
}
inline size_t previous(const std::string& s, size_t p) {
    p = boundary(s, p); if (p) --p;
    while (p && continuation(s[p])) --p;
    return p;
}
inline size_t next(const std::string& s, size_t p) {
    p = boundary(s, p); if (p < s.size()) ++p;
    while (p < s.size() && continuation(s[p])) ++p;
    return p;
}
inline bool validUTF8(const std::string& s) {
    for (size_t i=0; i<s.size();) {
        const auto c=static_cast<unsigned char>(s[i++]);
        if (c<128) { if (!c) return false; continue; }
        unsigned n=0; uint32_t v=0, minimum=0;
        if (c>=0xc2 && c<=0xdf) {n=1;v=c&31;minimum=128;}
        else if(c>=0xe0 && c<=0xef) {n=2;v=c&15;minimum=2048;}
        else if(c>=0xf0 && c<=0xf4) {n=3;v=c&7;minimum=65536;}
        else return false;
        if(i+n>s.size()) return false;
        while(n--) {const auto d=static_cast<unsigned char>(s[i++]);if(!continuation(d))return false;v=(v<<6)|(d&63);}
        if(v<minimum || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return false;
    }
    return true;
}
inline std::string normalized(std::string s, bool multiline) {
    std::string out;
    for(size_t i=0;i<s.size();++i) {
        char c=s[i];
        if(c=='\r') {if(i+1<s.size() && s[i+1]=='\n')++i;c='\n';}
        if(!multiline && (c=='\n'||c=='\t')) c=' ';
        out+=c;
    }
    return out;
}
inline int category(const std::string& s, size_t p) {
    if(p>=s.size()) return -1;
    unsigned char c=s[p];
    if(c==' '||c=='\n'||c=='\r'||c=='\t') return 0;
    if(c>=128 || (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_') return 1;
    return 2;
}
struct Snapshot { std::string text; size_t cursor=0, anchor=0; };
class Model {
public:
    std::string text, initial, error;
    size_t cursor=0, anchor=0, limit=65536;
    bool multiline=false;
    std::function<bool(const std::string&)> accepts;
    void begin(const std::string& value, size_t caret=std::string::npos) {
        text=initial=value;cursor=anchor=boundary(text,caret);past.clear();future.clear();breakGroup();error.clear();
    }
    size_t low() const {return std::min(cursor,anchor);}
    size_t high() const {return std::max(cursor,anchor);}
    bool selected() const {return cursor!=anchor;}
    std::string selection() const {return text.substr(low(),high()-low());}
    void breakGroup() {group=0;}
    void select(size_t p, bool extend=false) {cursor=boundary(text,p);if(!extend)anchor=cursor;breakGroup();}
    void selectAll() {anchor=0;cursor=text.size();breakGroup();}
    size_t word(size_t p,int direction) const {
        if(direction<0) {
            while(p && category(text,previous(text,p))==0)p=previous(text,p);
            if(p) {int kind=category(text,previous(text,p));while(p && category(text,previous(text,p))==kind)p=previous(text,p);}
        } else {
            int kind=category(text,p);while(p<text.size() && category(text,p)==kind)p=next(text,p);
            while(p<text.size() && category(text,p)==0)p=next(text,p);
        }
        return p;
    }
    void selectWord(size_t p) {
        p=boundary(text,p);if(p==text.size() && p)p=previous(text,p);
        const int kind=category(text,p);size_t a=p,b=p;
        while(a && category(text,previous(text,a))==kind)a=previous(text,a);
        while(b<text.size() && category(text,b)==kind)b=next(text,b);
        anchor=a;cursor=b;breakGroup();
    }
    void horizontal(int direction,bool extend=false,bool byWord=false) {
        if(selected()&&!extend&&!byWord) {select(direction<0?low():high());return;}
        select(byWord?word(cursor,direction):(direction<0?previous(text,cursor):next(text,cursor)),extend);
    }
    size_t lineStart(size_t p) const {auto n=p?text.rfind('\n',p-1):std::string::npos;return n==std::string::npos?0:n+1;}
    size_t lineEnd(size_t p) const {auto n=text.find('\n',p);return n==std::string::npos?text.size():n;}
    void edge(bool end,bool extend=false,bool document=false) {select(document?(end?text.size():0):(end?lineEnd(cursor):lineStart(cursor)),extend);}
    void vertical(int direction,bool extend=false) {
        size_t start=lineStart(cursor), column=0;
        if(direction<0 && start==0) {select(0,extend);return;}
        if(direction>0 && lineEnd(cursor)==text.size()) {select(text.size(),extend);return;}
        for(size_t p=start;p<cursor;p=next(text,p))++column;
        size_t p=direction<0?(start?lineStart(start-1):0):(lineEnd(cursor)<text.size()?lineEnd(cursor)+1:text.size());
        const size_t end=lineEnd(p);
        while(column-- && p<end)p=next(text,p);
        select(p,extend);
    }
    bool insert(std::string value,double now=0,bool typing=false) {
        value=normalized(std::move(value),multiline);
        if(!validUTF8(value)) {error="Invalid UTF-8";return false;}
        std::string candidate=text.substr(0,low())+value+text.substr(high());
        if(candidate.size()>limit || (accepts&&!accepts(candidate))) {error="Invalid text or length limit exceeded";return false;}
        if(candidate==text && value.empty())return true;
        const size_t p=low()+value.size();record(typing&&!selected()?1:0,now);
        text=std::move(candidate);cursor=anchor=p;error.clear();return true;
    }
    void erase(int direction,bool byWord=false,double now=0) {
        const bool selectionBefore=selected();
        size_t a=low(),b=high();
        if(!selected()) {if(direction<0)a=byWord?word(cursor,-1):previous(text,cursor);else b=byWord?word(cursor,1):next(text,cursor);}
        if(a==b)return;
        record(byWord||selectionBefore?0:(direction<0?2:3),now);
        text.erase(a,b-a);cursor=anchor=a;error.clear();
    }
    void undo() {if(past.empty())return;future.push_back(snapshot());restore(past.back());past.pop_back();breakGroup();}
    void redo() {if(future.empty())return;past.push_back(snapshot());restore(future.back());future.pop_back();breakGroup();}
    void cancel() {begin(initial);}
private:
    std::vector<Snapshot> past,future;
    int group=0;double last=0;
    Snapshot snapshot() const {return {text,cursor,anchor};}
    void restore(const Snapshot& s) {text=s.text;cursor=s.cursor;anchor=s.anchor;error.clear();}
    void record(int kind,double now) {
        if(!kind||kind!=group||now-last>0.75||now<last) {past.push_back(snapshot());if(past.size()>256)past.erase(past.begin());}
        future.clear();group=kind;last=now;
    }
};
}
