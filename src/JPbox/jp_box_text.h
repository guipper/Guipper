#pragma once
#include "jp_box.h"
#include "../JPutils/jp_text_render.h"

class JPbox_text : public JPbox {
public:
    std::string draft="Tu texto", published="Tu texto", family="Sans", fontFile;
    bool live=true, focused=false;
    int alignment=1, anchor=1;
    std::string status;
    unsigned renderCount=0;
    using JPbox::setup;
    void setup(string directory,string label) override;
    void update() override;
    void draw() override;
    void clear() override;
    void setCaptureActive(bool active) override {if(!active)focused=filterFocused=false;}
    void saveCustomState(ofXml &node) const override;
    void loadCustomState(const ofXml &node) override;
    void copyCustomStateFrom(const JPbox *source) override;
    void applyText();
    void setDraft(const std::string &value);
    std::string snapshot() const;
    void restore(const std::string &value);
    bool importFont(const std::string &path);
    float editorHeight() const {return choosingFont?586:366;}
    void layoutEditor(float left,float top,float width);
    void drawEditor(const std::function<void(const std::string&,const std::string&)> &record);
    bool clickEditor(float x,float y);
private:
    ofTexture texture;
    std::string signature, editBefore;
    bool choosingFont=false,filterFocused=false;
    std::string fontFilter;
    int fontPage=0;
    std::vector<std::string> fontNames;
    std::vector<std::pair<ofRectangle,std::string>> fontRows;
    ofRectangle editor,modeLive,modeApply,applyButton,discardButton,fontButton,importButton,filterField,fontPrev,fontNext,alignButtons[3],anchorButtons[3],messageRect;
    std::function<void(const std::string&,const std::string&)> recordEdit;
    jp_text::Style style();
    void drawFallback(const jp_text::Style &style);
};
