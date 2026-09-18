#include "TransitionSR.h"
#include "jp_app_paths.h"

TransitionSR::TransitionSR()
	: fbo1(nullptr), fbo2(nullptr), lerpValue(1.0f) {}
TransitionSR::~TransitionSR(){

}
void TransitionSR::setup() {
	dummyfbo.allocate(100, 100);
	dummyfbo.begin();
	ofClear(0, 0, 0, 0);
	dummyfbo.end();


	fbo1 = &dummyfbo;
	fbo2 = &dummyfbo;
	lerpValue = 1.0f;
	//dir = "shaders/blending/mix.frag";
	ensureShader();
	//este.allocate(ofGetWidth(), ofGetHeight());
	este.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
	este.begin();
	ofClear(0, 0, 0, 0);
	este.end();

	cout << "CARGA EL SHADER TRANSITION " << endl;
}
void TransitionSR::setup(ofFbo * _fbo1, ofFbo * _fbo2){

	fbo1 = _fbo1;
	fbo2 = _fbo2;
	lerpValue = 1.0f;
	//dir = "shaders/blending/mix.frag";
	ensureShader();
	//este.allocate(ofGetWidth(), ofGetHeight());

	este.allocate(_fbo1 && _fbo1->isAllocated() ? _fbo1->getWidth() : jp_constants::renderWidth,
        _fbo1 && _fbo1->isAllocated() ? _fbo1->getHeight() : jp_constants::renderHeight);
	este.begin();
	ofClear(0, 0, 0, 0);
	este.end();
}
jp_transition::Config &TransitionSR::preferences() {
    static jp_transition::Config config;
    return config;
}
void TransitionSR::advance() {
    if (!armed) return;
    ensureShader();
    const double now = ofGetElapsedTimef();
    if (timeline.phase() == jp_transition::Phase::Preparing)
        timeline.ready(now, jp_constants::bpm, jp_constants::beatOriginSeconds);
    timeline.tick(now, jp_constants::bpm, jp_constants::beatOriginSeconds);
    lerpValue = timeline.progress();
}
void TransitionSR::advance(float deltaSeconds) {
    if (!armed) return;
    if (timeline.phase() == jp_transition::Phase::Preparing)
        timeline.ready(explicitClock, jp_constants::bpm, 0.);
    timeline.tick(explicitClock);
    explicitClock += std::max(0.f, deltaSeconds);
    timeline.tick(explicitClock);
    lerpValue = timeline.progress();
}

void TransitionSR::setDurationMs(float _durationMs) {
	durationMs = std::max(1.0f, _durationMs);
}

float TransitionSR::getDurationMs() const {
	return durationMs;
}

void TransitionSR::update(bool advanceClock) {
	if(advanceClock) advance();
	ofPushStyle();
	ofSetColor(255, 255);
	// Rect mode is global and this runs during UPDATE, so it inherits whatever
	// the previous frame's draw happened to leave - and JPbox::draw leaves
	// OF_RECTMODE_CENTER. Both draws below place a rectangle at (0,0) with the
	// FBO's full size, which under CENTER lands three quarters outside it: the
	// crossfade ends up painted into the top-left quadrant of `este` alone, and
	// the live output shows a quarter-filled screen for as long as the fade
	// lasts. Invisible the rest of the time, because a finished transition is
	// never the thing drawn - which is exactly why it only ever showed up right
	// after an undo, a group or an ungroup, the three moments that arm one.
	//
	// ofPushStyle covers rectMode, so this is restored on the way out.
	ofSetRectMode(OF_RECTMODE_CORNER);
	este.begin();
	// Transparent pixels must replace the previous frame. Blending a new
	// transparent frame over the old transition canvas leaves position trails.
	ofClear(0, 0, 0, 0);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	// Smoothstep keeps the crossfade gentle at both ends of the transition.
	float easedLerpValue = lerpValue * lerpValue * (3.0f - 2.0f * lerpValue);
	if (!renderStraightMix(fbo1, fbo2, easedLerpValue,
		este.getWidth(), este.getHeight()) && fbo2 != nullptr)
	{
		fbo2->draw(0, 0, este.getWidth(), este.getHeight());
	}
	este.end();
	ofEnableAlphaBlending();
	ofPopStyle();

	/*lerpValue += 0.02;
	lerpValue = ofClamp(lerpValue, 0.0, 1.0);
	ofSetColor(255, 255);
	este.begin();
	ofSetColor(255, 200, 100);
	ofRect(0, 0, este.getWidth(), este.getHeight());

	este.end();*/
}
const char *TransitionSR::typeLabel(int type) {
    return jp_transition::catalog[std::clamp(type, 0, TYPE_COUNT-1)].en;
}
void TransitionSR::setType(int type) {
    transitionType = std::clamp(type, 0, TYPE_COUNT-1);
}

int TransitionSR::getType() const
{
	return transitionType;
}

bool TransitionSR::ensureShader() {
    if (shader.isLoaded() && shader.getShader(GL_VERTEX_SHADER) != 0 &&
        shader.getShader(GL_FRAGMENT_SHADER) != 0) return true;

    // Internal rendering resources belong to this executable. Development
    // profiles are migrated once, so their library can lack newly shipped files.
    const auto &bundle = jp::AppPaths::current().bundle;
    const std::filesystem::path root = bundle.empty() ?
        std::filesystem::path(ofToDataPath("", true)) : bundle;
    auto loadCompleteProgram = [&](const char *fragment) {
        shader.unload();
        // ofShader::load() ignores setupShaderFromFile failures and may link a
        // vertex-only program successfully. That program produces no valid mix.
        if (!shader.setupShaderFromFile(GL_VERTEX_SHADER, root / "shaders/default.vert") ||
            !shader.setupShaderFromFile(GL_FRAGMENT_SHADER, root / fragment)) {
            shader.unload();
            return false;
        }
        shader.bindDefaults();
        if (shader.linkProgram()) return true;
        shader.unload();
        return false;
    };
    if (loadCompleteProgram("shaders/private/transition_catalog.frag")) return true;
    ofLogError("TransitionSR") << "Transition catalog unavailable; falling back to crossfade";
    return loadCompleteProgram("shaders/private/mix.frag");
}

