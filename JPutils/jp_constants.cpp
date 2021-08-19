#include "jp_constants.h"


int jp_constants::renderWidth;
int jp_constants::renderHeight;
int jp_constants::window_width;
int jp_constants::window_height;
int jp_constants::window_mousex;
int jp_constants::window_mousey;

ofTrueTypeFont jp_constants::p_font;
ofTrueTypeFont jp_constants::h_font;
ofTrueTypeFont jp_constants::p2_font;

 bool jp_constants::systemDialog_open;
ofVec2f jp_constants::mousePressedPos;


vector < ofColor> jp_constants::CmouseOver;
vector < ofColor> jp_constants::Cfront;
vector < ofColor> jp_constants::Cback;
vector < ofColor> jp_constants::Cactive;
 ofColor jp_constants::textcolor;

 vector<ofImage> jp_constants::imgs;

void jp_constants::init(int _renderwidth, int _renderheight, int _window_width, int _window_height){
	
	

	renderWidth = _renderwidth;
	renderHeight = _renderheight;
	window_width = _window_width;
	window_height = _window_height;
	
	p_font.loadFont("font/Montserrat-Regular.ttf", 11);
	h_font.loadFont("font/Montserrat-Regular.ttf", 20);
	p2_font.loadFont("font/Montserrat-Regular.ttf", 10);

	CmouseOver.clear();
	Cfront.clear();
	Cback.clear();
	Cactive.clear();

	//Seteamos los colores.
	//Paleta 1: standart
	CmouseOver.push_back(ofColor(220, 255));
	Cfront.push_back(ofColor(180));
	Cback.push_back(ofColor(100));
	Cactive.push_back(ofColor(255));
	
	//Paleta 2: SPEED SLIDER
	CmouseOver.push_back(ofColor(220,0,0, 255));
	Cfront.push_back(ofColor(180,0,0));
	Cback.push_back(ofColor(100,0,0));
	Cactive.push_back(ofColor(255,0,0));

	
	textcolor = ofColor(0);
	string dir = "img/design/componentes/";
	addImage(dir+"actual.png");
	addImage(dir + "der.png");
	addImage(dir + "izq.png");
	addImage("img/design/componentes/speed.png");
	addImage("img/design/componentes/timeline.png");
	addImage("img/design/componentes/ida_y_vuelta.png");
	addImage("img/design/componentes/ran.png");
	addImage("img/design/componentes/una_direccion.png");
	addImage("img/design/componentes/fondo_valor.png");
	addImage("img/design/componentes/fondo_parametro.png");
	addImage("img/design/componentes/fondo.png");
	addImage("img/design/componentes/fondo.png");

	//imgs.push_back("ofImage(img / design / componentes / actual.png)");
	
	/*actual.load("img/design/componentes/actual.png");
	handlerder.load("img/design/componentes/der.png");
	handlerizq.load("img/design/componentes/izq.png");
	speed.load("img/design/componentes/speed.png");
	timeline.load("img/design/componentes/timeline.png");
	idayvuelta.load("img/design/componentes/ida_y_vuelta.png");
	ran.load("img/design/componentes/ran.png");
	una_direccion.load("img/design/componentes/una_direccion.png");
	fondo_valor.load("img/design/componentes/fondo_valor.png");
	fondo_parametro.load("img/design/componentes/fondo_parametro.png");
	outlet_img.load("img/design/componentes/outlet.png");
	background.load("img/design/componentes/fondo.png");*/

}
void jp_constants::setrenderWidth(int _renderwidth) { renderWidth = _renderwidth; }
void jp_constants::setrenderHeight(int _renderheight){ renderHeight = _renderheight; }
void jp_constants::setwindow_width(int _window_width){ window_width = _window_width; }
void jp_constants::setwindow_height(int _window_height){ window_height = _window_height; }
void jp_constants::set_mousePressedPos(ofVec2f _mousePressedPos) { mousePressedPos = _mousePressedPos; }
void jp_constants::setwindow_mousex(int _window_mousex){
	window_mousex = _window_mousex;
}
void jp_constants::setwindow_mousey(int _window_mousey){
	window_mousey = _window_mousey;
}
void jp_constants::set_systemDialog_open(bool _systemDialog_open) {

	systemDialog_open = _systemDialog_open;
}

void jp_constants::addImage(string str1){
	ofImage img;
	img.load(str1);
	if (str1 == "fondo_valor.png") {
		float mult = .2;
		img.resize(img.getWidth()* mult, img.getHeight()* mult);
	}
	imgs.push_back(img);
}



/*******************************************************************************************/

ofImage jp_constants_img::actual;
ofImage jp_constants_img::handlerder;
ofImage jp_constants_img::handlerizq;
ofImage jp_constants_img::speed;
ofImage jp_constants_img::timeline;
ofImage jp_constants_img::idayvuelta;
ofImage jp_constants_img::ran;
ofImage jp_constants_img::una_direccion;
ofImage jp_constants_img::fondo_valor;
ofImage jp_constants_img::outlet_img;
ofImage jp_constants_img::fondo_parametro;
ofImage jp_constants_img::background;
void jp_constants_img::init() {


	//BUENO vamos a transformar esto a un array. Así es mas facil cargar imagenes y todo eso.

	actual.load("img/design/componentes/actual.png");
	handlerder.load("img/design/componentes/der.png");
	handlerizq.load("img/design/componentes/izq.png");
	speed.load("img/design/componentes/speed.png");
	timeline.load("img/design/componentes/timeline.png");
	idayvuelta.load("img/design/componentes/ida_y_vuelta.png");
	ran.load("img/design/componentes/ran.png");
	una_direccion.load("img/design/componentes/una_direccion.png");
	fondo_valor.load("img/design/componentes/fondo_valor.png");
	fondo_parametro.load("img/design/componentes/fondo_parametro.png");
	outlet_img.load("img/design/componentes/outlet.png");
	background.load("img/design/componentes/fondo.png");
}
void jp_constants_img::drawCenterImage(ofImage _img, float _x, float _y) {
	_img.draw(_x - _img.getWidth() / 2, _y - _img.getHeight() / 2);
}
void jp_constants_img::drawCenterImage(ofImage _img, float _x, float _y, float _multiplyer) {
	_img.draw(_x - _img.getWidth() / 2 * _multiplyer,
		_y - _img.getHeight() / 2 * _multiplyer,
		_img.getWidth() * _multiplyer,
		_img.getHeight() * _multiplyer);
}
void jp_constants_img::drawCenterImage(ofImage _img, float _x, float _y, float _width, float _height) {
	_img.draw(_x - _height / 2,
		_y - _width / 2,
		_width,
		_height);
}