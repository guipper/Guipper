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
void TransitionSR::update() {
	lerpValue += 0.02;
	lerpValue = ofClamp(lerpValue, 0.0, 1.0);
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
	
	shader.setUniform1f("mixst", lerpValue);
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
	lerpValue = 0.;
}
void TransitionSR::reload() {
	shader.load("", "shaders/blending/mix.frag");
}
void TransitionSR::draw(float _x, float _y, float _w, float _h){
	ofSetColor(255, 255);
	ofSetRectMode(OF_RECTMODE_CORNER);


	//fbo1->draw(_x, _y, _w/2, _h);
	//fbo2->draw(_x+_w/2, _y, _w/2, _h);
	//ofSetRectMode(OF_RECTMODE_CORNER);
	//este.draw(ofGetWidth()/2, ofGetHeight()/2,300 ,300);
	este.draw(_x,_y,_w,_h);
//	ofDrawEllipse(ofGetWidth() / 2, ofGetHeight() / 2, 100, 100);
}
void TransitionSR::resize() {
	este.allocate(ofGetWidth(), ofGetHeight());
}

void TransitionSR::setFboPointer1(ofFbo * _fbo1) {
	fbo1 = _fbo1;
}

void TransitionSR::setFboPointer2(ofFbo* _fbo2) {
	fbo2 = _fbo2;
}