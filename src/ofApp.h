/*
	Made by JPUPPER vieja
	Ultima modificaci�n : 7/5/2021
	Cambios a hacer :
*/

#pragma once

// ADDONS :
// OTHERS:
#include "ofMain.h"

#include "JPbox/jp_box.h"
#include "JPbox/jp_box_shader.h"
#include "JPbox/JPboxgroup.h"
// #include "JPbox/Shaderrender.h"
#include "JPutils/jp_fileloader.h"
#include "JPutils/jp_constants.h"
#include "ofxOsc.h"

#ifdef NDI
#include "ofxNDI.h"
#endif

// #include "RenderWindowApp.h"

#define PORT 5000

class ofApp : public ofBaseApp
{

public:
	void setup();
	void update();
	void draw();

	void draw_debugInfo();

	void draw_instrucciones();

	void draw_opciones();

	void drawRender();

	void keyPressed(int key);
	void openRenderWindow();
	void keycodePressed(ofKeyEventArgs &e);
	void exit(ofEventArgs &e); // LISTENER FOR EXIT APP .

	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);
	
	void exit();
	// ofxKFW2::Device kinect;

	ofTrueTypeFont font_p; // Titulo de compo
	// JPGui gui;

	JPboxgroup boxes;
	float a;
	int activerender = 0;
	ofFbo output;
	bool isDebug = false;
	bool isRecording = false;

	int prevKey = 0;

	string savedirectory; // directorio en donde se guarda. Cambia si haces un save as.

	bool InitGLtexture(GLuint &texID, unsigned int width, unsigned int height);

	char sendername[256]; // Window name (Spout uses it as sender name)

#ifdef SPOUT
	// SPOUT SENDER:
	SpoutSender spoutsender; // A sender object
	GLuint sendertexture;		 // Local OpenGL texture used for sharing
	bool bInitialized;			 // Initialization result
	ofImage myTextureImage;	 // Texture image for the 3D demo
	float rotX, rotY;
	void drawSpout();
	bool spoutActive = false;
#endif

#ifdef NDI
	// NDI SENDER:
	ofxNDIsender ndiSender; // NDI sender
	ofFbo ndiFbo;						// Fbo used for graphics and sending
#endif

	// WINDOW MANAGMENT:
	std::vector<shared_ptr<ofAppBaseWindow>> windows; // Esto es para las ventanas de los renders.
	std::shared_ptr<ofAppBaseWindow> mainWindow;
	bool isRenderWindowOpen = false;
	void window_drawRender(ofEventArgs &args);
	void window_resized(ofResizeEventArgs &args);
	void window_mouseMove(ofMouseEventArgs &e);
	void window_keyPressed(ofKeyEventArgs &e);

	void loadSettings();
	void saveSettings();

	float window_initialposx;
	float window_initialposy;
	bool window_fullscreen;

	// OSC MANAGMENT
	ofxOscSender sender;
	ofxOscReceiver receiver;
	int current_msg_string;
	int mouseX, mouseY;
	char mouseButtonState[128];
	void updateOSC();

	// Esto ser�a mejor en uno tal vez ? por ahora lo dejamos con 2.
	OpenLoader openloader;
	// OpenSaveFileLoader opensavefile_loader;
	SaveAsSaver saveas_saver;
	ofImage outletimg;

	enum MENUACTIVO
	{
		NODOS,
		OPCIONES,
		TUTORIAL
	};
	int pantallaActiva;
	bool loadAspreset; // ESTO ES PARA QUE TODO EL TIEMPO ME DIGA SI TENGO APRETADO EL BOTON DE LA IZQ O NO .

	bool oscout_mode1;
	bool oscout_mode2;

	ofVec2f resolution_spoutext;

	DirectoryManager dirmanager;

	// ACA PARA AGREGAR MAS LENGUAJES EVENTUALMENTE SUPONGO :
	// 0 INSTRUCCIONES EN INGLES.
	// 1 INSTRUCCIONES EN ESPAÑOL.
	int language = 0;
};
