#include "jp_box_image.h"
#include "jp_box_cam.h"

JPbox_image::JPbox_image(){}
JPbox_image::~JPbox_image(){}
void JPbox_image::reload(){
	img.loadImage(dir);
}
void JPbox_image::setup(string _dir, string _nombre){
	//JPbox::setup(_font);
	JPbox::setup(_dir, _nombre);
	img.clear();
	img.loadImage(_dir);
		
	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");

	tipo = IMAGEBOX;

	if (img.isAllocated()) {
		cout << "CARGO BIEN LA IMAGEN" << endl;
	}
	else {

		cout << "CARGO COMO EL ORTO LA IMAGEN" << endl;
	}
	lasttime_autoreload = ofGetElapsedTimeMillis();
	duration_autoreload = 2000;

	//fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
}
void JPbox_image::update() {
	JPbox::update();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255, 255);
	
	//Esto es para que si no recargo, recargue bien carajo.
	if (!img.isAllocated() && ofGetElapsedTimeMillis()- lasttime_autoreload > duration_autoreload) {
		img.loadImage(dir);
		lasttime_autoreload = ofGetElapsedTimeMillis();
		cout << "RECARGA LA IMAGEN YA QUE LA CARGO COMO EL ORTO" << endl;
	}

	updateFBO();
}
void JPbox_image::updateFBO()
{

	if (onoff.boolValue) {
		float mscalex = ofMap(parameters.getFloatValue(0), 0.0, 1.0, 0.0, jp_constants::renderWidth);
		float mscaley = ofMap(parameters.getFloatValue(1), 0.0, 1.0, 0.0, jp_constants::renderHeight);
		float moffsetx = ofMap(parameters.getFloatValue(2), 0.0, 1.0,
			-jp_constants::renderWidth / 2 - mscalex / 2,
			jp_constants::renderWidth / 2 + mscalex / 2);

		float moffsety = ofMap(parameters.getFloatValue(3), 0.0, 1.0,
			-jp_constants::renderHeight / 2 - mscaley / 2,
			jp_constants::renderHeight / 2 + mscaley / 2);


		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();
		
		if (!parameters.getBoolValue(4)) {
			ofSetColor(0, 255);
			ofDrawRectangle(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
			ofSetColor(255, 255);
			img.draw(jp_constants::renderWidth / 2 - mscalex / 2 + moffsetx,
				jp_constants::renderHeight / 2 - mscaley / 2 + moffsety,
				mscalex,
				mscaley);
		}
		else {
			img.draw(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
		}
		fbo.end();
	}
	else {
		JPbox::updateFBO();
	}
}
void JPbox_image::draw(){
	ofSetRectMode(OF_RECTMODE_CORNER);
	//PARA QUE EL FBO FUNCIONE BIEN NECESITA OFRECTMODE CORNER CUANDO LEVANTA EL SHADER, ASÏ QUE LO PONEMOS ASI
	//shaderrender.fbo.draw(x- width/2, y-height/2, width, height); 
	ofSetColor(255);
	JPbox::draw();
	//fbo.draw(x - width / 2, y - height / 2, width, height);
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
	
}
void JPbox_image::clear(){
	JPbox::clear();
	img.clear();
	cout << "CORRE CLEAR SHADERBOX " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
