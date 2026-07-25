/*
	Made by JPUPPER vieja
	Ultima modificaci�n : 7/5/2021
	Cambios a hacer :
*/


#pragma once

// ADDONS :
// OTHERS:
#include "ofMain.h"
#include "defines.h"
#include "JPbox/jp_box.h"
#include "JPbox/jp_box_shader.h"
#include "JPbox/JPboxgroup.h"
//#include "JPbox/Shaderrender.h"
#include "JPutils/jp_fileloader.h"
#include "JPutils/jp_constants.h"
#include "JPutils/jp_midi_keymap.h"
#include "JPgui/jp_shader_editor.h"
#include "ofxOsc.h"
#ifdef NDI
#include "ofxNDI.h"
#endif

//#include "RenderWindowApp.h"

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
	void mouseScrolled(int x, int y, float scrollX, float scrollY);
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
	GLuint sendertexture;	 // Local OpenGL texture used for sharing
	bool bInitialized;		 // Initialization result
	ofImage myTextureImage;	 // Texture image for the 3D demo
	float rotX, rotY;
	void drawSpout();
	bool spoutActive = false;
#endif

#ifdef NDI
	// NDI SENDER:
	ofxNDIsender ndiSender; // NDI sender
	ofFbo ndiFbo;			// Fbo used for graphics and sending
	bool ndiActive = true;
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
	void saveSession(string path);
	void loadSession(string path);

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
	JPMidiKeymap midiKeymap;

	// Esto ser�a mejor en uno tal vez ? por ahora lo dejamos con 2.
	OpenLoader openloader;
	// OpenSaveFileLoader opensavefile_loader;
	SaveAsSaver saveas_saver;
	ofImage outletimg;

	struct ShaderEntry {
		string name;
		string path;
		bool favorite = false;
	};

	struct ShaderFolder {
		string name;
		string path;
		bool expanded = false;
		bool isFavorites = false; // synthetic "favorites" folder pinned on top
		vector<ShaderEntry> shaders;
	};

	enum MENUACTIVO
	{
		NODOS,       // NODES
		OPCIONES,    // SETTINGS
		TUTORIAL,    // HELP
		SHADER_INDEX,// IMPORT
		EDITOR       // SHADER EDITOR
	};
	int pantallaActiva;
	void drawScreenTabs();
	int getScreenTabAtPos(int x, int y);
	bool loadAspreset; // ESTO ES PARA QUE TODO EL TIEMPO ME DIGA SI TENGO APRETADO EL BOTON DE LA IZQ O NO .

	bool oscout_mode1;
	bool oscout_mode2;

	ofVec2f resolution_spoutext;

	DirectoryManager dirmanager;

	// Shader index data
	int shaderScroll = 0;
	vector<ShaderFolder> shaderFolders;
	int selectedShaderFolder = -1;
	int selectedShaderIndex = -1;

	// Preview
	ofShader previewShader;
	ofFbo previewFbo;
	bool previewShaderLoaded = false;
	ofImage previewImg1, previewImg2;

	// Shader index hover state
	int hoveredShaderFolder = -1;
	int hoveredShaderIndex = -1;
	bool showShaderHitBoxes = false;

	// Shader index search
	string shaderSearchText;
	bool shaderSearchFocused = false;
	int shaderSearchCursor = 0;
	// Preview random values for RDM button
	vector<string> previewUniformNames;
	vector<float> previewUniformMins;
	vector<float> previewUniformMaxs;
	vector<float> previewRdmValues;
	bool previewRdmActive = false;

	// LOAD distribution counter
	int loadBoxCount = 0;

	// Favorites (persisted to bin/data/shader_favorites.xml). Paths of starred
	// shaders; a synthetic "favorites" folder is pinned to the top of the list.
	vector<string> favoritePaths;
	void loadFavorites();
	void saveFavorites();
	bool isFavorite(const string &path) const;
	void toggleFavorite(const string &path);
	void rebuildFavoritesFolder();

	// Import-page inline MIDI bind: true while waiting for a MIDI message to
	// bind the selected shader's add-shader command.
	bool importBindWaiting = false;

	// Shared preview-selection + keyboard navigation for the shader index.
	void selectShaderForPreview(int f, int s);
	void moveShaderSelection(int dir); // -1 = up, +1 = down
	void ensureShaderSelectionVisible();
	void loadSelectedShaderBox(); // add the selected shader to the canvas

	// Double-click detection for the shader list (load on double-click).
	float lastShaderClickTime = -1.0f;
	int lastShaderClickFolder = -1;
	int lastShaderClickIndex = -1;

	void scanShaders();
	void draw_shaderindex();





	//ACA PARA AGREGAR MAS LENGUAJES EVENTUALMENTE SUPONGO : 
	//0 INSTRUCCIONES EN INGLES.
	//1 INSTRUCCIONES EN ESPAÑOL.
	int language = 0;

	// Options screen text fields
	enum { FIELD_OSC_PORT_IN = 0, FIELD_OSC_PORT_OUT, FIELD_RENDER_WIDTH, FIELD_RENDER_HEIGHT, FIELD_BPM, FIELD_OSC_IP_OUT, FIELD_DEFAULT_COMPO, OPTIONS_FIELD_COUNT };
	string optionsFieldText[OPTIONS_FIELD_COUNT];
	int focusedOptionsField = -1;
	int optionsFieldCursor = 0;
	void applyOptionsField();
	void initOptionsFields();

	// AutoTap for BPM
	vector<float> tapTimestamps;
	void autoTap();

	// Save feedback
	string saveFeedbackText;
	float saveFeedbackTime = 0.0f;

	// Track if options fields have been initialized to avoid reset on tab switch
	bool optionsFieldsInitialized = false;

	// Default composition path (loaded at startup)
	string defaultCompoPath;

	// Save-as modal state
	bool saveModalActive = false;
	string saveModalName = "";
	int saveModalCursor = 0;
	void drawSaveModal();
	void confirmSaveModal();
	void updateSaveModal();
	void cancelSaveModal();
	ofTrueTypeFont modalFont; // Larger font for modal text

	// Shader Editor
	JPShaderEditor shaderEditor;

};
