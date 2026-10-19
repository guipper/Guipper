#pragma once
#include "jp_box.h"
#include "../JPutils/jp_screen_capture.h"

class JPbox_screen : public JPbox {
    jp_screen::Capture capture;
    ofTexture texture;
    std::shared_ptr<const jp_screen::Frame> uploaded;
    bool retained = false;
public:
    bool choosingMonitor = false;
    std::vector<jp_screen::Monitor> monitorChoices;
    JPbox_screen() : capture([this](const std::string &id) {
        return windowMode() ? jp_screen::makeWindowBackend(id) : jp_screen::makeBackend(id);
    }) {}
    explicit JPbox_screen(jp_screen::Factory factory) : capture(std::move(factory)) {}
    ~JPbox_screen() override { capture.stop(); }
    using JPbox::setup;
    void setup(string directory, string name) override;
    void update() override;
    void updateFBO() override;
    void draw() override;
    void clear() override;
    void setCaptureActive(bool active) override;
    bool windowMode() const { return parameters.parameters.size()>6 && parameters.parameters[6]->boolValue; }
    void setWindowMode(bool window);
    void selectScreen();
    void selectMonitor(size_t index);
    void stopCapture();
    bool transitionReady() const { return uploaded != nullptr; }
    jp_screen::Snapshot captureStatus() const { return capture.snapshot(); }
    string statusLabel() const;
};
