#include "TransitionSR.h"

TransitionSR::TransitionSR(){

}
TransitionSR::~TransitionSR(){

}
void TransitionSR::setup() {
	dummyfbo.allocate(100, 100);
	
	ofSetColor(0, 0);
	dummyfbo.begin();
	dummyfbo.end();


	fbo1 = &dummyfbo;
	fbo2 = &dummyfbo;
	//dir = "shaders/blending/mix.frag";
	shader.load("shaders/default.vert", "shaders/private/mix.frag");
	//este.allocate(ofGetWidth(), ofGetHeight());
	este.allocate(jp_constants::renderWidth, jp_constants::renderHeight);

	cout << "CARGA EL SHADER TRANSITION " << endl;
}
void TransitionSR::setup(ofFbo * _fbo1, ofFbo * _fbo2){

	fbo1 = _fbo1;
	fbo2 = _fbo2;
	//dir = "shaders/blending/mix.frag";
	//shader.load("shaders/default.vert", "shaders/private/mix.frag");
	//este.allocate(ofGetWidth(), ofGetHeight());




	este.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
	este.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
}
void TransitionSR::advance() {
	lerpValue += 0.02;
	lerpValue = ofClamp(lerpValue, 0.0, 1.0);
}

void TransitionSR::update() {
	advance();
	ofSetColor(255, 255);
	este.begin();
	shader.begin();
	

	//Pareciera que le gusto esta forma de evaluar si un puntero esta vacio o no :
	if (fbo1 != 0) {
		shader.setUniformTexture("textura1", *fbo1, 1);
	}
	if (fbo2 != 0) {
		shader.setUniformTexture("textura2", *fbo2, 2);
	}
	
	// Smoothstep keeps the crossfade gentle at both ends of the transition.
	float easedLerpValue = lerpValue * lerpValue * (3.0f - 2.0f * lerpValue);
	shader.setUniform1f("mixst", easedLerpValue);
	shader.setUniform2f("resolution", este.getWidth(), este.getHeight());
	ofRect(0, 0, este.getWidth(), este.getHeight());
	shader.end();
	este.end();

	/*lerpValue += 0.02;
	lerpValue = ofClamp(lerpValue, 0.0, 1.0);
	ofSetColor(255, 255);
	este.begin();
	ofSetColor(255, 200, 100);
	ofRect(0, 0, este.getWidth(), este.getHeight());

	este.end();*/
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
