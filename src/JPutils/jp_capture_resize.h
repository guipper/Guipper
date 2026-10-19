#pragma once
#include <algorithm>
#include <cstdint>

namespace jp_screen {
// A quiet static window is valid. Retry only after an observed size change,
// once per change, and disarm once frames have flowed for two seconds.
class ResizeRecovery {
    int width=0,height=0;
    int64_t changedAt=0,lastFrame=0;
    bool armed=false;
public:
    void size(int w,int h,int64_t now) {
        if(w<=0 || h<=0 || (w==width && h==height))return;
        armed=width>0; width=w;height=h;changedAt=now;
    }
    void frame(int64_t now) {
        lastFrame=now;
        if(armed && now-changedAt>=2000)armed=false;
    }
    bool reconnect(int64_t now) {
        if(!armed || now-std::max(changedAt,lastFrame)<2000)return false;
        armed=false;return true;
    }
};
}
