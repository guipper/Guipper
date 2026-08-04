#include "ofApp.h"
#include "JPutils/jp_textfield.h"
#include <iostream>
#include <algorithm>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {
constexpr float SHADER_ROW_INDENT = 22.0f;
constexpr float SHADER_STAR_OFFSET = 10.0f;
constexpr float SHADER_STAR_HIT_HALF_WIDTH = 10.0f;
constexpr float SHADER_NAME_OFFSET = 22.0f;
constexpr int PREVIEW_MAX_WIDTH = 512;
constexpr int PREVIEW_MAX_HEIGHT = 288;
constexpr float PREVIEW_FRAME_INTERVAL = 1.0f / 30.0f;

bool shaderHasMain(const string &path)
{
	const string source = ofBufferFromFile(path).getText();
	// Shader files use the GLSL entry-point spelling directly. Avoid the
	// regex-based filter here: it can reject valid standalone shaders when
	// their source contains preprocessing directives before main().
	return source.find("void main") != string::npos;
}
}

//--------------------------------------------------------------
void ofApp::setup() {

	// ESC is used to cancel modals / leave screens, not to quit the whole app.
	ofSetEscapeQuitsApp(false);

	font_p.loadFont("font/Montserrat-Regular.ttf", 11); // Inicio fuente.

	// Modal font - loaded at readable size for the save dialog
	modalFont.loadFont("font/Montserrat-Medium.ttf", 14);

	// Shader editor
	shaderEditor.setup();
	boxes.shaderEditor = &shaderEditor;

	ofSetVerticalSync(false);
	// FreeConsole();

	ofDisableArbTex();
	// ESTAS COSAS LAS SETEA DESDE EL SETTINGS XML DESPUES. PERO ACA LES DA UN VALOR INICIAL  POR LAS DUDAS :
	float altoventana = ofGetScreenHeight() * 3 / 4;
	float anchoventana = ofGetScreenWidth() * 3 / 4;
	ofSetWindowShape(anchoventana, altoventana);
	ofSetWindowPosition(ofGetScreenWidth() / 2 - anchoventana / 2,
		ofGetScreenHeight() / 2 - altoventana / 2);

	/*ofSetWindowPosition(ofGetScreenWidth() / 2 - anchoventana / 2 - ofGetScreenWidth(),
		ofGetScreenHeight() / 2 - altoventana / 2);
	*/

	jp_constants::init(ofGetScreenWidth(), ofGetScreenHeight(), 600, 600);                   
	jp_constants::setwindow_mousex(300);
	jp_constants::setwindow_mousex(300);
	jp_constants::set_systemDialog_open(false);
	jp_constants_img::init();
	cout << "Render width  " << jp_constants::renderWidth << endl;
	boxes.setup(font_p, activerender);
	ofSetBackgroundColor(0);
	ofSetFrameRate(60);
	openloader.setJPboxgroupPointer(boxes);
	ofAddListener(ofGetWindowPtr()->events().keyPressed, this,
		&ofApp::keycodePressed);

	savedirectory = "savefiles/data.xml";

	dirmanager.loadDirectorys();

	receiver.setup(PORT);
	oscout_mode1 = true;
	oscout_mode2 = true;
	midiKeymap.setup(&boxes, [this]() { autoTap(); });

	loadSettings();

	ofSetWindowTitle("GUIPPER");

	loadAspreset = true; // Default: drag XML as boxgroup. Press 't' to toggle to full session load.

	// Use the Guipper artwork as the primary shader-preview input. Keep the
	// secondary image for blending shaders that need two distinct textures.
	previewImg1.load("guipper.png");
	previewImg2.load("preview2.png");
	if (previewImg1.isAllocated()) cout << "guipper.png preview loaded OK" << endl;
	if (previewImg2.isAllocated()) cout << "preview2.png loaded OK" << endl;

	//.----------------------------------------------------------------/

#ifdef NDI
	// INIT NDI :
	//  Optionally set fbo readback using OpenGL pixel buffers
	ndiSender.SetReadback(); // Change to false to compare
	// Optionally set NDI asynchronous sending
	// instead of clocked at the specified frame rate (60fps default)
	ndiSender.SetAsync();
	// ndiSender.
	ndiSender.CreateSender(sendername, jp_constants::renderWidth, jp_constants::renderHeight);
//---------------------------------------------------/
#endif

	pantallaActiva = NODOS;

	// INIT SPOUT SENDER :
	ofBackground(COL_BG_DARK);
	ofEnableNormalizedTexCoords(); // explicitly normalize tex coords for ofBox

	strcpy(sendername, "Guipper"); // Set the sender name
	ofSetWindowTitle(sendername); // show it on the title bar

// ====== SPOUT =====
#ifdef SPOUT
	bInitialized = false; // Spout sender initialization

	// Create an OpenGL texture for data transfers
	sendertexture = 0; // make sure the ID is zero for the first time
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	// OK . Esto tira error si no pasas otros valores que no sean : ofGetWidth(), ofGetHeight().
	// HAbria que ver por que . . . . . Y si le tiras un resize la mata.
	// cout << "ANCHO VENTANA " << ofGetWidth() << endl;//1440
	// cout << "ALTO VENTANA " << ofGetHeight() << endl;//810
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?
	// InitGLtexture(sendertexture, jp_constants::renderWidth, jp_constants::renderHeight); //!?!??!!?

	resolution_spoutext = ofVec2f(jp_constants::renderWidth, jp_constants::renderHeight);
	// resolution_spoutext = ofVec2f(ofGetWidth(),ofGetHeight());

	// LA RECONCHA DEL PATO, NO HAY MANERA POSIBLE PARA QUE PASE EL SPOUT EN UNA RESOLUCION
	// MAS GRANDE QUE EN LA VENTANA DEL ORTO!??!?!?!?!?!
	//InitGLtexture(sendertexture,1920, 1080); //!?!??!!?
#endif

	// 3D drawing setup for a sender
	// glEnable(GL_DEPTH_TEST);							// enable depth comparisons and update the depth buffer
	// glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
	//ofDisableArbTex(); // needed for textures to work
	// myTextureImage.loadImage("SpoutBox1.png");			// Load a texture image for the demo
	// If a default composition is configured, use it instead of savedirectory
	if (!defaultCompoPath.empty()) {
		savedirectory = defaultCompoPath;
	}
	loadSession(savedirectory);
	midiKeymap.load(ofToDataPath("midi_keymap.xml"));
}
void ofApp::update() {
	boxes.update();
	updateRetiredLiveOutputWindows();
	const float now = ofGetElapsedTimef();
	if (lastLiveOutputMonitorRefresh < 0.0f ||
		now - lastLiveOutputMonitorRefresh >= 2.0f)
	{
		refreshLiveOutputMonitors();
		for (int i = 0; i < (int)liveOutputs.size(); i++)
		{
			LiveOutputRuntime &output = liveOutputs[i];
			if (output.config.enabled && output.window &&
				resolveLiveOutputMonitor(output.config) < 0)
			{
				closeLiveOutputWindow(i, true);
				output.createAttempted = true;
			}
		}
	}

	// Auto-switch to EDITOR screen when a shader is opened from inspector/index
	if (shaderEditor.justOpened()) {
		shaderEditor.clearJustOpened();
		pantallaActiva = EDITOR;
	}

	midiKeymap.update();

	// Clear the Import-page bind-waiting state once the MIDI message is captured
	// (or learning was cancelled elsewhere).
	if (importBindWaiting && !midiKeymap.isLearning()) {
		importBindWaiting = false;
	}

#ifdef SPOUT
	if (spoutActive && boxes.getBoxesSize() > 0) {
		ofClear(0);
		drawSpout();
	}
#endif

#ifdef NDI
	if (ndiActive && boxes.getBoxesSize() > 0) {
		ndiSender.SendImage(*boxes.getActiverender());
	}
#endif

	updateOSC();

	if (saveas_saver.activeflag) {
		savedirectory = saveas_saver.path;
		cout << "Save session to " << savedirectory << endl;
		saveSession(savedirectory);
		saveas_saver.activeflag = false;
	}

	// PARA MANEJAR UN PROCESO PARALELO QUE CARGA LOS ARCHIVOS.
	/*if (openloader.activeflag) {
		try
		{
			boxes.addBox(openloader.path);
			//if (openloader.activeFiletype == openloader.SHADER) {
				//boxes.addShaderBox(openloader.path);
		//	}
			//else if (openloader.activeFiletype == openloader.SAVEFILE) {
				//savedirectory = openloader.path;
				//boxes.load(savedirectory);
			//}
			//else if (openloader.activeFiletype == openloader.IMAGE) {
				//boxes.addImageBox(openloader.path);
			//}
			//else if (openloader.activeFiletype == openloader.VIDEO) {
				//boxes.addVideoBox(openloader.path);
			//}
		//}
		catch (const std::exception&)
		{
			cout << "COULD NOT OPEN FILE" << endl;
		}

		openloader.activeflag = false;
	}*/

	// Keep animated previews responsive without competing with the live graph
	// for a full-resolution render on every application frame.
	if (previewShaderLoaded && pantallaActiva == SHADER_INDEX) {
		const float now = ofGetElapsedTimef();
		if (lastPreviewRenderTime < 0.0f ||
			now - lastPreviewRenderTime >= PREVIEW_FRAME_INTERVAL) {
			renderShaderPreview(true);
		}
	}
}
void ofApp::draw() {
	if (pantallaActiva == NODOS) {
		boxes.drawNodeEditorBackground(ofGetWidth(), ofGetHeight());
		drawScreenTabs();
		boxes.draw();
		ofSetColor(COL_TEXT_PRIMARY);
		// outletimg.draw(ofGetWidth() / 2, ofGetHeight() / 2, 200, 200);
		if (isDebug) {
			draw_debugInfo();
		}
	}
	else if (pantallaActiva == SHADER_INDEX) {
		// Draw the live node canvas behind so the right (uncovered) half shows
		// it; a LOADed box appears there for immediate visual feedback.
		boxes.drawNodeEditorBackground(ofGetWidth(), ofGetHeight());
		boxes.draw();
		drawScreenTabs();
		draw_shaderindex();
	}
	else if (pantallaActiva == EDITOR) {
		// Draw active render as background behind the editor
		boxes.draw_activerender(ofGetWidth(), ofGetHeight());
		drawScreenTabs();
		shaderEditor.draw();
	}
	else {
		drawScreenTabs();
		boxes.draw_activerender(ofGetWidth(), ofGetHeight());
	}
	if (pantallaActiva == TUTORIAL) {
		draw_instrucciones();
	}
	if (pantallaActiva == OPCIONES) {
		draw_opciones();
	}
	midiKeymap.drawMappingTargets();
	midiKeymap.draw();

	drawScreenTabs();

	drawSaveModal();
}
void ofApp::draw_debugInfo() {

	float sepy = 20;
	float posy = ofGetHeight();
	font_p.drawString("Active Render :" + ofToString(activerender), 30, posy -= sepy);
	font_p.drawString("FPS :" + ofToString(ofGetFrameRate()), 30, posy -= sepy);
	font_p.drawString("Boxes size : " + ofToString(boxes.getBoxesSize()), 30, posy -= sepy);
	font_p.drawString("Active Sequence : " + ofToString(boxes.activeSequence), 30, posy -= sepy);
	// Active session file
	string sessionName = ofFilePath::getFileName(savedirectory);
	if (sessionName.empty()) sessionName = "none";
	font_p.drawString("Active Compo : " + sessionName, 30, posy -= sepy);
	//font_p.drawString("DIALOG BOX : " + ofToString(jp_constants::systemDialog_open), 30, posy -= sepy);
}
void ofApp::draw_instrucciones() {
	float panelX = 30, panelY = 44; // below the top screen-tab bar, left-aligned
	float panelW = ofGetWidth() - 60;
	float lineH = 17;
	float sepy = 17;
	int totalLines = 0;

	// Count lines for both languages (take the larger one)
	int esLines = 0, enLines = 0;
	string es[50], en[50];

	// SPANISH
	es[esLines++] = "INSTRUCCIONES";
	es[esLines++] = "";
	es[esLines++] = "Cargar archivo: Arrastrar a la ventana";
	es[esLines++] = "";
	es[esLines++] = "TECLAS:";
	es[esLines++] = "1 : Editor de nodos";
	es[esLines++] = "2 : Configuracion (Settings)";
	es[esLines++] = "3 : Instrucciones (esta pantalla)";
	es[esLines++] = "4 : Shader Index (navegador de shaders)";
	es[esLines++] = "Tambien podes usar los botones NODES, SETTINGS, HELP, IMPORT de arriba";
	es[esLines++] = "t : Alternar carga como preset o sesion completa";
	es[esLines++] = "w : Abre ventana de render aparte";
	es[esLines++] = "f : FullScreen sobre ventana de render";
	es[esLines++] = "s : Guardar sesion en el XML actual";
	es[esLines++] = "Ctrl+S : Guardar como (save-as)";
	es[esLines++] = "l : Cargar sesion";
	es[esLines++] = "d : Mostrar datos de debug";
	es[esLines++] = "r : Recargar shader activo";
	es[esLines++] = "u : Agrupar cajas seleccionadas";
	es[esLines++] = "z : Alternar cue (seleccion rapida)";
	es[esLines++] = "x : Disparar codigo en shader activo";
	es[esLines++] = "h : Agregar caja SPOUT INPUT";
	es[esLines++] = "c : Agregar caja camara";
	es[esLines++] = "n : Agregar caja NDI RECEIVER";
	es[esLines++] = "i : Agregar caja Frame Difference";
	es[esLines++] = "k : Abrir/cerrar panel MIDI Keymap";
	es[esLines++] = "m : Exportar imagen (captura de pantalla)";
	es[esLines++] = "e : Activar/desactivar modo secuencia";
	es[esLines++] = "DEL : Eliminar shader seleccionado";
	es[esLines++] = "Ctrl+C : Copiar cajas seleccionadas";
	es[esLines++] = "Ctrl+V : Pegar cajas";
	es[esLines++] = "Esc : Cerrar shader index";
	es[esLines++] = "";
	es[esLines++] = "COMANDOS OSC:";
	es[esLines++] = "/load/(dir) : Cargar archivo especifico";
	es[esLines++] = "/setactiverender/(num) : Activar render";
	es[esLines++] = "/openguinumber/(value) : Control de active";
	es[esLines++] = "/(shader)/(param) : Control por nombre";
	es[esLines++] = "";
	es[esLines++] = "UNIFORMES GLOBALES DISPONIBLES:";
	es[esLines++] = "uniform float time;";
	es[esLines++] = "uniform vec2  resolution;";
	es[esLines++] = "uniform float bpm;";
	es[esLines++] = "uniform vec4  mouse;";
	es[esLines++] = "uniform vec2  window_mouse;";
	es[esLines++] = "uniform int   globalframeNum;";
	es[esLines++] = "uniform int   boxframeNum;";
	es[esLines++] = "uniform sampler2D feedback;";
	es[esLines++] = "";
	es[esLines++] = "Boton [Lang] en la esquina para cambiar idioma";

	// ENGLISH
	en[enLines++] = "INSTRUCTIONS";
	en[enLines++] = "";
	en[enLines++] = "Drag any file to this window to load it";
	en[enLines++] = "";
	en[enLines++] = "KEYS:";
	en[enLines++] = "1 : Node editor";
	en[enLines++] = "2 : Settings (XML Configuration)";
	en[enLines++] = "3 : Instructions (this screen)";
	en[enLines++] = "4 : Shader Index (shader browser)";
	en[enLines++] = "You can also use the NODES, SETTINGS, HELP, IMPORT buttons above";
	en[enLines++] = "t : Toggle load as preset or full session";
	en[enLines++] = "w : Open separate render window";
	en[enLines++] = "f : Fullscreen on render window";
	en[enLines++] = "s : Save session to current XML";
	en[enLines++] = "Ctrl+S : Save-as (new XML)";
	en[enLines++] = "l : Load session";
	en[enLines++] = "d : Toggle debug info";
	en[enLines++] = "r : Reload active shader";
	en[enLines++] = "u : Group selected boxes";
	en[enLines++] = "z : Toggle cue (quick select)";
	en[enLines++] = "x : Trigger code on active shader";
	en[enLines++] = "h : Add SPOUT INPUT box";
	en[enLines++] = "c : Add Camera box";
	en[enLines++] = "n : Add NDI RECEIVER box";
	en[enLines++] = "i : Add Frame Difference box";
	en[enLines++] = "k : Open/close MIDI Keymap panel";
	en[enLines++] = "m : Export screenshot to exportimgs/";
	en[enLines++] = "e : Toggle sequence mode";
	en[enLines++] = "DEL : Delete selected shader";
	en[enLines++] = "Ctrl+C : Copy selected boxes";
	en[enLines++] = "Ctrl+V : Paste boxes";
	en[enLines++] = "Esc : Close shader index";
	en[enLines++] = "";
	en[enLines++] = "OSC COMMANDS:";
	en[enLines++] = "/load/(dir) : Load specific savefile";
	en[enLines++] = "/setactiverender/(num) : Set active render";
	en[enLines++] = "/openguinumber/(value) : Control active";
	en[enLines++] = "/(shader)/(param) : Control by name";
	en[enLines++] = "";
	en[enLines++] = "AVAILABLE GLOBAL UNIFORMS:";
	en[enLines++] = "uniform float time;";
	en[enLines++] = "uniform vec2  resolution;";
	en[enLines++] = "uniform float bpm;";
	en[enLines++] = "uniform vec4  mouse;";
	en[enLines++] = "uniform vec2  window_mouse;";
	en[enLines++] = "uniform int   globalframeNum;";
	en[enLines++] = "uniform int   boxframeNum;";
	en[enLines++] = "uniform sampler2D feedback;";
	en[enLines++] = "";
	en[enLines++] = "Use [Lang] button at top-right to switch language";

	int maxLines = (language == 0) ? esLines : enLines;
	string* lines = (language == 0) ? es : en;

	float panelH = 50 + maxLines * sepy + 30;
	if (panelH > ofGetHeight() - 60) {
		panelH = ofGetHeight() - 60;
	}
	if (panelH < 300) panelH = 300;

	// Glassmorphism panel background
	ofSetColor(ofColor(COL_BG_DARK, 235));
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofFill();
	ofSetLineWidth(1.0f);

	// Title
	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString(lines[0], panelX + 15, panelY + 30);

	// Language toggle button (top-right of panel)
	float langBtnW = 52;
	float langBtnH = 22;
	float langBtnX = panelX + panelW - langBtnW - 15;
	float langBtnY = panelY + 13;
	string langLabel = (language == 0) ? "EN" : "ES";
	ofSetColor(language == 0 ? COL_ACCENT_CYAN : COL_ACCENT_GOLD);
	ofDrawRectRounded(langBtnX, langBtnY, langBtnW, langBtnH, 4.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(langBtnX, langBtnY, langBtnW, langBtnH, 4.0f);
	ofFill();
	ofSetLineWidth(1.0f);
	ofSetColor(COL_TEXT_PRIMARY);
	float lw = font_p.stringWidth(langLabel);
	font_p.drawString(langLabel, langBtnX + langBtnW / 2 - lw / 2, langBtnY + 16);
	jp_tooltip::draw("Switch help language", langBtnX, langBtnY, langBtnW, langBtnH);

	float drawY = panelY + 55;
	for (int i = 0; i < maxLines; i++) {
		if (drawY > panelY + panelH - 15) break;

		string line = lines[i];
		if (line.empty()) {
			drawY += sepy * 0.5f;
			continue;
		}

		// Section headers (TECLAS:, COMANDOS OSC:)
		if (line.find("TECLAS:") != string::npos || line.find("KEYS:") != string::npos ||
			line.find("COMANDOS OSC") != string::npos || line.find("OSC COMMANDS") != string::npos ||
			line.find("UNIFORMES GLOBALES") != string::npos || line.find("AVAILABLE GLOBAL") != string::npos) {
			ofSetColor(COL_ACCENT_CYAN);
			font_p.drawString(line, panelX + 15, drawY);
		}
		// Uniform declarations — dim cyan
		else if (line.rfind("uniform ", 0) == 0) {
			ofSetColor(100, 160, 180);
			font_p.drawString(line, panelX + 25, drawY);
		}
		// First instruction line
		else if (line.find("Cargar") != string::npos || line.find("Drag any") != string::npos) {
			ofSetColor(COL_TEXT_DIM);
			font_p.drawString(line, panelX + 15, drawY);
		}
		// Language toggle hint text (the actual button is at top-right)
		else if (line.find("[Lang]") != string::npos) {
			ofSetColor(80, 120, 140);
			font_p.drawString(line, panelX + 20, drawY);
		}
		// Everything else
		else {
			ofSetColor(COL_TEXT_DIM);
			font_p.drawString(line, panelX + 20, drawY);
		}

		drawY += sepy;
	}
}
void ofApp::draw_opciones() {
	float panelW = 500;
	float panelX = 30;           // left-aligned, consistent across pages
	float panelY = 44 - settingsScroll; // below the top screen-tab bar
	float fieldX = panelX + 175; // room for graph-render labels
	float fieldW = 200;
	float rowH = 28;
	float sepy = 40;
	int totalRows = FIELD_OSC_IP_OUT + 6; // fields + spout + ndi + osc ip + compo + activecompo + save
	float panelH = 55 + totalRows * sepy + 25;
	const float actionBtnW = 100;
	auto drawSettingsButton = [&](const ofRectangle &bounds,
		const string &label, const ofColor &accent, bool active) {
		const bool hovered = bounds.inside(
			ofGetMouseX(), ofGetMouseY());
		ofColor fillColor = active ?
			ofColor(accent, 215) :
			(hovered ? COL_BG_HOVER : COL_BG_BUTTON);
		ofSetColor(fillColor);
		ofDrawRectRounded(bounds, 4.0f);
		ofNoFill();
		ofSetColor(active || hovered ? accent : COL_BORDER_DEFAULT);
		ofSetLineWidth(1.0f);
		ofDrawRectRounded(bounds, 4.0f);
		ofFill();
		ofSetColor(active ? COL_TEXT_PRIMARY :
			(hovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		font_p.drawString(label,
			bounds.getCenter().x - font_p.stringWidth(label) * 0.5f,
			bounds.getBottom() - 7.0f);
	};

	// Glassmorphism panel background
	ofSetColor(ofColor(COL_BG_DARK, 235));
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 8);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 8);
	ofFill();
	ofSetLineWidth(1.0f);

	// Title
	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString("SETTINGS.XML Configuration", panelX + 15, panelY + 30);

	// Field labels & inputs
	string labels[FIELD_OSC_IP_OUT] = {
		"OSC Port In:",
		"OSC Port Out:",
		"Graph Render Width:",
		"Graph Render Height:",
		"BPM:"
	};

	for (int i = 0; i < FIELD_OSC_IP_OUT; i++) {
		float rowY = panelY + 55 + i * sepy;

		// Label
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString(labels[i], panelX + 15, rowY + rowH - 7);

		// Field background
		ofSetColor(focusedOptionsField == i ? COL_BG_HOVER : COL_BG_INPUT);
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);

		// Field border
		ofNoFill();
		if (focusedOptionsField == i) {
			ofSetColor(COL_ACCENT_CYAN);
			ofSetLineWidth(2.0f);
		} else {
			ofSetColor(COL_MAPPED_OFF);
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);

		// Field text with insertion caret if focused
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(optionsFieldText[i], fieldX + 6, rowY + rowH - 7);
		if (focusedOptionsField == i) {
			jp_textfield::drawCaret(font_p, optionsFieldText[i], optionsFieldCursor,
									fieldX + 6, rowY + rowH / 2, rowH - 8);
		}

		// AUTOTAP button next to BPM field
		if (i == FIELD_BPM) {
			float tapX = fieldX + fieldW + 10;
			drawSettingsButton(
				ofRectangle(tapX, rowY, actionBtnW, rowH),
				"AUTOTAP", COL_ACCENT_GOLD, false);
		}
		const char *fieldTooltips[] = {
			"Edit OSC input port",
			"Edit OSC output port",
			"Edit graph render width",
			"Edit graph render height",
			"Edit BPM"
		};
		jp_tooltip::draw(fieldTooltips[i], fieldX, rowY, fieldW, rowH);
	}

	// --- Toggle: Spout ---
	int toggleRow = FIELD_OSC_IP_OUT;
#ifdef SPOUT
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString("Spout Output", panelX + 15, rowY + rowH - 7);

		float btnX = fieldX;
		bool isOn = spoutActive;
		drawSettingsButton(
			ofRectangle(btnX, rowY, actionBtnW, rowH),
			isOn ? "ON" : "OFF", COL_ACCENT_GREEN, isOn);
		jp_tooltip::draw("Toggle Spout output",
			btnX, rowY, actionBtnW, rowH);
	}
	toggleRow++;
