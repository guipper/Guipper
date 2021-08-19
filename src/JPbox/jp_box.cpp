#include "jp_box.h"
#include "../ofApp.h"

JPbox::JPbox(){}
JPbox::~JPbox(){}
void JPbox::reloadShaderonly(){}
void JPbox::reload(){}
void JPbox::setup(ofTrueTypeFont & _font) {
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2, 
		fbowidth + padding_leftright,
		fboheight + padding_top + padding_bottom);

	Cfront = ofColor(0,120);
	border = ofColor(0, 200, 200,120);
	border_mouseover = ofColor(0, 200, 200, 255);
	border_grab = ofColor(0, 255, 0, 255);

	//font_p = &_font;
	name = "Prueba";

	outlet_x = x + width / 2;
	outlet_y = y;
	outlet_size = 30;

	inlet_size = 20;

	onoff.setup(outlet_x, outlet_y, outlet_size/2, outlet_size/2);
	onoff.boolValue = false;
	fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
}
void JPbox::setup(string _directory,string _name) {
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2,
		fbowidth + padding_leftright,
		fboheight + padding_top + padding_bottom);

	Cfront = ofColor(0, 120);
	border = ofColor(0, 200, 200, 120);
	border_mouseover = ofColor(0, 200, 200, 255);
	border_grab = ofColor(0, 255, 0, 255);

	outlet_x = x + width / 2;
	outlet_y = y;
	outlet_size = 30;

	inlet_size = 20;

	onoff.setup(outlet_x, outlet_y, outlet_size / 2, outlet_size / 2);
	onoff.boolValue = false;
	fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);

	name = _name;
	dir  = _directory;
}
void JPbox::update(){
	parameters.update();
	//onoff.update();
	onoff.setPos(x+width/2- outlet_size / 2,
				 y-height/2+ outlet_size *.4);
	//updateFBO();

	outlet_x = x + width / 2 - outlet_size / 2;
	outlet_y = y;
}
void JPbox::draw() {
	
	
	ofSetColor(Cfront);
	ofNoFill();

	if(mouseOver() || activeFlag){
		if (activeFlag) {
			ofSetColor(border_grab);
		}
		else {
			ofSetColor(border_mouseover);
		}
	}
	else {
		ofSetColor(border);
	}

	if (!ofGetMousePressed()) {
		activeFlag = false;
		outletActiveFlag = false;
	}

	//CAJA GRIS:
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofSetLineWidth(3);
	ofSetColor(0);
	ofRectRounded(x, y, width, height, 10);
	ofSetColor(200);
	ofFill();
	ofRectRounded(x, y, width, height, 10);

	ofSetColor(Cfront);
	ofSetColor(0);
	
	float sepsize = 10; //SEPARACION ENTRE LA LINEA Y LA CAJA Y LA ALINEACION DEL TEXTO.
	float linewidth = width/2- sepsize;
	float lineheight = 2;

	//LINEA DEBAJO DEL TEXTO : 
	ofSetLineWidth(lineheight);
	ofDrawLine(x - linewidth, y - height / 2 + padding_top * 2 / 3+ lineheight
		     , x + linewidth, y - height / 2 + padding_top * 2 / 3+ lineheight);
	
	
	//TEXTO : 
	string shortname = name;
	int numberofdisplayletter = 6;
	if (name.size() > numberofdisplayletter) {
		shortname = shortname.substr(0, numberofdisplayletter);
		shortname += "...";
	}

	jp_constants::p_font.drawString(shortname,
		               x - width/2 + sepsize,
					   y - height/2 + padding_top*2/3);

	//BOTON SET ACTIVE RENDER : 

	
	//DIBUJAR CABLECITO.
	ofSetColor(255);
	
	if(outletActiveFlag){
		ofDrawLine(outlet_x, outlet_y, ofGetMouseX(), ofGetMouseY());
	}
	//JPbox::draw_outlet();
	
	ofSetRectMode(OF_RECTMODE_CENTER);
	onoff.draw();
	ofSetColor(255, 255, 255, 255);
}
void JPbox::updateFBO() {
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();
	
	ofSetColor(255,255);
	//ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
	fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	//ofSetColor(255, 0, 0);
	//ofNoFill();
	//ofDrawEllipse(fbo.getWidth() / 2, fbo.getHeight() / 2,500,500);
	//ofFill();
	fbo.end();
}
void JPbox::draw_outlet() {
	float bordersizemult =0.6;

	float trianglesize = outlet_size*5.0; //ESTO LO MODIFICO ACA PARA NO MODIFICAR EL DRAGOBJECT
	float spheresize = outlet_size * 1.0;

	float bordertrianglesize = trianglesize * (1.0 + bordersizemult);
	float borderoffsetx = trianglesize * (bordersizemult / 2);

	float xtri = outlet_size / 2;
	float ytri = 0;

	ofSetLineWidth(3);
	
	
	
	//DIBUJAR TRIANGULO BORDE: 
	/*ofPushMatrix();
	ofTranslate(outlet_x + trianglesize / 2, outlet_y);
	ofRotate(ofRadToDeg(triangleangle));

	ofSetColor(0);
	ofTranslate(-trianglesize / 2, -trianglesize / 2);
	ofTranslate( (trianglesize / 2)*0.6, trianglesize / 2);
	ofScale(1.08);
	ofTranslate(-trianglesize / 2 * 0.6, -trianglesize / 2);
	ofNoFill();
	ofDrawTriangle(xtri, ytri,
		outlet_size + xtri, outlet_size / 2 + ytri,
		0 + xtri, outlet_size + ytri);
	//ofDrawEllipse(0, 0, spheresize, spheresize);
	ofPopMatrix();*/

	ofFill();
	
	//DIBUJAR TRIANGULO EXTERNO
	ofPushMatrix();
	ofTranslate(outlet_x+ outlet_size/2, outlet_y);
	ofRotate(ofRadToDeg(triangleangle) + 90);
	if (mouseOverOutlet()) {
		ofSetColor(255, 217, 15, 255);
	}
	else {
		ofSetColor(255, 217, 15, 250);
	}
	//ofTranslate(-outlet_size / 2, -outlet_size / 2);
	/*ofDrawTriangle(xtri, ytri,
		outlet_size+ xtri, outlet_size/2+ytri,
		0+ xtri, outlet_size+ ytri);*/
	
	//ofDrawEllipse(outlet_size / 2, outlet_size / 2, spheresize, spheresize);

	float gotasize =0.2;
	jp_constants_img::outlet_img.draw(0, 0, 
		jp_constants_img::outlet_img.getWidth()*gotasize,
		jp_constants_img::outlet_img.getHeight()*gotasize);
	
	ofPopMatrix();

	ofFill();
	ofSetColor(255, 255, 255, 255);
	}
void JPbox::clear(){
	cout << "JP_BOX clear" << endl;
	parameters.clear();
	fbohandlergroup.clear();
	
	fbo.destroy();
	//fbo = nullptr;
}
bool JPbox::mouseOverOutlet() {
	if (ofGetMouseX() > outlet_x - outlet_size / 2
		&& ofGetMouseX() < outlet_x + outlet_size / 2
		&& ofGetMouseY() > outlet_y - outlet_size / 2
		&& ofGetMouseY() < outlet_y + outlet_size / 2) {
		return true;
	}
	else {
		return false;
	}
}
int JPbox::getTipo(){
	return tipo;
}
void JPbox::setonoff(bool _val){
	onoff.boolValue = _val;
	onoff.value = _val;
}
bool JPbox::getonoff()
{
	return onoff.boolValue;
}
