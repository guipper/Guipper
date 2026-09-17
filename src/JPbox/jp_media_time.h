#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace jp_media_time {
// Accept seconds, mm:ss, hh:mm:ss, or an integral frame index followed by f.
// Conversion is atomic: an invalid draft never changes the caller's value.
inline bool parse(const std::string& text,double duration,int frames,float& normalized) {
    try {
        const auto first=text.find_first_not_of(" \t\r\n");
        if(first==std::string::npos)return false;
        const auto value=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
        auto number=[](const std::string& part) {
            size_t end=0;const double n=std::stod(part,&end);
            if(end!=part.size()||!std::isfinite(n)||n<0)throw std::invalid_argument("time");
            return n;
        };
        if(value.back()=='f'||value.back()=='F') {
            const auto digits=value.substr(0,value.size()-1);
            if(digits.empty()||digits.find_first_not_of("0123456789")!=std::string::npos)return false;
            const double frame=number(digits);
            if(frame>std::max(0,frames-1))return false;
            normalized=frames>1?frame/(frames-1):0;return true;
        }
        double seconds=0;size_t start=0;int parts=0;
        for(;;) {
            const auto end=value.find(':',start);
            const auto part=value.substr(start,end==std::string::npos?end:end-start);
            const double n=number(part);
            if(++parts>3 || (parts>1&&n>=60))return false;
            seconds=seconds*60+n;
            if(end==std::string::npos)break;
            start=end+1;
        }
        if(seconds>duration)return false;
        normalized=duration>0?seconds/duration:0;return true;
    }catch(...) {return false;}
}
inline bool stepFrames(std::string& draft,double duration,int frames,int direction) {
    float position=0;
    if(frames<=1 || !parse(draft,duration,frames,position))return false;
    const int frame=std::clamp((int)std::lround(position*(frames-1))+(direction<0?-1:1),0,frames-1);
    draft=std::to_string(frame)+"f";return true;
}

}
