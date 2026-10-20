#pragma once
#include "ofMain.h"
#include "../JPgui/jp_gl_state.h"
// Synthetic visual fixture: color/grid surface for image-effect screenshots.
namespace jp_image_fixture {
inline void paint(ofFbo &image){
    jp_gl::ScopedNoScissor clip;ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);ofDisableBlendMode();
    const float w=image.getWidth(),h=image.getHeight();
    image.begin();ofClear(0,0,0,0);
    for(int y=0;y<12;++y)for(int x=0;x<20;++x){ofSetColor(20+x*3,35+y*6,100+x*5);ofDrawRectangle(x*w/20,y*h/12,w/20+1,h/12+1);}
    ofSetLineWidth(2);ofSetColor(60,180,210);for(int i=1;i<20;++i)ofDrawLine(i*w/20,0,i*w/20,h);for(int i=1;i<12;++i)ofDrawLine(0,i*h/12,w,i*h/12);
    ofSetColor(249,178,47);ofDrawCircle(w*.5,h*.5,h*.26);
    ofSetColor(245,90,110);ofDrawRectangle(w*.18,h*.25,w*.12,h*.5);
    ofSetColor(74,224,178);ofDrawRectangle(w*.72,h*.2,w*.1,h*.6);
    ofSetColor(250);ofDrawCircle(w*.5,h*.5,h*.045);
    ofSetColor(0,0,0,0);ofDrawRectangle(0,0,w*.04,h*.04);image.end();
    ofPopStyle();
}
}
