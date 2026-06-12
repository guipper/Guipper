#include "ofApp.h"
#include <iostream>

//--------------------------------------------------------------
void ofApp::setup() {

	font_p.loadFont("font/Montserrat-Regular.ttf", 11); // Inicio fuente.

	// Modal font - loaded at readable size for the save dialog
	modalFont.loadFont("font/Montserrat-Medium.ttf", 14);

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
	ofBackground(10, 100, 140);
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
	loadSession(savedirectory);
	midiKeymap.load(ofToDataPath("midi_keymap.xml"));
}
void ofApp::update() {
	boxes.update();
	midiKeymap.update();

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
}
void ofApp::draw() {
	if (pantallaActiva == NODOS) {
		boxes.drawNodeEditorBackground(ofGetWidth(), ofGetHeight());
		boxes.draw();
		ofSetColor(255, 255, 255, 255);
		// outletimg.draw(ofGetWidth() / 2, ofGetHeight() / 2, 200, 200);
		if (isDebug) {
			draw_debugInfo();
		}
	}
	else {
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

	drawSaveModal();
}
void ofApp::draw_debugInfo() {

	float sepy = 20;
	float posy = ofGetHeight();
	font_p.drawString("Active Render :" + ofToString(activerender), 30, posy -= sepy);
	font_p.drawString("FPS :" + ofToString(ofGetFrameRate()), 30, posy -= sepy);
	font_p.drawString("Boxes size : " + ofToString(boxes.getBoxesSize()), 30, posy -= sepy);
	font_p.drawString("Active Sequence : " + ofToString(boxes.activeSequence), 30, posy -= sepy);
	//font_p.drawString("DIALOG BOX : " + ofToString(jp_constants::systemDialog_open), 30, posy -= sepy);
}
void ofApp::draw_instrucciones() {
	float x = 30;
	float y = 30;
	float sepy = 30;

	float x2 = 30;
	float y2 = 30;

	ofSetColor(0, 100);
	ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
	ofSetColor(255);

	//AGREGAR BOTON EN ALGUN MOMENTO
	if (language == 0) {
		jp_constants::p_font.drawString("INSTRUCCIONES : ", x, y += sepy);
		jp_constants::p_font.drawString("Para cargar archivo (todos) arrastrarlo hasta la ventana", x, y += sepy);
		jp_constants::p_font.drawString("TECLAS : ", x, y += sepy);
		jp_constants::p_font.drawString("t : Cambia ente cargar un xml como preset o como compo", x, y += sepy);
		jp_constants::p_font.drawString("w : Abre ventana aparte", x, y += sepy);
		jp_constants::p_font.drawString("f : Sobre la ventana aparte abierta : FullScreen", x, y += sepy);
		jp_constants::p_font.drawString("s : Guardar sobre el xml actual : " + savedirectory, x, y += sepy);
		jp_constants::p_font.drawString("ctrl+s : Genera un nuevo archivo XML(guardarlo con la extension correcta)", x, y += sepy);
		jp_constants::p_font.drawString("d : Muestra los datos de debug", x, y += sepy);
		jp_constants::p_font.drawString("h : Agrega caja SPOUT INPUT", x, y += sepy);
		jp_constants::p_font.drawString("c : Agrega caja camara", x, y += sepy);
		jp_constants::p_font.drawString("k : Abre/cierra el panel MIDI Keymap", x, y += sepy);
		jp_constants::p_font.drawString("m : Exportar imagen", x, y += sepy);
		jp_constants::p_font.drawString("e : Activar modo secuencia", x, y += sepy);
		jp_constants::p_font.drawString("COMANDOS OSC ", x, y += sepy);
		jp_constants::p_font.drawString("/load/(dir) : Cargar archivo especifico", x, y += sepy);
		jp_constants::p_font.drawString("/setactiverender/(num) : set active render", x, y += sepy);
		jp_constants::p_font.drawString("/openguinumber/(value) : control de active", x, y += sepy);
		jp_constants::p_font.drawString("/(name of shader)/(name of param) : to control using by name", x, y += sepy);
	}
	if (language == 1) {
		jp_constants::p_font.drawString("INSTRUCTIONS : ", x, y += sepy);
		jp_constants::p_font.drawString("To load any file drag it to this window(any kind)", x, y += sepy);
		jp_constants::p_font.drawString("KEYS : ", x, y += sepy);
		jp_constants::p_font.drawString("t : Change between loading a box as a preset or as a full compo", x, y += sepy);
		jp_constants::p_font.drawString("w : Opens another window", x, y += sepy);
		jp_constants::p_font.drawString("f : Click over the another window to make it fullscreen", x, y += sepy);
		jp_constants::p_font.drawString("s : Save on the actual XML : " + savedirectory, x, y += sepy);
		jp_constants::p_font.drawString("ctrl+s : Generates a new XML (save it with the correct extension please)", x, y += sepy);
		jp_constants::p_font.drawString("d : Show debug data", x, y += sepy);
		jp_constants::p_font.drawString("h : Add spout input box", x, y += sepy);
		jp_constants::p_font.drawString("c : Add Camera box", x, y += sepy);
		jp_constants::p_font.drawString("k : Open/close the MIDI Keymap panel", x, y += sepy);
		jp_constants::p_font.drawString("m : Export image to exportimgs folder", x, y += sepy);
		jp_constants::p_font.drawString("e : Activate sequence mode", x, y += sepy);
		jp_constants::p_font.drawString("OSC COMMANDS ", x, y += sepy);
		jp_constants::p_font.drawString("/load/(dir) : load directory of a savefile", x, y += sepy);
		jp_constants::p_font.drawString("/setactiverender/(num) : set active render", x, y += sepy);
		jp_constants::p_font.drawString("/openguinumber/(value) : control de active", x, y += sepy);
		jp_constants::p_font.drawString("/(name of shader)/(name of param) : to control using by name", x, y += sepy);
	}
}
void ofApp::draw_opciones() {
	float panelX = 30, panelY = 30;
	float panelW = 500;
	float fieldX = 175;
	float fieldW = 200;
	float rowH = 28;
	float sepy = 40;
	int totalRows = OPTIONS_FIELD_COUNT + 4; // fields + 2 toggles + save + osc ip
	float panelH = 55 + totalRows * sepy + 25;
	float toggleBtnW = 70;

	// Glassmorphism panel background
	ofSetColor(12, 16, 20, 235);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofNoFill();
	ofSetColor(0, 230, 230, 80);
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(panelX, panelY, panelW, panelH, 12);
	ofFill();
	ofSetLineWidth(1.0f);

	// Title
	ofSetColor(0, 230, 230);
	font_p.drawString("SETTINGS.XML Configuration", panelX + 15, panelY + 30);

	// Field labels & inputs
	string labels[OPTIONS_FIELD_COUNT] = {
		"OSC Port In:",
		"OSC Port Out:",
		"Render Width:",
		"Render Height:",
		"BPM:"
	};

	for (int i = 0; i < OPTIONS_FIELD_COUNT; i++) {
		float rowY = panelY + 55 + i * sepy;

		// Label
		ofSetColor(220);
		font_p.drawString(labels[i], panelX + 15, rowY + rowH - 7);

		// Field background
		ofSetColor(focusedOptionsField == i ? ofColor(30, 40, 50) : ofColor(20, 25, 30));
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);

		// Field border
		ofNoFill();
		if (focusedOptionsField == i) {
			ofSetColor(0, 230, 230);
			ofSetLineWidth(2.0f);
		} else {
			ofSetColor(60, 70, 80);
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(fieldX, rowY, fieldW, rowH, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);

		// Field text with cursor if focused
		ofSetColor(255);
		string displayText = optionsFieldText[i];
		if (focusedOptionsField == i) {
			displayText += "|";
		}
		font_p.drawString(displayText, fieldX + 6, rowY + rowH - 7);
	}

	// --- Toggle: Spout ---
	int toggleRow = OPTIONS_FIELD_COUNT;
#ifdef SPOUT
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(220);
		font_p.drawString("Spout Output", panelX + 15, rowY + rowH - 7);

		float btnX = fieldX;
		bool isOn = spoutActive;
		ofSetColor(isOn ? ofColor(0, 180, 80) : ofColor(80, 30, 30));
		ofDrawRectRounded(btnX, rowY, toggleBtnW, rowH, 4.0f);
		ofSetColor(255);
		font_p.drawString(isOn ? "ON" : "OFF", btnX + toggleBtnW / 2 - 12, rowY + rowH - 7);
	}
	toggleRow++;
#endif

	// --- Toggle: NDI ---
#ifdef NDI
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(220);
		font_p.drawString("NDI Output", panelX + 15, rowY + rowH - 7);

		float btnX = fieldX;
		bool isOn = ndiActive;
		ofSetColor(isOn ? ofColor(0, 180, 80) : ofColor(80, 30, 30));
		ofDrawRectRounded(btnX, rowY, toggleBtnW, rowH, 4.0f);
		ofSetColor(255);
		font_p.drawString(isOn ? "ON" : "OFF", btnX + toggleBtnW / 2 - 12, rowY + rowH - 7);
	}
	toggleRow++;