#endif

	// --- Toggle: NDI ---
#ifdef NDI
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString("NDI Output", panelX + 15, rowY + rowH - 7);

		float btnX = fieldX;
		bool isOn = ndiActive;
		drawSettingsButton(
			ofRectangle(btnX, rowY, actionBtnW, rowH),
			isOn ? "ON" : "OFF", COL_ACCENT_GREEN, isOn);
		jp_tooltip::draw("Toggle NDI output",
			btnX, rowY, actionBtnW, rowH);
	}
	toggleRow++;
#endif

	// --- OSC IP Out (editable text field) ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString("OSC IP Out:", panelX + 15, rowY + rowH - 7);

		int fieldIdx = FIELD_OSC_IP_OUT;
		// Field background
		ofSetColor(focusedOptionsField == fieldIdx ? COL_BG_HOVER : COL_BG_INPUT);
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		// Field border
		ofNoFill();
		if (focusedOptionsField == fieldIdx) {
			ofSetColor(COL_ACCENT_CYAN);
			ofSetLineWidth(2.0f);
		} else {
			ofSetColor(COL_MAPPED_OFF);
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		// Field text with insertion caret if focused
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(optionsFieldText[fieldIdx], fieldX + 6, rowY + rowH - 7);
		if (focusedOptionsField == fieldIdx) {
			jp_textfield::drawCaret(font_p, optionsFieldText[fieldIdx], optionsFieldCursor,
									fieldX + 6, rowY + rowH / 2, rowH - 8);
		}
		jp_tooltip::draw("Edit OSC output address", fieldX, rowY, fieldW, rowH);
	}
	toggleRow++;

	// --- Default Compo (editable text field + browse button) ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString("Default Compo:", panelX + 15, rowY + rowH - 7);

		int fieldIdx = FIELD_DEFAULT_COMPO;
		// Field background
		ofSetColor(focusedOptionsField == fieldIdx ? COL_BG_HOVER : COL_BG_INPUT);
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		// Field border
		ofNoFill();
		if (focusedOptionsField == fieldIdx) {
			ofSetColor(COL_ACCENT_CYAN);
			ofSetLineWidth(2.0f);
		} else {
			ofSetColor(COL_MAPPED_OFF);
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		// Field text with insertion caret if focused
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(optionsFieldText[fieldIdx], fieldX + 6, rowY + rowH - 7);
		if (focusedOptionsField == fieldIdx) {
			jp_textfield::drawCaret(font_p, optionsFieldText[fieldIdx], optionsFieldCursor,
									fieldX + 6, rowY + rowH / 2, rowH - 8);
		}

		// BROWSE button next to the field
		float browseX = fieldX + fieldW + 10;
		drawSettingsButton(
			ofRectangle(browseX, rowY, actionBtnW, rowH),
			"BROWSE", COL_ACCENT_CYAN, false);
		jp_tooltip::draw("Edit default composition path", fieldX, rowY, fieldW, rowH);
	}
	toggleRow++;

	// --- Active Compo (read-only display) ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(COL_ACCENT_CYAN);
		font_p.drawString("Active Compo:", panelX + 15, rowY + rowH - 7);

		// Show just the filename portion, or full path if short
		string activeName = ofFilePath::getFileName(savedirectory);
		if (activeName.empty()) activeName = "none";
		ofSetColor(COL_TEXT_DIM);
		font_p.drawString(activeName, fieldX, rowY + rowH - 7);
	}
	toggleRow++;

	// --- Save button ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		float saveW = fieldW;
		float saveX = fieldX;
		drawSettingsButton(
			ofRectangle(saveX, rowY, saveW, rowH),
			"SAVE SETTINGS", COL_ACCENT_CYAN, true);

		// Save feedback text
		if (saveFeedbackTime > 0 && ofGetElapsedTimef() - saveFeedbackTime < 3.0f) {
			ofSetColor(COL_MAPPED_ON);
			float fw = font_p.stringWidth(saveFeedbackText);
			font_p.drawString(saveFeedbackText,
				saveX + saveW / 2 - fw / 2,
				rowY + rowH + 20);
		} else {
			saveFeedbackTime = 0;
		}
	}

	// Hint text when focused
	if (focusedOptionsField >= 0) {
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("Enter to apply | Click outside to cancel", panelX + 15, panelY + panelH - 10);
	}
	drawLiveOutputSettings();
}

vector<string> ofApp::getLiveOutputSourceOptions() const
{
	vector<string> options;
	options.push_back("MAIN Active");
	const vector<string> boxNames = boxes.getBoxNames();
	options.insert(options.end(), boxNames.begin(), boxNames.end());
	return options;
}

ofApp::LiveOutputSettingsLayout
ofApp::getLiveOutputSettingsLayout() const
{
	LiveOutputSettingsLayout layout;
	const float margin = 30.0f;
	const float generalPanelW = 500.0f;
	const float generalPanelH =
		55.0f + (FIELD_OSC_IP_OUT + 6) * 40.0f + 25.0f;
	layout.twoColumns = ofGetWidth() >= 1080;
	layout.panel.x = layout.twoColumns ?
		margin + generalPanelW + 16.0f : margin;
	layout.panel.y = layout.twoColumns ?
		44.0f : 44.0f + generalPanelH + 16.0f;
	layout.panel.y -= settingsScroll;
	layout.panel.width = layout.twoColumns ?
		std::max(500.0f, ofGetWidth() - layout.panel.x - margin) :
		std::max(500.0f, ofGetWidth() - margin * 2.0f);
	layout.panel.height = generalPanelH;

	layout.addButton.set(
		layout.panel.getRight() - 58.0f,
		layout.panel.y + 13.0f, 22.0f, 22.0f);
	layout.deleteButton.set(
		layout.panel.getRight() - 30.0f,
		layout.panel.y + 13.0f, 22.0f, 22.0f);

	const float listW = std::min(184.0f, layout.panel.width * 0.34f);
	layout.list.set(layout.panel.x + 14.0f, layout.panel.y + 50.0f,
		listW, layout.panel.height - 64.0f);
	const int visibleRows = std::max(1, (int)(layout.list.height / 31.0f));
	const int maxListScroll = std::max(
		0, (int)liveOutputs.size() - visibleRows);
	const int firstOutput = ofClamp(
		liveOutputListScroll, 0, maxListScroll);
	for (int row = 0;
		row < visibleRows && firstOutput + row < (int)liveOutputs.size();
		row++)
	{
		layout.rows.push_back(ofRectangle(
			layout.list.x, layout.list.y + row * 31.0f,
			layout.list.width, 28.0f));
		layout.rowIndices.push_back(firstOutput + row);
	}

	const float editorX = layout.list.getRight() + 18.0f;
	const float editorW =
		layout.panel.getRight() - 14.0f - editorX;
	const float controlX = editorX + 94.0f;
	const float controlW = std::max(120.0f, editorW - 94.0f);
	float rowY = layout.panel.y + 78.0f;
	layout.enabledToggle.set(controlX, rowY, 70.0f, 28.0f);
	rowY += 43.0f;
	layout.sourceButton.set(controlX, rowY, controlW, 28.0f);
	rowY += 43.0f;
	layout.monitorButton.set(controlX, rowY, controlW, 28.0f);
	rowY += 47.0f;
	layout.windowModeButton.set(
		controlX, rowY, controlW * 0.5f - 3.0f, 28.0f);
	layout.fullscreenModeButton.set(
		controlX + controlW * 0.5f + 3.0f, rowY,
		controlW * 0.5f - 3.0f, 28.0f);
	rowY += 47.0f;
	layout.widthField.set(
		controlX, rowY, controlW * 0.5f - 3.0f, 28.0f);
	layout.heightField.set(
		controlX + controlW * 0.5f + 3.0f, rowY,
		controlW * 0.5f - 3.0f, 28.0f);

	if (liveOutputMenu != LIVE_OUTPUT_MENU_NONE)
	{
		const ofRectangle &anchor =
			liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE ?
				layout.sourceButton : layout.monitorButton;
		const int optionCount =
			liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE ?
				(int)getLiveOutputSourceOptions().size() :
				(int)liveOutputMonitors.size();
		const int visibleOptions = std::min(8, optionCount);
		const int firstOption = ofClamp(liveOutputMenuScroll, 0,
			std::max(0, optionCount - visibleOptions));
		layout.popup.set(anchor.x, anchor.getBottom() + 2.0f,
			anchor.width, visibleOptions * 27.0f + 4.0f);
		for (int i = 0; i < visibleOptions; i++)
		{
			layout.popupRows.push_back(ofRectangle(
				layout.popup.x + 2.0f,
				layout.popup.y + 2.0f + i * 27.0f,
				layout.popup.width - 4.0f, 25.0f));
			layout.popupOptionIndices.push_back(firstOption + i);
		}
	}
	return layout;
}

