#include "jp_tooglelist.h"
//BUENO A VER, YA NO HACE FALTA USAR UN TOOGLE LIST. PERO ERA COMO ESTABA ANTES ASI QUE LO DEJO.
void JPTooglelist::setup(float _x, float _y, float _width, float _height) {
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	activeFlag = true;
	boolValue = true;
	activable = true;
	showtext = false;
	movtype = 0;
	controllertype = TOOGLELIST;
	paleta = 0;
}
void JPTooglelist::draw() {

	//YO SE QUE ESTO ES UN CHOCLAZO Y QUE SE PUEDE SINTETIZAR. PERO ESTOY QUEMADEN 

	if (ofGetMousePressed() && mouseOver() && activable) {
		//activeFlag = true;

		cout << "ACTIVE BOTON " << endl;
		activable = false;
		/*movtype++; //ESTO LO PASAMOS AL UPDATE DE JPGROUP
		if (movtype > 4) {
			movtype = 0;
		}*/
	}

	if (!ofGetMousePressed()) {
		activable = true;
	}

	/*if (activeFlag) {
		activeFlag = false;
		boolValue = !boolValue;
		ofSetColor(jp_constants::Cactive);
	}

	if (boolValue) {
		ofSetColor(jp_constants::Cback);

	}
	else {
		ofSetColor(jp_constants::Cactive);
	}*/

	ofSetColor(jp_constants::Cback[paleta]);
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofRect(x, y, width, height);

	ofSetColor(ofColor::white);

	if (showtext) {

		string Strvalue = ofToString(movtype);
		jp_constants::p_font.drawString(Strvalue,
			x - jp_constants::p_font.stringWidth(Strvalue) / 2,
			y + jp_constants::p_font.stringHeight(Strvalue) / 2);

	}
	else {
		ofPushMatrix();
		ofTranslate(x, y);
		if (movtype == 0) {
			ofRotate(-90);
		}
		jp_constants_img::actual.draw(0,0);
		ofPopMatrix();
	}

	ofSetColor(255, 0, 0);
	//ofDrawEllipse(x,y, 30, 30);
	//lele.drawString(name, x - name.length() * 5, y + height * 0.1); // ESTA LINEA DE CODIGO ME GENERA UN ERROR TOTALMENTE INTENDIBLE MIKE*/
}
