#pragma once
#include "jp_hand_tracking.h"

namespace jp_hand {
// Packed RGB on the capture worker. OF 0.12.1 leaves the destination unwritten
// for OF_INTERPOLATE_BILINEAR, so do not route detector frames through it.
inline Frame fitRgbFrame(const Frame &input, int maxWidth=960, int maxHeight=540) {
 Frame out=input;
 if(!input.valid()||maxWidth<=0||maxHeight<=0)return out;
 const float scale=std::min({1.f,float(maxWidth)/input.width,float(maxHeight)/input.height});
 if(scale==1)return out;
 out.width=std::max(1,int(input.width*scale));out.height=std::max(1,int(input.height*scale));
 out.rgb.resize(size_t(out.width)*out.height*3);
 for(int y=0;y<out.height;++y){
  const float sy=std::clamp((y+.5f)*input.height/out.height-.5f,0.f,float(input.height-1));
  const int y0=int(sy),y1=std::min(y0+1,input.height-1);const float fy=sy-y0;
  for(int x=0;x<out.width;++x){
   const float sx=std::clamp((x+.5f)*input.width/out.width-.5f,0.f,float(input.width-1));
   const int x0=int(sx),x1=std::min(x0+1,input.width-1);const float fx=sx-x0;
   for(int c=0;c<3;++c){
    auto sample=[&](int xx,int yy){return input.rgb[(size_t(yy)*input.width+xx)*3+c];};
    const float top=sample(x0,y0)*(1-fx)+sample(x1,y0)*fx;
    const float bottom=sample(x0,y1)*(1-fx)+sample(x1,y1)*fx;
    out.rgb[(size_t(y)*out.width+x)*3+c]=static_cast<unsigned char>(std::lround(top*(1-fy)+bottom*fy));
   }
  }
 }
 return out;
}
}
