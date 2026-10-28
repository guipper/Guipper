#pragma once
#include "jp_box.h"
#include "jp_box_kinect2.h"
#include "jp_kinect_particles.h"

class JPbox_kinectparticles : public JPbox {
public:
    ~JPbox_kinectparticles() override { clear(); }
    void setup(string directory,string name) override;
    void update() override;
    void updateFBO() override;
    void draw() override;
    void clear() override;
    void setCaptureActive(bool active) override;
    string getCaptureStatus() const;
    vector<string> inspectorChoiceLabels() const override { return {"Encuadre","Superficie","Flotantes"}; }
    int inspectorChoice() const override { return inspectorSection; }
    void setInspectorChoice(int choice) override { inspectorSection=std::clamp(choice,0,2); }
    string inspectorChoiceStatus() const override;
    bool usesDepthCapture() const override { return true; }
    bool inspectorParameterVisible(const string& parameter) const;
    string inspectorParameterLabel(const string& parameter) const;
    // Diagnostic/test injection uses the identical depth path; never persists.
    void submitDepth(jp_particles::Engine::Depth frame, int width, int height,
                     double time, jp_particles::Intrinsics intrinsics);
    void advanceSimulation(double now, float dt);
    size_t particleCount() const { return simulation.particles.size(); }
    bool shaderReady() const { return shader.isLoaded(); }
private:
    enum { DENSITY, SIZE, LIFE, TURBULENCE, DRIFT, SPREAD, DELAY, STRIPES,
           SILHOUETTE, NEAR_M, FAR_M, ZOOM, PALETTE, BRIGHTNESS, MIRROR, ADDITIVE, DETAIL };
    void ensureTarget();
    std::shared_ptr<KinectV2CaptureSource> capture;
    jp_particles::Engine simulation;
    ofVboMesh mesh;
    ofShader shader;
    uint64_t version=0;
    double lastUpdate=0;
    bool captureAllowed=true, cleared=false;
    int targetWidth=0,targetHeight=0;
    float depthFocalY=365;
    int inspectorSection=1;
};
