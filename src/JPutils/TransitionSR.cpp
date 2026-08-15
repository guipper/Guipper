#include "TransitionSR.h"

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

	este.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
	este.begin();
	ofClear(0, 0, 0, 0);
	este.end();
}
void TransitionSR::advance() {
	// ofGetLastFrameTime() is 0 during setup and on the first frames, and a
	// transition armed there would sit at 0 forever rather than merely running
	// slowly. Fall back to a nominal frame so progress is always made - which
	// also reproduces the old per-frame behaviour exactly in that case.
	const float measured = (float)ofGetLastFrameTime();
	advance(measured > 0.0f ? measured : 1.0f / 60.0f);
}

void TransitionSR::advance(float deltaSeconds) {
	// Time-based, not per-frame. The old `lerpValue += 0.02` meant a fade took
	// 833ms at 60fps and 2s at 25fps: it stretched precisely when the machine
	// was already struggling, and no duration control could mean anything while
	// the unit was "frames".
	const float duration = std::max(1.0f, durationMs);
	const float dt = ofClamp(deltaSeconds, 0.0f, 0.1f);
	lerpValue += dt * 1000.0f / duration;
	lerpValue = ofClamp(lerpValue, 0.0, 1.0);
}

void TransitionSR::setDurationMs(float _durationMs) {
	durationMs = std::max(1.0f, _durationMs);
}

float TransitionSR::getDurationMs() const {
	return durationMs;
}

void TransitionSR::update() {
	advance();
	ofPushStyle();
	ofSetColor(255, 255);
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
const char *TransitionSR::typeLabel(int _type)
{
	switch (_type)
	{
		case TYPE_WARP:   return "warp";
		case TYPE_DITHER: return "dither";
		default:          return "mix";
	}
}

void TransitionSR::setType(int _type)
{
	const int next = ofClamp(_type, 0, TYPE_COUNT - 1);
	if (next == transitionType) return;
	transitionType = next;
	// Force ensureShader to reload on the next draw. Unloading rather than
	// loading here keeps every shader load on the draw thread, where a GL
	// context is guaranteed - setType can be called from settings load.
	shader.unload();
}

int TransitionSR::getType() const
{
	return transitionType;
}

bool TransitionSR::ensureShader()
{
	if (shader.isLoaded()) return true;
	const char *fragment = "shaders/private/mix.frag";
	if (transitionType == TYPE_WARP)
		fragment = "shaders/private/transition_warp.frag";
	else if (transitionType == TYPE_DITHER)
		fragment = "shaders/private/transition_dither.frag";
	if (shader.load("shaders/default.vert", fragment)) return true;
	// A broken or missing transition shader must not take the whole output
	// with it: fall back to the one that has always been there.
	ofLogError("TransitionSR") << "failed to load " << fragment
		<< " - falling back to mix";
	return shader.load("shaders/default.vert", "shaders/private/mix.frag");
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
	ofDrawRectangle(0, 0, width, height);
	shader.end();
	return true;
}
void TransitionSR::setLerpValue(float _val) {
	lerpValue = ofClamp(_val, 0.0f, 1.0f);
}
void TransitionSR::reload() {
	shader.load("", "shaders/blending/mix.frag");
}
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
