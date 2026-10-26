#pragma once
#include "jp_box.h"

// A transparent moving canvas. Runtime phase is intentionally not persisted.
class JPbox_dvd : public JPbox {
public:
    using JPbox::setup;
    void setup(string directory, string label) override;
    void update() override;
    void updateFBO() override;
    void draw() override;
private:
    double phaseX=.25, phaseY=.35, lastTime=0;
    float value(const char* name) const;
};