#endif

	// --- OSC IP Out info ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		ofSetColor(160);
		font_p.drawString("OSC IP Out: " + sender.getHost(), panelX + 15, rowY + rowH - 7);
	}
	toggleRow++;

	// --- Save button ---
	{
		float rowY = panelY + 55 + toggleRow * sepy;
		float saveW = 160;
		float saveX = panelX + panelW / 2 - saveW / 2;
		ofSetColor(0, 160, 160);
		ofDrawRectRounded(saveX, rowY, saveW, rowH + 4, 6.0f);
		ofSetColor(255);
		font_p.drawString("SAVE SETTINGS", saveX + saveW / 2 - 48, rowY + rowH + 4 - 6);
	}

	// Hint text when focused
	if (focusedOptionsField >= 0) {
		ofSetColor(100, 120, 130);
		font_p.drawString("Enter to apply | Click outside to cancel", panelX + 15, panelY + panelH - 10);
	}
}

void ofApp::initOptionsFields() {
	optionsFieldText[FIELD_OSC_PORT_IN] = ofToString(receiver.getPort());
	optionsFieldText[FIELD_OSC_PORT_OUT] = ofToString(sender.getPort());
	optionsFieldText[FIELD_RENDER_WIDTH] = ofToString(jp_constants::renderWidth);
	optionsFieldText[FIELD_RENDER_HEIGHT] = ofToString(jp_constants::renderHeight);
	optionsFieldText[FIELD_BPM] = ofToString((int)jp_constants::bpm);
	focusedOptionsField = -1;
}

