#pragma once

#include "defines.h"
#include "ofMain.h"

// ============================================================
// PALETA UNIFICADA: aesthetic dark + cyan (referencia MIDI panel)
// Todas las llamadas de color deben usar estas constantes
// ============================================================

// --- Backgrounds ---
#define COL_BG_DARK       ofColor(12, 16, 20)       // Fondo mas oscuro (root, main canvas)
#define COL_BG_PANEL      ofColor(15, 20, 25)       // Paneles, dropdowns, ventanas
#define COL_BG_TAB        ofColor(18, 18, 22)       // Tab bar background
#define COL_BG_BUTTON     ofColor(25, 30, 35)       // Botones, componentes
#define COL_BG_INPUT      ofColor(20, 25, 30)       // Input fields
#define COL_BG_SLIDER     ofColor(80, 85, 90)       // Slider troughs - gris medio visible, contrasta con fondo oscuro
#define COL_BG_HOVER      ofColor(40, 48, 56)       // Hover backgrounds
#define COL_BG_ACTIVE     ofColor(0, 160, 160, 220) // Active/selected backgrounds
#define COL_BG_SCROLLBAR  ofColor(90, 100, 110)     // Scrollbar track

// --- Accent colors ---
#define COL_ACCENT_CYAN      ofColor(0, 175, 190)       // Primary accent (borders, highlights, active) - muted for dark bg
#define COL_ACCENT_CYAN_DIM  ofColor(0, 135, 150)       // Secondary accent (selected states) - toned down
#define COL_ACCENT_CYAN_DARK ofColor(0, 65, 75)         // Subtle cyan
// Semantic hues on the dark base: GREEN = live/active/on, AMBER(gold) = cued/staged,
// CYAN = selected/focused/accent, RED = bypass/off. Soft, muted for the dark bg.
#define COL_ACCENT_GREEN     ofColor(46, 190, 120)       // live / active-render / toggle ON
#define COL_ACCENT_GREEN_BR  ofColor(74, 214, 140)       // brighter green (active halo / borders)
#define COL_ACCENT_GOLD      ofColor(226, 174, 64)       // cued / staged (amber)
#define COL_ACCENT_GOLD_DIM  ofColor(198, 150, 74)       // amber dim (hover/secondary)
#define COL_ACCENT_RED       ofColor(255, 80, 80)       // Bypass active, errors
#define COL_ACCENT_RED_DIM   ofColor(255, 120, 120)     // Bypass hover text

// --- Text ---
#define COL_TEXT_PRIMARY   ofColor(255)                 // Texto blanco
#define COL_TEXT_SECONDARY ofColor(200)                 // Texto gris claro
#define COL_TEXT_DIM       ofColor(150)                 // Texto gris medio
#define COL_TEXT_MUTED     ofColor(100, 110, 120)       // Texto gris oscuro
#define COL_TEXT_DARK      ofColor(20, 20, 28)          // Texto oscuro sobre fondo claro

// --- Borders ---
#define COL_BORDER_DEFAULT  ofColor(70, 80, 90)         // Borde normal
#define COL_BORDER_HOVER    ofColor(100, 110, 120)      // Borde hover
#define COL_BORDER_MUTED    ofColor(55, 55, 65)         // Borde inactivo
#define COL_BORDER_ACTIVE   COL_ACCENT_CYAN             // Borde activo

// --- Card / Box fills ---
#define COL_BG_BOX        ofColor(22, 28, 34)         // Box card fill - dark, blends with theme

// --- Component states ---
#define COL_BOX_BORDER        ofColor(0, 175, 190, 180) // Box border default - cyan sutil visible
#define COL_BOX_BORDER_HOVER  ofColor(0, 200, 200, 255) // Box border hover
#define COL_BOX_BORDER_GRAB   COL_ACCENT_CYAN           // Box border while dragging (was green)

// --- Mapped indicators ---
#define COL_MAPPED_ON   COL_ACCENT_CYAN_DIM         // Mapped active (was greenish)
#define COL_MAPPED_OFF  ofColor(60, 70, 80)          // Mapped inactive

// --- Slider / Toggle ---
#define COL_SLIDER_FILL  ofColor(50, 60, 70)
#define COL_SLIDER_TROUGH COL_BG_SLIDER               // Slider background trough
#define COL_TOGGLE_OFF   ofColor(40, 40, 45)

// --- Tab inactive ---
#define COL_TAB_INACTIVE_BG    ofColor(35, 35, 42)
#define COL_TAB_INACTIVE_BRD   ofColor(55, 55, 65)

// --- Unresolved error ---
#define COL_ERROR_BG    ofColor(120, 30, 30)
#define COL_ERROR_TEXT  ofColor(230, 70, 70)
#define COL_ERROR_BR    ofColor(230, 70, 70, 220)

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
	static void setdurationgallery(float _window_mousey);
	static void setBpm(float _bpm);

	static int renderWidth;
	static int renderHeight;
	static int window_width;
	static int window_height;
	static float durationgallery;
	static float bpm;
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
