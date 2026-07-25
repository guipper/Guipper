#include "ofApp.h"
#include "JPutils/jp_textfield.h"
#include <iostream>
#include <algorithm>

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
	midiKeymap.setup(&boxes);

	loadSettings();

	ofSetWindowTitle("GUIPPER");

	loadAspreset = true; // Default: drag XML as boxgroup. Press 't' to toggle to full session load.

	// Load preview images for shader index sampler2D preview
	previewImg1.load("preview1.png");
	previewImg2.load("preview2.png");
	if (previewImg1.isAllocated()) cout << "preview1.png loaded OK" << endl;
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

	// Update preview shader FBO every frame (for animated shaders)
	if (previewShaderLoaded && pantallaActiva == SHADER_INDEX) {
		previewFbo.begin();
		ofClear(0, 0, 0, 255);
		previewShader.begin();
		previewShader.setUniform1f("time", ofGetElapsedTimef());
		previewShader.setUniform2f("resolution", previewFbo.getWidth(), previewFbo.getHeight());
		previewShader.setUniform1f("bpm", jp_constants::bpm);
		previewShader.setUniform4f("mouse",
			ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1),
			ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1),
			ofMap(jp_constants::mousePressedPos.x, 0, ofGetWidth(), 0, 1),
			ofMap(jp_constants::mousePressedPos.y, 0, ofGetHeight(), 0, 1));
		previewShader.setUniform2f("window_mouse",
			ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1),
			ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1));
		previewShader.setUniform1i("globalframeNum", ofGetFrameNum());
		previewShader.setUniform1i("boxframeNum", ofGetFrameNum());
		// Random uniform values for preview (set by RDM button)
		if (previewRdmActive && !previewRdmValues.empty()) {
			for (int i = 0; i < (int)previewRdmValues.size() && i < (int)previewUniformNames.size(); i++) {
				previewShader.setUniform1f(previewUniformNames[i], previewRdmValues[i]);
			}
		}
		// Bind preview textures for imageprocessing/blending shaders
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
	float panelY = 44;           // below the top screen-tab bar
	float fieldX = panelX + 145; // panel-relative
	float fieldW = 200;
	float rowH = 28;
	float sepy = 40;
	int totalRows = FIELD_OSC_IP_OUT + 6; // fields + spout + ndi + osc ip + compo + activecompo + save
	float panelH = 55 + totalRows * sepy + 25;
	float toggleBtnW = 70;

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
	font_p.drawString("SETTINGS.XML Configuration", panelX + 15, panelY + 30);

	// Field labels & inputs
	string labels[FIELD_OSC_IP_OUT] = {
		"OSC Port In:",
		"OSC Port Out:",
		"Render Width:",
		"Render Height:",
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
			float tapW = 100;
			ofSetColor(COL_ACCENT_GOLD_DIM);
			ofDrawRectRounded(tapX, rowY, tapW, rowH, 4.0f);
			ofSetColor(COL_TEXT_PRIMARY);
			font_p.drawString("AUTOTAP", tapX + 14, rowY + rowH - 7);
		}
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
		ofSetColor(isOn ? COL_ACCENT_GREEN : COL_ACCENT_RED_DIM);
		ofDrawRectRounded(btnX, rowY, toggleBtnW, rowH, 4.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(isOn ? "ON" : "OFF", btnX + toggleBtnW / 2 - 12, rowY + rowH - 7);
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
		ofSetColor(isOn ? COL_ACCENT_GREEN : COL_ACCENT_RED_DIM);
		ofDrawRectRounded(btnX, rowY, toggleBtnW, rowH, 4.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(isOn ? "ON" : "OFF", btnX + toggleBtnW / 2 - 12, rowY + rowH - 7);
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
		float browseW = 70;
		ofSetColor(60, 80, 140);
		ofDrawRectRounded(browseX, rowY, browseW, rowH, 4.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString("BROWSE", browseX + 10, rowY + rowH - 7);
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
		float saveW = 160;
		float saveX = panelX + panelW / 2 - saveW / 2;
		ofSetColor(COL_ACCENT_CYAN_DIM);
		ofDrawRectRounded(saveX, rowY, saveW, rowH + 4, 6.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString("SAVE SETTINGS", saveX + saveW / 2 - 48, rowY + rowH + 4 - 6);

		// Save feedback text
		if (saveFeedbackTime > 0 && ofGetElapsedTimef() - saveFeedbackTime < 3.0f) {
			ofSetColor(COL_MAPPED_ON);
			float fw = font_p.stringWidth(saveFeedbackText);
			font_p.drawString(saveFeedbackText, saveX + saveW / 2 - fw / 2, rowY + rowH + 4 + 20);
		} else {
			saveFeedbackTime = 0;
		}
	}

	// Hint text when focused
	if (focusedOptionsField >= 0) {
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("Enter to apply | Click outside to cancel", panelX + 15, panelY + panelH - 10);
	}
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
				ShaderEntry e;
				e.name = ofFilePath::getBaseName(path);
				e.path = path;
				rootFolder.shaders.push_back(e);
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
				ShaderEntry e;
				e.name = ofFilePath::getBaseName(path);
				e.path = path;
				folder.shaders.push_back(e);
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
	loadFavorites();
	rebuildFavoritesFolder(); // pins a "favorites" folder on top when any exist
	cout << "Shader index: found " << shaderFolders.size() << " folders" << endl;
	int totalShaders = 0;
	for (auto &f : shaderFolders) totalShaders += (int)f.shaders.size();
	cout << "  total shaders: " << totalShaders << endl;
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
	if (selectedShaderFolder >= 0 && selectedShaderFolder < (int)shaderFolders.size() &&
		selectedShaderIndex >= 0 && selectedShaderIndex < (int)shaderFolders[selectedShaderFolder].shaders.size()) {
		selPath = shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].path;
	}

	// Drop any existing favorites pseudo-folder so we rebuild it cleanly.
	if (!shaderFolders.empty() && shaderFolders.front().isFavorites) {
		shaderFolders.erase(shaderFolders.begin());
	}

	// Re-mark real entries and collect the favorited ones (in file order).
	ShaderFolder fav;
	fav.name = "favorites";
	fav.isFavorites = true;
	fav.expanded = true;
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
	if (!fav.shaders.empty()) {
		shaderFolders.insert(shaderFolders.begin(), fav);
	}

	// Restore selection by path.
	selectedShaderFolder = -1;
	selectedShaderIndex = -1;
	if (!selPath.empty()) {
		for (size_t f = 0; f < shaderFolders.size() && selectedShaderIndex < 0; f++) {
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
void ofApp::selectShaderForPreview(int f, int s) {
	if (f < 0 || f >= (int)shaderFolders.size()) return;
	if (s < 0 || s >= (int)shaderFolders[f].shaders.size()) return;
	selectedShaderFolder = f;
	selectedShaderIndex = s;

	// Load into preview shader (kept for RDM/EDIT + optional preview render).
	string shaderPath = shaderFolders[f].shaders[s].path;
	previewShader.unload();
	previewShaderLoaded = false;
	if (previewShader.load("shaders/default.vert", shaderPath)) {
		previewShaderLoaded = true;
		// Parse user-defined uniform float declarations for RDM
		previewUniformNames.clear();
		previewUniformMins.clear();
		previewUniformMaxs.clear();
		previewRdmValues.clear();
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
		previewRdmActive = false;
		if (!previewFbo.isAllocated()) {
			previewFbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
		}
		// Render first frame immediately with sampler textures
		previewFbo.begin();
		ofClear(0, 0, 0, 255);
		previewShader.begin();
		previewShader.setUniform1f("time", ofGetElapsedTimef());
		previewShader.setUniform2f("resolution", previewFbo.getWidth(), previewFbo.getHeight());
		previewShader.setUniform1f("bpm", jp_constants::bpm);
		previewShader.setUniform4f("mouse", 0.5, 0.5, 0.5, 0.5);
		previewShader.setUniform2f("window_mouse", 0.5, 0.5);
		previewShader.setUniform1i("globalframeNum", 0);
		previewShader.setUniform1i("boxframeNum", 0);
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
	// Flat list of currently-visible shader entries (respecting search filter +
	// folder expansion), mirroring what draw_shaderindex renders.
	vector<std::pair<int, int>> vis;
	bool searchActive = !shaderSearchText.empty();
	string searchLower = ofToLower(shaderSearchText);
	for (size_t f = 0; f < shaderFolders.size(); f++) {
		bool folderMatch = false;
		if (searchActive) {
			folderMatch = ofToLower(shaderFolders[f].name).find(searchLower) != string::npos;
			bool hasMatch = folderMatch;
			if (!hasMatch)
				for (auto &e : shaderFolders[f].shaders)
					if (ofToLower(e.name).find(searchLower) != string::npos) { hasMatch = true; break; }
			if (!hasMatch) continue;
		}
		bool expanded = searchActive ? true : shaderFolders[f].expanded;
		if (!expanded) continue;
		for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
			if (searchActive && !folderMatch &&
				ofToLower(shaderFolders[f].shaders[s].name).find(searchLower) == string::npos) continue;
			vis.push_back({(int)f, (int)s});
		}
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
	bool searchActive = !shaderSearchText.empty();
	string searchLower = ofToLower(shaderSearchText);
	int line = 0, target = -1;
	for (size_t f = 0; f < shaderFolders.size(); f++) {
		bool folderMatch = false;
		if (searchActive) {
			folderMatch = ofToLower(shaderFolders[f].name).find(searchLower) != string::npos;
			bool hasMatch = folderMatch;
			if (!hasMatch)
				for (auto &e : shaderFolders[f].shaders)
					if (ofToLower(e.name).find(searchLower) != string::npos) { hasMatch = true; break; }
			if (!hasMatch) continue;
		}
		line++; // folder header line
		bool expanded = searchActive ? true : shaderFolders[f].expanded;
		if (!expanded) continue;
		for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
			if (searchActive && !folderMatch &&
				ofToLower(shaderFolders[f].shaders[s].name).find(searchLower) == string::npos) continue;
			line++;
			if ((int)f == selectedShaderFolder && (int)s == selectedShaderIndex) target = line;
		}
	}
	if (target < 0) return;
	float panelY = 44;
	float panelH = ofGetHeight() - panelY - 30;
	float fbY = panelY + panelH - 40;
	float drawTop = panelY + 106;
	float drawBottom = fbY - 188; // above the 170px preview pane
	int visibleCount = std::max(1, (int)((drawBottom - drawTop) / 16.0f));
	if (target <= shaderScroll) shaderScroll = target - 1;
	else if (target > shaderScroll + visibleCount) shaderScroll = target - visibleCount;
	if (shaderScroll < 0) shaderScroll = 0;
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
	// boxes.draw() (called just before this on the Import tab) leaves the rect
	// mode as CENTER; force CORNER so panel/search/button rects size correctly.
	ofSetRectMode(OF_RECTMODE_CORNER);

	// Navigator occupies the LEFT half; the right half is left clear so the live
	// node canvas (drawn behind) shows through and a LOADed box appears there.
	float panelX = 30, panelY = 44; // below the top screen-tab bar
	float panelW = ofGetWidth() * 0.5f - panelX - 4;
	float panelH = ofGetHeight() - panelY - 30;

	// Opaque panel background (left half only). Must be opaque so the live node
	// canvas drawn behind does not bleed through the navigator's empty space.
	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofFill();
	ofSetLineWidth(1.0f);

	// Title
	float y = panelY + 26;
	ofSetColor(COL_ACCENT_CYAN);
	int totalShaders = 0;
	for (auto &f : shaderFolders) if (!f.isFavorites) totalShaders += (int)f.shaders.size();
	string title = language == 0 ?
		"IMPORT  |  " + ofToString(totalShaders) + " shaders" :
		"IMPORTAR  |  " + ofToString(totalShaders) + " shaders";
	font_p.drawString(title, panelX + 16, y);

	// Separator
	ofSetColor(ofColor(COL_ACCENT_CYAN, 60));
	ofDrawLine(panelX + 16, y + 6, panelX + panelW - 16, y + 6);

	// Hint (ASCII only; the TTF has no unicode arrows/star glyphs)
	ofSetColor(COL_TEXT_MUTED);
	string hint = language == 0 ?
		"Up/Down navigate  |  star = favorite  |  LOAD adds to canvas >" :
		"Arriba/Abajo navegar  |  estrella = favorito  |  LOAD anade >";
	font_p.drawString(hint, panelX + 16, y + 20);

	y += 30;

	float listX = panelX + 15;
	float listW = panelW - 30;

	// Footer button bar + preview pane both reserve space at the bottom of the
	// panel. Layout (top->bottom): search, list, preview pane, footer buttons.
	float fbH = 30;
	float fbY = panelY + panelH - fbH - 10;
	float pvAreaH = 170;
	float pvAreaY = fbY - pvAreaH - 10;

	// ---- SEARCH BAR ----
	float searchH = 22;
	float searchY = y + 8;
	float searchW = listW;

	ofSetColor(COL_BG_BUTTON);
	ofDrawRectRounded(listX, searchY, searchW, searchH, 4);
	ofNoFill();
	ofSetColor(ofColor(COL_ACCENT_CYAN, shaderSearchFocused ? 120 : 50));
	ofSetLineWidth(1.0f);
	ofDrawRectRounded(listX, searchY, searchW, searchH, 4);
	ofFill();
	ofSetLineWidth(1.0f);

	ofSetColor(COL_TEXT_MUTED);
	float searchLabelX = listX + 5;
	string searchLabel = language == 0 ? "Search:" : "Buscar:";
	font_p.drawString(searchLabel, searchLabelX, searchY + searchH - 5);

	float textX = searchLabelX + font_p.stringWidth(searchLabel) + 3;
	float textMaxW = searchW - (textX - listX) - 5;
	string displayText = shaderSearchText;
	// Truncate text if it exceeds available width
	if (font_p.stringWidth(displayText) > textMaxW) {
		while (!displayText.empty() && font_p.stringWidth(displayText) > textMaxW) {
			displayText = displayText.substr(1);
		}
		displayText = ".." + displayText;
	}

	if (shaderSearchText.empty()) {
		ofSetColor(COL_TEXT_MUTED);
		string ph = language == 0 ? "type shader name..." : "escribe nombre...";
		font_p.drawString(ph, textX, searchY + searchH - 5);
	} else {
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(displayText, textX, searchY + searchH - 5);
		// Insertion caret: follows the cursor when the text fits; falls back to
		// the end when the field is truncated with a ".." prefix.
		if (shaderSearchFocused) {
			if (displayText == shaderSearchText) {
				jp_textfield::drawCaret(font_p, shaderSearchText, shaderSearchCursor,
										textX, searchY + searchH / 2, searchH - 6);
			} else if ((int)(ofGetElapsedTimef() * 2) % 2 == 0) {
				float cursorX = textX + font_p.stringWidth(displayText);
				ofSetColor(COL_ACCENT_CYAN);
				ofDrawLine(cursorX, searchY + 3, cursorX, searchY + searchH - 3);
			}
		}
	}

	// ---- FOLDER LIST ----
	float drawTop = searchY + searchH + 20;
	float drawBottom = pvAreaY - 8; // stop above the preview pane
	float folderEntryH = 16;
	float shaderEntryH = 14;
	float indentStep = 12;

	int currentLine = 0;
	float drawY = drawTop;

	// Calculate total visible lines for scroll
	int totalLines = 0;
	for (size_t f = 0; f < shaderFolders.size(); f++) {
		// When searching, only count folders/shaders that match
		bool searchActive = !shaderSearchText.empty();
		if (searchActive) {
			string searchLower = ofToLower(shaderSearchText);
			string folderNameLower = ofToLower(shaderFolders[f].name);
			bool folderMatch = folderNameLower.find(searchLower) != string::npos;
			bool hasMatch = folderMatch;
			if (!hasMatch) {
				for (size_t ss = 0; ss < shaderFolders[f].shaders.size(); ss++) {
					if (ofToLower(shaderFolders[f].shaders[ss].name).find(searchLower) != string::npos) {
						hasMatch = true;
						break;
					}
				}
			}
			if (!hasMatch) continue;
			totalLines++; // folder header (always shown when match)
			// Count only matching shaders
			for (size_t ss = 0; ss < shaderFolders[f].shaders.size(); ss++) {
				string sn = ofToLower(shaderFolders[f].shaders[ss].name);
				if (folderMatch || sn.find(searchLower) != string::npos) {
					totalLines++;
				}
			}
		} else {
			totalLines++; // folder header
			if (shaderFolders[f].expanded) {
				totalLines += (int)shaderFolders[f].shaders.size();
			}
		}
	}
	int visibleCount = (int)((drawBottom - drawTop) / folderEntryH);
	int maxScroll = std::max(0, totalLines - visibleCount);
	if (shaderScroll > maxScroll) shaderScroll = maxScroll;
	if (shaderScroll < 0) shaderScroll = 0;

	// Scroll indicators
	if (shaderScroll > 0) {
		ofSetColor(ofColor(COL_ACCENT_CYAN, 120));
		font_p.drawString("^", listX + listW - 10, drawTop + 4);
	}
	if (shaderScroll < maxScroll) {
		ofSetColor(ofColor(COL_ACCENT_CYAN, 120));
		font_p.drawString("v", listX + listW - 10, drawBottom - 4);
	}

	for (size_t f = 0; f < shaderFolders.size(); f++) {
		if (drawY > drawBottom) break;

		// Search filtering: skip folders with no matching content
		bool searchActive = !shaderSearchText.empty();
		bool folderNameMatch = false;
		bool hasMatchingShader = false;
		string searchLower = ofToLower(shaderSearchText);
		if (searchActive) {
			string folderNameLower = ofToLower(shaderFolders[f].name);
			folderNameMatch = folderNameLower.find(searchLower) != string::npos;
			if (!folderNameMatch) {
				for (size_t ss = 0; ss < shaderFolders[f].shaders.size(); ss++) {
					string shaderNameLower = ofToLower(shaderFolders[f].shaders[ss].name);
					if (shaderNameLower.find(searchLower) != string::npos) {
						hasMatchingShader = true;
						break;
					}
				}
			}
			if (!folderNameMatch && !hasMatchingShader) {
				continue;
			}
			// Force-expand when searching
			shaderFolders[f].expanded = true;
		}

		// Folder header
		currentLine++;
		if (currentLine > shaderScroll) {
			if (drawY + folderEntryH <= drawBottom) {
				bool isSelected = ((int)f == selectedShaderFolder && selectedShaderIndex == -1);
				bool isHovered = ((int)f == hoveredShaderFolder && hoveredShaderIndex == -1);

				// Hover highlight background
				if (isHovered && !isSelected) {
					ofSetColor(ofColor(COL_ACCENT_CYAN, 30));
					ofDrawRectangle(listX, drawY - folderEntryH, listW, folderEntryH);
				}

				// Hit box debug visualization
				if (showShaderHitBoxes) {
					ofNoFill();
					ofSetColor(isHovered ? 255 : 80, isHovered ? 80 : 230, isHovered ? 80 : 0, isHovered ? 200 : 100);
					ofSetLineWidth(1.0f);
					ofDrawRectangle(listX, drawY - folderEntryH, listW, folderEntryH);
					ofFill();
					ofSetLineWidth(1.0f);
				}

				bool arrowHovered = isHovered;
				// Draw triangle arrow (right=empty, down=expanded)
				float arrowSize = 5.0f;
				float arrowX = listX + arrowSize;
				float arrowYcenter = drawY - folderEntryH / 2.0f;
				if (arrowHovered) {
					ofSetColor(0, 255, 255);
				} else if (isSelected) {
					ofSetColor(COL_ACCENT_CYAN);
				} else {
					ofSetColor(120, 200, 255);
				}
				ofFill();
				if (shaderFolders[f].expanded) {
					// Downward triangle (expanded)
					ofDrawTriangle(arrowX - arrowSize, arrowYcenter - arrowSize * 0.6f,
							arrowX + arrowSize, arrowYcenter - arrowSize * 0.6f,
							arrowX, arrowYcenter + arrowSize * 0.6f);
				} else {
					// Rightward triangle (collapsed)
					ofDrawTriangle(arrowX - arrowSize * 0.6f, arrowYcenter - arrowSize,
							arrowX - arrowSize * 0.6f, arrowYcenter + arrowSize,
							arrowX + arrowSize * 0.6f, arrowYcenter);
				}
				// Folder name after arrow. The synthetic favorites folder gets a
				// gold star prefix + gold text to stand out on top of the list.
				float fnX = listX + arrowSize * 2 + 6;
				bool isFavFolder = shaderFolders[f].isFavorites;
				if (isFavFolder) {
					ofSetColor(COL_ACCENT_GOLD);
					drawStarGlyph(fnX + 4, drawY - folderEntryH / 2.0f, 5.0f, true);
					fnX += 14;
				}
				if (arrowHovered) {
					ofSetColor(0, 255, 255);
				} else if (isFavFolder) {
					ofSetColor(COL_ACCENT_GOLD);
				} else if (isSelected) {
					ofSetColor(COL_ACCENT_CYAN);
				} else {
					ofSetColor(120, 200, 255);
				}
				font_p.drawString(shaderFolders[f].name, fnX, drawY);

				// Draw shader count just after the folder name
				ofSetColor(COL_TEXT_MUTED);
				string countStr = "(" + ofToString((int)shaderFolders[f].shaders.size()) + ")";
				font_p.drawString(countStr, fnX + font_p.stringWidth(shaderFolders[f].name) + 8, drawY);

				drawY += folderEntryH;
			}
		}

		// Shader entries (if expanded)
		if (shaderFolders[f].expanded) {
			for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
				// Skip non-matching shaders when searching
				if (searchActive) {
					string shaderNameLower = ofToLower(shaderFolders[f].shaders[s].name);
					if (!folderNameMatch && shaderNameLower.find(searchLower) == string::npos) {
						continue;
					}
				}
				currentLine++;
				if (currentLine > shaderScroll) {
					if (drawY + shaderEntryH > drawBottom) break;

					bool isSelected = ((int)f == selectedShaderFolder && (int)s == selectedShaderIndex);
					bool isHovered = ((int)f == hoveredShaderFolder && (int)s == hoveredShaderIndex);

					// Hover highlight background
					if (isHovered && !isSelected) {
						ofSetColor(ofColor(COL_ACCENT_CYAN, 30));
						ofDrawRectangle(listX + indentStep * 2, drawY - shaderEntryH, listW - indentStep * 2, shaderEntryH);
					}

					// Hit box debug visualization
					if (showShaderHitBoxes) {
						ofNoFill();
						ofSetColor(isHovered ? 255 : 80, isHovered ? 80 : 230, isHovered ? 80 : 0, isHovered ? 200 : 100);
						ofSetLineWidth(1.0f);
						ofDrawRectangle(listX + indentStep * 2, drawY - shaderEntryH, listW - indentStep * 2, shaderEntryH);
						ofFill();
						ofSetLineWidth(1.0f);
					}

					if (isSelected) {
						ofSetColor(COL_TEXT_PRIMARY);
					} else if (isHovered) {
						ofSetColor(COL_TEXT_PRIMARY);
					} else {
						ofSetColor(COL_TEXT_DIM);
					}
					float nameX = listX + indentStep * 2;
					font_p.drawString(shaderFolders[f].shaders[s].name, nameX, drawY);

					// Favorite star just after the name so they read as one row
					// (filled gold = favorite, dim outline = not). Click toggles it.
					float starCx = nameX + font_p.stringWidth(shaderFolders[f].shaders[s].name) + 12;
					float starMax = listX + listW - 10;
					if (starCx > starMax) starCx = starMax;
					float starCy = drawY - shaderEntryH / 2.0f;
					bool isFav = shaderFolders[f].shaders[s].favorite;
					if (isFav) {
						ofSetColor(COL_ACCENT_GOLD);
						drawStarGlyph(starCx, starCy, 5.0f, true);
					} else {
						ofSetColor(isHovered ? ofColor(COL_TEXT_MUTED) : ofColor(COL_TEXT_MUTED, 90));
						drawStarGlyph(starCx, starCy, 5.0f, false);
					}
					drawY += shaderEntryH;
				}
			}
		}
	}

	// ---- FOOTER: selected shader label + LOAD / BIND / EDIT buttons ----
	bool hasSel = (selectedShaderFolder >= 0 && selectedShaderIndex >= 0 &&
				   selectedShaderFolder < (int)shaderFolders.size() &&
				   selectedShaderIndex < (int)shaderFolders[selectedShaderFolder].shaders.size());
	// (The selected shader name is shown in the preview-pane title below the
	// list, so no separate label is drawn above the buttons here.)

	float fbGap = 8;
	float fbW = (listW - fbGap * 2) / 3.0f;
	float loadX = listX;
	float bindX = listX + fbW + fbGap;
	float editX = listX + (fbW + fbGap) * 2;

	// Helper to draw a labelled button
	auto drawFooterBtn = [&](float bx, ofColor fill, ofColor border, const string &label, bool enabled) {
		ofSetColor(enabled ? fill : ofColor(fill, 60));
		ofDrawRectRounded(bx, fbY, fbW, fbH, 4.0f);
		ofNoFill();
		ofSetColor(enabled ? border : ofColor(border, 80));
		ofSetLineWidth(1.5f);
		ofDrawRectRounded(bx, fbY, fbW, fbH, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		ofSetColor(enabled ? ofColor(COL_TEXT_PRIMARY) : ofColor(COL_TEXT_MUTED));
		float tw = font_p.stringWidth(label);
		font_p.drawString(label, bx + fbW / 2 - tw / 2, fbY + fbH / 2 + 4);
	};

	drawFooterBtn(loadX, COL_ACCENT_CYAN_DIM, COL_ACCENT_CYAN,
				  language == 0 ? "LOAD" : "CARGAR", hasSel);

	// BIND button — inline MIDI-learn for the add-shader command. Shows a
	// waiting state while armed for the next MIDI message.
	if (importBindWaiting) {
		drawFooterBtn(bindX, COL_ACCENT_GOLD_DIM, COL_ACCENT_GOLD,
					  language == 0 ? "MOVE MIDI" : "MUEVE MIDI", true);
	} else {
		drawFooterBtn(bindX, COL_BG_BUTTON, COL_ACCENT_GOLD_DIM, "BIND", hasSel);
	}

	drawFooterBtn(editX, COL_BG_BUTTON, COL_ACCENT_GOLD_DIM, "EDIT", hasSel);

	// ---- PREVIEW PANE (inside the panel, between list and footer) ----
	// Live render of the currently-selected shader so you can preview before
	// loading, framed inside the navigator's background.
	{
		float titleH = 18.0f;
		float pad = 6.0f;
		// Window frame spanning the panel width in the reserved preview area.
		ofSetColor(COL_BG_INPUT);
		ofDrawRectRounded(listX, pvAreaY, listW, pvAreaH, 6);
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 90));
		ofSetLineWidth(1.0f);
		ofDrawRectRounded(listX, pvAreaY, listW, pvAreaH, 6);
		ofFill();

		if (hasSel && previewShaderLoaded && previewFbo.isAllocated()) {
			// Title
			ofSetColor(COL_ACCENT_CYAN);
			string pvTitle = (language == 0 ? "PREVIEW: " : "PREVISTA: ") +
							 shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].name;
			while (font_p.stringWidth(pvTitle) > listW - 12 && pvTitle.size() > 3)
				pvTitle = pvTitle.substr(0, pvTitle.size() - 1);
			font_p.drawString(pvTitle, listX + pad, pvAreaY + titleH - 3);

			// Aspect-fit the render into the remaining area.
			float availW = listW - pad * 2;
			float availH = pvAreaH - titleH - pad * 2;
			float aspect = previewFbo.getHeight() / (float)previewFbo.getWidth();
			float drawW = availW;
			float drawH = drawW * aspect;
			if (drawH > availH) { drawH = availH; drawW = drawH / aspect; }
			float drawXp = listX + (listW - drawW) / 2.0f;
			float drawYp = pvAreaY + titleH + pad + (availH - drawH) / 2.0f;
			ofSetColor(COL_TEXT_PRIMARY);
			previewFbo.draw(drawXp, drawYp, drawW, drawH);
		} else {
			ofSetColor(COL_TEXT_MUTED);
			string none = language == 0 ? "select a shader to preview" : "selecciona un shader";
			font_p.drawString(none, listX + 8, pvAreaY + pvAreaH / 2);
		}
	}
}
// Esta es la que se dibuja en la otra ventana
void ofApp::drawRender() {
	boxes.draw_activerender();
}
void ofApp::keyPressed(int key) {

	if (midiKeymap.keyPressed(key)) {
		return;
	}

	// Shader Editor key capture (when visible, consumes all keys)
	if (shaderEditor.wantsKeyCapture()) {
		shaderEditor.keyPressed(key);
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

	// Shader index: Up/Down navigate the shader list (works whether or not the
	// search field is focused; Left/Right stay with the text cursor).
	if (pantallaActiva == SHADER_INDEX && (key == OF_KEY_UP || key == OF_KEY_DOWN)) {
		moveShaderSelection(key == OF_KEY_DOWN ? 1 : -1);
		return;
	}

	// Shader index: Enter loads the selected shader onto the canvas (works
	// whether or not the search field is focused).
	if (pantallaActiva == SHADER_INDEX && (key == OF_KEY_RETURN || key == '\r')) {
		loadSelectedShaderBox();
		return;
	}

	// Shader index search input
	if (pantallaActiva == SHADER_INDEX && shaderSearchFocused) {
		if (key == OF_KEY_ESC) {
			shaderSearchFocused = false;
			return;
		}
		if (key == OF_KEY_RETURN || key == '\r') {
			return;
		}
		if (jp_textfield::handleKey(shaderSearchText, shaderSearchCursor, key)) {
			shaderScroll = 0;
		}
		return;
	}

	if (key == '1') {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
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
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '4') {
		pantallaActiva = SHADER_INDEX;
		focusedOptionsField = -1;
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
		if (shaderFolders.empty()) {
			scanShaders();
		}
	}

	if (key == '5') {
		pantallaActiva = EDITOR;
		focusedOptionsField = -1;
		shaderEditor.setVisible(true);
	}

	// ESC from shader index goes back to NODOS
	if (key == OF_KEY_ESC && pantallaActiva == SHADER_INDEX) {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
		return;
	}

	// ESC from editor goes back to NODOS
	if (key == OF_KEY_ESC && pantallaActiva == EDITOR) {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
		shaderEditor.setVisible(false);
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
			cout << "Z PRESSED: pantallaActiva=" << pantallaActiva << " saveModalActive=" << saveModalActive << " focusedOptionsField=" << focusedOptionsField;
			if (boxes.isGroupViewActive()) {
				cout << " groupView=TRUE groupInspectorIndex=" << boxes.groupInspectorIndex << endl;
				boxes.toggleCueBoxByIndex(boxes.groupInspectorIndex);
			} else {
				int cueIndex = boxes.openguinumber;
				if (cueIndex < 0 || cueIndex >= boxes.getBoxesSize()) {
					cueIndex = boxes.getActiverenderNum();
					cout << "  -> fallback to activerender=" << cueIndex << endl;
				}
				cout << " groupView=FALSE cueIndex=" << cueIndex << " boxes.size=" << boxes.getBoxesSize() << endl;
				boxes.toggleCueBoxByIndex(cueIndex);
			}
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

	// Ctrl+C (key=3) -> copy selected boxes
	if (e.key == 3 && pantallaActiva == NODOS) {
		boxes.copySelectedBoxes();
		return;
	}

	// Ctrl+V (key=22) -> paste boxes
	if (e.key == 22 && pantallaActiva == NODOS) {
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

				// Hide editor when leaving EDITOR screen
				if (pantallaActiva != EDITOR) shaderEditor.setVisible(false);

				if (tabScreen == OPCIONES) {
					initOptionsFields();
					optionsFieldsInitialized = true;
				} else if (tabScreen == SHADER_INDEX) {
					if (shaderFolders.empty()) {
						scanShaders();
					}
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
		return;
	}

	if (pantallaActiva == NODOS) {
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
		// Layout constants matching draw_opciones()
		float panelW = 500;
		float panelX = 30;
		float panelY = 44;
		float fieldX = panelX + 145;
		float fieldW = 200;
		float rowH = 28;
		float sepy = 40;
		float toggleBtnW = 70;

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
				float tapW = 100;
				if (x >= tapX && x <= tapX + tapW &&
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
			if (x >= fieldX && x <= fieldX + toggleBtnW &&
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
			if (x >= fieldX && x <= fieldX + toggleBtnW &&
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
			float browseW = 70;
			if (x >= browseX && x <= browseX + browseW &&
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
			float saveW = 160;
			float saveX = panelX + panelW / 2 - saveW / 2;
			if (x >= saveX && x <= saveX + saveW &&
				y >= rowY && y <= rowY + rowH + 4) {
				saveSettings();
				saveFeedbackText = "Saved!";
				saveFeedbackTime = ofGetElapsedTimef();
				return;
			}
		}
	}
	if (pantallaActiva == SHADER_INDEX && button == 0) {
		float panelX = 30, panelY = 44;
		float panelW = ofGetWidth() * 0.5f - panelX - 4;
		float panelH = ofGetHeight() - panelY - 30;
		float listX = panelX + 15;
		float listW = panelW - 30;
		float fbH = 30;
		float fbY = panelY + panelH - fbH - 10;

		// Only the left navigator panel is interactive; clicks in the right
		// (uncovered) half just unfocus the search field and fall through.
		if (x < panelX || x > panelX + panelW || y < panelY || y > panelY + panelH) {
			shaderSearchFocused = false;
			return;
		}

		// ---- SEARCH BAR CLICK CHECK ----
		float searchH = 22;
		float searchY = panelY + 64;
		if (x >= listX && x <= listX + listW && y >= searchY && y <= searchY + searchH) {
			shaderSearchFocused = true;
			shaderSearchCursor = shaderSearchText.size();
			return;
		}
		shaderSearchFocused = false;

		// ---- FOOTER BUTTONS: LOAD / BIND / EDIT ----
		if (selectedShaderFolder >= 0 && selectedShaderIndex >= 0 &&
			selectedShaderFolder < (int)shaderFolders.size() &&
			selectedShaderIndex < (int)shaderFolders[selectedShaderFolder].shaders.size()) {
			float fbGap = 8;
			float fbW = (listW - fbGap * 2) / 3.0f;
			float loadX = listX;
			float bindX = listX + fbW + fbGap;
			float editX = listX + (fbW + fbGap) * 2;
			string selPath = shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].path;
			string selName = shaderFolders[selectedShaderFolder].shaders[selectedShaderIndex].name;

			if (y >= fbY && y <= fbY + fbH) {
				// LOAD - add the box near the centre of the right (visible) half so
				// it appears in the uncovered area for immediate visual feedback.
				if (x >= loadX && x <= loadX + fbW) {
					loadSelectedShaderBox();
					return;
				}
				// BIND - inline MIDI-learn for the add-shader command (toggle).
				if (x >= bindX && x <= bindX + fbW) {
					if (importBindWaiting || midiKeymap.isLearning()) {
						midiKeymap.cancelInlineLearn();
						importBindWaiting = false;
					} else {
						midiKeymap.beginAddShaderLearn(selName);
						importBindWaiting = true;
					}
					return;
				}
				// EDIT - open the shader in the code editor.
				if (x >= editX && x <= editX + fbW) {
					shaderEditor.openShader(selPath, selName);
					return;
				}
			}
		}

		// ---- FOLDER/SHADER LIST (left side) ----
		// drawTop = searchY(64) + searchH(22) + 20 = panelY + 106; list stops
		// above the preview pane (fbY - 188).
		double clickStartY = panelY + 106;
		if (x >= listX && x <= listX + listW && y >= clickStartY && y <= fbY - 188) {
			float drawTop = panelY + 106;
			float folderEntryH = 16;
			float shaderEntryH = 14;
			float indentStep = 12;

			// Iterate the actual tree to compute exact Y ranges (matching draw_shaderindex)
			int currentLine = 0;
			float drawY = drawTop;

			for (size_t f = 0; f < shaderFolders.size(); f++) {
				// Search filtering: skip folders with no matching content
				{
					bool searchActive = !shaderSearchText.empty();
					if (searchActive) {
						string searchLower = ofToLower(shaderSearchText);
						string folderNameLower = ofToLower(shaderFolders[f].name);
						bool folderMatch = folderNameLower.find(searchLower) != string::npos;
						bool hasMatch = folderMatch;
						if (!hasMatch) {
							for (size_t ss = 0; ss < shaderFolders[f].shaders.size(); ss++) {
								if (ofToLower(shaderFolders[f].shaders[ss].name).find(searchLower) != string::npos) {
									hasMatch = true;
									break;
								}
							}
						}
						if (!hasMatch) continue;
					}
				}
				currentLine++;
				if (currentLine > shaderScroll) {
					float folderTop = drawY - folderEntryH;
					if (y >= folderTop && y < drawY) {
						// Clicked on folder header - toggle expand
						shaderFolders[f].expanded = !shaderFolders[f].expanded;
						selectedShaderFolder = (int)f;
						selectedShaderIndex = -1;
						previewShaderLoaded = false;
						return;
					}
					drawY += folderEntryH;
				}

				if (shaderFolders[f].expanded) {
					bool searchActive = !shaderSearchText.empty();
					string searchLower;
					string folderNameLower;
					bool folderMatch = false;
					if (searchActive) {
						searchLower = ofToLower(shaderSearchText);
						folderNameLower = ofToLower(shaderFolders[f].name);
						folderMatch = folderNameLower.find(searchLower) != string::npos;
					}
					for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
						// Skip non-matching shaders when searching
						if (searchActive) {
							string sn = ofToLower(shaderFolders[f].shaders[s].name);
							if (!folderMatch && sn.find(searchLower) == string::npos) {
								continue;
							}
						}
						currentLine++;
						if (currentLine > shaderScroll) {
							float shaderTop = drawY - shaderEntryH;
							if (y >= shaderTop && y < drawY) {
								// Star toggle hit box (just after the name, matching draw)
								float nameX = listX + indentStep * 2;
								float sCx = nameX + font_p.stringWidth(shaderFolders[f].shaders[s].name) + 12;
								float sMax = listX + listW - 10;
								if (sCx > sMax) sCx = sMax;
								if (x >= sCx - 10 && x <= sCx + 10) {
									toggleFavorite(shaderFolders[f].shaders[s].path);
									return;
								}
								// Clicked on shader entry - select; double-click loads.
								float now = ofGetElapsedTimef();
								bool dbl = (lastShaderClickFolder == (int)f &&
											lastShaderClickIndex == (int)s &&
											(now - lastShaderClickTime) < 0.35f);
								selectShaderForPreview((int)f, (int)s);
								if (dbl) {
									loadSelectedShaderBox();
									lastShaderClickTime = -1.0f; // reset so a 3rd click isn't a dbl
								} else {
									lastShaderClickTime = now;
									lastShaderClickFolder = (int)f;
									lastShaderClickIndex = (int)s;
								}
								return;
							}
							drawY += shaderEntryH;
						}
					}
				}
			}
		}
	}
}
void ofApp::windowResized(int w, int h) {

	// El resize lo hace solo para mover la interfaz. Los tamaos de render se mantienen igual
	boxes.update_resized(ofGetWidth(), ofGetHeight());
	// InitGLtexture(sendertexture, ofGetWidth(), ofGetHeight()); //!?!??!!?
	//	boxes.update_resized(jp_constants::renderWidth, jp_constants::renderHeight);
}
void ofApp::keyReleased(int key) { }
void ofApp::mouseMoved(int x, int y) {
	if (pantallaActiva == SHADER_INDEX) {
		float panelX = 30, panelY = 44;
		float panelW = ofGetWidth() * 0.5f - panelX - 4;
		float panelH = ofGetHeight() - panelY - 30;
		float listX = panelX + 15;
		float listW = panelW - 30;
		float fbY = panelY + panelH - 40;

		// Reset hover state
		hoveredShaderFolder = -1;
		hoveredShaderIndex = -1;

		// Search bar offset: drawTop = searchY(64) + searchH(22) + 20 = panelY + 106
		double clickStartY = panelY + 106;
		if (x >= listX && x <= listX + listW && y >= clickStartY && y <= fbY - 188) {
			float folderEntryH = 16;
			float shaderEntryH = 14;
			float drawBottom = fbY - 188;
			float drawY = clickStartY;
			int currentLine = 0; // mirror draw_shaderindex scroll handling

			for (size_t f = 0; f < shaderFolders.size(); f++) {
				if (drawY > drawBottom) break;
				bool searchActive = !shaderSearchText.empty();
				string searchLower = ofToLower(shaderSearchText);
				bool folderNameMatch = false;
				bool hasMatchingShader = false;
				if (searchActive) {
					folderNameMatch = ofToLower(shaderFolders[f].name).find(searchLower) != string::npos;
					if (!folderNameMatch) {
						for (size_t ss = 0; ss < shaderFolders[f].shaders.size(); ss++) {
							if (ofToLower(shaderFolders[f].shaders[ss].name).find(searchLower) != string::npos) { hasMatchingShader = true; break; }
						}
					}
					if (!folderNameMatch && !hasMatchingShader) continue;
				}

				// Folder header — count every line, gate visibility by scroll (matches draw)
				currentLine++;
				if (currentLine > shaderScroll) {
					if (drawY + folderEntryH <= drawBottom) {
						if (y >= drawY - folderEntryH && y < drawY) {
							hoveredShaderFolder = (int)f;
							hoveredShaderIndex = -1;
							break;
						}
						drawY += folderEntryH;
					}
				}

				if (shaderFolders[f].expanded) {
					for (size_t s = 0; s < shaderFolders[f].shaders.size(); s++) {
						if (searchActive && !folderNameMatch &&
							ofToLower(shaderFolders[f].shaders[s].name).find(searchLower) == string::npos) {
							continue;
						}
						currentLine++;
						if (currentLine > shaderScroll) {
							if (drawY + shaderEntryH > drawBottom) break;
							if (y >= drawY - shaderEntryH && y < drawY) {
								hoveredShaderFolder = (int)f;
								hoveredShaderIndex = (int)s;
								break;
							}
							drawY += shaderEntryH;
						}
					}
					if (hoveredShaderFolder >= 0) break;
				}
			}
		}
	}
}
void ofApp::mouseReleased(int x, int y, int button) {
	midiKeymap.mouseReleased(x, y, button);
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
		float panelX = 30, panelY = 44;
		float panelW = ofGetWidth() * 0.5f - panelX - 4;
		float panelH = ofGetHeight() - panelY - 30;
		if (x >= panelX && x <= panelX + panelW && y >= panelY && y <= panelY + panelH) {
			shaderScroll -= (int)scrollY * 3;
			if (shaderScroll < 0) shaderScroll = 0;
		}
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
		string path = dragInfo.files[i];
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
void ofApp::openRenderWindow() {
	if (!isRenderWindowOpen) {
		windows.clear();
		isRenderWindowOpen = true;
		ofGLFWWindowSettings settings;

		// Match the main window's GL context (see main.cpp). Required on
		// Linux/macOS: without an explicit core-profile version the render
		// window's context can mismatch the main window and fail to create.
		settings.setGLVersion(3, 2);
		settings.setSize(jp_constants::window_width, jp_constants::window_height);
		settings.setPosition(ofVec2f(window_initialposx, window_initialposy));
		settings.resizable = true;
		settings.shareContextWith = mainWindow;

		windows.push_back(ofCreateWindow(settings));

		cout << "windows size" << windows.size() << endl;

		// window_width = 600;
		// window_height = 600;
		ofAddListener(windows.back()->events().draw, this, &ofApp::window_drawRender);
		ofAddListener(windows.back()->events().exit, this, &ofApp::exit);
		ofAddListener(windows.back()->events().keyPressed, this, &ofApp::window_keyPressed);
		ofAddListener(windows.back()->events().mouseMoved, this, &ofApp::window_mouseMove);
		ofAddListener(windows.back()->events().windowResized, this, &ofApp::window_resized);
	}
}
void ofApp::loadSettings() {

	const auto settingsPath = ofToDataPath("settings.xml");

	// If file does not exist, do not change anything (use default values)
	if (!ofFile(settingsPath).exists())
		return;

	ofXml xml;
	xml.load(settingsPath);

	auto settings = xml.getChild("settings");
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

	cout << "/****************************************************/" << endl;
	cout << "INITIAL VALUES FROM SETTINGS.XML " << endl;
	cout << "renderwidth " << renderwidthaux.getIntValue() << endl;
	cout << "renderheight " << renderheightaux.getIntValue() << endl;
	cout << "windowx " << windowx.getIntValue() << endl;
	cout << "windowy " << windowy.getIntValue() << endl;
	cout << "windowwidth " << windowwidth.getIntValue() << endl;
	cout << "windowheight " << windowheight.getIntValue() << endl;
	cout << "windowopen " << windowopen.getBoolValue() << endl;
	cout << "windowfullscreen " << window_fullscreenaux.getBoolValue() << endl;
	cout << "oscportin " << oscportin.getIntValue() << endl;
	cout << "oscportout " << oscportout.getIntValue() << endl;
	cout << "oscipout " << oscipout.getValue() << endl;
#ifdef SPOUT
	cout << "spouton " << spouton.getBoolValue() << endl;
#endif
	cout << "oscout1 " << oscout1.getBoolValue() << endl;
	cout << "oscout2 " << oscout2.getBoolValue() << endl;
	cout << "durationgallery " << durationgallery.getFloatValue() << endl;
	cout << "/****************************************************/" << endl;

	jp_constants::init(renderwidthaux.getIntValue(),
		renderheightaux.getIntValue(),
		windowwidth.getIntValue(),
		windowheight.getIntValue());
	boxes.setDurationGalleryMs(durationgallery.getFloatValue());
	if (cuePanelX && cuePanelY && cuePanelW && cuePanelH) {
		boxes.setCuePanelLayout(
			24.0f, // siempre izquierda
			ofGetHeight() - cuePanelH.getFloatValue() - 24.0f, // siempre abajo
			cuePanelW.getFloatValue(),
			cuePanelH.getFloatValue());
	}

	cout << "window_width " << jp_constants::window_width << endl;

	window_initialposx = windowx.getIntValue();
	window_initialposy = windowy.getIntValue();
	window_fullscreen = window_fullscreenaux;
#ifdef SPOUT
	spoutActive = spouton.getBoolValue();
#endif
#ifdef NDI
	{
		auto ndion = settings.getChild("ndion");
		if (ndion) ndiActive = ndion.getBoolValue();
	}
#endif
	oscout_mode1 = oscout1.getBoolValue();
	oscout_mode2 = oscout2.getBoolValue();

	if (defaultCompoChild) {
		defaultCompoPath = defaultCompoChild.getValue();
		cout << "defaultcompo: " << defaultCompoPath << endl;
	}

	receiver.setup(oscportin.getIntValue());
	sender.setup(oscipout.getValue(), oscportout.getIntValue());
	{
		auto bpmaux = settings.getChild("bpm");
		if (bpmaux) jp_constants::bpm = (float)bpmaux.getIntValue();
	}
	if (windowopen.getBoolValue()) {
		openRenderWindow();
	}
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

	auto settings = xml.appendChild("settings");
	settings.appendChild("renderwidth").set(jp_constants::renderWidth);
	settings.appendChild("renderheight").set(jp_constants::renderHeight);
	settings.appendChild("durationgallery").set(boxes.getDurationGalleryMs());
	settings.appendChild("cue_panel_x").set(cuePanelX);
	settings.appendChild("cue_panel_y").set(cuePanelY);
	settings.appendChild("cue_panel_w").set(cuePanelW);
	settings.appendChild("cue_panel_h").set(cuePanelH);
	settings.appendChild("window_x").set(ceil(window_initialposx));
	settings.appendChild("window_y").set(ceil(window_initialposy));
	//settings.appendChild("window_width").set(jp_constants::window_width);
	//settings.appendChild("window_height").set(jp_constants::window_height)s;
	//settings.appendChild("window_fullscreen").set(toXmlString(window_fullscreen));

	settings.appendChild("window_width").set(600);
	settings.appendChild("window_height").set(600);
	settings.appendChild("window_fullscreen").set(false);
	settings.appendChild("window_open").set(toXmlString(isRenderWindowOpen));
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
void ofApp::exit() {
	//cout << "LALA" << endl;
	//windows.back()->close();
	saveSettings();
	midiKeymap.exit();
}

// LISTENERS DE LAS VENTANAS:
void ofApp::window_drawRender(ofEventArgs & args) {
	boxes.draw_activerender(jp_constants::window_width, jp_constants::window_height);
}
void ofApp::exit(ofEventArgs & e) {
	cout << "EXIT WINDOW " << endl;
	window_fullscreen = false;
	isRenderWindowOpen = false;
	// Save after updating render-window state, but do not destroy the window
	// vector from inside the window's own exit callback.
	saveSettings();
}
void ofApp::window_mouseMove(ofMouseEventArgs & e) {

	/*window_mousex = e.x;
	window_mousey = e.y;*/

	jp_constants::setwindow_mousex(e.x);
	jp_constants::setwindow_mousey(e.y);
}
void ofApp::window_resized(ofResizeEventArgs & args) {
	cout << "WINDOWS RESIZED PAPA " << endl;
	cout << "WIDTH " << args.width << endl;
	cout << "HEIGHT " << args.height << endl;

	jp_constants::setwindow_width(args.width);
	jp_constants::setwindow_height(args.height);
}
void ofApp::window_keyPressed(ofKeyEventArgs & e) {
	cout << "KEYCODE ON WINDOWS : " << e.keycode << endl;

	if (e.keycode == 70 || e.keycode == 71) {
		window_fullscreen = !window_fullscreen;
		windows.back()->setFullscreen(window_fullscreen);
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

	// --- UPDATE button (amber / gold) ---
	ofSetColor(220, 190, 50, 230);
	ofDrawRectRounded(updateBtnX, btnY, btnW, btnH, 4);
	ofSetColor(COL_BG_DARK);
	string updateLabel = "UPDATE";
	float updateLabelW = modalFont.stringWidth(updateLabel);
	modalFont.drawString(updateLabel, updateBtnX + btnW / 2 - updateLabelW / 2, btnY + btnH / 2 + 5);

	// --- CANCEL button (gray) ---
	ofSetColor(ofColor(COL_TEXT_MUTED, 230));
	ofDrawRectRounded(cancelBtnX, btnY, btnW, btnH, 4);
	ofSetColor(COL_TEXT_SECONDARY);
	string cancelLabel = "CANCEL";
	float cancelLabelW = modalFont.stringWidth(cancelLabel);
	modalFont.drawString(cancelLabel, cancelBtnX + btnW / 2 - cancelLabelW / 2, btnY + btnH / 2 + 5);
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
	};
	vector<ScreenTab> tabs = {
		{"NODES", NODOS},
		{"SETTINGS", OPCIONES},
		{"HELP", TUTORIAL},
		{"IMPORT", SHADER_INDEX},
		{"EDITOR", EDITOR}
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
