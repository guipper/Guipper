#pragma once

#include "ofMain.h"

class jp_constants
{

public:
	// static void init();
	static void init(int _renderwidth, int _renderheight, int _window_width, int _window_height);
	static void setrenderWidth(int _renderwidth);
	static void setrenderHeight(int _renderheight);
	static void setwindow_width(int _window_width);
	static void setwindow_height(int _window_height);

	static void setwindow_mousex(int _window_mousex);
	static void setwindow_mousey(int _window_mousey);

	static int renderWidth;
	static int renderHeight;
	static int window_width;
	static int window_height;

	static int window_mousex;
	static int window_mousey;

	static void set_mousePressedPos(ofVec2f _mousePressedPos);
	static ofVec2f mousePressedPos;

	static void set_systemDialog_open(bool _mousePressedPos);

	static bool systemDialog_open;

	// Vamos a hacer finalmente un puntero a la tipografia as� no me vuelvo totalmente desquiciado.

	static ofTrueTypeFont p_font; // Esta es la fuente mas utilizada en todo el programa.
	static ofTrueTypeFont h_font;
	static ofTrueTypeFont p2_font;

	// Aca tal vez convendr�a pasar todo esto a tipo, otra clase? Algo especifico para manejar los colores? vamos a dejarlo aca pora ahora
	static vector<ofColor> CmouseOver; // Color para cuando el mouse esta por arriba :
	static vector<ofColor> Cfront;
	static vector<ofColor> Cback;
	static vector<ofColor> Cactive;
	static ofColor textcolor;

	static vector<ofImage> imgs;

private:
	static void addImage(string str1);
	// void addImage(string const& str1);
};

class jp_constants_img
{
public:
	static ofImage actual;
	static ofImage handlerder;
	static ofImage handlerizq;
	static ofImage speed;
	static ofImage timeline;
	static ofImage idayvuelta;
	static ofImage ran;
	static ofImage una_direccion;
	static ofImage fondo_valor;
	static ofImage fondo_parametro;
	static ofImage outlet_img; // Esta es para la imagen del outlet que es tipo el cosito ese.
	static ofImage background;
	static void init();
	static void drawCenterImage(ofImage _img, float _x, float _y);
	static void drawCenterImage(ofImage _img, float _x, float _y, float _multiplyer);
	static void drawCenterImage(ofImage _img, float _x, float _y, float _width, float _height);
};