#pragma once
#include "jp_text_edit.h"
#include <functional>
namespace jp_text {
inline std::vector<std::string> wrap(const std::string &text,float width,const std::function<float(const std::string&)> &measure) {
    std::vector<std::string> lines;size_t start=0;
    while(start<=text.size()) {
        size_t end=start,space=std::string::npos;
        while(end<text.size()&&text[end]!='\n') {
            size_t next=jp_text_edit::next(text,end);
            if(end>start&&measure(text.substr(start,next-start))>width)break;
            if(text[end]==' '||text[end]=='\t')space=end;
            end=next;
        }
        if(end<text.size()&&text[end]!='\n'&&space!=std::string::npos&&space>start)end=space;
        lines.push_back(text.substr(start,end-start));
        if(end==text.size())break;
        start=(text[end]=='\n'||text[end]==' '||text[end]=='\t')?end+1:end;
    }
    return lines;
}
}
