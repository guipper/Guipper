#include "../src/JPutils/jp_text_render.h"
#include "../src/JPutils/jp_text_layout.h"
#include <cassert>
#include <iostream>
#include <numeric>
using namespace jp_text;
static uint64_t alpha(const Raster &r){uint64_t sum=0;for(size_t i=3;i<r.rgba.size();i+=4)sum+=r.rgba[i];return sum;}
int main(){
    auto measure=[](const std::string &s){int n=0;for(size_t p=0;p<s.size();p=jp_text_edit::next(s,p))++n;return float(n);};
    assert((wrap("niño canción\n\nfin",7,measure)==std::vector<std::string>{"niño","canción","","fin"}));
    assert((wrap("abcdef",2,measure)==std::vector<std::string>{"ab","cd","ef"}));
    Style s;s.size=36;s.margin=4;
    auto empty=rasterize("","Sans",s,640,360);assert(empty.error.empty()&&alpha(empty)==0);
    auto text=rasterize("Canción, niño & <texto>\nΕλληνικά Привет","Sans",s,640,360);
    assert(text.error.empty()&&alpha(text)>0&&alpha(text)<uint64_t(640)*360*255);
    assert(text.rgba[3]==0);assert(text.rgba==rasterize("Canción, niño & <texto>\nΕλληνικά Привет","Sans",s,640,360).rgba);
    auto changed=rasterize("Otro texto","Sans",s,640,360);assert(changed.rgba!=text.rgba);
    s.outline=3;s.shadow=.7;s.background={.2,.3,.4,.5};auto styled=rasterize("Otro texto","Sans",s,640,360);assert(alpha(styled)>alpha(changed));
    s.y=0;s.anchor=2;assert(rasterize("Fuera","Sans",s,640,360).clipped);
    assert(!rasterize("x","Sans",s,100000,100000).error.empty());
    s.y=.5;s.anchor=1;auto resized=rasterize("Resize","Sans",s,800,450);assert(resized.rgba.size()==800*450*4&&alpha(resized)>0);
    assert(!families().empty());std::string name;assert(!registerFont("/missing/font.ttf",name));
    std::cout<<"PASS: UTF-8 wrapping, shaped text, alpha, styles, resize and clipping\n";
}