void ofApp::applyOptionsField() {
	for (int i = 0; i < OPTIONS_FIELD_COUNT; i++) {
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
	saveSettings();
	focusedOptionsField = -1;
}
// Esta es la que se dibuja en la otra ventana
void ofApp::drawRender() {
	boxes.draw_activerender();
}
void ofApp::keyPressed(int key) {

	if (midiKeymap.keyPressed(key)) {
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
		if (key == OF_KEY_BACKSPACE) {
			if (!saveModalName.empty()) {
				saveModalName.erase(saveModalName.size() - 1);
			}
			return;
		}
		// Allow alphanumeric, dash, underscore, dot, space
		if ((key >= 'a' && key <= 'z') ||
			(key >= 'A' && key <= 'Z') ||
			(key >= '0' && key <= '9') ||
			key == '-' || key == '_' || key == '.' || key == ' ') {
			saveModalName += (char)key;
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
		if (key == OF_KEY_BACKSPACE) {
			string &text = optionsFieldText[focusedOptionsField];
			if (!text.empty()) {
				text.erase(text.size() - 1);
			}
			return;
		}
		// Allow digits 0-9
		if (key >= '0' && key <= '9') {
			optionsFieldText[focusedOptionsField] += (char)key;
			return;
		}
		return; // consume other keys while focused
	}

	if (key == '1') {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
	}

	if (key == '2') {
		pantallaActiva = OPCIONES;
		initOptionsFields();
	}

	if (key == '3') {
		pantallaActiva = TUTORIAL;
		focusedOptionsField = -1;
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
			if (boxes.isGroupViewActive()) {
				boxes.toggleCueBoxByIndex(boxes.groupInspectorIndex);
			} else {
				boxes.toggleCueBoxByIndex(boxes.openguinumber);
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

	// Ctrl+S (keycodes 46 or 19 depending on platform) -> open save-as modal
	if (e.key == 46 || e.key == 19) {
		if (!saveModalActive) {
			saveModalActive = true;
			saveModalName = "";
			cout << "Save modal opened" << endl;
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
	if (pantallaActiva == NODOS) {
		if (boxes.update_cueMouseDragged(button)) {
			return;
		}
		// jp_constants::set_mousePressedPos(ofVec2f(ofGetMouseX(), ofGetMouseY()));
		boxes.update_mouseDragged(button);
	}
}
void ofApp::mousePressed(int x, int y, int button) {
	if (midiKeymap.mousePressed(x, y, button)) {
		return;
	}
	if (midiKeymap.captureFunctionClick(x, y, button)) {
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
		if (language == 0) {
			language = 1;
		} else if (language == 1) {
			language = 0;
		}
	}
	if (pantallaActiva == OPCIONES) {
		// Layout constants matching draw_opciones()
		float panelX = 30, panelY = 30;
		float panelW = 500;
		float fieldX = 175;
		float fieldW = 200;
		float rowH = 28;
		float sepy = 40;
		float toggleBtnW = 70;

		// Check if clicked inside any text field
		focusedOptionsField = -1;
		for (int i = 0; i < OPTIONS_FIELD_COUNT; i++) {
			float rowY = panelY + 55 + i * sepy;
			if (x >= fieldX && x <= fieldX + fieldW &&
				y >= rowY && y <= rowY + rowH) {
				focusedOptionsField = i;
				return;
			}
		}

		// Check toggle buttons
		int toggleRow = OPTIONS_FIELD_COUNT;

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

		toggleRow++; // skip OSC IP info row (not clickable)

		// Check Save button
		{
			float rowY = panelY + 55 + toggleRow * sepy;
			float saveW = 160;
			float saveX = panelX + panelW / 2 - saveW / 2;
			if (x >= saveX && x <= saveX + saveW &&
				y >= rowY && y <= rowY + rowH + 4) {
				saveSettings();
				return;
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
void ofApp::mouseMoved(int x, int y) { }
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
		boxes.setCuePanelLayout(cuePanelX.getFloatValue(),
			cuePanelY.getFloatValue(),
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
	ofSetColor(255);
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

// Save-as modal: draws a centered overlay with a text input field
void ofApp::drawSaveModal() {
	if (!saveModalActive) return;

	float w = ofGetWidth();
	float h = ofGetHeight();

	// Full scene overlay (dark translucent)
	ofSetColor(0, 0, 0, 180);
	ofDrawRectangle(0, 0, w, h);

	// Modal box dimensions
	float boxW = 420;
	float boxH = 190;
	float boxX = (w - boxW) * 0.5f;
	float boxY = (h - boxH) * 0.5f;
	float corner = 10;

	// Rounded shadow behind modal
	ofSetColor(0, 0, 0, 100);
	ofDrawRectRounded(boxX + 5, boxY + 5, boxW, boxH, corner);

	// Panel background — same glassmorphism as draw_opciones
	ofSetColor(12, 16, 20, 238);
	ofDrawRectRounded(boxX, boxY, boxW, boxH, corner);

	// Panel border — cyan with matching alpha
	ofNoFill();
	ofSetColor(0, 230, 230, 80);
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(boxX, boxY, boxW, boxH, corner);
	ofFill();
	ofSetLineWidth(1.0f);

	float pad = 20;

	// Title — cyan, same style as "SETTINGS.XML Configuration"
	ofSetColor(0, 230, 230);
	modalFont.drawString("SAVE COMPOSITION", boxX + pad, boxY + 34);

	// Thin separator
	ofSetColor(50, 60, 75);
	ofDrawLine(boxX + pad, boxY + 45, boxX + boxW - pad, boxY + 45);

	// Input field
	float fieldX = boxX + pad;
	float fieldY = boxY + 58;
	float fieldW = boxW - pad * 2;
	float fieldH = 32;

	// Field background
	ofSetColor(18, 22, 28);
	ofDrawRectRounded(fieldX, fieldY, fieldW, fieldH, 4);

	// Field border — cyan (always focused while modal is active)
	ofNoFill();
	ofSetColor(0, 230, 230);
	ofSetLineWidth(2.0f);
	ofDrawRectRounded(fieldX, fieldY, fieldW, fieldH, 4);
	ofFill();
	ofSetLineWidth(1.0f);

	// Filename text inside the field — white with blinking cursor
	ofSetColor(255);
	string displayText = saveModalName;
	// Cursor blinks every 25 frames
	if ((ofGetFrameNum() / 25) % 2 == 0) {
		displayText += "|";
	}
	// Vertically center text at baseline ~ fieldY + fieldH/2 + font_size/3
	float textY = fieldY + fieldH * 0.5f + 5.0f;
	modalFont.drawString(displayText, fieldX + 8, textY);

	// Preview path below the field
	ofSetColor(100, 130, 160);
	string previewName = saveModalName.empty() ? string("composition") : saveModalName;
	string preview = "savefiles/" + previewName + ".xml";
	modalFont.drawString(preview, boxX + pad, fieldY + fieldH + 22);

	// Bottom hint — Enter (cyan, left)
	ofSetColor(0, 230, 230, 200);
	modalFont.drawString("Enter  Save", boxX + pad, boxY + boxH - 14);

	// Bottom hint — Esc (gray, right)
	ofSetColor(130, 140, 165);
	string escHint = "Esc  Cancel";
	modalFont.drawString(escHint, boxX + boxW - pad - modalFont.stringWidth(escHint), boxY + boxH - 14);
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
