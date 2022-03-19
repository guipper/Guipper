#pragma once
#include "ofMain.h"
#include "jp_box_shader.h"
#include "jp_box.h"
#include "jp_box_image.h"
#include "jp_box_video.h"
#include "jp_box_cam.h"
#ifdef SPOUT
#include "jp_box_spout.h"
#endif
#include "jp_box_preset.h"
#include "jp_box_framedifference.h"
#ifdef NDI
#include "jp_box_ndi.h"
#endif

#include "../JPgui/jp_slider.h"
#include "../JPgui/jp_bang.h"
#include "../JPgui/jp_toogle.h"
#include "../JPgui/jp_complexslider.h"
// Esta clase como que va a manejar todos los shaderboxs y esas cosas:
#include "../JPutils/jp_constants.h"
class JPboxgroup
{

public:
	JPboxgroup();
	~JPboxgroup();
	string test;

	void setup(ofTrueTypeFont &_font, int &_activerender);
	void draw();
	void draw_activerender(); // Dibuja el render activo. Esta es la <que corre en el ofApp.cpp
	void draw_activerender(float _width, float _height);

	void update();
	void update_paramswindow();
	void update_resized(int w, int h);		   // Lo que hace cuando pinta resize
	void update_mouseDragged(int mousebutton); // Lo que hace cuando arrastras en la pantalla.
	void update_mousePressed(int mouseButton); // Lo que hace cada vez que haces click(ponele).

	void save(string _diroutput);
	void load2(string _dirinput);
	// Guarda los valores a un XML
	void load(string _dirinput);

	void addBox(string directory, float _x, float _y);

	void addBox(string dir);
	void deleteSelectedShader();

	// ACA ESTA TODO LO QUE TENGA QUE VER CON EL INSPECTOR PANEL DIGAMOS :
	// ESTO TE DICE QUE PANEL ESTA ABIERTO. SI EL PANEL QUE ESTA ABIERTO ES -1 ENTONCES EL PANEL NO ESTA ABIERTO

	/*ofColor CmouseOver;
	ofColor Cfront;
	ofColor Cback;
	ofColor Cactive;
	ofColor textcolor;
	*/
	ofFbo *getActiverender();
	void reloadActiveshader();
	void listenToOsc(string _dir, float _val);

	bool mouseOverGui();

	// void setVideoGrabberPointer(ofVideoGrabber &_ofvideograb);
	void clear();
	ofTexture *getActiveTexture();
	/***************GETTERS **************************/
	int getBoxesSize();

	bool draw_SelectionRect = false;
	ofVec2f lastMouseClick;

	vector<JPcontroller *> controllers; // ESTE ARRAY ES DINAMICO , QUIERE DECIR QUE DEPENDE DE CUANDO CAMBIEN LOS COSOS
										// ESTO ES SOLO PARA QUE LERPEE LOS VALORES HACIA ESTO.
	vector<JPbox *> boxes; // TODOS LOS SHADERRENDERS QUE TIENE EL OBJETO.

	int openguinumber = -1;
	int controllerselected; // ME INDICA QUE VARIABLE ESTA AGARRADA
private:
	vector<JPTooglelist *> botones_modo;
	vector<JPToogle *> botones_speed;
	vector<JPSlider *> sliders_speed;

	int *activerender;

	void draw_cursorrect();
	void setControllers();
	void setupShaderRendersFromDataFolder(); // Esta es para que levante todos

	ofTrueTypeFont *font_p;

	void setinspectorsetactiveparams();
	void draw_paramswindow(); // Dibuja la ventanita del inspector.

	float inspectorwindow_width;
	float inspectorwindow_height;
	float inspectorwindow_x;
	float inspectorwindow_y;
	float inspectorwindow_sepy; // Esta es para el espacio que hay entre distintos sliders.

	JPBang inspectorsetactive;			 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	JPBang inspectorreload;				 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	float inspectorwindow_setactivesize; // Para el size del setactive:

	void draw_conections();

	// ofFbo boxesdrawing;

	float offsetx;
	float offsety;

	// FOR RANDOMISIN THE VALUES :
	int randomcnt = 0; // si supera este numero 5 veces los parametros se randomizan.

	// COSAS DE AGARRE
	bool shaderboxagarrado;
	bool ouletagarrado;
	int cualestaagarrado = -1;
	int outlet_cualestaagarrado = -1;

	// Vamos a ver si podemos emular un doble click.
	bool isDoubleClick;
	float lasttime_mouseclick;
	float duration_mouseclick;
};
