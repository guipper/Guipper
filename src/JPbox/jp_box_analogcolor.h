#pragma once
#include "jp_box.h"
class JPbox_analogcolor : public JPbox {
public:
    using JPbox::setup;
    void setup(string directory,string label) override;
    void update() override;
    void updateFBO() override;
    void draw() override;
    void reload() override;
    void clear() override;
    bool ready()const{return shader.isLoaded();}
    void layoutInspector(float x,float y,float width);
    void drawInspector();
    bool clickInspector(float x,float y);
    float inspectorHeight()const{return 126;}
private:
    ofShader shader;
    ofRectangle palettes[3],status;
    float value(const char *name)const;
};