void ofApp::drawLiveOutputSettings()
{
	const LiveOutputSettingsLayout layout =
		getLiveOutputSettingsLayout();
	auto clippedText = [&](const string &text, float maxWidth) {
		if (font_p.stringWidth(text) <= maxWidth)
		{
			return text;
		}
		string clipped = text;
		while (!clipped.empty() &&
			font_p.stringWidth(clipped + "...") > maxWidth)
		{
			clipped.pop_back();
		}
		return clipped + "...";
	};
	auto drawControl = [&](const ofRectangle &bounds,
		const string &label, bool active, bool disabled = false) {
		ofSetColor(disabled ? COL_BG_DARK :
			(active ? COL_ACCENT_CYAN_DIM : COL_BG_INPUT));
		ofDrawRectRounded(bounds, 4.0f);
		ofNoFill();
		ofSetColor(active ? COL_ACCENT_CYAN :
			(disabled ? COL_TEXT_MUTED : COL_MAPPED_OFF));
		ofDrawRectRounded(bounds, 4.0f);
		ofFill();
		ofSetColor(disabled ? COL_TEXT_MUTED : COL_TEXT_PRIMARY);
		const string visible = bounds.width < 40.0f ?
			label : clippedText(label, bounds.width - 14.0f);
		const float textX = bounds.width < 40.0f ?
			bounds.getCenter().x - font_p.stringWidth(visible) * 0.5f :
			bounds.x + 7.0f;
		font_p.drawString(visible, textX,
			bounds.getBottom() - 7.0f);
	};

	ofSetColor(ofColor(COL_BG_DARK, 242));
	ofDrawRectRounded(layout.panel, 8.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(layout.panel, 8.0f);
	ofFill();
	ofSetLineWidth(1.0f);

	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString("LIVE OUTPUTS", layout.panel.x + 15.0f,
		layout.panel.y + 30.0f);
	drawControl(layout.addButton, "+", false);
	drawControl(layout.deleteButton, "x", false, liveOutputs.empty());

	ofSetColor(COL_BG_INPUT);
	ofDrawRectRounded(layout.list, 5.0f);
	if (liveOutputs.empty())
	{
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("No outputs", layout.list.x + 10.0f,
			layout.list.y + 24.0f);
	}
	for (int row = 0; row < (int)layout.rows.size(); row++)
	{
		const int outputIndex = layout.rowIndices[row];
		const LiveOutputRuntime &output = liveOutputs[outputIndex];
		const bool selected = outputIndex == selectedLiveOutput;
		const ofRectangle &bounds = layout.rows[row];
		if (selected)
		{
			ofSetColor(COL_ACCENT_CYAN_DIM);
			ofDrawRectRounded(bounds, 4.0f);
		}
		else if (bounds.inside(ofGetMouseX(), ofGetMouseY()))
		{
			ofSetColor(COL_BG_HOVER);
			ofDrawRectRounded(bounds, 4.0f);
		}

		ofSetColor(output.window ? COL_ACCENT_GREEN :
			(output.config.enabled ? COL_ACCENT_GOLD : COL_TEXT_MUTED));
		ofDrawCircle(bounds.x + 10.0f, bounds.getCenter().y, 3.5f);
		ofSetColor(selected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		font_p.drawString(getLiveOutputDisplayName(outputIndex),
			bounds.x + 20.0f, bounds.getBottom() - 8.0f);
	}

	if (selectedLiveOutput >= 0 &&
		selectedLiveOutput < (int)liveOutputs.size())
	{
		const LiveOutputRuntime &output =
			liveOutputs[selectedLiveOutput];
		const LiveOutputConfig &config = output.config;
		const float editorX = layout.list.getRight() + 18.0f;
		const float labelX = editorX;
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(getLiveOutputDisplayName(selectedLiveOutput),
			editorX, layout.panel.y + 59.0f);

		const float labelYs[] = {
			layout.enabledToggle.getBottom() - 7.0f,
			layout.sourceButton.getBottom() - 7.0f,
			layout.monitorButton.getBottom() - 7.0f,
			layout.windowModeButton.getBottom() - 7.0f,
			layout.widthField.getBottom() - 7.0f
		};
		const char *labels[] = {
			"Enabled", "Source", "Monitor", "Mode", "Resolution"
		};
		for (int i = 0; i < 5; i++)
		{
			ofSetColor(COL_TEXT_SECONDARY);
			font_p.drawString(labels[i], labelX, labelYs[i]);
		}

		drawControl(layout.enabledToggle,
			config.enabled ? "ON" : "OFF", config.enabled);
		const string sourceLabel =
			config.sourceMode == LIVE_OUTPUT_MAIN_ACTIVE ?
				"MAIN Active" : config.sourceBox;
		drawControl(layout.sourceButton,
			sourceLabel + "  v", false);

		const int monitorIndex = resolveLiveOutputMonitor(config);
		string monitorLabel = config.monitorName.empty() ?
			"Select monitor" : config.monitorName;
		if (monitorIndex >= 0)
		{
			const LiveOutputMonitor &monitor =
				liveOutputMonitors[monitorIndex];
			monitorLabel += " | " + ofToString(monitor.width) +
				"x" + ofToString(monitor.height);
			if (monitor.primary)
			{
				monitorLabel += " | Primary";
			}
		}
		drawControl(layout.monitorButton,
			monitorLabel + "  v", false);
		drawControl(layout.windowModeButton, "WINDOW",
			!config.fullscreen);
		drawControl(layout.fullscreenModeButton, "FULLSCREEN",
			config.fullscreen);
		drawControl(layout.widthField,
			liveOutputFieldText[0],
			focusedLiveOutputField == 0, config.fullscreen);
		drawControl(layout.heightField,
			liveOutputFieldText[1],
			focusedLiveOutputField == 1, config.fullscreen);
		if (focusedLiveOutputField >= 0 && !config.fullscreen)
		{
			const ofRectangle &field = focusedLiveOutputField == 0 ?
				layout.widthField : layout.heightField;
			jp_textfield::drawCaret(font_p,
				liveOutputFieldText[focusedLiveOutputField],
				liveOutputFieldCursor, field.x + 7.0f,
				field.getCenter().y, field.height - 8.0f);
		}

		const bool sourceMissing =
			config.sourceMode == LIVE_OUTPUT_FIXED_BOX &&
			!boxes.hasBoxName(config.sourceBox);
		string status = "Disabled";
		ofColor statusColor = COL_TEXT_MUTED;
		if (config.enabled && monitorIndex < 0)
		{
			status = "Monitor disconnected";
			statusColor = COL_ACCENT_RED;
		}
		else if (sourceMissing)
		{
			status = "Missing source";
			statusColor = COL_ACCENT_RED;
		}
		else if (output.window)
		{
			status = "Live";
			statusColor = COL_ACCENT_GREEN;
		}
		else if (config.enabled)
		{
			status = "Opening...";
			statusColor = COL_ACCENT_GOLD;
		}
		ofSetColor(statusColor);
		font_p.drawString(status, layout.sourceButton.x,
			layout.heightField.getBottom() + 31.0f);

		jp_tooltip::draw("Enable this live output",
			layout.enabledToggle.x, layout.enabledToggle.y,
			layout.enabledToggle.width, layout.enabledToggle.height);
		jp_tooltip::draw("Choose the render source",
			layout.sourceButton.x, layout.sourceButton.y,
			layout.sourceButton.width, layout.sourceButton.height);
		jp_tooltip::draw("Choose the output monitor",
			layout.monitorButton.x, layout.monitorButton.y,
			layout.monitorButton.width, layout.monitorButton.height);
		jp_tooltip::draw("Use a resizable output window",
			layout.windowModeButton.x, layout.windowModeButton.y,
			layout.windowModeButton.width,
			layout.windowModeButton.height);
		jp_tooltip::draw("Use the monitor native fullscreen mode",
			layout.fullscreenModeButton.x,
			layout.fullscreenModeButton.y,
			layout.fullscreenModeButton.width,
			layout.fullscreenModeButton.height);
	}
	else
	{
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("Add an output to configure it",
			layout.list.getRight() + 18.0f,
			layout.panel.y + 82.0f);
	}

	jp_tooltip::draw("Add live output",
		layout.addButton.x, layout.addButton.y,
		layout.addButton.width, layout.addButton.height);
	if (!liveOutputs.empty())
	{
		jp_tooltip::draw("Delete selected live output",
			layout.deleteButton.x, layout.deleteButton.y,
			layout.deleteButton.width, layout.deleteButton.height);
	}

	if (liveOutputMenu != LIVE_OUTPUT_MENU_NONE)
	{
		const vector<string> sourceOptions =
			getLiveOutputSourceOptions();
		ofSetColor(COL_BG_DARK);
		ofDrawRectRounded(layout.popup, 4.0f);
		ofNoFill();
		ofSetColor(COL_ACCENT_CYAN);
		ofDrawRectRounded(layout.popup, 4.0f);
		ofFill();
		for (int row = 0; row < (int)layout.popupRows.size(); row++)
		{
			const ofRectangle &bounds = layout.popupRows[row];
			const int optionIndex = layout.popupOptionIndices[row];
			if (bounds.inside(ofGetMouseX(), ofGetMouseY()))
			{
				ofSetColor(COL_BG_HOVER);
				ofDrawRectangle(bounds);
			}
			string label;
			if (liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE)
			{
				label = sourceOptions[optionIndex];
			}
			else
			{
				const LiveOutputMonitor &monitor =
					liveOutputMonitors[optionIndex];
				label = monitor.name + " | " +
					ofToString(monitor.width) + "x" +
					ofToString(monitor.height);
				if (monitor.primary)
				{
					label += " | Primary";
				}
			}
			ofSetColor(COL_TEXT_PRIMARY);
			font_p.drawString(
				clippedText(label, bounds.width - 12.0f),
				bounds.x + 6.0f, bounds.getBottom() - 7.0f);
		}
	}
}

void ofApp::setSelectedLiveOutput(int index)
{
	selectedLiveOutput = liveOutputs.empty() ? -1 :
		ofClamp(index, 0, (int)liveOutputs.size() - 1);
	focusedLiveOutputField = -1;
	liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
	initLiveOutputFields();
}

void ofApp::initLiveOutputFields()
{
	if (selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		liveOutputFieldText[0].clear();
		liveOutputFieldText[1].clear();
		focusedLiveOutputField = -1;
		return;
	}
	liveOutputFieldText[0] = ofToString(
		liveOutputs[selectedLiveOutput].config.width);
	liveOutputFieldText[1] = ofToString(
		liveOutputs[selectedLiveOutput].config.height);
}

void ofApp::applyLiveOutputField()
{
	if (focusedLiveOutputField < 0 ||
		selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		focusedLiveOutputField = -1;
		return;
	}
	LiveOutputRuntime &output = liveOutputs[selectedLiveOutput];
	const int value = ofClamp(ofToInt(
		liveOutputFieldText[focusedLiveOutputField]), 64, 16384);
	if (focusedLiveOutputField == 0)
	{
		output.config.width = value;
	}
	else
	{
		output.config.height = value;
	}
	focusedLiveOutputField = -1;
	initLiveOutputFields();
	if (output.window && !output.config.fullscreen)
	{
		output.window->setWindowShape(
			output.config.width, output.config.height);
	}
	saveSettings();
}

bool ofApp::handleLiveOutputSettingsClick(int x, int y, int button)
{
	if (button != OF_MOUSE_BUTTON_LEFT)
	{
		return false;
	}
	LiveOutputSettingsLayout layout = getLiveOutputSettingsLayout();

	if (liveOutputMenu != LIVE_OUTPUT_MENU_NONE)
	{
		for (int row = 0; row < (int)layout.popupRows.size(); row++)
		{
			if (!layout.popupRows[row].inside(x, y))
			{
				continue;
			}
			if (selectedLiveOutput < 0 ||
				selectedLiveOutput >= (int)liveOutputs.size())
			{
				liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
				return true;
			}
			const int optionIndex =
				layout.popupOptionIndices[row];
			LiveOutputRuntime &output =
				liveOutputs[selectedLiveOutput];
			if (liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE)
			{
				const vector<string> options =
					getLiveOutputSourceOptions();
				if (optionIndex == 0)
				{
					output.config.sourceMode =
						LIVE_OUTPUT_MAIN_ACTIVE;
				}
				else if (optionIndex < (int)options.size())
				{
					output.config.sourceMode =
						LIVE_OUTPUT_FIXED_BOX;
					output.config.sourceBox =
						options[optionIndex];
				}
			}
			else if (optionIndex >= 0 &&
				optionIndex < (int)liveOutputMonitors.size())
			{
				const LiveOutputMonitor &monitor =
					liveOutputMonitors[optionIndex];
				output.config.monitorName = monitor.name;
					output.config.monitorIndex = monitor.index;
					output.config.hasPosition = false;
					requestLiveOutputRecreate(selectedLiveOutput);
					updateLiveOutputs();
				}
			liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
			liveOutputMenuScroll = 0;
			saveSettings();
			return true;
		}
		if (layout.popup.inside(x, y))
		{
			return true;
		}
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		liveOutputMenuScroll = 0;
		layout = getLiveOutputSettingsLayout();
	}

	if (focusedLiveOutputField >= 0 &&
		!layout.widthField.inside(x, y) &&
		!layout.heightField.inside(x, y))
	{
		applyLiveOutputField();
		layout = getLiveOutputSettingsLayout();
	}

	if (layout.addButton.inside(x, y))
	{
		addLiveOutput();
		return true;
	}
	if (!liveOutputs.empty() &&
		layout.deleteButton.inside(x, y))
	{
		removeSelectedLiveOutput();
		return true;
	}
	for (int row = 0; row < (int)layout.rows.size(); row++)
	{
		if (layout.rows[row].inside(x, y))
		{
			setSelectedLiveOutput(layout.rowIndices[row]);
			return true;
		}
	}

	if (selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		return layout.panel.inside(x, y);
	}
	LiveOutputRuntime &output = liveOutputs[selectedLiveOutput];
	if (layout.enabledToggle.inside(x, y))
	{
		output.config.enabled = !output.config.enabled;
		output.createAttempted = false;
		if (output.config.enabled)
		{
			requestLiveOutputRecreate(selectedLiveOutput);
		}
		else
		{
			output.closePending = true;
		}
		updateLiveOutputs();
		saveSettings();
		return true;
	}
	if (layout.sourceButton.inside(x, y))
	{
		liveOutputMenu = LIVE_OUTPUT_MENU_SOURCE;
		liveOutputMenuScroll = 0;
		focusedLiveOutputField = -1;
		return true;
	}
	if (layout.monitorButton.inside(x, y))
	{
		refreshLiveOutputMonitors();
		liveOutputMenu = LIVE_OUTPUT_MENU_MONITOR;
		liveOutputMenuScroll = 0;
		focusedLiveOutputField = -1;
		return true;
	}
	if (layout.windowModeButton.inside(x, y))
	{
		if (output.config.fullscreen)
		{
			output.config.fullscreen = false;
			requestLiveOutputRecreate(selectedLiveOutput);
			updateLiveOutputs();
			saveSettings();
		}
		return true;
	}
	if (layout.fullscreenModeButton.inside(x, y))
	{
		if (!output.config.fullscreen)
		{
			output.config.fullscreen = true;
			requestLiveOutputRecreate(selectedLiveOutput);
			updateLiveOutputs();
			saveSettings();
		}
		return true;
	}
	if (!output.config.fullscreen &&
		(layout.widthField.inside(x, y) ||
		 layout.heightField.inside(x, y)))
	{
		focusedLiveOutputField =
			layout.widthField.inside(x, y) ? 0 : 1;
		liveOutputFieldCursor =
			liveOutputFieldText[focusedLiveOutputField].size();
		focusedOptionsField = -1;
		return true;
	}
	return layout.panel.inside(x, y);
}

void ofApp::clampSettingsScroll()
{
	if (ofGetWidth() >= 1080)
	{
		settingsScroll = 0.0f;
		return;
	}
	const float panelH =
		55.0f + (FIELD_OSC_IP_OUT + 6) * 40.0f + 25.0f;
	const float contentHeight =
		44.0f + panelH + 16.0f + panelH + 30.0f;
	settingsScroll = ofClamp(settingsScroll, 0.0f,
		std::max(0.0f, contentHeight - ofGetHeight()));
}

void ofApp::initOptionsFields() {
	optionsFieldText[FIELD_OSC_PORT_IN] = ofToString(receiver.getPort());
	optionsFieldText[FIELD_OSC_PORT_OUT] = ofToString(sender.getPort());
	optionsFieldText[FIELD_RENDER_WIDTH] = ofToString(jp_constants::renderWidth);
	optionsFieldText[FIELD_RENDER_HEIGHT] = ofToString(jp_constants::renderHeight);
	optionsFieldText[FIELD_BPM] = ofToString((int)jp_constants::bpm);
	optionsFieldText[FIELD_OSC_IP_OUT] = sender.getHost();
	if (optionsFieldText[FIELD_OSC_IP_OUT].empty()) {
		optionsFieldText[FIELD_OSC_IP_OUT] = "127.0.0.1";
	}
	optionsFieldText[FIELD_DEFAULT_COMPO] = defaultCompoPath;
	if (optionsFieldText[FIELD_DEFAULT_COMPO].empty()) {
		optionsFieldText[FIELD_DEFAULT_COMPO] = "savefiles/data.xml";
	}
	focusedOptionsField = -1;
	focusedLiveOutputField = -1;
	liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
	refreshLiveOutputMonitors();
	bool reopenConnectedOutput = false;
	for (LiveOutputRuntime &output : liveOutputs)
	{
		if (output.config.enabled && !output.window &&
			resolveLiveOutputMonitor(output.config) >= 0)
		{
			output.createAttempted = false;
			reopenConnectedOutput = true;
		}
	}
	if (reopenConnectedOutput)
	{
		updateLiveOutputs();
	}
	initLiveOutputFields();
	clampSettingsScroll();
}

void ofApp::applyOptionsField() {
	for (int i = 0; i < FIELD_OSC_IP_OUT; i++) {
		string text = optionsFieldText[i];
		if (text.empty()) continue;
		int val = ofToInt(text);
		switch (i) {
			case FIELD_OSC_PORT_IN:
				receiver.setup(val);
				break;
			case FIELD_OSC_PORT_OUT:
				sender.setup(sender.getHost(), val);
				break;
			case FIELD_RENDER_WIDTH:
				jp_constants::setrenderWidth(val);
				break;
			case FIELD_RENDER_HEIGHT:
				jp_constants::setrenderHeight(val);
				break;
			case FIELD_BPM:
				jp_constants::setBpm((float)val);
				break;
			}
		}
	// Apply OSC IP Out (string field)
	{
		string ip = optionsFieldText[FIELD_OSC_IP_OUT];
		if (!ip.empty()) {
			sender.setup(ip, sender.getPort());
			cout << "OSC IP Out set to: " << ip << endl;
		}
	}
	// Apply Default Compo path (string field)
	{
		string path = optionsFieldText[FIELD_DEFAULT_COMPO];
		if (!path.empty()) {
			defaultCompoPath = path;
			cout << "Default compo set to: " << path << endl;
		}
	}
	saveSettings();
	focusedOptionsField = -1;
}
void ofApp::autoTap() {
	float now = ofGetElapsedTimef();
	// Keep only taps within the last 3 seconds
	float cutoff = now - 3.0f;
	for (int i = tapTimestamps.size() - 1; i >= 0; i--) {
		if (tapTimestamps[i] < cutoff) {
			tapTimestamps.erase(tapTimestamps.begin() + i);
		}
	}
	tapTimestamps.push_back(now);
	jp_constants::syncBeat(now);

	if (tapTimestamps.size() >= 2) {
		// Calculate average interval
		float totalInterval = 0;
		for (size_t i = 1; i < tapTimestamps.size(); i++) {
			totalInterval += tapTimestamps[i] - tapTimestamps[i - 1];
		}
		float avgInterval = totalInterval / (tapTimestamps.size() - 1);
		int bpmVal = (int)(60.0f / avgInterval + 0.5f);
		bpmVal = ofClamp(bpmVal, 0, 999);
		optionsFieldText[FIELD_BPM] = ofToString(bpmVal);
		jp_constants::setBpm((float)bpmVal);
	}
}
void ofApp::scanShaders() {
	shaderFolders.clear();

	// Only scan these specific root folders (no sub-subdirectories)
	vector<string> targetFolders = { "blending", "contrib", "generative", "imageprocessing" };
	int helperFragmentsSkipped = 0;
	auto appendStandaloneShader = [&](ShaderFolder &folder, const string &path) {
		if (!shaderHasMain(path)) {
			helperFragmentsSkipped++;
			return;
		}
		ShaderEntry entry;
		entry.name = ofFilePath::getBaseName(path);
		entry.path = path;
		folder.shaders.push_back(entry);
	};

	// Root shaders/ folder (files directly in shaders/)
	{
		ShaderFolder rootFolder;
		rootFolder.name = "root";
		rootFolder.path = "shaders";
		rootFolder.expanded = true;

		ofDirectory rootDir;
		rootDir.listDir("shaders");
		rootDir.sort();
		for (size_t i = 0; i < rootDir.size(); i++) {
			if (rootDir.getFile(i).isDirectory()) continue;
			string path = rootDir.getPath(i);
			string ext = ofToLower(ofFilePath::getFileExt(path));
			if (ext == "frag") {
				appendStandaloneShader(rootFolder, path);
			}
		}
		if (!rootFolder.shaders.empty()) {
			shaderFolders.push_back(rootFolder);
		}
	}

	// Scan each target folder (only immediate frag files, no subdirs)
	for (const string &folderName : targetFolders) {
		string folderPath = "shaders/" + folderName;

		ShaderFolder folder;
		folder.name = folderName;
		folder.path = folderPath;
		folder.expanded = false;

		ofDirectory dir;
		dir.listDir(folderPath);
		dir.sort();
		for (size_t j = 0; j < dir.size(); j++) {
			string path = dir.getPath(j);
			if (dir.getFile(j).isDirectory()) continue;
			string ext = ofToLower(ofFilePath::getFileExt(path));
			if (ext == "frag") {
				appendStandaloneShader(folder, path);
			}
		}
		if (!folder.shaders.empty()) {
			shaderFolders.push_back(folder);
		}
	}

	shaderScroll = 0;
	selectedShaderFolder = -1;
	selectedShaderIndex = -1;
	previewShaderLoaded = false;
	previewShaderPath.clear();
	lastPreviewRenderTime = -1.0f;
	loadFavorites();
	rebuildFavoritesFolder(); // pins a "favorites" folder on top when any exist
	cout << "Shader index: found " << shaderFolders.size() << " folders" << endl;
	int totalShaders = 0;
	for (auto &f : shaderFolders) {
		if (!f.isFavorites) totalShaders += (int)f.shaders.size();
	}
	cout << "  total shaders: " << totalShaders << endl;
	if (helperFragmentsSkipped > 0) {
		cout << "  helper fragments hidden: " << helperFragmentsSkipped << endl;
	}
}
bool ofApp::isFavorite(const string &path) const {
	return std::find(favoritePaths.begin(), favoritePaths.end(), path) != favoritePaths.end();
}
void ofApp::loadFavorites() {
	favoritePaths.clear();
	string p = ofToDataPath("shader_favorites.xml");
	if (!ofFile(p).exists()) return;
	ofXml xml;
	if (!xml.load(p)) return;
	auto favs = xml.find("/favorite");
	for (auto &n : favs) {
		string s = n.getValue();
		if (!s.empty()) favoritePaths.push_back(s);
	}
}
void ofApp::saveFavorites() {
	ofXml xml;
	for (auto &p : favoritePaths) {
		xml.appendChild("favorite").set(p);
	}
	xml.save(ofToDataPath("shader_favorites.xml"));
}
void ofApp::toggleFavorite(const string &path) {
	auto it = std::find(favoritePaths.begin(), favoritePaths.end(), path);
	if (it == favoritePaths.end()) {
		favoritePaths.push_back(path);
	} else {
		favoritePaths.erase(it);
	}
	saveFavorites();
	rebuildFavoritesFolder();
}
void ofApp::rebuildFavoritesFolder() {
	// Preserve the current selection by path across the rebuild (folder indices
	// shift when the favorites folder appears/disappears).
	string selPath;
	bool selectedWasFavorites = false;
	if (selectedShaderFolder >= 0 && selectedShaderFolder < (int)shaderFolders.size() &&
		selectedShaderIndex >= 0 && selectedShaderIndex < (int)shaderFolders[selectedShaderFolder].shaders.size()) {
		selPath = shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].path;
		selectedWasFavorites = shaderFolders[selectedShaderFolder].isFavorites;
	}

	// Drop any existing favorites pseudo-folder so we rebuild it cleanly.
	if (!shaderFolders.empty() && shaderFolders.front().isFavorites) {
		favoritesFolderExpanded = shaderFolders.front().expanded;
		shaderFolders.erase(shaderFolders.begin());
	}

	// Re-mark real entries and collect the favorited ones (in file order).
	ShaderFolder fav;
	fav.name = "favorites";
	fav.isFavorites = true;
	fav.expanded = favoritesFolderExpanded;
	for (auto &folder : shaderFolders) {
		for (auto &e : folder.shaders) {
			e.favorite = isFavorite(e.path);
		}
	}
	for (const string &favPath : favoritePaths) {
		for (auto &folder : shaderFolders) {
			bool found = false;
			for (auto &e : folder.shaders) {
				if (e.path == favPath) {
					ShaderEntry c = e;
					c.favorite = true;
					fav.shaders.push_back(c);
					found = true;
					break;
				}
			}
			if (found) break;
		}
	}
	if (favoritesDisplayMode == FAVORITES_TOP && !fav.shaders.empty()) {
		shaderFolders.insert(shaderFolders.begin(), fav);
	}

	// Restore selection by path.
	selectedShaderFolder = -1;
	selectedShaderIndex = -1;
	if (!selPath.empty()) {
		for (int pass = 0; pass < 2 && selectedShaderIndex < 0; pass++) {
			for (size_t f = 0; f < shaderFolders.size() && selectedShaderIndex < 0; f++) {
				bool preferredFolder = selectedWasFavorites ?
					shaderFolders[f].isFavorites : !shaderFolders[f].isFavorites;
				if (pass == 0 && !preferredFolder) continue;
				for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
					if (shaderFolders[f].shaders[s].path == selPath) {
						selectedShaderFolder = (int)f;
						selectedShaderIndex = (int)s;
						break;
					}
				}
			}
		}
	}
	rebuildShaderFolderOrder();
}
void ofApp::rebuildShaderFolderOrder() {
	shaderFolderOrder.clear();
	shaderFolderOrder.resize(shaderFolders.size());
	for (int f = 0; f < (int)shaderFolders.size(); f++) {
		const ShaderFolder &folder = shaderFolders[f];
		vector<int> &indices = shaderFolderOrder[f];
		indices.reserve(folder.shaders.size());
		for (int i = 0; i < (int)folder.shaders.size(); i++) {
			indices.push_back(i);
		}
		if (favoritesDisplayMode == FAVORITES_IN_FOLDERS && !folder.isFavorites) {
			std::stable_partition(indices.begin(), indices.end(), [&](int shaderIndex) {
				return folder.shaders[shaderIndex].favorite;
			});
		}
	}
}
void ofApp::toggleFavoritesDisplayMode() {
	favoritesDisplayMode = favoritesDisplayMode == FAVORITES_TOP ?
		FAVORITES_IN_FOLDERS : FAVORITES_TOP;
	rebuildFavoritesFolder();
	shaderScroll = 0;
	ensureShaderSelectionVisible();
	saveSettings();
}
ofApp::ShaderBrowserLayout ofApp::getShaderBrowserLayout() const {
	ShaderBrowserLayout layout;
	const float panelX = 30.0f;
	const float panelY = 44.0f;
	const float panelW = std::max(300.0f, ofGetWidth() * 0.5f - panelX - 4.0f);
	const float panelH = std::max(420.0f, ofGetHeight() - panelY - 30.0f);
	const float inset = 15.0f;
	const float contentX = panelX + inset;
	const float contentW = panelW - inset * 2.0f;
	const float footerH = 30.0f;
	const float footerY = panelY + panelH - footerH - 10.0f;
	const float previewH = 170.0f;
	const float previewY = footerY - previewH - 10.0f;
	const float searchY = panelY + 64.0f;
	const float searchH = 26.0f;
	const float buttonGap = 8.0f;
	const float footerW = (contentW - buttonGap * 2.0f) / 3.0f;

	layout.panel.set(panelX, panelY, panelW, panelH);
	layout.titleBaseline = panelY + 26.0f;
	layout.hintBaseline = panelY + 47.0f;
	layout.favoritesModeButton.set(panelX + panelW - inset - 52.0f, panelY + 10.0f, 52.0f, 24.0f);
	layout.search.set(contentX, searchY, contentW, searchH);
	layout.searchClear.set(contentX + contentW - searchH, searchY, searchH, searchH);
	layout.list.set(contentX, searchY + searchH + 9.0f,
		contentW, std::max(0.0f, previewY - 8.0f - (searchY + searchH + 9.0f)));
	layout.preview.set(contentX, previewY, contentW, previewH);
	layout.loadButton.set(contentX, footerY, footerW, footerH);
	layout.bindButton.set(contentX + footerW + buttonGap, footerY, footerW, footerH);
	layout.editButton.set(contentX + (footerW + buttonGap) * 2.0f, footerY, footerW, footerH);
	return layout;
}
const vector<int> &ofApp::getOrderedShaderIndices(int folderIndex) const {
	static const vector<int> empty;
	if (folderIndex < 0 || folderIndex >= (int)shaderFolderOrder.size()) return empty;
	return shaderFolderOrder[folderIndex];
}
vector<ofApp::ShaderBrowserRow> ofApp::buildShaderBrowserRows() const {
	vector<ShaderBrowserRow> rows;
	const bool searchActive = !shaderSearchText.empty();
	const string searchLower = ofToLower(shaderSearchText);

	for (int f = 0; f < (int)shaderFolders.size(); f++) {
		const ShaderFolder &folder = shaderFolders[f];
		if (searchActive && folder.isFavorites) continue;

		vector<int> visibleShaders;
		if (searchActive) {
			const bool folderMatch =
				ofToLower(folder.name).find(searchLower) != string::npos;
			for (int shaderIndex : getOrderedShaderIndices(f)) {
				if (folderMatch ||
					ofToLower(folder.shaders[shaderIndex].name).find(searchLower) != string::npos) {
					visibleShaders.push_back(shaderIndex);
				}
			}
			if (!folderMatch && visibleShaders.empty()) continue;
		}

		ShaderBrowserRow folderRow;
		folderRow.folderHeader = true;
		folderRow.folderIndex = f;
		folderRow.height = 19.0f;
		rows.push_back(folderRow);

		if (!searchActive && !folder.expanded) continue;
		const vector<int> &shaderIndices =
			searchActive ? visibleShaders : getOrderedShaderIndices(f);
		for (int shaderIndex : shaderIndices) {
			ShaderBrowserRow shaderRow;
			shaderRow.folderIndex = f;
			shaderRow.shaderIndex = shaderIndex;
			shaderRow.height = 18.0f;
			rows.push_back(shaderRow);
		}
	}
	return rows;
}
int ofApp::getMaxShaderScroll(const vector<ShaderBrowserRow> &rows, float viewportHeight) const {
	if (rows.empty()) return 0;
	float usedHeight = 0.0f;
	int firstVisible = (int)rows.size() - 1;
	for (int i = (int)rows.size() - 1; i >= 0; i--) {
		if (usedHeight + rows[i].height > viewportHeight && i < (int)rows.size() - 1) break;
		usedHeight += rows[i].height;
		firstVisible = i;
	}
	return std::max(0, firstVisible);
}
void ofApp::clampShaderScroll(const ShaderBrowserLayout &layout) {
	const vector<ShaderBrowserRow> rows = buildShaderBrowserRows();
	shaderScroll = ofClamp(shaderScroll, 0, getMaxShaderScroll(rows, layout.list.height));
}
vector<ofApp::ShaderBrowserRow> ofApp::getVisibleShaderBrowserRows(const ShaderBrowserLayout &layout) const {
	const vector<ShaderBrowserRow> rows = buildShaderBrowserRows();
	return getVisibleShaderBrowserRows(layout, rows);
}
vector<ofApp::ShaderBrowserRow> ofApp::getVisibleShaderBrowserRows(
	const ShaderBrowserLayout &layout,
	const vector<ShaderBrowserRow> &rows) const {
	vector<ShaderBrowserRow> visibleRows;
	const int start = ofClamp(shaderScroll, 0, getMaxShaderScroll(rows, layout.list.height));
	float rowY = layout.list.y;
	for (int i = start; i < (int)rows.size(); i++) {
		if (rowY + rows[i].height > layout.list.getBottom() + 0.01f) break;
		ShaderBrowserRow row = rows[i];
		row.bounds.set(layout.list.x, rowY, layout.list.width, row.height);
		visibleRows.push_back(row);
		rowY += row.height;
	}
	return visibleRows;
}
void ofApp::clearShaderSearch() {
	shaderSearchText.clear();
	shaderSearchCursor = 0;
	shaderScroll = 0;
	shaderSearchFocused = true;
	if (selectedShaderFolder >= 0 && selectedShaderFolder < (int)shaderFolders.size() &&
		!shaderFolders[selectedShaderFolder].isFavorites) {
		shaderFolders[selectedShaderFolder].expanded = true;
	}
	ensureShaderSelectionVisible();
}
string ofApp::getSelectedShaderPath() const {
	if (selectedShaderFolder < 0 || selectedShaderIndex < 0) return "";
	if (selectedShaderFolder >= (int)shaderFolders.size()) return "";
	if (selectedShaderIndex >= (int)shaderFolders[selectedShaderFolder].shaders.size()) return "";
	return shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].path;
}
void ofApp::ensurePreviewFbo() {
	const int renderWidth = std::max(1, jp_constants::renderWidth);
	const int renderHeight = std::max(1, jp_constants::renderHeight);
	const float scale = std::min({
		1.0f,
		PREVIEW_MAX_WIDTH / (float)renderWidth,
		PREVIEW_MAX_HEIGHT / (float)renderHeight
	});
	const int previewWidth = std::max(1, (int)std::round(renderWidth * scale));
	const int previewHeight = std::max(1, (int)std::round(renderHeight * scale));
	if (!previewFbo.isAllocated() ||
		(int)previewFbo.getWidth() != previewWidth ||
		(int)previewFbo.getHeight() != previewHeight) {
		previewFbo.allocate(previewWidth, previewHeight);
	}
}
void ofApp::renderShaderPreview(bool useLiveMouse) {
	if (!previewShaderLoaded) return;
	ensurePreviewFbo();

	const float mouseX = useLiveMouse ?
		ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1) : 0.5f;
	const float mouseY = useLiveMouse ?
		ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1) : 0.5f;
	const float pressedX = useLiveMouse ?
		ofMap(jp_constants::mousePressedPos.x, 0, ofGetWidth(), 0, 1) : 0.5f;
	const float pressedY = useLiveMouse ?
		ofMap(jp_constants::mousePressedPos.y, 0, ofGetHeight(), 0, 1) : 0.5f;
	const int previewFrame = useLiveMouse ? ofGetFrameNum() : 0;

	previewFbo.begin();
	ofClear(0, 0, 0, 255);
	previewShader.begin();
	previewShader.setUniform1f("time", ofGetElapsedTimef());
	previewShader.setUniform2f("resolution", previewFbo.getWidth(), previewFbo.getHeight());
	previewShader.setUniform1f("bpm", jp_constants::bpm);
	previewShader.setUniform4f("mouse", mouseX, mouseY, pressedX, pressedY);
	previewShader.setUniform2f("window_mouse", mouseX, mouseY);
	previewShader.setUniform1i("globalframeNum", previewFrame);
	previewShader.setUniform1i("boxframeNum", previewFrame);
	if (previewRdmActive && !previewRdmValues.empty()) {
		for (int i = 0;
			i < (int)previewRdmValues.size() && i < (int)previewUniformNames.size();
			i++) {
			previewShader.setUniform1f(previewUniformNames[i], previewRdmValues[i]);
		}
	}
	if (previewImg1.isAllocated()) {
		previewImg1.getTexture().bind(0);
		previewShader.setUniform1i("texture1", 0);
		previewShader.setUniform1i("textura1", 0);
		previewShader.setUniform1i("input_texture", 0);
		previewShader.setUniform1i("tex0", 0);
		previewShader.setUniform1i("textura", 0);
		previewShader.setUniform1i("texture", 0);
	}
	if (previewImg2.isAllocated()) {
		previewImg2.getTexture().bind(1);
		previewShader.setUniform1i("texture2", 1);
		previewShader.setUniform1i("textura2", 1);
		previewShader.setUniform1i("tex1", 1);
	}
	ofSetColor(COL_TEXT_PRIMARY);
	ofDrawRectangle(0, 0, previewFbo.getWidth(), previewFbo.getHeight());
	previewShader.end();
	if (previewImg1.isAllocated()) previewImg1.getTexture().unbind(0);
	if (previewImg2.isAllocated()) previewImg2.getTexture().unbind(1);
	previewFbo.end();
	lastPreviewRenderTime = ofGetElapsedTimef();
}
void ofApp::selectShaderForPreview(int f, int s) {
	if (f < 0 || f >= (int)shaderFolders.size()) return;
	if (s < 0 || s >= (int)shaderFolders[f].shaders.size()) return;
	const string shaderPath = shaderFolders[f].shaders[s].path;
	selectedShaderFolder = f;
	selectedShaderIndex = s;
	if (shaderPath == previewShaderPath && previewShaderLoaded) return;

	// Load into preview shader (kept for RDM/EDIT + optional preview render).
	previewShaderPath = shaderPath;
	previewShader.unload();
	previewShaderLoaded = false;
	lastPreviewRenderTime = -1.0f;
	previewUniformNames.clear();
	previewUniformMins.clear();
	previewUniformMaxs.clear();
	previewRdmValues.clear();
	previewRdmActive = false;
	if (previewShader.load("shaders/default.vert", shaderPath)) {
		previewShaderLoaded = true;
		// Parse user-defined uniform float declarations for RDM
		ofBuffer shaderBuf2 = ofBufferFromFile(shaderPath);
		for (auto line : shaderBuf2.getLines()) {
			if (line.rfind("uniform", 0) == 0 && line.find("float") != string::npos) {
				string sl = line;
				vector<string> tokens;
				string tok;
				for (char c : sl) {
					if (c == ' ' || c == '\t') { if (!tok.empty()) { tokens.push_back(tok); tok.clear(); } }
					else { tok += c; }
				}
				if (!tok.empty()) tokens.push_back(tok);
				if (!tokens.empty()) {
					string &last = tokens.back();
					if (!last.empty() && last.back() == ';') last.pop_back();
				}
				for (int ti = 2; ti < (int)tokens.size(); ti++) {
					if (tokens[ti] != "=" && tokens[ti] != "float" && tokens[ti] != "uniform") {
						string uname = tokens[ti];
						if (uname == "time" || uname == "resolution" || uname == "bpm" ||
							uname == "mouse" || uname == "window_mouse" ||
							uname == "globalframeNum" || uname == "boxframeNum" ||
							uname == "texture1" || uname == "texture2" ||
							uname == "textura" || uname == "textura1" || uname == "textura2" ||
							uname == "tex0" || uname == "tex1" || uname == "input_texture" ||
							uname == "texture") continue;
						previewUniformNames.push_back(uname);
						previewUniformMins.push_back(0.0f);
						previewUniformMaxs.push_back(1.0f);
						previewRdmValues.push_back(0.0f);
						break;
					}
				}
			}
		}
		renderShaderPreview(false);
	}
}
void ofApp::loadSelectedShaderBox() {
	if (selectedShaderFolder < 0 || selectedShaderIndex < 0) return;
	if (selectedShaderFolder >= (int)shaderFolders.size()) return;
	if (selectedShaderIndex >= (int)shaderFolders[selectedShaderFolder].shaders.size()) return;
	string selPath = shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].path;
	cout << "SHADER INDEX: Loading " << selPath << endl;
	// Add near the centre of the right (visible) half so the box appears in the
	// uncovered area; grid subsequent adds so they don't stack.
	float sepx = 112, sepy = 128;
	int cols = 3;
	int row = loadBoxCount / cols;
	int col = loadBoxCount % cols;
	float rightHalfX = ofGetWidth() * 0.75f;
	ofVec2f anchor = boxes.screenToCanvas(ofVec2f(rightHalfX, ofGetHeight() * 0.45f));
	float startX = anchor.x - cols * sepx * 0.5f;
	boxes.addBox(selPath, startX + col * sepx, anchor.y + row * sepy);
	loadBoxCount++;
}
void ofApp::moveShaderSelection(int dir) {
	vector<std::pair<int, int>> vis;
	for (const ShaderBrowserRow &row : buildShaderBrowserRows()) {
		if (!row.folderHeader) vis.push_back({row.folderIndex, row.shaderIndex});
	}
	if (vis.empty()) return;
	int cur = -1;
	for (size_t i = 0; i < vis.size(); i++)
		if (vis[i].first == selectedShaderFolder && vis[i].second == selectedShaderIndex) { cur = (int)i; break; }
	int next = (cur < 0) ? (dir > 0 ? 0 : (int)vis.size() - 1)
						 : (cur + dir + (int)vis.size()) % (int)vis.size();
	selectShaderForPreview(vis[next].first, vis[next].second);
	ensureShaderSelectionVisible();
}
void ofApp::ensureShaderSelectionVisible() {
	if (selectedShaderFolder < 0 || selectedShaderIndex < 0) return;
	const ShaderBrowserLayout layout = getShaderBrowserLayout();
	const vector<ShaderBrowserRow> rows = buildShaderBrowserRows();
	int target = -1;
	for (int i = 0; i < (int)rows.size(); i++) {
		if (!rows[i].folderHeader &&
			rows[i].folderIndex == selectedShaderFolder &&
			rows[i].shaderIndex == selectedShaderIndex) {
			target = i;
			break;
		}
	}
	if (target < 0) return;
	if (target < shaderScroll) {
		shaderScroll = target;
	} else {
		float usedHeight = 0.0f;
		for (int i = shaderScroll; i <= target; i++) usedHeight += rows[i].height;
		if (usedHeight > layout.list.height) {
			usedHeight = 0.0f;
			int firstVisible = target;
			for (int i = target; i >= 0; i--) {
				if (usedHeight + rows[i].height > layout.list.height && i < target) break;
				usedHeight += rows[i].height;
				firstVisible = i;
			}
			shaderScroll = firstVisible;
		}
	}
	clampShaderScroll(layout);
}
// Draw a small 5-pointed star (filled or outline) at (cx,cy) with radius r.
static void drawStarGlyph(float cx, float cy, float r, bool filled) {
	ofBeginShape();
	for (int i = 0; i < 10; i++) {
		float rad = (i % 2 == 0) ? r : r * 0.45f;
		float a = -PI / 2 + i * PI / 5.0f;
		ofVertex(cx + cosf(a) * rad, cy + sinf(a) * rad);
	}
	if (filled) {
		ofEndShape(true);
	} else {
		ofNoFill();
		ofSetLineWidth(1.0f);
		ofEndShape(true);
		ofFill();
	}
}
void ofApp::draw_shaderindex() {
	ofSetRectMode(OF_RECTMODE_CORNER);
	shaderSearchFocused = true;

	const ShaderBrowserLayout layout = getShaderBrowserLayout();
	clampShaderScroll(layout);

	auto fitText = [&](const string &text, float maxWidth) {
		if (maxWidth <= 0.0f) return string();
		if (font_p.stringWidth(text) <= maxWidth) return text;
		string result = text;
		const string suffix = "...";
		while (!result.empty() && font_p.stringWidth(result + suffix) > maxWidth) {
			result.pop_back();
		}
		return result.empty() ? suffix : result + suffix;
	};

	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(layout.panel, 8.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_BORDER_MUTED, 210));
	ofSetLineWidth(1.0f);
	ofDrawRectRounded(layout.panel, 8.0f);
	ofFill();

	int totalShaders = 0;
	for (const ShaderFolder &folder : shaderFolders) {
		if (!folder.isFavorites) totalShaders += (int)folder.shaders.size();
	}
	const string title = language == 0 ?
		"IMPORT  |  " + ofToString(totalShaders) + " shaders" :
		"IMPORTAR  |  " + ofToString(totalShaders) + " shaders";
	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString(title, layout.panel.x + 16.0f, layout.titleBaseline);

	const bool modeHovered = layout.favoritesModeButton.inside(ofGetMouseX(), ofGetMouseY());
	ofSetColor(ofColor(COL_BG_BUTTON, 235));
	ofDrawRectRounded(layout.favoritesModeButton, 4.0f);
	const float segmentW = layout.favoritesModeButton.width * 0.5f;
	const float activeSegmentX = favoritesDisplayMode == FAVORITES_TOP ?
		layout.favoritesModeButton.x : layout.favoritesModeButton.x + segmentW;
	ofSetColor(modeHovered ? ofColor(COL_ACCENT_GOLD_DIM, 105) :
		ofColor(COL_ACCENT_GOLD_DIM, 65));
	ofDrawRectRounded(activeSegmentX + 2.0f, layout.favoritesModeButton.y + 2.0f,
		segmentW - 4.0f, layout.favoritesModeButton.height - 4.0f, 3.0f);
	ofNoFill();
	ofSetColor(modeHovered ? COL_ACCENT_GOLD : COL_BORDER_MUTED);
	ofDrawRectRounded(layout.favoritesModeButton, 4.0f);
	ofFill();
	ofSetColor(ofColor(COL_BORDER_MUTED, 150));
	ofDrawLine(layout.favoritesModeButton.x + segmentW,
		layout.favoritesModeButton.y + 4.0f,
		layout.favoritesModeButton.x + segmentW,
		layout.favoritesModeButton.getBottom() - 4.0f);

	const float topModeCx = layout.favoritesModeButton.x + segmentW * 0.5f;
	const float topModeY = layout.favoritesModeButton.y;
	ofSetColor(COL_ACCENT_GOLD);
	drawStarGlyph(topModeCx, topModeY + 7.0f, 3.5f, true);
	ofSetColor(favoritesDisplayMode == FAVORITES_TOP ?
		COL_ACCENT_CYAN : COL_TEXT_MUTED);
	ofSetLineWidth(1.4f);
	ofDrawLine(topModeCx - 7.0f, topModeY + 15.0f,
		topModeCx + 7.0f, topModeY + 15.0f);
	ofDrawLine(topModeCx - 7.0f, topModeY + 19.0f,
		topModeCx + 4.0f, topModeY + 19.0f);

	const float folderModeX = layout.favoritesModeButton.x + segmentW;
	for (int i = 0; i < 3; i++) {
		const float rowY = layout.favoritesModeButton.y + 6.0f + i * 6.0f;
		ofSetColor(COL_ACCENT_GOLD);
		drawStarGlyph(folderModeX + 7.0f, rowY, 2.4f, true);
		ofSetColor(favoritesDisplayMode == FAVORITES_IN_FOLDERS ?
			COL_ACCENT_CYAN : COL_TEXT_MUTED);
		ofDrawLine(folderModeX + 12.0f, rowY,
			folderModeX + segmentW - 5.0f, rowY);
	}
	ofSetLineWidth(1.0f);
	jp_tooltip::draw(
		favoritesDisplayMode == FAVORITES_TOP ?
			"Show favorites inside folders" :
			"Show favorites at top",
		layout.favoritesModeButton.x,
		layout.favoritesModeButton.y,
		layout.favoritesModeButton.width,
		layout.favoritesModeButton.height);

	ofSetColor(ofColor(COL_BORDER_MUTED, 150));
	ofDrawLine(layout.panel.x + 16.0f, layout.titleBaseline + 6.0f,
		layout.favoritesModeButton.x - 8.0f, layout.titleBaseline + 6.0f);

	ofSetColor(COL_TEXT_MUTED);
	const string hint = language == 0 ?
		"Up/Down navigate | double click/Enter to load" :
		"Arriba/Abajo navegar | doble clic/Enter para cargar";
	font_p.drawString(hint, layout.panel.x + 16.0f, layout.hintBaseline);

	const bool clearVisible = !shaderSearchText.empty();
	const bool clearHovered = clearVisible &&
		layout.searchClear.inside(ofGetMouseX(), ofGetMouseY());
	ofSetColor(COL_BG_BUTTON);
	ofDrawRectRounded(layout.search, 4.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 135));
	ofSetLineWidth(1.0f);
	ofDrawRectRounded(layout.search, 4.0f);
	ofFill();

	const string searchLabel = language == 0 ? "Search:" : "Buscar:";
	const float searchLabelX = layout.search.x + 7.0f;
	const float searchBaseline = layout.search.y + layout.search.height * 0.5f + 4.0f;
	ofSetColor(COL_TEXT_MUTED);
	font_p.drawString(searchLabel, searchLabelX, searchBaseline);

	const float textX = searchLabelX + font_p.stringWidth(searchLabel) + 5.0f;
	const float textRight = layout.searchClear.x - 5.0f;
	const float textMaxW = std::max(1.0f, textRight - textX);
	shaderSearchCursor = ofClamp(shaderSearchCursor, 0, (int)shaderSearchText.size());

	int displayStart = 0;
	int displayEnd = (int)shaderSearchText.size();
	while (displayStart < shaderSearchCursor &&
		font_p.stringWidth(shaderSearchText.substr(displayStart,
			shaderSearchCursor - displayStart)) > textMaxW) {
		displayStart++;
	}
	while (displayEnd > displayStart &&
		font_p.stringWidth(shaderSearchText.substr(displayStart,
			displayEnd - displayStart)) > textMaxW) {
		displayEnd--;
	}
	const string displayText = shaderSearchText.substr(displayStart, displayEnd - displayStart);
	if (shaderSearchText.empty()) {
		ofSetColor(ofColor(COL_TEXT_MUTED, 150));
		const string placeholder = language == 0 ? "type shader name..." : "escribe nombre...";
		font_p.drawString(placeholder, textX + 3.0f, searchBaseline);
	} else {
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(displayText, textX, searchBaseline);
	}
	jp_textfield::drawCaret(font_p, displayText,
		shaderSearchCursor - displayStart, textX,
		layout.search.getCenter().y, layout.search.height - 8.0f);

	if (clearVisible) {
		if (clearHovered) {
			ofSetColor(ofColor(COL_TEXT_MUTED, 45));
			ofDrawRectRounded(layout.searchClear.x + 3.0f, layout.searchClear.y + 3.0f,
				layout.searchClear.width - 6.0f, layout.searchClear.height - 6.0f, 3.0f);
		}
		ofSetColor(clearHovered ? COL_TEXT_PRIMARY : COL_TEXT_MUTED);
		const float cx = layout.searchClear.getCenter().x;
		const float cy = layout.searchClear.getCenter().y;
		ofSetLineWidth(1.5f);
		ofDrawLine(cx - 4.0f, cy - 4.0f, cx + 4.0f, cy + 4.0f);
		ofDrawLine(cx + 4.0f, cy - 4.0f, cx - 4.0f, cy + 4.0f);
		ofSetLineWidth(1.0f);
	}

	const vector<ShaderBrowserRow> allRows = buildShaderBrowserRows();
	const vector<ShaderBrowserRow> visibleRows =
		getVisibleShaderBrowserRows(layout, allRows);
	const int maxScroll = getMaxShaderScroll(allRows, layout.list.height);

	if (shaderScroll > 0) {
		const float cx = layout.list.getRight() - 8.0f;
		const float cy = layout.list.y + 5.0f;
		ofSetColor(ofColor(COL_ACCENT_CYAN, 140));
		ofDrawTriangle(cx - 4.0f, cy + 3.0f, cx + 4.0f, cy + 3.0f, cx, cy - 2.0f);
	}
	if (shaderScroll < maxScroll) {
		const float cx = layout.list.getRight() - 8.0f;
		const float cy = layout.list.getBottom() - 5.0f;
		ofSetColor(ofColor(COL_ACCENT_CYAN, 140));
		ofDrawTriangle(cx - 4.0f, cy - 3.0f, cx + 4.0f, cy - 3.0f, cx, cy + 2.0f);
	}

	for (const ShaderBrowserRow &row : visibleRows) {
		const bool isSelected = row.folderIndex == selectedShaderFolder &&
			row.shaderIndex == selectedShaderIndex;
		const bool isHovered = row.folderIndex == hoveredShaderFolder &&
			row.shaderIndex == hoveredShaderIndex;

		if (row.folderHeader) {
			const ShaderFolder &folder = shaderFolders[row.folderIndex];
			if (isSelected) {
				ofSetColor(ofColor(COL_ACCENT_CYAN_DIM, 80));
				ofDrawRectRounded(row.bounds, 3.0f);
			} else if (isHovered) {
				ofSetColor(ofColor(COL_BG_HOVER, 210));
				ofDrawRectRounded(row.bounds, 3.0f);
			}

			const bool expanded = !shaderSearchText.empty() || folder.expanded;
			const float arrowCx = row.bounds.x + 7.0f;
			const float arrowCy = row.bounds.getCenter().y;
			ofSetColor(folder.isFavorites ? COL_ACCENT_GOLD :
				(isHovered ? COL_ACCENT_CYAN : ofColor(120, 200, 255)));
			if (expanded) {
				ofDrawTriangle(arrowCx - 4.0f, arrowCy - 2.5f,
					arrowCx + 4.0f, arrowCy - 2.5f,
					arrowCx, arrowCy + 2.5f);
			} else {
				ofDrawTriangle(arrowCx - 2.5f, arrowCy - 4.0f,
					arrowCx - 2.5f, arrowCy + 4.0f,
					arrowCx + 2.5f, arrowCy);
			}

			float folderNameX = row.bounds.x + 16.0f;
			if (folder.isFavorites) {
				drawStarGlyph(folderNameX + 4.0f, arrowCy, 5.0f, true);
				folderNameX += 14.0f;
			}
			const string countText = "(" + ofToString((int)folder.shaders.size()) + ")";
			const float countWidth = font_p.stringWidth(countText);
			const string folderName = fitText(folder.name,
				row.bounds.width - (folderNameX - row.bounds.x) - countWidth - 16.0f);
			ofSetColor(folder.isFavorites ? COL_ACCENT_GOLD :
				(isHovered ? COL_TEXT_PRIMARY : ofColor(120, 200, 255)));
			font_p.drawString(folderName, folderNameX, row.bounds.y + row.bounds.height - 4.0f);
			ofSetColor(COL_TEXT_MUTED);
			font_p.drawString(countText, row.bounds.getRight() - countWidth - 7.0f,
				row.bounds.y + row.bounds.height - 4.0f);
		} else {
			const ShaderEntry &entry =
				shaderFolders[row.folderIndex].shaders[row.shaderIndex];
			ofRectangle shaderBounds = row.bounds;
			shaderBounds.x += SHADER_ROW_INDENT;
			shaderBounds.width -= SHADER_ROW_INDENT;
			if (isSelected) {
				ofSetColor(ofColor(COL_ACCENT_CYAN_DIM, 120));
				ofDrawRectRounded(shaderBounds, 3.0f);
				ofSetColor(COL_ACCENT_CYAN);
				ofDrawRectangle(shaderBounds.x, shaderBounds.y + 2.0f, 2.0f,
					shaderBounds.height - 4.0f);
			} else if (isHovered) {
				ofSetColor(ofColor(COL_BG_HOVER, 220));
				ofDrawRectRounded(shaderBounds, 3.0f);
			}

			const float starCx = shaderBounds.x + SHADER_STAR_OFFSET;
			const float starCy = shaderBounds.getCenter().y;
			if (entry.favorite) {
				ofSetColor(COL_ACCENT_GOLD);
				drawStarGlyph(starCx, starCy, 5.0f, true);
			} else {
				ofSetColor(isHovered || isSelected ?
					ofColor(COL_TEXT_MUTED, 180) : ofColor(COL_TEXT_MUTED, 65));
				drawStarGlyph(starCx, starCy, 5.0f, false);
			}
			const float nameX = shaderBounds.x + SHADER_NAME_OFFSET;
			const string bindingKey =
				midiKeymap.getAddShaderBindingLabel(entry.name, entry.path);
			string bindingLabel = bindingKey.empty() ? "" : "MIDI " + bindingKey;
			float nameRight = shaderBounds.getRight() - 7.0f;
			float bindingX = nameRight;
			if (!bindingLabel.empty()) {
				const float bindingMaxW =
					std::min(145.0f, shaderBounds.width * 0.44f);
				bindingLabel = fitText(bindingLabel, bindingMaxW);
				const float bindingWidth = font_p.stringWidth(bindingLabel);
				bindingX = shaderBounds.getRight() - bindingWidth - 7.0f;
				nameRight = bindingX - 10.0f;
			}
			const string shaderName =
				fitText(entry.name, std::max(20.0f, nameRight - nameX));
			ofSetColor(isSelected || isHovered ? COL_TEXT_PRIMARY : COL_TEXT_DIM);
			font_p.drawString(shaderName, nameX, shaderBounds.y + shaderBounds.height - 4.0f);
			if (!bindingLabel.empty()) {
				ofSetColor(isSelected || isHovered ? COL_ACCENT_CYAN : COL_MAPPED_ON);
				font_p.drawString(bindingLabel, bindingX,
					shaderBounds.y + shaderBounds.height - 4.0f);
			}
		}

		if (showShaderHitBoxes) {
			ofNoFill();
			ofSetColor(isHovered ? ofColor(255, 80, 80, 200) : ofColor(80, 230, 0, 100));
			ofDrawRectangle(row.bounds);
			ofFill();
		}
	}

	if (allRows.empty()) {
		ofSetColor(COL_TEXT_MUTED);
		const string emptyText = shaderSearchText.empty() ?
			(language == 0 ? "No shaders found" : "No se encontraron shaders") :
			(language == 0 ? "No shaders match this search" : "Ningun shader coincide");
		font_p.drawString(emptyText, layout.list.x + 8.0f, layout.list.y + 22.0f);
	}

	const bool hasSelection = !getSelectedShaderPath().empty();
	auto drawFooterButton = [&](const ofRectangle &button, ofColor fill,
		ofColor border, const string &label, bool enabled) {
		const bool hovered = enabled && button.inside(ofGetMouseX(), ofGetMouseY());
		ofSetColor(enabled ? (hovered ? fill.getLerped(COL_TEXT_PRIMARY, 0.08f) : fill) :
			ofColor(fill, 55));
		ofDrawRectRounded(button, 4.0f);
		ofNoFill();
		ofSetColor(enabled ? border : ofColor(border, 70));
		ofDrawRectRounded(button, 4.0f);
		ofFill();
		ofSetColor(enabled ? COL_TEXT_PRIMARY : COL_TEXT_MUTED);
		const float labelWidth = font_p.stringWidth(label);
		font_p.drawString(label, button.getCenter().x - labelWidth * 0.5f,
			button.getCenter().y + 4.0f);
	};

	drawFooterButton(layout.loadButton, COL_ACCENT_CYAN_DIM, COL_ACCENT_CYAN,
		language == 0 ? "LOAD" : "CARGAR", hasSelection);
	if (importBindWaiting) {
		drawFooterButton(layout.bindButton, COL_ACCENT_GOLD_DIM, COL_ACCENT_GOLD,
			language == 0 ? "MOVE MIDI" : "MUEVE MIDI", true);
	} else {
		drawFooterButton(layout.bindButton, COL_BG_BUTTON, COL_ACCENT_GOLD_DIM,
			"BIND", hasSelection);
	}
	drawFooterButton(layout.editButton, COL_BG_BUTTON, COL_ACCENT_GOLD_DIM,
		"EDIT", hasSelection);

	ofSetColor(COL_BG_INPUT);
	ofDrawRectRounded(layout.preview, 6.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_BORDER_MUTED, 210));
	ofDrawRectRounded(layout.preview, 6.0f);
	ofFill();

	const float previewTitleH = 18.0f;
	const float previewPad = 6.0f;
	if (hasSelection && previewShaderLoaded && previewFbo.isAllocated()) {
		const ShaderEntry &selected =
			shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex];
		const string previewTitle = fitText(
			(language == 0 ? "PREVIEW: " : "PREVISTA: ") + selected.name,
			layout.preview.width - previewPad * 2.0f);
		ofSetColor(COL_ACCENT_CYAN);
		font_p.drawString(previewTitle, layout.preview.x + previewPad,
			layout.preview.y + previewTitleH - 3.0f);

		const float availableW = layout.preview.width - previewPad * 2.0f;
		const float availableH = layout.preview.height - previewTitleH - previewPad * 2.0f;
		const float aspect = previewFbo.getHeight() / (float)previewFbo.getWidth();
		float previewW = availableW;
		float previewH = previewW * aspect;
		if (previewH > availableH) {
			previewH = availableH;
			previewW = previewH / aspect;
		}
		const float previewX = layout.preview.x + (layout.preview.width - previewW) * 0.5f;
		const float previewY = layout.preview.y + previewTitleH + previewPad +
			(availableH - previewH) * 0.5f;
		ofSetColor(COL_TEXT_PRIMARY);
		previewFbo.draw(previewX, previewY, previewW, previewH);
	} else {
		ofSetColor(COL_TEXT_MUTED);
		const string previewState = hasSelection ?
			(language == 0 ? "preview unavailable" : "vista previa no disponible") :
			(language == 0 ? "select a shader to preview" : "selecciona un shader");
		font_p.drawString(previewState, layout.preview.x + 8.0f,
			layout.preview.getCenter().y);
	}
}
// Esta es la que se dibuja en la otra ventana
void ofApp::drawRender() {
	boxes.draw_activerender();
}

