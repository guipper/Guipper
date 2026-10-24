#pragma once
#include "jp_box.h"
#include "jp_box_kinect2.h"
#include <deque>

// Indexed GPU feedback: one persistent particle for each resampled depth pixel.
class JPbox_sarv : public JPbox {
public:
    ~JPbox_sarv() override { clear(); }
    void setup(string directory,string name) override;
    void update() override;
    void updateFBO() override;
    void draw() override;
    void clear() override;
    void setCaptureActive(bool active) override;
    vector<string> inspectorChoiceLabels() const override { return {"SPHERES","CUBES","POINTS"}; }
    int inspectorChoice() const override;
    void setInspectorChoice(int choice) override;
    string inspectorChoiceStatus() const override;
private:
    void allocate();
    void buildSource(double now);
    void step(double now);
    shared_ptr<KinectV2CaptureSource> capture;
    std::deque<JPbox_kinect2::MonoFrame> history;
    JPbox_kinect2::MonoFrame latest;
    JPbox_kinect2::DepthIntrinsics intrinsics;
    ofFloatPixels pixels;
    ofTexture source;
    ofFbo state[2];
    ofShader evolve,render;
    ofVboMesh indices,surface;
    uint64_t version=0;
    int current=0,sw=0,sh=0;
    double previous=0,accumulator=0,lastDepth=0,simulationTime=0;
    bool active=true,initialized=false,cleared=false;
};
