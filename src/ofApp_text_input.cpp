#include "ofApp.h"
#include "JPutils/jp_text_input.h"

void ofApp::drawOptionsTextInput(int index,const ofRectangle& bounds) {
    auto f=jp_text_input::field("settings-"+ofToString(index),"settings",bounds,font_p,optionsFieldText[index],focusedOptionsField==index);
    f.focus=[this,index]{focusedOptionsField=index;};
    f.cancel=[this]{focusedOptionsField=-1;initOptionsFields();};
    f.commit=[this]{applyOptionsField();return focusedOptionsField<0;};
    f.validate=[this,index](const string& value)->string {
        if(index==FIELD_DEFAULT_COMPO)return {};
        if(index==FIELD_OSC_IP_OUT)return value.empty()?jp_text_input::message("Enter a host","Ingresá un host"):"";
        if(index==FIELD_OSC_PORT_IN || index==FIELD_OSC_PORT_OUT)return jp_text_input::integer(value,0,65535);
        if(index==FIELD_BPM)return jp_text_input::integer(value,0,2147483647);
        return jp_text_input::integer(value,1,16384);
    };
    jp_text_input::controller().add(std::move(f));
}
void ofApp::drawOutputTextInput(int index,const ofRectangle& bounds,bool split) {
    auto f=jp_text_input::field((split?"split-":"output-")+ofToString(index),"outputs",bounds,font_p,
        split?splitFieldText[index]:liveOutputFieldText[index],(split?focusedSplitField:focusedLiveOutputField)==index);
    f.focus=[this,index,split]{if(split)focusedSplitField=index;else focusedLiveOutputField=index;};
    f.cancel=[this,split]{if(split)focusedSplitField=-1;else {focusedLiveOutputField=-1;initLiveOutputFields();}};
    f.commit=[this,split]{if(split)applySplitField();else applyLiveOutputField();return true;};
    f.validate=[this,index,split](const string& v)->string {
        if(split)return jp_text_input::integer(v,1,16);
        if(index==LO_FIELD_WIDTH || index==LO_FIELD_HEIGHT)return jp_text_input::integer(v,64,16384);
        if(wallMode==WALL_MODE_SPATIAL)return jp_text_input::integer(v,(index==LO_FIELD_CROP_X||index==LO_FIELD_CROP_Y)?-2147483647:0,2147483647);
        if(selectedLiveOutput<0 || selectedLiveOutput>=(int)liveOutputs.size())return jp_text_input::message("Select an output","Elegí una salida");
        const auto& config=liveOutputs[selectedLiveOutput].config;
        const auto canvas=getLiveOutputCanvasSize(config);
        int low=0,high=0;
        if(index==LO_FIELD_CROP_W){low=1;high=canvas.x;}
        if(index==LO_FIELD_CROP_H){low=1;high=canvas.y;}
        if(index==LO_FIELD_CROP_X)high=std::max(0,(int)std::lround(canvas.x*(1-config.cropW)));
        if(index==LO_FIELD_CROP_Y)high=std::max(0,(int)std::lround(canvas.y*(1-config.cropH)));
        if(index==LO_FIELD_BEZEL)high=std::max(0,(int)(std::min(config.cropW*canvas.x,config.cropH*canvas.y)/2)-1);
        return jp_text_input::integer(v,low,high);
    };
    jp_text_input::controller().add(std::move(f));
}