void ofApp::closeShaderEditorToMain()
{
	shaderEditor.setVisible(false);
	pantallaActiva = NODOS;
	focusedOptionsField = -1;
	boxes.navigateToBreadcrumbLevel(0);
}

void ofApp::keyPressed(int key) {

	if (midiKeymap.keyPressed(key)) {
		return;
	}

	// Shader Editor key capture (when visible, consumes all keys)
	if (shaderEditor.wantsKeyCapture()) {
		shaderEditor.keyPressed(key);
		if (!shaderEditor.isVisible()) {
			closeShaderEditorToMain();
		}
		return;
	}

	// Forward key to boxgroup for inline tab renaming
	if (boxes.tabRenaming) {
		boxes.keyPressed(key);
		return;
	}

	// Save-as modal input handling
	if (saveModalActive) {
		if (key == OF_KEY_RETURN || key == '\r') {
			confirmSaveModal();
			return;
		}
		if (key == OF_KEY_ESC) {
			cancelSaveModal();
			return;
		}
		// Cursor navigation + backspace/del/insert at cursor.
		if (key == OF_KEY_LEFT || key == OF_KEY_RIGHT || key == OF_KEY_HOME ||
			key == OF_KEY_END || key == OF_KEY_BACKSPACE || key == OF_KEY_DEL) {
			jp_textfield::handleKey(saveModalName, saveModalCursor, key);
			return;
		}
		// Allow alphanumeric, dash, underscore, dot, space (insert at cursor).
		if ((key >= 'a' && key <= 'z') ||
			(key >= 'A' && key <= 'Z') ||
			(key >= '0' && key <= '9') ||
			key == '-' || key == '_' || key == '.' || key == ' ') {
			jp_textfield::handleKey(saveModalName, saveModalCursor, key);
		}
		return;
	}

	// keyIsDown[key] = true;

	// Options screen text field input — run BEFORE tab shortcuts so digits
	// '1','2','3' get consumed by the field instead of switching tabs.
	if (focusedOptionsField >= 0) {
		if (key == OF_KEY_RETURN || key == '\r') {
			applyOptionsField();
			return;
		}
		// IP/path fields accept any printable char; numeric fields digits only.
		bool numericOnly = !(focusedOptionsField == FIELD_OSC_IP_OUT || focusedOptionsField == FIELD_DEFAULT_COMPO);
		jp_textfield::handleKey(optionsFieldText[focusedOptionsField], optionsFieldCursor, key, numericOnly);
		return; // consume other keys while focused
	}
	if (focusedLiveOutputField >= 0)
	{
		if (key == OF_KEY_RETURN || key == '\r')
		{
			applyLiveOutputField();
			return;
		}
		if (key == OF_KEY_ESC)
		{
			focusedLiveOutputField = -1;
			initLiveOutputFields();
			return;
		}
		jp_textfield::handleKey(
			liveOutputFieldText[focusedLiveOutputField],
			liveOutputFieldCursor, key, true);
		return;
	}
	if (pantallaActiva == OPCIONES &&
		liveOutputMenu != LIVE_OUTPUT_MENU_NONE &&
		key == OF_KEY_ESC)
	{
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		liveOutputMenuScroll = 0;
		return;
	}

	if (pantallaActiva == SHADER_INDEX) {
		if (key == OF_KEY_ESC) {
			if (!shaderSearchText.empty()) {
				clearShaderSearch();
			} else {
				pantallaActiva = NODOS;
				shaderSearchFocused = false;
				focusedOptionsField = -1;
			}
			return;
		}
		if (key == OF_KEY_UP || key == OF_KEY_DOWN) {
			moveShaderSelection(key == OF_KEY_DOWN ? 1 : -1);
			return;
		}
		if (key == OF_KEY_RETURN || key == '\r') {
			loadSelectedShaderBox();
			return;
		}
		if (jp_textfield::handleKey(shaderSearchText, shaderSearchCursor, key)) {
			shaderScroll = 0;
			clampShaderScroll(getShaderBrowserLayout());
		}
		return;
	}

	if (key == '1') {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
		focusedLiveOutputField = -1;
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '2') {
		if (pantallaActiva != OPCIONES) {
			pantallaActiva = OPCIONES;
			initOptionsFields();
			optionsFieldsInitialized = true;
		} else {
			// Already on options tab, just refocus without resetting field values
			// (keep existing text as-is)
		}
		focusedOptionsField = -1;
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '3') {
		pantallaActiva = TUTORIAL;
		focusedOptionsField = -1;
		focusedLiveOutputField = -1;
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '4') {
		pantallaActiva = SHADER_INDEX;
		focusedOptionsField = -1;
		focusedLiveOutputField = -1;
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		shaderSearchFocused = true;
		shaderSearchCursor = ofClamp(shaderSearchCursor, 0, (int)shaderSearchText.size());
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
		if (shaderFolders.empty()) {
			scanShaders();
		}
	}

	if (key == '5') {
		pantallaActiva = EDITOR;
		focusedOptionsField = -1;
		focusedLiveOutputField = -1;
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		shaderEditor.setVisible(true);
	}

	// ESC from editor goes back to NODOS
	if (key == OF_KEY_ESC && pantallaActiva == EDITOR) {
		closeShaderEditorToMain();
		return;
	}

	if (key == OF_KEY_ESC && boxes.isMappingEditActive()) {
		boxes.endMappingEdit();
		return;
	}

	// H toggles hit-box visualization in shader index
	if (key == 'h' && pantallaActiva == SHADER_INDEX) {
		showShaderHitBoxes = !showShaderHitBoxes;
		return;
	}

	if (key == 'k') {
		midiKeymap.togglePanel();
	}

	// The detailed key event handles graph clipboard shortcuts below. Consume
	// their legacy key callback so Ctrl+C cannot also create a camera box.
	if (pantallaActiva == NODOS &&
		ofGetKeyPressed(OF_KEY_CONTROL) &&
		(key == 'c' || key == 'C' || key == 'v' || key == 'V')) {
		return;
	}

	if (pantallaActiva == NODOS) {
		if (key == 't') {
			loadAspreset = !loadAspreset;
		}
		if (key == 'f') {
			// ofToggleFullscreen();
		}
		if (key == 'h') {
#ifdef SPOUT
			boxes.addBox("spoutReceiver");
#else
			std::cerr << "Spout not supported" << std::endl;
#endif
		}
		if (key == 'i') {
			boxes.addBox("framedifference");
		}
		if (key == 'c') {
			// boxes.addCamBox();
			boxes.addBox("cam");
		}
		if (key == 'n') {
#ifdef NDI
			boxes.addBox("ndiReceiver");
#else
			std::cerr << "NDI not supported" << std::endl;
#endif
		}

		if (key == OF_KEY_DEL) {
			cout << "DEL " << endl;
			boxes.deleteSelectedShader();
		}

		if (key == 'o') {
			// openloader.startThread();
			// ESTO ES LO QUE HABRIA QUE PROBAR EN MAC PARA VER SI FUNCA O NO . CUANDO ESTEMOS AH�
			/*ofFileDialogResult result = ofSystemLoadDialog("Load file");
			if (result.bSuccess) {
				string path = result.getPath();
				cout << "path " << path << endl;
				if (path.find("data") != std::string::npos) {
					cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
					path = path.substr(path.find("data"), path.size());
					cout << "NEW PATH CONVERSION :" << path << endl;
				}
				else {
					cout << "WARNING: OUTSIDE DATA FOLDER " << endl;
				}
				if (path.find(".frag") != std::string::npos) {
					cout << "LOAD SHADER" << endl;
					boxes.addShaderBox(path);
				}
				else if (path.find(".xml") != std::string::npos) {
					cout << "LOAD SAVEFILE" << endl;
					savedirectory = path;
					boxes.load(savedirectory);
				}
				else if (path.find(".png") != std::string::npos ||
					path.find(".jpg") != std::string::npos ||
					path.find(".JPEG") != std::string::npos
					) {
					cout << "LOAD IMAGE FILE" << endl;
					boxes.addImageBox(path);
				}
				else if (path.find(".mov") != std::string::npos ||
					path.find(".mkv") != std::string::npos ||
					path.find(".mp4") != std::string::npos ||
					path.find(".flv") != std::string::npos ||
					path.find(".vob") != std::string::npos ||
					path.find(".avi") != std::string::npos
					) {
					boxes.addVideoBox(path);
				}
			}*/
		}

		/*if (key == 'l') {
			cout << "savedirectory" << savedirectory << endl;
			if (loadAspreset) {
				boxes.load2(savedirectory);
			}else{
				boxes.load(savedirectory);
			}//boxes.update_resized(renderwidth, renderheight); //ESTA LINEA ES RE CACUIJA .
		}*/

		if (key == 's') {
			cout << "Save session to " << savedirectory << endl;
			saveSession(savedirectory);
		}
		if (key == 'l') {
			cout << "Load session from " << savedirectory << endl;
			loadSession(savedirectory);
		}
		if (key == 'd') {
			isDebug = !isDebug;
			cout << "IS DEBUG " << isDebug << endl;
		}
		if (key == 'r') {
			boxes.reloadActiveshader();
		}
		if (key == 'w') {
			openRenderWindow();
		}
		// if (key == 'm') {

		//}
		if (key == 'm') {

			ofFbo fbo;

			fbo = *boxes.getActiverender();
			ofPixels pix;

			/*cout << "DAY " << ofGetDay() << endl;
			cout << "MONTH " << ofGetMonth() << endl;
			cout << "YEAT " << ofGetYear() << endl;
			cout << "CURRENT TIME " << ofToString(ofGetCurrentTime()) << endl;
			*/
			string txtname = "-" + ofToString(ofGetDay()) + "-" + ofToString(ofGetMonth()) + "-" + ofToString(ofGetYear()) + "-" + ofToString(ofGetHours()) + "-" + ofToString(ofGetMinutes()) + "-" + ofToString(ofGetSeconds()) + "-";
			fbo.readToPixels(pix);
			ofSaveImage(pix, "exportimgs/export" + txtname + ".png");
		}
		if (key == 'e') {
			boxes.activeSequence = !boxes.activeSequence;
		}

		if (key == 'u') {
			boxes.groupSelectedBoxes();
		}

		if (key == 'z') {
			// Cue the selected box for the graph on screen; falls back to that
			// graph's active render (works in main and inside any box-group).
			int cueIndex = boxes.getCueEntryIndexForCurrentView();
			cout << "Z PRESSED: groupView=" << boxes.isGroupViewActive()
				 << " cueIndex=" << cueIndex << endl;
			boxes.toggleCueBoxByIndex(cueIndex);
		}
		if (key == 'x') {
			cout << "Trigger CODE " << endl;
			boxes.triggerCodeOnActiveShader();
		}
		if (key == 'c') {
		}
	}
	/*if (prevKey == OF_KEY_CONTROL && key == 's') {
		cout << "lala" << endl;
	}
	if (key == OF_KEY_CONTROL) {
		cout << "PRESS CONTROL " << endl;
	}
	prevKey = key;*/
}
void ofApp::keycodePressed(ofKeyEventArgs & e) {

	// cout << "KEY : " << e.key << endl;

	// CUANDO APRETAS CONTROL TE TOMA COMO DOS INPUTS EN EL MOMENTO.
	cout << "-------------------------------------" << endl;

	// Forward Ctrl+key combos to shader editor (copy/paste/cut/select all)
	if (shaderEditor.wantsKeyCapture()) {
		shaderEditor.keycodePressed(e.key);
		// Don't return yet — Ctrl+S also needs to save
	}

	// Ctrl+S (keycodes 46 or 19 depending on platform) -> save-as modal or save shader
	if (e.key == 46 || e.key == 19) {
		// If shader editor is visible, save the shader file instead
		if (shaderEditor.wantsKeyCapture()) {
			shaderEditor.saveCurrentTab();
			return;
		}
		if (!saveModalActive) {
			saveModalActive = true;
			saveModalName = "";
			// Pre-fill with the active session filename (from savedirectory)
			{
				string sessionFile = ofFilePath::getFileName(savedirectory);
				if (!sessionFile.empty()) {
					// Strip .xml extension for the text field
					if (sessionFile.size() > 4 && sessionFile.substr(sessionFile.size() - 4) == ".xml") {
						sessionFile = sessionFile.substr(0, sessionFile.size() - 4);
					}
					saveModalName = sessionFile;
				}
			}
			saveModalCursor = saveModalName.size();
			cout << "Save modal opened, name='" << saveModalName << "'" << endl;
		}
		return;
	}

	const bool controlDown = e.hasModifier(OF_KEY_CONTROL);
	const bool copyShortcut =
		e.key == 3 ||
		(controlDown &&
			(e.keycode == GLFW_KEY_C || e.key == 'c' || e.key == 'C'));
	const bool pasteShortcut =
		e.key == 22 ||
		(controlDown &&
			(e.keycode == GLFW_KEY_V || e.key == 'v' || e.key == 'V'));

	// GLFW reports Ctrl+letter as a normal letter plus a modifier on Linux.
	if (copyShortcut && pantallaActiva == NODOS) {
		boxes.copySelectedBoxes();
		return;
	}

	if (pasteShortcut && pantallaActiva == NODOS) {
		boxes.pasteBoxes();
		return;
	}

	if (prevKey == 12) {
		openloader.startThread();
	}

	prevKey = e.keycode;
}
void ofApp::mouseDragged(int x, int y, int button) {
	midiKeymap.mouseDragged(x, y, button);

	// Forward to shader editor for selection dragging
	if (shaderEditor.isVisible()) {
		shaderEditor.mouseDragged(x, y, button);
	}

	if (pantallaActiva == NODOS) {
		if (boxes.update_mappingMouseDragged(button)) {
			return;
		}
		if (boxes.update_cueMouseDragged(button)) {
			return;
		}
		// jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
		boxes.update_mouseDragged(button);
	}
}
void ofApp::mousePressed(int x, int y, int button) {

	// Save modal button clicks — consume before anything else when modal is active
	if (saveModalActive) {
		float w = ofGetWidth();
		float h = ofGetHeight();
		float boxW = 420;
		float boxH = 240;
		float boxX = (w - boxW) * 0.5f;
		float boxY = (h - boxH) * 0.5f;
		float pad = 20;
		float btnY = boxY + boxH - 42;
		float btnH = 28;
		float btnGap = 12;
		float totalBtnsW = boxW - pad * 2;
		float btnW = (totalBtnsW - btnGap * 2) / 3.0f;

		float saveBtnX = boxX + pad;
		float updateBtnX = saveBtnX + btnW + btnGap;
		float cancelBtnX = updateBtnX + btnW + btnGap;

		// SAVE button
		if (x >= saveBtnX && x <= saveBtnX + btnW &&
			y >= btnY && y <= btnY + btnH) {
			confirmSaveModal();
			return;
		}
		// UPDATE button
		if (x >= updateBtnX && x <= updateBtnX + btnW &&
			y >= btnY && y <= btnY + btnH) {
			updateSaveModal();
			return;
		}
		// CANCEL button
		if (x >= cancelBtnX && x <= cancelBtnX + btnW &&
			y >= btnY && y <= btnY + btnH) {
			cancelSaveModal();
			return;
		}
		// Click outside modal box → cancel
		if (x < boxX || x > boxX + boxW || y < boxY || y > boxY + boxH) {
			cancelSaveModal();
			return;
		}
	}

	if (midiKeymap.mousePressed(x, y, button)) {
		return;
	}
	if (midiKeymap.captureFunctionClick(x, y, button)) {
		return;
	}

	// Screen tab click handling
	{
		int tabScreen = getScreenTabAtPos(x, y);
		if (tabScreen >= 0) {
			if (pantallaActiva != tabScreen) {
				pantallaActiva = tabScreen;
				focusedOptionsField = -1;
				focusedLiveOutputField = -1;
				liveOutputMenu = LIVE_OUTPUT_MENU_NONE;

				// Hide editor when leaving EDITOR screen
				if (pantallaActiva != EDITOR) shaderEditor.setVisible(false);

				if (tabScreen == OPCIONES) {
					initOptionsFields();
					optionsFieldsInitialized = true;
				} else if (tabScreen == SHADER_INDEX) {
					if (shaderFolders.empty()) {
						scanShaders();
					}
					shaderSearchFocused = true;
					shaderSearchCursor = ofClamp(shaderSearchCursor, 0, (int)shaderSearchText.size());
				} else if (tabScreen == EDITOR) {
					shaderEditor.setVisible(true);
				}
			}
			return;
		}
	}

	// Shader Editor clicks
	if (pantallaActiva == EDITOR && shaderEditor.isVisible()) {
		shaderEditor.mousePressed(x, y, button);
		if (!shaderEditor.isVisible()) {
			closeShaderEditorToMain();
		}
		return;
	}

	if (pantallaActiva == NODOS) {
		if (boxes.update_mappingMousePressed(button)) {
			return;
		}
		if (boxes.update_cueMousePressed(button)) {
			return;
		}
		jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
		boxes.update_mousePressed(button);
	}
	if (pantallaActiva == TUTORIAL) {
		// Language toggle — only click on the top-right button area
		float panelX = 30, panelY = 44;
		float panelW = ofGetWidth() - 60;
		float langBtnW = 52;
		float langBtnH = 22;
		float langBtnX = panelX + panelW - langBtnW - 15;
		float langBtnY = panelY + 13;
		if (x >= langBtnX && x <= langBtnX + langBtnW && y >= langBtnY && y <= langBtnY + langBtnH) {
			language = (language == 0) ? 1 : 0;
		}
	}
	if (pantallaActiva == OPCIONES) {
		if (handleLiveOutputSettingsClick(x, y, button)) {
			return;
		}
		// Layout constants matching draw_opciones()
		float panelX = 30;
		float panelY = 44 - settingsScroll;
		float fieldX = panelX + 175;
		float fieldW = 200;
		float rowH = 28;
		float sepy = 40;
		const float actionBtnW = 100;

		// Check if clicked inside any text field
		focusedOptionsField = -1;
		for (int i = 0; i < FIELD_OSC_IP_OUT; i++) {
			float rowY = panelY + 55 + i * sepy;
			if (x >= fieldX && x <= fieldX + fieldW &&
				y >= rowY && y <= rowY + rowH) {
				focusedOptionsField = i;
				optionsFieldCursor = optionsFieldText[i].size();
				return;
			}
			// AUTOTAP button next to BPM field
			if (i == FIELD_BPM) {
				float tapX = fieldX + fieldW + 10;
				if (x >= tapX && x <= tapX + actionBtnW &&
					y >= rowY && y <= rowY + rowH) {
					autoTap();
					return;
				}
			}
		}

		// Check toggle buttons
		int toggleRow = FIELD_OSC_IP_OUT;

#ifdef SPOUT
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			if (x >= fieldX && x <= fieldX + actionBtnW &&
				y >= rowY && y <= rowY + rowH) {
				spoutActive = !spoutActive;
				return;
			}
		}
		toggleRow++;
#endif

#ifdef NDI
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			if (x >= fieldX && x <= fieldX + actionBtnW &&
				y >= rowY && y <= rowY + rowH) {
				ndiActive = !ndiActive;
				return;
			}
		}
		toggleRow++;
