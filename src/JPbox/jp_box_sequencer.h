#pragma once

#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_shader.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"

class JPbox_sequencer : public JPbox
{
public:
    JPbox_sequencer();
    ~JPbox_sequencer();

    void setup(string _directory, string _name);
    void update();
    void updateFBO();
    void draw();
    void clear();
    void setPos(float _x, float _y)
    {
        JPdragobject::setPos(_x, _y);
    }

    // Slot management
    void addSlot();
    void removeSlot();

    // Sequencer controls
    int getCurrentSlot() const { return currentSlot; }
    void setCurrentSlot(int index);

    // Handle clicks on +, -, or slots. Returns true if handled.
    bool handleClick(float mx, float my);

    ofShader shader;
    int frameNum;
    vector<JPbox *> slots;

private:
    int currentSlot;
    float transition;
    static const int MAX_SLOTS = 8;

    // Layout constants
    float slotPreviewSize;
    float btnSize;
    float slotGap;

    // Button hit rects (set during draw, used by handleClick)
    ofRectangle plusBtnRect;
    ofRectangle minusBtnRect;

    // Slot hover
    int hoveredSlot;
    uint64_t hoverStartMillis;

    void update_globalUniforms();
    void update_sequencerUniforms();
};
