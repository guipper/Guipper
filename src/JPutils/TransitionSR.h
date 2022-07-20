

#pragma once
#include "ofMain.h"
#include "jp_constants.h"
class TransitionSR {

public:
	TransitionSR();
	~TransitionSR();
	void setup();
	void setup(ofFbo * _fbo1, ofFbo * _fbo2);
	void update();
	void setLerpValue(float _val);
	void setLerpValue();
	void reload();
	void draw(float _x, float _y, float _w, float _h);
	void resize();
	void setFboPointer1(ofFbo* _fbo1);
	void setFboPointer2(ofFbo* _fbo2);
	void draw();

	ofShader shader;
	ofFbo dummyfbo;
	//void update(Shaderrender * _Sh, Shaderrender * _Sh2);
private:

	float x;
	float y;
	float w;
	float h;
	ofFbo * fbo1; //OBJETIVO
	ofFbo * fbo2; //JUGADOR
	ofFbo este;
	vector <float> uniformValues;
	vector <string > uniformNames;
	string dir;

	float lerpValue;
};