#endif

		// Check OSC IP Out text field
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			// Click on text field area
			if (x >= fieldX && x <= fieldX + fieldW &&
				y >= rowY && y <= rowY + rowH) {
				focusedOptionsField = FIELD_OSC_IP_OUT;
				optionsFieldCursor = optionsFieldText[FIELD_OSC_IP_OUT].size();
				return;
			}
		}
		toggleRow++;

		// Check Default Compo text field + BROWSE button
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			// Click on text field area
			if (x >= fieldX && x <= fieldX + fieldW &&
				y >= rowY && y <= rowY + rowH) {
				focusedOptionsField = FIELD_DEFAULT_COMPO;
				optionsFieldCursor = optionsFieldText[FIELD_DEFAULT_COMPO].size();
				return;
			}
			// Click on BROWSE button
			float browseX = fieldX + fieldW + 10;
			if (x >= browseX && x <= browseX + actionBtnW &&
				y >= rowY && y <= rowY + rowH) {
				// Launch system file dialog to select the XML
				ofFileDialogResult result = ofSystemLoadDialog("Select default composition XML", false);
				if (result.bSuccess) {
					string path = result.getPath();
					optionsFieldText[FIELD_DEFAULT_COMPO] = path;
					defaultCompoPath = path;
					cout << "Default compo selected: " << path << endl;
				}
				return;
			}
		}
		toggleRow++;

		// Skip "Active Compo" row (read-only, not clickable)
		toggleRow++;

		// Check Save button
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			float saveX = fieldX;
			if (x >= saveX && x <= saveX + fieldW &&
				y >= rowY && y <= rowY + rowH) {
				saveSettings();
				saveFeedbackText = "Saved!";
				saveFeedbackTime = ofGetElapsedTimef();
				return;
			}
		}
	}
	if (pantallaActiva == SHADER_INDEX && button == 0) {
		const ShaderBrowserLayout layout = getShaderBrowserLayout();
		shaderSearchFocused = true;
		if (!layout.panel.inside(x, y)) return;

		if (layout.favoritesModeButton.inside(x, y)) {
			toggleFavoritesDisplayMode();
			return;
		}
		if (!shaderSearchText.empty() && layout.searchClear.inside(x, y)) {
			clearShaderSearch();
			return;
		}
		if (layout.search.inside(x, y)) {
			const string searchLabel = language == 0 ? "Search:" : "Buscar:";
			const float textX = layout.search.x + 7.0f +
				font_p.stringWidth(searchLabel) + 5.0f;
			const float textMaxW = std::max(1.0f, layout.searchClear.x - 5.0f - textX);
			shaderSearchCursor = ofClamp(shaderSearchCursor, 0, (int)shaderSearchText.size());
			int displayStart = 0;
			int displayEnd = (int)shaderSearchText.size();
			while (displayStart < shaderSearchCursor &&
				font_p.stringWidth(shaderSearchText.substr(displayStart,
					shaderSearchCursor - displayStart)) > textMaxW) {
				displayStart++;
			}
			while (displayEnd > displayStart &&
				font_p.stringWidth(shaderSearchText.substr(displayStart,
					displayEnd - displayStart)) > textMaxW) {
				displayEnd--;
			}
			const string displayText =
				shaderSearchText.substr(displayStart, displayEnd - displayStart);
			int localCursor = 0;
			const float relativeX = std::max(0.0f, (float)x - textX);
			for (int i = 1; i <= (int)displayText.size(); i++) {
				const float previousWidth = font_p.stringWidth(displayText.substr(0, i - 1));
				const float currentWidth = font_p.stringWidth(displayText.substr(0, i));
				if (relativeX < (previousWidth + currentWidth) * 0.5f) break;
				localCursor = i;
			}
			shaderSearchCursor = ofClamp(displayStart + localCursor,
				0, (int)shaderSearchText.size());
			return;
		}

		if (!getSelectedShaderPath().empty()) {
			const string selectedPath = getSelectedShaderPath();
			const string selectedName =
				shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].name;
			if (layout.loadButton.inside(x, y)) {
				loadSelectedShaderBox();
				return;
			}
			if (layout.bindButton.inside(x, y)) {
				if (importBindWaiting || midiKeymap.isLearning()) {
					midiKeymap.cancelInlineLearn();
					importBindWaiting = false;
				} else {
					midiKeymap.beginAddShaderLearn(selectedName);
					importBindWaiting = true;
				}
				return;
			}
			if (layout.editButton.inside(x, y)) {
				shaderEditor.openShader(selectedPath, selectedName);
				return;
			}
		}

		for (const ShaderBrowserRow &row : getVisibleShaderBrowserRows(layout)) {
			if (!row.bounds.inside(x, y)) continue;
			if (row.folderHeader) {
				selectedShaderFolder = row.folderIndex;
				selectedShaderIndex = -1;
				if (shaderSearchText.empty()) {
					ShaderFolder &folder = shaderFolders[row.folderIndex];
					folder.expanded = !folder.expanded;
					if (folder.isFavorites) favoritesFolderExpanded = folder.expanded;
					clampShaderScroll(layout);
				}
				return;
			}

			const float starCx =
				row.bounds.x + SHADER_ROW_INDENT + SHADER_STAR_OFFSET;
			if (x >= starCx - SHADER_STAR_HIT_HALF_WIDTH &&
				x <= starCx + SHADER_STAR_HIT_HALF_WIDTH) {
				toggleFavorite(shaderFolders[row.folderIndex].shaders[row.shaderIndex].path);
				clampShaderScroll(layout);
				return;
			}

			const float now = ofGetElapsedTimef();
			const bool doubleClick =
				lastShaderClickFolder == row.folderIndex &&
				lastShaderClickIndex == row.shaderIndex &&
				now - lastShaderClickTime < 0.35f;
			selectShaderForPreview(row.folderIndex, row.shaderIndex);
			if (doubleClick) {
				loadSelectedShaderBox();
				lastShaderClickTime = -1.0f;
			} else {
				lastShaderClickTime = now;
				lastShaderClickFolder = row.folderIndex;
				lastShaderClickIndex = row.shaderIndex;
			}
			return;
		}
	}
}
void ofApp::windowResized(int w, int h) {

	// El resize lo hace solo para mover la interfaz. Los tamaos de render se mantienen igual
	boxes.update_resized(ofGetWidth(), ofGetHeight());
	clampSettingsScroll();
	// InitGLtexture(sendertexture, ofGetWidth(), ofGetHeight()); //!?!??!!?
	//	boxes.update_resized(jp_constants::renderWidth, jp_constants::renderHeight);
}
void ofApp::keyReleased(int key) { }
void ofApp::mouseMoved(int x, int y) {
	hoveredShaderFolder = -1;
	hoveredShaderIndex = -1;
	if (pantallaActiva != SHADER_INDEX) return;

	const ShaderBrowserLayout layout = getShaderBrowserLayout();
	for (const ShaderBrowserRow &row : getVisibleShaderBrowserRows(layout)) {
		if (row.bounds.inside(x, y)) {
			hoveredShaderFolder = row.folderIndex;
			hoveredShaderIndex = row.shaderIndex;
			break;
		}
	}
}
void ofApp::mouseReleased(int x, int y, int button) {
	midiKeymap.mouseReleased(x, y, button);
	if (boxes.update_mappingMouseReleased(button)) {
		saveSettings();
		return;
	}
	if (boxes.update_cueMouseReleased(button)) {
		saveSettings();
		return;
	}
	boxes.update_mouseReleased(button);
	if (button == 0) {
		// mouseButton_left = false;
	}
}
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
	if (midiKeymap.mouseScrolled(x, y, scrollX, scrollY)) {
		return;
	}
	// Forward to shader editor when visible
	if (shaderEditor.isVisible()) {
		shaderEditor.mouseScrolled(x, y, scrollX, scrollY);
		return;
	}
	if (pantallaActiva == SHADER_INDEX) {
		const ShaderBrowserLayout layout = getShaderBrowserLayout();
		if (layout.panel.inside(x, y)) {
			shaderScroll -= (int)scrollY * 3;
			clampShaderScroll(layout);
		}
		return;
	}
	if (pantallaActiva == OPCIONES)
	{
		const LiveOutputSettingsLayout layout =
			getLiveOutputSettingsLayout();
		if (liveOutputMenu != LIVE_OUTPUT_MENU_NONE &&
			(layout.popup.inside(x, y) ||
			 layout.sourceButton.inside(x, y) ||
			 layout.monitorButton.inside(x, y)))
		{
			const int optionCount =
				liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE ?
					(int)getLiveOutputSourceOptions().size() :
					(int)liveOutputMonitors.size();
			liveOutputMenuScroll = ofClamp(
				liveOutputMenuScroll - (int)scrollY,
				0, std::max(0, optionCount - 8));
			return;
		}
		if (layout.list.inside(x, y))
		{
			const int visibleRows = std::max(
				1, (int)(layout.list.height / 31.0f));
			liveOutputListScroll = ofClamp(
				liveOutputListScroll - (int)scrollY,
				0, std::max(0,
					(int)liveOutputs.size() - visibleRows));
			return;
		}
		settingsScroll -= scrollY * 36.0f;
		clampSettingsScroll();
		return;
	}
	if (pantallaActiva == NODOS) {
		boxes.mouseScrolled(x, y, scrollX, scrollY);
	}
}
void ofApp::mouseEntered(int x, int y) { }
void ofApp::mouseExited(int x, int y) { }
void ofApp::gotMessage(ofMessage msg) { }
void ofApp::dragEvent(ofDragInfo dragInfo) {

	cout << "WHAT " << dragInfo.position.t << endl;
	cout << "DIR : " << dragInfo.files[0] << endl;

	// ESTO TIENE QUE COINCIDIR CON LOS TAMA�OS DE LAS CAJAS QUE ACTUALMENTE ESTA EN 80x80
	float sepx = 80 * 1.4;
	float sepy = 80 * 1.6;

	ofVec2f dropPosition = boxes.screenToCanvas(ofVec2f(dragInfo.position.x, dragInfo.position.y));
	ofVec2f dropSpacing = boxes.screenDeltaToCanvas(ofVec2f(sepx, sepy));
	float xx = dropPosition.x;
	float yy = dropPosition.y;

	int indexx = 0;
	int indexy = 0;
	for (int i = 0; i < dragInfo.files.size(); i++) {
		string path = dragInfo.files[i].string();
		if (dragInfo.files.size() > 1) {

			if (indexx >= ceil(sqrt(dragInfo.files.size()))) {
				indexx = 0;
				xx = dropPosition.x;
				yy += dropSpacing.y;
			}
			indexx++;
		}
#ifdef RELATIVEDIRS
		cout << "path " << path << endl;
		if (path.find("data") != std::string::npos) {
			cout << "IS INSIDE DATA FOLDER SO LETS CONVERT IT TO RELATIVE DIR" << endl;
			path = path.substr(path.find("data"), path.size());
			cout << "NEW PATH CONVERSION :" << path << endl;
		} else {
			cout << "WARNING: OUTSIDE DATA FOLDER " << endl;
		}
#endif
		cout << "path " << path << endl;
		if (path.find(".xml") != std::string::npos && !loadAspreset) {
			savedirectory = path;
			loadSession(path);
		} else {

			boxes.addBox(path, xx, yy);
		}
		xx += dropSpacing.x;
	}
}

