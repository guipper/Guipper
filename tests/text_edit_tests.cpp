#include "../src/JPutils/jp_text_edit.h"
#include "../src/JPbox/jp_media_time.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
using jp_text_edit::Model;
int main() {
    Model m;m.begin(u8"niño canción");m.horizontal(-1);assert(m.text.substr(m.cursor)=="n");
    m.edge(false,false,true);m.horizontal(1,true,true);assert(m.selection()==u8"niño ");
    m.insert("otra ");assert(m.text==u8"otra canción");m.undo();assert(m.text==u8"niño canción");m.redo();assert(m.text==u8"otra canción");
    m.begin(u8"áñé");m.erase(-1);assert(m.text==u8"áñ");m.erase(-1);assert(m.text==u8"á");m.undo();assert(m.text==u8"áñé");
    m.begin("abc");m.select(3);m.select(1,true);assert(m.selection()=="bc");m.horizontal(-1);assert(m.cursor==1&&!m.selected());
    m.selectAll();m.limit=3;assert(!m.insert("abcd"));assert(m.selection()=="abc");assert(!m.insert("\xc3"));assert(m.text=="abc");
    m.limit=100;m.insert("a\r\nb\tc");assert(m.text=="a b c");
    m.multiline=true;m.begin("one\ntwo\nthree");m.select(5);m.edge(false);assert(m.cursor==4);m.edge(true);assert(m.cursor==7);m.vertical(-1,true);assert(m.selection()=="\ntwo");
    m.begin("/path/to/file.frag");m.horizontal(-1,false,true);assert(m.text.substr(m.cursor)=="frag");m.erase(-1,true);assert(m.text=="/path/to/filefrag");
    m.begin("");m.insert("a",1,true);m.insert("b",1.1,true);m.insert("c",1.2,true);m.undo();assert(m.text.empty());m.redo();assert(m.text=="abc");m.horizontal(-1);m.insert("!",1.3,true);m.undo();assert(m.text=="abc");
    m.begin("original");m.selectAll();m.insert("changed");m.cancel();assert(m.text=="original");
    m.begin("abc");m.select(2);m.vertical(-1,true);assert(m.selection()=="ab");
    m.begin(u8"niño");m.selectWord(2);assert(m.selection()==u8"niño");
    float time=.25f;
    assert(jp_media_time::parse("00:30",60,61,time)&&time==.5f);
    assert(jp_media_time::parse("30f",60,61,time)&&time==.5f);
    assert(jp_media_time::parse("1:00:00",7200,100,time)&&time==.5f);
    assert(!jp_media_time::parse("1junk",60,61,time)&&time==.5f);
    assert(!jp_media_time::parse("00:60",60,61,time));
    assert(!jp_media_time::parse("61f",60,61,time));
    assert(!jp_media_time::parse("NaN",60,61,time));
    assert(!jp_media_time::parse("-1",60,61,time));
    assert(!jp_media_time::parse("",60,61,time));
    std::string frames="0f";
    for(int i=0;i<100;++i)assert(jp_media_time::stepFrames(frames,1,121,1));
    assert(frames=="100f");
    assert(jp_media_time::stepFrames(frames,1,121,-1)&&frames=="99f");
    std::cout<<"text editing tests passed\n";
}