bool TransitionSR::renderStraightMix(ofFbo *first, ofFbo *second,
	float mixValue, float width, float height)
{
	if (first == nullptr || second == nullptr || !first->isAllocated() ||
		!second->isAllocated() || width <= 0.0f || height <= 0.0f ||
		!ensureShader())
	{
		return false;
	}
	shader.begin();
	shader.setUniformTexture("textura1", *first, 1);
	shader.setUniformTexture("textura2", *second, 2);
	shader.setUniform1f("mixst", ofClamp(mixValue, 0.0f, 1.0f));
	shader.setUniform2f("resolution", width, height);
    const auto &config = armed ? timeline.config() : preferences();
    shader.setUniform1i("transitionEffect", armed ? timeline.effect() : transitionType);
    shader.setUniform1i("transitionDirection", armed ? timeline.direction() : config.direction);
    shader.setUniform1f("transitionSeed", float(timeline.seed() % 65536));
    shader.setUniform2f("transitionCenter", config.centerX, config.centerY);
    shader.setUniform1f("edgeSoftness", config.softness);
    shader.setUniform1f("plasmaScale", config.plasmaScale);
    shader.setUniform1f("warpIntensity", config.intensity);
	ofPushStyle();
    ofFill();
    ofSetRectMode(OF_RECTMODE_CORNER);
	ofDrawRectangle(0, 0, width, height);
    ofPopStyle();
	shader.end();
    // Inputs are render targets again on the next frame. Release the sampler
    // bindings rather than leave scene textures attached to inactive units.
    for (int unit : {1, 2}) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
	return true;
}
void TransitionSR::setLerpValue(float value) {
    lerpValue = ofClamp(value, 0.f, 1.f);
    if (value <= 0.f) {
        static uint32_t serial = 0;
        auto config = preferences();
        config.effect = transitionType;
        config.duration = durationMs / 1000.;
        timeline.request(config, ofGetElapsedTimef(), ++serial * 2654435761u, capabilities);
        armed = true; outgoingFrozen = false;
        explicitClock = 0.;
        // Publish A immediately: another request in this same event frame must
        // capture what is visible, not an old/uninitialized compositor buffer.
        if(fbo1 && fbo1->isAllocated() && este.isAllocated() && fbo1!=&este) {
            este.begin();ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);
            ofEnableBlendMode(OF_BLENDMODE_DISABLED);ofSetColor(255);ofClear(0,0,0,0);
            fbo1->draw(0,0,este.getWidth(),este.getHeight());ofPopStyle();este.end();
        }
    } else if (value >= 1.f) {
        timeline.cancel(); armed = false;
        interruptedFrame.clear();
    }
}
void TransitionSR::captureInterruption() {
    if (lerpValue >= 1.f || !este.isAllocated()) return;
    interruptedFrame.allocate(este.getWidth(), este.getHeight(), GL_RGBA);
    interruptedFrame.begin();
    ofPushStyle(); ofSetRectMode(OF_RECTMODE_CORNER);
    ofEnableBlendMode(OF_BLENDMODE_DISABLED); ofSetColor(255);
    ofClear(0,0,0,0); este.draw(0,0); ofPopStyle();
    interruptedFrame.end();
    fbo1 = &interruptedFrame;
}
void TransitionSR::freezeOutgoing() {
    if(outgoingFrozen || !fbo1 || !fbo1->isAllocated()) return;
    ofFbo frozen; frozen.allocate(fbo1->getWidth(),fbo1->getHeight(),GL_RGBA);
    frozen.begin(); ofPushStyle(); ofSetRectMode(OF_RECTMODE_CORNER);
    ofEnableBlendMode(OF_BLENDMODE_DISABLED); ofSetColor(255);
    ofClear(0,0,0,0); fbo1->draw(0,0); ofPopStyle(); frozen.end();
    interruptedFrame=std::move(frozen); fbo1=&interruptedFrame; outgoingFrozen=true;
}
void TransitionSR::reload() { shader.unload(); }
void TransitionSR::draw(float _x, float _y, float _w, float _h){
	if (!este.isAllocated()) return;
	drawSubsection(_x, _y, _w, _h,
		0.0f, 0.0f, este.getWidth(), este.getHeight());
}
void TransitionSR::drawSubsection(float _x, float _y, float _w, float _h,
	float _sx, float _sy, float _sw, float _sh){
	// ofFbo::draw guards on allocation for us; going through the texture does
	// not, so guard here. getTexture() still resolves MSAA, so this is
	// equivalent to draw(), not a shortcut past it.
	if (!este.isAllocated()) return;
	ofSetColor(255, 255);
	ofSetRectMode(OF_RECTMODE_CORNER);
	este.getTexture().drawSubsection(_x, _y, _w, _h, _sx, _sy, _sw, _sh);
}
bool TransitionSR::isSourceAllocated() const {
	return este.isAllocated();
}
float TransitionSR::getSourceWidth() const {
	return este.getWidth();
}
float TransitionSR::getSourceHeight() const {
	return este.getHeight();
}

void TransitionSR::setFboPointer1(ofFbo * _fbo1) {
	fbo1 = _fbo1;
}

void TransitionSR::setFboPointer2(ofFbo* _fbo2) {
	fbo2 = _fbo2;
}

float TransitionSR::getLerpValue() const {
	return lerpValue;
}