void ofApp::refreshLiveOutputMonitors()
{
	liveOutputMonitors.clear();
	lastLiveOutputMonitorRefresh = ofGetElapsedTimef();
	int count = 0;
	GLFWmonitor **monitors = glfwGetMonitors(&count);
	GLFWmonitor *primary = glfwGetPrimaryMonitor();
	for (int i = 0; i < count; i++)
	{
		const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
		if (mode == nullptr)
		{
			continue;
		}

		LiveOutputMonitor monitor;
		const char *name = glfwGetMonitorName(monitors[i]);
		monitor.name = name != nullptr && name[0] != '\0' ?
			name : "Monitor " + ofToString(i + 1);
		monitor.index = i;
		glfwGetMonitorPos(monitors[i], &monitor.x, &monitor.y);
		monitor.width = mode->width;
		monitor.height = mode->height;
		monitor.primary = monitors[i] == primary;
		liveOutputMonitors.push_back(monitor);
	}
}

int ofApp::resolveLiveOutputMonitor(const LiveOutputConfig &config) const
{
	if (!config.monitorName.empty())
	{
		int firstNameMatch = -1;
		for (int i = 0; i < (int)liveOutputMonitors.size(); i++)
		{
			if (liveOutputMonitors[i].name != config.monitorName)
			{
				continue;
			}
			if (firstNameMatch < 0)
			{
				firstNameMatch = i;
			}
			if (liveOutputMonitors[i].index == config.monitorIndex)
			{
				return i;
			}
		}
		return firstNameMatch;
	}

	for (int i = 0; i < (int)liveOutputMonitors.size(); i++)
	{
		if (liveOutputMonitors[i].index == config.monitorIndex)
		{
			return i;
		}
	}
	return -1;
}

string ofApp::makeLiveOutputId()
{
	while (true)
	{
		const string candidate = "output_" + ofToString(nextLiveOutputId++);
		bool used = false;
		for (const LiveOutputRuntime &output : liveOutputs)
		{
			if (output.config.id == candidate)
			{
				used = true;
				break;
			}
		}
		if (!used)
		{
			return candidate;
		}
	}
}

string ofApp::getLiveOutputDisplayName(int index) const
{
	if (index >= 0 && index < (int)liveOutputs.size())
	{
		const string &id = liveOutputs[index].config.id;
		const string prefix = "output_";
		if (id.rfind(prefix, 0) == 0 &&
			id.size() > prefix.size())
		{
			return "Output " + id.substr(prefix.size());
		}
	}
	return "Output " + ofToString(index + 1);
}

void ofApp::initializeDefaultLiveOutput()
{
	if (liveOutputMonitors.empty())
	{
		refreshLiveOutputMonitors();
	}

	LiveOutputRuntime output;
	output.config.id = makeLiveOutputId();
	for (const LiveOutputMonitor &monitor : liveOutputMonitors)
	{
		if (monitor.primary)
		{
			output.config.monitorName = monitor.name;
			output.config.monitorIndex = monitor.index;
			break;
		}
	}
	if (output.config.monitorName.empty() && !liveOutputMonitors.empty())
	{
		output.config.monitorName = liveOutputMonitors[0].name;
		output.config.monitorIndex = liveOutputMonitors[0].index;
	}
	liveOutputs.push_back(output);
	selectedLiveOutput = (int)liveOutputs.size() - 1;
	initLiveOutputFields();
}

void ofApp::addLiveOutput()
{
	initializeDefaultLiveOutput();
	saveSettings();
}

void ofApp::removeSelectedLiveOutput()
{
	if (selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		return;
	}
	closeLiveOutputWindow(selectedLiveOutput, true);
	liveOutputs.erase(liveOutputs.begin() + selectedLiveOutput);
	selectedLiveOutput = liveOutputs.empty() ? -1 :
		ofClamp(selectedLiveOutput, 0, (int)liveOutputs.size() - 1);
	liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
	initLiveOutputFields();
	saveSettings();
}

int ofApp::findLiveOutputByWindow(ofAppBaseWindow *window) const
{
	if (window == nullptr)
	{
		return -1;
	}
	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		if (liveOutputs[i].window.get() == window)
		{
			return i;
		}
	}
	return -1;
}

void ofApp::closeLiveOutputWindow(int index, bool intentional)
{
	if (index < 0 || index >= (int)liveOutputs.size())
	{
		return;
	}
	LiveOutputRuntime &output = liveOutputs[index];
	if (!output.window)
	{
		output.closePending = false;
		return;
	}

	(void)intentional;
	ofRemoveListener(output.window->events().draw,
		this, &ofApp::window_drawRender);
	ofRemoveListener(output.window->events().exit,
		this, &ofApp::exit);
	ofRemoveListener(output.window->events().keyPressed,
		this, &ofApp::window_keyPressed);
	ofRemoveListener(output.window->events().mouseMoved,
		this, &ofApp::window_mouseMove);
	ofRemoveListener(output.window->events().windowResized,
		this, &ofApp::window_resized);
	ofRemoveListener(output.window->events().windowMoved,
		this, &ofApp::window_moved);
	output.window->setWindowShouldClose();
	RetiredLiveOutputWindow retired;
	retired.window = output.window;
	retiredLiveOutputWindows.push_back(retired);
	output.window.reset();
	output.closePending = false;
}

// The live outputs are separate windows owned by the main loop, so closing the
// GUI does not remove them: the loop keeps spinning on the leftovers and the
// process survives with orphaned output windows on screen. Tear them all down
// when the app exits. This deliberately leaves config.enabled alone, so the
// outputs come back on the next launch.
void ofApp::closeAllLiveOutputWindows()
{
	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		liveOutputs[i].recreatePending = false;
		liveOutputs[i].closePending = false;
		if (liveOutputs[i].window)
		{
			closeLiveOutputWindow(i, true);
		}
	}
	// Nothing will drain the retired list after exit, so release it here
	// instead of holding the windows alive until ofApp is destroyed.
	retiredLiveOutputWindows.clear();
}

void ofApp::createLiveOutputWindow(int index)
{
	if (index < 0 || index >= (int)liveOutputs.size())
	{
		return;
	}
	LiveOutputRuntime &output = liveOutputs[index];
	if (!output.config.enabled || output.window)
	{
		return;
	}

	output.createAttempted = true;
	refreshLiveOutputMonitors();
	const int resolvedMonitor = resolveLiveOutputMonitor(output.config);
	if (resolvedMonitor < 0)
	{
		return;
	}
	const LiveOutputMonitor &monitor = liveOutputMonitors[resolvedMonitor];
	output.config.monitorIndex = monitor.index;

	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 2);
	settings.shareContextWith = mainWindow;
	settings.monitor = monitor.index;
	settings.resizable = !output.config.fullscreen;
	settings.title = "Guipper - " + getLiveOutputDisplayName(index);
	if (output.config.fullscreen)
	{
		settings.windowMode = OF_FULLSCREEN;
		settings.setSize(monitor.width, monitor.height);
	}
	else
	{
		output.config.width = ofClamp(output.config.width, 64, 16384);
		output.config.height = ofClamp(output.config.height, 64, 16384);
		settings.windowMode = OF_WINDOW;
		settings.setSize(output.config.width, output.config.height);
		if (!output.config.hasPosition)
		{
			output.config.x = monitor.x +
				(monitor.width - output.config.width) / 2;
			output.config.y = monitor.y +
				(monitor.height - output.config.height) / 2;
			output.config.hasPosition = true;
		}
		settings.setPosition(ofVec2f(output.config.x, output.config.y));
	}

	output.window = ofCreateWindow(settings);
	if (!output.window)
	{
		return;
	}
	output.createAttempted = false;
	output.window->setWindowTitle(settings.title);
#ifdef TARGET_LINUX
	auto glfwWindow = dynamic_pointer_cast<ofAppGLFWWindow>(output.window);
	if (glfwWindow)
	{
		ofImage appIcon;
		if (appIcon.load("guipper.png"))
		{
			appIcon.setImageType(OF_IMAGE_COLOR_ALPHA);
			glfwWindow->setWindowIcon(appIcon.getPixels());
		}
	}
#endif

	ofAddListener(output.window->events().draw,
		this, &ofApp::window_drawRender);
	ofAddListener(output.window->events().exit,
		this, &ofApp::exit);
	ofAddListener(output.window->events().keyPressed,
		this, &ofApp::window_keyPressed);
	ofAddListener(output.window->events().mouseMoved,
		this, &ofApp::window_mouseMove);
	ofAddListener(output.window->events().windowResized,
		this, &ofApp::window_resized);
	ofAddListener(output.window->events().windowMoved,
		this, &ofApp::window_moved);
}

void ofApp::requestLiveOutputRecreate(int index)
{
	if (index < 0 || index >= (int)liveOutputs.size())
	{
		return;
	}
	liveOutputs[index].recreatePending = true;
	liveOutputs[index].createAttempted = false;
}

void ofApp::updateLiveOutputs()
{
	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		LiveOutputRuntime &output = liveOutputs[i];
		if (output.recreatePending)
		{
			if (output.window)
			{
				closeLiveOutputWindow(i, true);
			}
			output.recreatePending = false;
			output.createAttempted = false;
		}
		else if (output.closePending || (!output.config.enabled && output.window))
		{
			closeLiveOutputWindow(i, true);
			continue;
		}

		if (output.config.enabled && !output.window &&
			!output.createAttempted)
		{
			createLiveOutputWindow(i);
		}
	}
}

void ofApp::updateRetiredLiveOutputWindows()
{
	for (int i = (int)retiredLiveOutputWindows.size() - 1;
		i >= 0; i--)
	{
		RetiredLiveOutputWindow &retired =
			retiredLiveOutputWindows[i];
		if (retired.releaseCountdown > 0)
		{
			retired.releaseCountdown--;
			continue;
		}
		retiredLiveOutputWindows.erase(
			retiredLiveOutputWindows.begin() + i);
	}
}

void ofApp::openRenderWindow() {
	if (liveOutputs.empty())
	{
		initializeDefaultLiveOutput();
	}
	selectedLiveOutput = 0;
	liveOutputs[0].config.enabled = true;
	if (!liveOutputs[0].window)
	{
		liveOutputs[0].recreatePending = true;
	}
	updateLiveOutputs();
	saveSettings();
}
void ofApp::loadSettings() {
	const auto settingsPath = ofToDataPath("settings.xml");
	refreshLiveOutputMonitors();
	if (!ofFile(settingsPath).exists())
	{
		initializeDefaultLiveOutput();
		return;
	}

	ofXml xml;
	if (!xml.load(settingsPath))
	{
		initializeDefaultLiveOutput();
		return;
	}

	auto settings = xml.getChild("settings");
	if (!settings)
	{
		initializeDefaultLiveOutput();
		return;
	}
	auto intValue = [](const ofXml &parent, const string &name, int fallback) {
		auto child = parent.getChild(name);
		return child ? child.getIntValue() : fallback;
	};
	auto boolValue = [](const ofXml &parent, const string &name, bool fallback) {
		auto child = parent.getChild(name);
		return child ? child.getBoolValue() : fallback;
	};
	auto stringValue = [](const ofXml &parent, const string &name,
		const string &fallback) {
		auto child = parent.getChild(name);
		return child ? child.getValue() : fallback;
	};

	auto renderwidthaux = settings.getChild("renderwidth");
	auto renderheightaux = settings.getChild("renderheight");
	auto windowx = settings.getChild("window_x");
	auto windowy = settings.getChild("window_y");
	auto windowwidth = settings.getChild("window_width");
	auto windowheight = settings.getChild("window_height");
	auto windowopen = settings.getChild("window_open");
	auto window_fullscreenaux = settings.getChild("window_fullscreen");
#ifdef SPOUT
	auto spouton = settings.getChild("spouton");
#endif
	auto oscportin = settings.getChild("osc_port_in");
	auto oscportout = settings.getChild("osc_port_out");
	auto oscipout = settings.getChild("osc_ip_out");
	auto oscout1 = settings.getChild("oscout_mode1");
	auto oscout2 = settings.getChild("oscout_mode2");
	auto durationgallery = settings.getChild("durationgallery");
	auto defaultCompoChild = settings.getChild("defaultcompo");
	auto cuePanelX = settings.getChild("cue_panel_x");
	auto cuePanelY = settings.getChild("cue_panel_y");
	auto cuePanelW = settings.getChild("cue_panel_w");
	auto cuePanelH = settings.getChild("cue_panel_h");
	auto mappingPanelX = settings.getChild("mapping_panel_x");
	auto mappingPanelY = settings.getChild("mapping_panel_y");
	auto mappingPanelW = settings.getChild("mapping_panel_w");
	auto mappingPanelH = settings.getChild("mapping_panel_h");
	auto favoritesDisplayModeChild = settings.getChild("favorites_display_mode");

	const int graphWidth = renderwidthaux ?
		renderwidthaux.getIntValue() : jp_constants::renderWidth;
	const int graphHeight = renderheightaux ?
		renderheightaux.getIntValue() : jp_constants::renderHeight;
	const int legacyWidth = windowwidth ?
		windowwidth.getIntValue() : 1280;
	const int legacyHeight = windowheight ?
		windowheight.getIntValue() : 720;
	jp_constants::init(graphWidth, graphHeight, legacyWidth, legacyHeight);
	boxes.setDurationGalleryMs(durationgallery ?
		durationgallery.getFloatValue() : boxes.getDurationGalleryMs());
	if (cuePanelX && cuePanelY && cuePanelW && cuePanelH) {
		boxes.setCuePanelLayout(
			24.0f, // siempre izquierda
			ofGetHeight() - cuePanelH.getFloatValue() - 24.0f, // siempre abajo
			cuePanelW.getFloatValue(),
			cuePanelH.getFloatValue());
	}
	if (mappingPanelX && mappingPanelY && mappingPanelW && mappingPanelH) {
		boxes.setMappingPanelLayout(
			mappingPanelX.getFloatValue(),
			mappingPanelY.getFloatValue(),
			mappingPanelW.getFloatValue(),
			mappingPanelH.getFloatValue());
	}

#ifdef SPOUT
	if (spouton) spoutActive = spouton.getBoolValue();
#endif
#ifdef NDI
	{
		auto ndion = settings.getChild("ndion");
		if (ndion) ndiActive = ndion.getBoolValue();
	}
#endif
	if (oscout1) oscout_mode1 = oscout1.getBoolValue();
	if (oscout2) oscout_mode2 = oscout2.getBoolValue();

	if (defaultCompoChild) {
		defaultCompoPath = defaultCompoChild.getValue();
		cout << "defaultcompo: " << defaultCompoPath << endl;
	}
	if (favoritesDisplayModeChild) {
		favoritesDisplayMode = favoritesDisplayModeChild.getIntValue() == FAVORITES_IN_FOLDERS ?
			FAVORITES_IN_FOLDERS : FAVORITES_TOP;
	}

	receiver.setup(oscportin ? oscportin.getIntValue() : PORT);
	sender.setup(oscipout ? oscipout.getValue() : "127.0.0.1",
		oscportout ? oscportout.getIntValue() : 0);
	{
		auto bpmaux = settings.getChild("bpm");
		if (bpmaux) jp_constants::setBpm((float)bpmaux.getIntValue());
	}
	liveOutputs.clear();
	nextLiveOutputId = 1;
	auto liveOutputsNode = settings.getChild("live_outputs");
	if (liveOutputsNode)
	{
		for (auto &outputNode : liveOutputsNode.getChildren("output"))
		{
			LiveOutputRuntime output;
			output.config.id = stringValue(outputNode, "id", "");
			if (output.config.id.empty())
			{
				output.config.id = makeLiveOutputId();
			}
			output.config.enabled = boolValue(outputNode, "enabled", false);
			const string sourceMode = stringValue(
				outputNode, "source_mode", "main_active");
			output.config.sourceMode = sourceMode == "fixed_box" ?
				LIVE_OUTPUT_FIXED_BOX : LIVE_OUTPUT_MAIN_ACTIVE;
			output.config.sourceBox = stringValue(
				outputNode, "source_box", "");
			output.config.monitorName = stringValue(
				outputNode, "monitor_name", "");
			output.config.monitorIndex = intValue(
				outputNode, "monitor_index", 0);
			output.config.width = ofClamp(
				intValue(outputNode, "width", 1280), 64, 16384);
			output.config.height = ofClamp(
				intValue(outputNode, "height", 720), 64, 16384);
			output.config.x = intValue(outputNode, "x", 0);
			output.config.y = intValue(outputNode, "y", 0);
			output.config.hasPosition = boolValue(
				outputNode, "has_position", true);
			output.config.fullscreen = boolValue(
				outputNode, "fullscreen", false);
			liveOutputs.push_back(output);
		}
	}
	else
	{
		LiveOutputRuntime output;
		output.config.id = makeLiveOutputId();
		output.config.enabled = windowopen ?
			windowopen.getBoolValue() : false;
		output.config.width = ofClamp(legacyWidth, 64, 16384);
		output.config.height = ofClamp(legacyHeight, 64, 16384);
		output.config.x = windowx ? windowx.getIntValue() : 0;
		output.config.y = windowy ? windowy.getIntValue() : 0;
		output.config.hasPosition = windowx && windowy;
		output.config.fullscreen = window_fullscreenaux ?
			window_fullscreenaux.getBoolValue() : false;

		const int centerX = output.config.x + output.config.width / 2;
		const int centerY = output.config.y + output.config.height / 2;
		int monitorMatch = -1;
		for (int i = 0; i < (int)liveOutputMonitors.size(); i++)
		{
			const LiveOutputMonitor &monitor = liveOutputMonitors[i];
			if (centerX >= monitor.x &&
				centerX < monitor.x + monitor.width &&
				centerY >= monitor.y &&
				centerY < monitor.y + monitor.height)
			{
				monitorMatch = i;
				break;
			}
			if (monitor.primary)
			{
				monitorMatch = i;
			}
		}
		if (monitorMatch >= 0)
		{
			output.config.monitorName =
				liveOutputMonitors[monitorMatch].name;
			output.config.monitorIndex =
				liveOutputMonitors[monitorMatch].index;
		}
		liveOutputs.push_back(output);
	}
	selectedLiveOutput = liveOutputs.empty() ? -1 : 0;
	initLiveOutputFields();
	updateLiveOutputs();
}
std::string toXmlString(const bool value) {
	return value ? "true" : "false";
}
void ofApp::saveSettings() {
	const auto settingsPath = ofToDataPath("settings.xml");

	ofXml xml;
	float cuePanelX = 0.0f;
	float cuePanelY = 0.0f;
	float cuePanelW = 0.0f;
	float cuePanelH = 0.0f;
	boxes.getCuePanelLayout(cuePanelX, cuePanelY, cuePanelW, cuePanelH);
	float mappingPanelX = 0.0f;
	float mappingPanelY = 0.0f;
	float mappingPanelW = 0.0f;
	float mappingPanelH = 0.0f;
	boxes.getMappingPanelLayout(
		mappingPanelX, mappingPanelY, mappingPanelW, mappingPanelH);
	for (LiveOutputRuntime &output : liveOutputs)
	{
		if (!output.window || output.config.fullscreen ||
			output.recreatePending ||
			output.window->getWindowMode() != OF_WINDOW)
		{
			continue;
		}
		const glm::vec2 position = output.window->getWindowPosition();
		const glm::vec2 size = output.window->getWindowSize();
		output.config.x = (int)position.x;
		output.config.y = (int)position.y;
		output.config.width = std::max(64, (int)size.x);
		output.config.height = std::max(64, (int)size.y);
		output.config.hasPosition = true;
	}

	auto settings = xml.appendChild("settings");
	settings.appendChild("renderwidth").set(jp_constants::renderWidth);
	settings.appendChild("renderheight").set(jp_constants::renderHeight);
	settings.appendChild("durationgallery").set(boxes.getDurationGalleryMs());
	settings.appendChild("cue_panel_x").set(cuePanelX);
	settings.appendChild("cue_panel_y").set(cuePanelY);
	settings.appendChild("cue_panel_w").set(cuePanelW);
	settings.appendChild("cue_panel_h").set(cuePanelH);
	settings.appendChild("mapping_panel_x").set(mappingPanelX);
	settings.appendChild("mapping_panel_y").set(mappingPanelY);
	settings.appendChild("mapping_panel_w").set(mappingPanelW);
	settings.appendChild("mapping_panel_h").set(mappingPanelH);
	LiveOutputConfig legacyOutput;
	if (!liveOutputs.empty())
	{
		legacyOutput = liveOutputs.front().config;
	}
	settings.appendChild("window_x").set(legacyOutput.x);
	settings.appendChild("window_y").set(legacyOutput.y);
	settings.appendChild("window_width").set(legacyOutput.width);
	settings.appendChild("window_height").set(legacyOutput.height);
	settings.appendChild("window_fullscreen").set(
		toXmlString(legacyOutput.fullscreen));
	settings.appendChild("window_open").set(
		toXmlString(legacyOutput.enabled));
#ifdef SPOUT
	settings.appendChild("spouton").set(toXmlString(spoutActive));
#endif
#ifdef NDI
	settings.appendChild("ndion").set(toXmlString(ndiActive));
#endif
	settings.appendChild("osc_port_in").set(receiver.getPort());
	settings.appendChild("osc_port_out").set(sender.getPort());
	settings.appendChild("osc_ip_out").set(sender.getHost());
	settings.appendChild("oscout_mode1").set(toXmlString(oscout_mode1));
	settings.appendChild("oscout_mode2").set(toXmlString(oscout_mode2));
	settings.appendChild("defaultcompo").set(defaultCompoPath);
	settings.appendChild("bpm").set((int)jp_constants::bpm);
	settings.appendChild("favorites_display_mode").set((int)favoritesDisplayMode);

	auto liveOutputsNode = settings.appendChild("live_outputs");
	for (const LiveOutputRuntime &output : liveOutputs)
	{
		const LiveOutputConfig &config = output.config;
		auto outputNode = liveOutputsNode.appendChild("output");
		outputNode.appendChild("id").set(config.id);
		outputNode.appendChild("enabled").set(
			toXmlString(config.enabled));
		outputNode.appendChild("source_mode").set(
			config.sourceMode == LIVE_OUTPUT_FIXED_BOX ?
				"fixed_box" : "main_active");
		outputNode.appendChild("source_box").set(config.sourceBox);
		outputNode.appendChild("monitor_name").set(config.monitorName);
		outputNode.appendChild("monitor_index").set(config.monitorIndex);
		outputNode.appendChild("width").set(config.width);
		outputNode.appendChild("height").set(config.height);
		outputNode.appendChild("x").set(config.x);
		outputNode.appendChild("y").set(config.y);
		outputNode.appendChild("has_position").set(
			toXmlString(config.hasPosition));
		outputNode.appendChild("fullscreen").set(
			toXmlString(config.fullscreen));
	}

	xml.save(settingsPath);
}
void ofApp::saveSession(string path) {
	boxes.save(path);
}
void ofApp::loadSession(string path) {
	boxes.load(path);
}
void ofApp::updateOSC() {
	// hide old messages

	// RECEIVER
	//  check for waiting messages
	while (receiver.hasWaitingMessages()) {
		// get the next message
		ofxOscMessage m;
		receiver.getNextMessage(&m);
		boxes.listenToOsc(m.getAddress(), m.getArgAsFloat(0)); // ACA PROCESA TODO EL OSC DE LAS CAJAS BASICAMENTE TODO
		// cout << "ADDRES:" << m.getAddress() << endl;
		// cout << "VALUE:" << m.getArgAsFloat(0) << endl;
		cout << "LLEGA OSC  " << m.getAddress() << endl;
		if (m.getAddress().find("load") != std::string::npos) {
			cout << "ENCONTRO LOAD " << endl;
			string dir(m.getAddress(), 5, (m.getAddress().size()));
			cout << "DIR : " << dir << endl;

			string dirfinal = "savefiles/" + dir;
			cout << "DIR FINNAL : " << dirfinal << endl;
			loadSession(dirfinal);
			savedirectory = dirfinal;
		}
	}

	// SENDER
	// ESTO VA A HABER QUE CODEARLO MEJOR PERO VAMOS ASI POR AHORA:
	// FORMA 1 : MANDA CON NOMBRE DE CAJA TODO EL TIEMPO TODAS LAS VECESSSS
	if (oscout_mode1) {
		for (int i = 0; i < boxes.getBoxesSize(); i++) {
			for (int k = 0; k < boxes.boxes[i]->parameters.getSize(); k++) {
				ofxOscMessage m;
				string mensajefinal = boxes.boxes[i]->name + "/" + boxes.boxes[i]->parameters.getName(k);
				m.setAddress(mensajefinal);
				if (boxes.boxes[i]->parameters.getType(k) == boxes.boxes[i]->parameters.BOOL) {
					m.addBoolArg(boxes.boxes[i]->parameters.getBoolValue(k));
				} else {
					m.addFloatArg(boxes.boxes[i]->parameters.getFloatValue(k));
				}
				sender.sendMessage(m, false);
			}
		}
	}

	//FORMA 2: MANDA LOS NOMBRES COMO V1,V2,V3 Y SOLO DE LA CAJA DE LA INTERFAZ ACTIVA. :
	if (oscout_mode2) {
		if (boxes.openguinumber != -1) {
			for (int k = 0; k < boxes.boxes[boxes.openguinumber]->parameters.getSize(); k++) {
				ofxOscMessage m;
				string mensajefinal = "v" + ofToString(k);
				if (boxes.boxes[boxes.openguinumber]->parameters.getType(k) == boxes.boxes[boxes.openguinumber]->parameters.BOOL) {
					m.addBoolArg(boxes.boxes[boxes.openguinumber]->parameters.getBoolValue(k));
				} else {
					m.addFloatArg(boxes.boxes[boxes.openguinumber]->parameters.getFloatValue(k));
				}
				m.setAddress(mensajefinal);
				sender.sendMessage(m, false);
			}
		}
	}
}
// Shutting down has to be idempotent and reachable from either exit overload.
// ofBaseApp::exit(ofEventArgs&) is virtual and normally forwards to exit(), but
// we override the ofEventArgs version for the live output windows, which
// replaces that forwarder: OF routes the app's own exit event here too, so
// exit() alone never runs.
void ofApp::shutdownApp() {
	if (appShutdownDone) {
		return;
	}
	appShutdownDone = true;
	saveSettings();
	midiKeymap.exit();
	closeAllLiveOutputWindows();
}
void ofApp::exit() {
	shutdownApp();
}

// LISTENERS DE LAS VENTANAS:
void ofApp::window_drawRender(ofEventArgs & args) {
	const int index = findLiveOutputByWindow(ofGetWindowPtr());
	if (index < 0 || !liveOutputs[index].window)
	{
		return;
	}

	const LiveOutputConfig &config = liveOutputs[index].config;
	const float width = liveOutputs[index].window->getWidth();
	const float height = liveOutputs[index].window->getHeight();
	ofClear(0, 0, 0, 255);
	ofSetColor(255);
	const bool followMain =
		config.sourceMode == LIVE_OUTPUT_MAIN_ACTIVE;
	const bool sourceAvailable = boxes.drawLiveOutputSource(
		followMain, config.sourceBox, width, height);
	if (sourceAvailable)
	{
		boxes.drawMappingOverlayForSource(
			followMain, config.sourceBox, width, height);
	}
	else
	{
		const string message = followMain ?
			"No active source" : "Missing source";
		ofSetColor(COL_TEXT_MUTED);
		const float textWidth = font_p.stringWidth(message);
		font_p.drawString(message,
			(width - textWidth) * 0.5f, height * 0.5f);
	}
}
void ofApp::exit(ofEventArgs & e) {
	const int index = findLiveOutputByWindow(ofGetWindowPtr());
	if (index < 0)
	{
		// Not one of the live outputs, so this is the main window closing:
		// shut the whole app down, which also takes the output windows with it.
		shutdownApp();
		return;
	}
	LiveOutputRuntime &output = liveOutputs[index];
	output.config.enabled = false;
	output.closePending = false;
	RetiredLiveOutputWindow retired;
	retired.window = output.window;
	retiredLiveOutputWindows.push_back(retired);
	output.window.reset();
	saveSettings();
}
void ofApp::window_mouseMove(ofMouseEventArgs & e) {
	// Live outputs are display-only. Their pointer state is intentionally local.
}
void ofApp::window_resized(ofResizeEventArgs & args) {
	const int index = findLiveOutputByWindow(ofGetWindowPtr());
	if (index < 0 || liveOutputs[index].config.fullscreen ||
		liveOutputs[index].recreatePending ||
		!liveOutputs[index].window ||
		liveOutputs[index].window->getWindowMode() != OF_WINDOW)
	{
		return;
	}
	liveOutputs[index].config.width = std::max(64, args.width);
	liveOutputs[index].config.height = std::max(64, args.height);
	if (selectedLiveOutput == index && focusedLiveOutputField < 0)
	{
		initLiveOutputFields();
	}
}
void ofApp::window_moved(ofWindowPosEventArgs &args) {
	const int index = findLiveOutputByWindow(ofGetWindowPtr());
	if (index < 0 || liveOutputs[index].config.fullscreen ||
		liveOutputs[index].recreatePending ||
		!liveOutputs[index].window ||
		liveOutputs[index].window->getWindowMode() != OF_WINDOW)
	{
		return;
	}
	liveOutputs[index].config.x = (int)args.x;
	liveOutputs[index].config.y = (int)args.y;
	liveOutputs[index].config.hasPosition = true;
}
void ofApp::window_keyPressed(ofKeyEventArgs & e) {
	if (e.key == 'f' || e.key == 'F')
	{
		const int index = findLiveOutputByWindow(ofGetWindowPtr());
		if (index < 0)
		{
			return;
		}
		liveOutputs[index].config.fullscreen =
			!liveOutputs[index].config.fullscreen;
		requestLiveOutputRecreate(index);
		updateLiveOutputs();
		saveSettings();
	}
}

#ifdef SPOUT
// ESTA FUNCION CORRE EN EL SPOUT Y HACE TODO LO QUE TENGA QUE VER CON DIBUJAR EL SPOUT SENDER:
void ofApp::drawSpout() {

	char str[256];
	ofSetColor(COL_TEXT_PRIMARY);
	// ====== SPOUT =====
	// A render window must be available for Spout initialization and might not be
	// available in "update" so do it now when there is definitely a render window.
	if (!bInitialized) {
		// Create the sender
		bInitialized = spoutsender.CreateSender(sendername, resolution_spoutext.x, resolution_spoutext.y);
	}

	//ofSetColor(255, 255, 0, 255);
	//ofDrawRectangle(0, 0, resolution_spoutext.x, resolution_spoutext.y);
	//ofSetColor(255, 255);
	//ofDrawRectangle(0, 0, ofGetWidth() * .9, ofGetHeight() * .9);
	//boxes.draw_activerender(resolution_spoutext.x, resolution_spoutext.y);

	// ====== SPOUT =====
	if (bInitialized) {
		if (ofGetWidth() > 0 && ofGetHeight() > 0) { // protect against user minimize
			ofFbo & fbo = *boxes.getActiverender();
			GLuint texID = fbo.getTexture().getTextureData().textureID;
			spoutsender.SendTexture(texID, GL_TEXTURE_2D, resolution_spoutext.x, resolution_spoutext.y);
		}
	}
}
#endif

bool ofApp::InitGLtexture(GLuint & texID, unsigned int width, unsigned int height) {
	if (texID != 0)
		glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

// Save-as modal: draws a centered overlay with a text input field and
// SAVE / UPDATE / CANCEL buttons
void ofApp::drawSaveModal() {
	if (!saveModalActive) return;

	// Ensure CORNER rect mode — node editor (ventana 1) may leave CENTER set
	ofSetRectMode(OF_RECTMODE_CORNER);

	float w = ofGetWidth();
	float h = ofGetHeight();

	// Full scene overlay (dark translucent)
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, w, h);

	// Modal box dimensions — taller to fit buttons
	float boxW = 420;
	float boxH = 240;
	float boxX = (w - boxW) * 0.5f;
	float boxY = (h - boxH) * 0.5f;
	float corner = 10;

	// Rounded shadow behind modal
	ofSetColor(0, 0, 0, 100);
	ofDrawRectRounded(boxX + 5, boxY + 5, boxW, boxH, corner);

	// Panel background — same glassmorphism as draw_opciones
	ofSetColor(ofColor(COL_BG_DARK, 238));
	ofDrawRectRounded(boxX, boxY, boxW, boxH, corner);

	// Panel border — cyan with matching alpha
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(boxX, boxY, boxW, boxH, corner);
	ofFill();
	ofSetLineWidth(1.0f);

	float pad = 20;

	// Title — cyan, same style as "SETTINGS.XML Configuration"
	ofSetColor(COL_ACCENT_CYAN);
	modalFont.drawString("SAVE COMPOSITION", boxX + pad, boxY + 34);

	// Thin separator
	ofSetColor(COL_SLIDER_FILL);
	ofDrawLine(boxX + pad, boxY + 45, boxX + boxW - pad, boxY + 45);

	// Input field
	float fieldX = boxX + pad;
	float fieldY = boxY + 58;
	float fieldW = boxW - pad * 2;
	float fieldH = 32;

	// Field background
	ofSetColor(COL_BG_TAB);
	ofDrawRectRounded(fieldX, fieldY, fieldW, fieldH, 4);

	// Field border — cyan (always focused while modal is active)
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofSetLineWidth(2.0f);
	ofDrawRectRounded(fieldX, fieldY, fieldW, fieldH, 4);
	ofFill();
	ofSetLineWidth(1.0f);

	// Filename text inside the field — white with blinking cursor
	ofSetColor(COL_TEXT_PRIMARY);
	float textY = fieldY + fieldH * 0.5f + 5.0f;
	modalFont.drawString(saveModalName, fieldX + 8, textY);
	jp_textfield::drawCaret(modalFont, saveModalName, saveModalCursor,
							fieldX + 8, fieldY + fieldH * 0.5f, fieldH - 10);
	jp_tooltip::draw("Enter composition filename", fieldX, fieldY, fieldW, fieldH);

	// Preview path below the field
	ofSetColor(100, 130, 160);
	string previewName = saveModalName.empty() ? string("composition") : saveModalName;
	string preview = "savefiles/" + previewName + ".xml";
	modalFont.drawString(preview, boxX + pad, fieldY + fieldH + 22);

	// ─── Buttons: SAVE | UPDATE | CANCEL ──────────────────────────────
	float btnY = boxY + boxH - 42;
	float btnH = 28;
	float btnGap = 12;

	// Three buttons, evenly spaced
	float totalBtnsW = boxW - pad * 2;
	float btnW = (totalBtnsW - btnGap * 2) / 3.0f;

	float saveBtnX = boxX + pad;
	float updateBtnX = saveBtnX + btnW + btnGap;
	float cancelBtnX = updateBtnX + btnW + btnGap;

	// --- SAVE button (cyan) ---
	ofSetColor(ofColor(COL_BOX_BORDER, 230));
	ofDrawRectRounded(saveBtnX, btnY, btnW, btnH, 4);
	ofSetColor(COL_BG_DARK);
	string saveLabel = "SAVE";
	float saveLabelW = modalFont.stringWidth(saveLabel);
	modalFont.drawString(saveLabel, saveBtnX + btnW / 2 - saveLabelW / 2, btnY + btnH / 2 + 5);
	jp_tooltip::draw("Save as new composition", saveBtnX, btnY, btnW, btnH);

	// --- UPDATE button (amber / gold) ---
	ofSetColor(220, 190, 50, 230);
	ofDrawRectRounded(updateBtnX, btnY, btnW, btnH, 4);
	ofSetColor(COL_BG_DARK);
	string updateLabel = "UPDATE";
	float updateLabelW = modalFont.stringWidth(updateLabel);
	modalFont.drawString(updateLabel, updateBtnX + btnW / 2 - updateLabelW / 2, btnY + btnH / 2 + 5);
	jp_tooltip::draw("Overwrite the selected composition", updateBtnX, btnY, btnW, btnH);

	// --- CANCEL button (gray) ---
	ofSetColor(ofColor(COL_TEXT_MUTED, 230));
	ofDrawRectRounded(cancelBtnX, btnY, btnW, btnH, 4);
	ofSetColor(COL_TEXT_SECONDARY);
	string cancelLabel = "CANCEL";
	float cancelLabelW = modalFont.stringWidth(cancelLabel);
	modalFont.drawString(cancelLabel, cancelBtnX + btnW / 2 - cancelLabelW / 2, btnY + btnH / 2 + 5);
	jp_tooltip::draw("Close without saving", cancelBtnX, btnY, btnW, btnH);
}

void ofApp::confirmSaveModal() {
	if (saveModalName.empty()) return;

	string filename = saveModalName;
	// Ensure .xml extension
	if (filename.find(".xml") == string::npos) {
		filename += ".xml";
	}
	string path = "savefiles/" + filename;
	cout << "Save modal confirmed: " << path << endl;
	savedirectory = path;
	saveSession(path);
	saveModalActive = false;
	saveModalName = "";
}

void ofApp::cancelSaveModal() {
	cout << "Save modal cancelled" << endl;
	saveModalActive = false;
	saveModalName = "";
}

void ofApp::updateSaveModal() {
	cout << "Update save: " << savedirectory << endl;
	saveSession(savedirectory);
	saveModalActive = false;
	saveModalName = "";
}

void ofApp::drawScreenTabs() {
	const float tabX = 0;
	const float tabY = 0;
	const float tabH = 28;
	const float pad = 8;
	const float gap = 2;

	struct ScreenTab {
		string label;
		int screenId;
		string tooltip;
	};
	vector<ScreenTab> tabs = {
		{"NODES", NODOS, "Edit the node graph"},
		{"SETTINGS", OPCIONES, "Configure ports, render size, BPM, and output"},
		{"HELP", TUTORIAL, "View keyboard and workflow help"},
		{"IMPORT", SHADER_INDEX, "Browse and preview shader boxes"},
		{"EDITOR", EDITOR, "Edit the selected shader source"}
	};

	float x = tabX + pad;
	const float y = tabY + pad;

	for (int i = 0; i < (int)tabs.size(); i++) {
		const string &label = tabs[i].label;
		int screenId = tabs[i].screenId;
		bool active = (pantallaActiva == screenId);

		float textW = jp_constants::p_font.stringWidth(label);
		float tabMinWidth = 90;
		float tabW = max(tabMinWidth, textW + 24);

		// Draw tab background
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		if (active) {
			ofSetColor(ofColor(COL_ACCENT_GREEN, 235));
		} else {
			ofSetColor(ofColor(COL_TAB_INACTIVE_BG, 225));
		}
		ofDrawRectRounded(x, y, tabW, tabH, 3);

		// Border
		ofNoFill();
		ofSetLineWidth(1);
		if (active) {
			ofSetColor(COL_ACCENT_GREEN_BR);
		} else {
			ofSetColor(ofColor(COL_BORDER_MUTED, 200));
		}
		ofDrawRectRounded(x, y, tabW, tabH, 3);
		ofFill();

		// Text
		ofSetColor(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		jp_constants::p_font.drawString(label, x + (tabW - textW) * 0.5f, y + tabH * 0.5f + 5);

		ofPopStyle();
		jp_tooltip::draw(tabs[i].tooltip, x, y, tabW, tabH);

		x += tabW + gap;
	}
}

int ofApp::getScreenTabAtPos(int x, int y) {
	const float tabX = 0;
	const float tabY = 0;
	const float tabH = 28;
	const float pad = 8;
	const float gap = 2;

	// Check if y is within screen tab bar
	if (y < tabY || y > tabY + tabH + pad * 2 + gap * 2) {
		return -1;
	}

	struct ScreenTab {
		string label;
		int screenId;
	};
	vector<ScreenTab> tabs = {
		{"NODES", NODOS},
		{"SETTINGS", OPCIONES},
		{"HELP", TUTORIAL},
		{"IMPORT", SHADER_INDEX},
		{"EDITOR", EDITOR}
	};

	float cx = tabX + pad;
	float cy = tabY + pad;

	for (int i = 0; i < (int)tabs.size(); i++) {
		float textW = jp_constants::p_font.stringWidth(tabs[i].label);
		float tabMinWidth = 90;
		float tabW = max(tabMinWidth, textW + 24);

		if (x >= cx && x <= cx + tabW && y >= cy && y <= cy + tabH) {
			return tabs[i].screenId;
		}
		cx += tabW + gap;
	}
	return -1;
}
