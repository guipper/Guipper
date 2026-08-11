#include "ofApp.h"
#include "JPutils/jp_uishot.h"
#include "JPutils/jp_persistence_test.h"
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

	// After loadSettings so the saved device, gain and enable state apply.
	jp_audio::setup();
	if (std::getenv("GUIPPER_PERSISTENCE_TEST") != nullptr)
	{
		const bool passed = jp_persistence_test::run(*this);
		jp_audio::shutdown();
		std::exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	ofSetWindowTitle("GUIPPER");

	// After loadSettings: it auto-loads the default composition, which the
	// harness has to clobber with its own fixture.
	jp_uishot::setup(*this);

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
	registerSurfaces();
}

bool ofApp::anyFieldFocused() const
{
	return focusedOptionsField >= 0 || focusedLiveOutputField >= 0 ||
		focusedSplitField >= 0 || boxes.tabRenaming ||
		(pantallaActiva == SHADER_INDEX && shaderSearchFocused);
}

void ofApp::clearFieldFocus()
{
	// Reverting, not just unfocusing: re-initialising a field group re-reads
	// the committed values, so an abandoned edit is discarded. The settings
	// screen already promised this on screen ("Click outside to cancel") while
	// actually leaving the typed text in the buffer.
	if (focusedOptionsField >= 0)
	{
		focusedOptionsField = -1;
		initOptionsFields();
	}
	if (focusedLiveOutputField >= 0)
	{
		focusedLiveOutputField = -1;
		liveOutputFieldSelectAll = false;
		initLiveOutputFields();
	}
	if (focusedSplitField >= 0)
	{
		focusedSplitField = -1;
		splitFieldSelectAll = false;
	}
	if (boxes.tabRenaming) boxes.cancelTabRename();
	shaderSearchFocused = false;
}

// Declared once, here, so ESC / modality / click blocking all derive from the
// same table instead of from the order of early returns in the input handlers.
// These are adapters over the flags that already exist - no panel had to change
// how it stores its own state.
void ofApp::registerSurfaces()
{
	JPSurface s;

	s = JPSurface();
	s.id = s.order = SURFACE_INSPECTOR;
	s.isOpen = [this]() { return boxes.getInspectorBox() != nullptr; };
	s.close = [this]() { boxes.closeInspector(); };
	s.bounds = [this]() { return boxes.getInspectorBounds(); };
	surfaces.add(s);

	s = JPSurface();
	s.id = s.order = SURFACE_CUE_PANEL;
	s.isOpen = [this]() { return boxes.getCuePreviewBox() != nullptr; };
	s.close = [this]() { boxes.setCueBoxByIndex(-1); };
	s.bounds = [this]() { return boxes.getCuePanelBounds(); };
	surfaces.add(s);

	s = JPSurface();
	s.id = s.order = SURFACE_MAPPING_PANEL;
	s.isOpen = [this]() { return boxes.isMappingEditActive(); };
	s.close = [this]() { boxes.endMappingEdit(); };
	s.bounds = [this]() { return boxes.getMappingPanelBounds(); };
	surfaces.add(s);

	s = JPSurface();
	s.id = s.order = SURFACE_SHADER_EDITOR;
	s.isOpen = [this]() {
		return pantallaActiva == EDITOR && shaderEditor.isVisible();
	};
	s.close = [this]() { closeShaderEditorToMain(); };
	s.bounds = [this]() { return ofRectangle(); };
	surfaces.add(s);

	// No bounds: an editing field stacks and answers to ESC, but it must not
	// swallow clicks by position - clicking away from a field commits it.
	s = JPSurface();
	s.id = s.order = SURFACE_FIELD_EDIT;
	s.isOpen = [this]() { return anyFieldFocused(); };
	s.close = [this]() { clearFieldFocus(); };
	s.bounds = [this]() { return ofRectangle(); };
	surfaces.add(s);

	// The MIDI conflict prompt is modal within its screen; register it so the
	// app-wide ESC rule cancels it like any other modal. Its order comes from
	// jp_pointer so the prompt can scope its own drawing to the same number -
	// it used to be SURFACE_SAVE_MODAL - 1, a value nothing else could name.
	s = JPSurface();
	s.id = s.order = SURFACE_MIDI_CONFLICT;
	s.modal = true;
	s.isOpen = [this]() { return midiKeymap.hasConflictPrompt(); };
	s.close = [this]() { midiKeymap.cancelConflict(); };
	s.bounds = [this]() { return midiKeymap.conflictPromptRect(); };
	surfaces.add(s);

	s = JPSurface();
	s.id = s.order = SURFACE_DROPDOWN;
	// Real bounds: this was an empty rect, so an open dropdown never blocked
	// the controls it visually covers.
	s.bounds = [this]() {
		const ofRectangle audio = getAudioMenuBounds();
		return audio.getWidth() > 0.0f ? audio :
			midiKeymap.getOpenDropdownBounds();
	};
	s.isOpen = [this]() {
		return midiKeymap.hasOpenDropdown() ||
			liveOutputMenu != LIVE_OUTPUT_MENU_NONE ||
			audioMenuOpen;
	};
	s.close = [this]() {
		midiKeymap.closeDropdowns();
		liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
		audioMenuOpen = false;
	};
	surfaces.add(s);

	s = JPSurface();
	s.id = s.order = SURFACE_SAVE_MODAL;
	s.modal = true;
	s.isOpen = [this]() { return saveModalActive; };
	s.close = [this]() { cancelSaveModal(); };
	s.bounds = [this]() { return ofRectangle(); };
	surfaces.add(s);

	// The node canvas must yield to anything stacked above it, not just to the
	// inspector and the mapping panel it happens to own.
	boxes.setExternalGuiHitTest([this](float x, float y) {
		return surfaces.blockedAt(x, y, SURFACE_MAPPING_PANEL);
	});

	// One pointer-owner rule for every control that opts into a layer.
	jp_pointer::setOcclusionTest([this](float x, float y, int order) {
		return surfaces.blockedAt(x, y, order);
	});
}
void ofApp::update() {
	jp_uishot::update(*this);
	// Arming a learn from the canvas (Map mode) brings its screen up, so the
	// binding you just armed is visible and is not cancelled by the sync below.
	if (midiKeymap.consumeShowRequest()) pantallaActiva = MIDI_KEYMAP;
	midiKeymap.setPanelVisible(pantallaActiva == MIDI_KEYMAP);
	// Before boxes.update(): that is what drives JPParameter::update(), so
	// analysing after it would leave every audio-driven parameter a frame stale.
	jp_audio::update();
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
			// Virtual screens have no monitor to lose; without this guard they
			// would be closed every two seconds.
			if (output.config.enabled && output.window &&
				!output.config.virtualMonitor &&
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
	if (pantallaActiva == MIDI_KEYMAP) {
		midiKeymap.draw();
	}

	drawScreenTabs();

	drawSaveModal();

	// LAST: grabScreen is glReadPixels on the back buffer, so it must run
	// before the swap.
	jp_uishot::draw(*this);
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
// Greedy word wrap. Nothing in the codebase did this before: the old help
// hard-wrapped its longest lines by hand inside the string array, so they never
// re-flowed when the window changed size.
static vector<string> wrapText(const ofTrueTypeFont &font, const string &text,
	float maxWidth)
{
	vector<string> out;
	string line, word;
	istringstream words(text);
	while (words >> word)
	{
		const string candidate = line.empty() ? word : line + " " + word;
		if (!line.empty() && font.stringWidth(candidate) > maxWidth)
		{
			out.push_back(line);
			line = word;
		}
		else
		{
			line = candidate;
		}
	}
	if (!line.empty()) out.push_back(line);
	if (out.empty()) out.push_back("");
	return out;
}

ofApp::HelpLayout ofApp::getHelpLayout() const
{
	const ofRectangle frame = jp_screen::frame();
	const ofRectangle body = jp_screen::body(frame);
	HelpLayout &l = helpLayoutCache;

	// Wrapping measures every string in the table, so rebuild only when
	// something that changes the wrap changed. frame and body are pure
	// functions of the window size, so body's size is the whole geometry key.
	const bool reuse = helpCacheLang == language &&
		std::abs(l.body.width - body.width) < 0.5f &&
		std::abs(l.body.height - body.height) < 0.5f;

	if (!reuse)
	{
		l = HelpLayout();
		l.frame = frame;
		l.body = body;
		l.langBtn = jp_screen::actionSlot(frame, 0, 52.0f);

		const float scrollbarW = 6.0f;
		const float availableW = std::max(0.0f,
			body.width - scrollbarW - 10.0f);
		l.contentW = std::min(availableW, jp_screen::kContentMaxW);
		l.contentX = body.x + (availableW - l.contentW) * 0.5f;
		l.keysW = 118.0f;
		l.descX = l.contentX + l.keysW + 12.0f;

		const float kRowH = 17.0f;    // first line of a row
		const float kWrapH = 15.0f;   // each wrapped continuation
		const float kGapH = 9.0f;
		const float kHeadLead = 22.0f;
		const float kHeadH = 26.0f;   // heading text plus its rule
		const float kTagW = 62.0f;    // reserved so a NODES tag never collides

		float y = 0.0f;
		for (const jp_help::Line &src : jp_help::table())
		{
			HelpRow r;
			r.kind = src.kind;
			r.scope = src.scope;
			r.keys = src.keys;
			const string copy = jp_help::text(src, language);

			switch (src.kind)
			{
			case jp_help::Kind::Gap:
				r.h = kGapH;
				break;

			case jp_help::Kind::Heading:
				if (y > 0.0f) y += kHeadLead;
				r.desc.push_back(copy);
				r.h = kHeadH;
				break;

			case jp_help::Kind::Note:
				r.desc = wrapText(jp_constants::p_font, copy, l.contentW);
				r.h = kRowH + (float)(r.desc.size() - 1) * kWrapH;
				break;

			case jp_help::Kind::Step:
				r.desc = wrapText(jp_constants::p_font, copy,
					std::max(80.0f, l.contentW - 34.0f));
				r.h = kRowH + (float)(r.desc.size() - 1) * kWrapH + 3.0f;
				break;

			case jp_help::Kind::Entry:
			default:
			{
				// A key label wider than the gutter drops its description to
				// the next line rather than colliding with it.
				r.keysOwnLine = jp_constants::p_font.stringWidth(r.keys) >
					l.keysW - 10.0f;
				const float tagW =
					jp_help::scopeTag(src.scope)[0] != '\0' ? kTagW : 0.0f;
				const float descW = r.keysOwnLine ?
					std::max(80.0f, l.contentW - tagW) :
					std::max(80.0f, l.contentX + l.contentW - l.descX - tagW);
				r.desc = wrapText(jp_constants::p_font, copy, descW);
				r.h = kRowH + (float)(r.desc.size() - 1) * kWrapH;
				if (r.keysOwnLine) r.h += kRowH;
				break;
			}
			}

			r.y = y;
			y += r.h;
			l.rows.push_back(r);
		}

		l.contentH = y + 20.0f;   // a little air under the last row
		l.viewH = body.height;
		helpCacheLang = language;
	}

	// Scroll-dependent parts are cheap and must not be cached, or the thumb
	// would freeze at wherever it sat when the rows were last built.
	l.maxScroll = std::max(0.0f, l.contentH - l.viewH);
	l.showScrollbar = l.maxScroll > 0.5f;
	if (l.showScrollbar)
	{
		const float w = 6.0f;
		const float trackX = l.body.getMaxX() - w - 2.0f;
		l.scrollTrack = ofRectangle(trackX, l.body.y, w, l.body.height);
		const float thumbH = std::max(30.0f,
			l.body.height * (l.viewH / l.contentH));
		const float t = ofClamp(helpScroll / l.maxScroll, 0.0f, 1.0f);
		l.scrollThumb = ofRectangle(trackX,
			l.body.y + t * (l.body.height - thumbH), w, thumbH);
	}
	else
	{
		l.scrollTrack = ofRectangle();
		l.scrollThumb = ofRectangle();
	}
	return l;
}

void ofApp::draw_instrucciones() {
	const HelpLayout L = getHelpLayout();
	// Clamped BEFORE drawing, against a height measured in the layout. This
	// used to be measured as a side effect of drawing, so the clamp ran on the
	// previous frame's numbers and overshot after a resize or language switch.
	helpScroll = ofClamp(helpScroll, 0.0f, L.maxScroll);

	jp_screen::drawFrame(L.frame,
		language == 0 ? "HELP" : "AYUDA",
		language == 0 ? "keyboard and workflow" : "teclado y flujo de trabajo");

	// The button names the language it switches TO, like every other two-letter
	// toggle in the app.
	jp_button::draw(L.langBtn, language == 0 ? "ES" : "EN", false);
	jp_tooltip::draw(language == 0 ? "Ver esta pantalla en espanol" :
		"Show this screen in English",
		L.langBtn.x, L.langBtn.y, L.langBtn.width, L.langBtn.height);

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);

	// Clip to the body so a partially scrolled row is cut at the frame edge
	// instead of painting over the header rule. Same primitive the MIDI panel
	// uses (jp_midi_keymap.cpp:1521).
	const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLint previousScissor[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
	glEnable(GL_SCISSOR_TEST);
	glScissor((int)L.body.x, (int)(ofGetHeight() - (L.body.y + L.body.height)),
		(int)L.body.width, (int)L.body.height);

	for (size_t i = 0; i < L.rows.size(); i++)
	{
		const HelpRow &r = L.rows[i];
		const float top = L.body.y + r.y - helpScroll;
		if (top + r.h < L.body.y) continue;
		if (top > L.body.getMaxY()) break;
		if (r.kind == jp_help::Kind::Gap) continue;

		if (r.kind == jp_help::Kind::Heading)
		{
			ofSetColor(COL_ACCENT_CYAN);
			modalFont.drawString(r.desc.empty() ? "" : r.desc[0],
				L.contentX, top + 13.0f);
			ofSetColor(ofColor(COL_BORDER_MUTED, 150));
			ofSetLineWidth(1.0f);
			ofDrawLine(L.contentX, top + 20.0f,
				L.contentX + L.contentW, top + 20.0f);
			continue;
		}

		float lineY = top + 12.0f;
		if (r.kind == jp_help::Kind::Step)
		{
			const ofRectangle badge(L.contentX, top, 22.0f, 18.0f);
			ofFill();
			ofSetColor(ofColor(COL_ACCENT_CYAN, 185));
			ofDrawRectRounded(badge, 4.0f);
			ofSetColor(COL_TEXT_PRIMARY);
			const float numberW = jp_constants::p_font.stringWidth(r.keys);
			jp_constants::p_font.drawString(r.keys,
				badge.x + (badge.width - numberW) * 0.5f, badge.y + 13.0f);
			ofSetColor(COL_TEXT_SECONDARY);
			for (size_t d = 0; d < r.desc.size(); d++)
			{
				jp_constants::p_font.drawString(r.desc[d],
					L.contentX + 34.0f, lineY);
				lineY += 15.0f;
			}
			continue;
		}
		if (r.kind == jp_help::Kind::Entry && !r.keys.empty())
		{
			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p_font.drawString(r.keys, L.contentX, lineY);
			if (r.keysOwnLine) lineY += 17.0f;
		}

		const bool fullWidth =
			r.kind == jp_help::Kind::Note || r.keysOwnLine;
		ofSetColor(r.kind == jp_help::Kind::Note ?
			COL_TEXT_DIM : COL_TEXT_SECONDARY);
		for (size_t d = 0; d < r.desc.size(); d++)
		{
			jp_constants::p_font.drawString(r.desc[d],
				fullWidth ? L.contentX : L.descX, lineY);
			lineY += 15.0f;
		}

		// Which screen the shortcut is live on. The old help listed the
		// NODES-only letter keys as though they worked everywhere.
		const char *tag = jp_help::scopeTag(r.scope);
		if (r.kind == jp_help::Kind::Entry && tag[0] != '\0')
		{
			ofSetColor(COL_TEXT_MUTED);
			const float tw = jp_constants::p2_font.stringWidth(tag);
			jp_constants::p2_font.drawString(tag,
				L.contentX + L.contentW - tw, top + 12.0f);
		}
	}
	if (L.showScrollbar)
	{
		ofFill();
		ofSetColor(ofColor(COL_BG_SCROLLBAR, 60));
		ofDrawRectRounded(L.scrollTrack, 3.0f);
		const bool over = L.scrollTrack.inside(
			(float)ofGetMouseX(), (float)ofGetMouseY());
		ofSetColor(over ? COL_BORDER_HOVER : COL_BORDER_DEFAULT);
		ofDrawRectRounded(L.scrollThumb, 3.0f);
	}
	if (scissorWasEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(previousScissor[0], previousScissor[1],
			previousScissor[2], previousScissor[3]);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}
	ofPopStyle();
}
ofApp::SettingsLayout ofApp::getSettingsLayout() const
{
	SettingsLayout l;
	const float panelW = 500.0f;
	const float sepy = 40.0f;
	const int totalRows = FIELD_OSC_IP_OUT + 13;
	l.rowH = 28.0f;
	l.panel.set(jp_screen::kMarginX, jp_screen::kTop - settingsScroll, panelW,
		jp_screen::kHeaderH + totalRows * sepy + 25.0f);
	l.labelX = l.panel.x + 15.0f;

	const float fieldX = l.panel.x + 175.0f;
	const float fieldW = 200.0f;
	const float actionBtnW = 100.0f;
	// Row 0 sits one header-gap below the header rule, like every screen.
	auto row = [&](int index) {
		return l.panel.y + jp_screen::kHeaderH + (float)index * sepy;
	};

	for (int i = 0; i < FIELD_OSC_IP_OUT; i++)
	{
		l.fields[i].set(fieldX, row(i), fieldW, l.rowH);
	}
	l.autoTapButton.set(fieldX + fieldW + 10.0f, row(FIELD_BPM),
		actionBtnW, l.rowH);

	int r = FIELD_OSC_IP_OUT;
#ifdef SPOUT
	l.spoutToggle.set(fieldX, row(r), actionBtnW, l.rowH);
	r++;
#endif
#ifdef NDI
	l.ndiToggle.set(fieldX, row(r), actionBtnW, l.rowH);
	r++;
#endif
	l.fields[FIELD_OSC_IP_OUT].set(fieldX, row(r), fieldW, l.rowH);
	r++;
	l.fields[FIELD_DEFAULT_COMPO].set(fieldX, row(r), fieldW, l.rowH);
	l.browseButton.set(fieldX + fieldW + 10.0f, row(r), actionBtnW, l.rowH);
	r++;
	l.activeCompoRow.set(fieldX, row(r), fieldW, l.rowH);
	r++;
	l.saveButton.set(fieldX, row(r), fieldW, l.rowH);
	r++;

	// AUDIO IN
	r++;   // blank row as a separator
	l.audioEnableButton.set(fieldX, row(r), actionBtnW, l.rowH);
	r++;
	l.audioDeviceField.set(fieldX, row(r), fieldW + actionBtnW + 10.0f, l.rowH);
	r++;
	l.audioGainSlider.set(fieldX, row(r), fieldW, l.rowH);
	l.audioDivButton.set(fieldX + fieldW + 10.0f, row(r), actionBtnW, l.rowH);
	r++;
	l.audioAutoGainButton.set(fieldX, row(r), 94.0f, l.rowH);
	l.audioChannelButton.set(fieldX + 100.0f, row(r), 94.0f, l.rowH);
	l.audioCalibrateButton.set(fieldX + 200.0f, row(r), 110.0f, l.rowH);
	r++;
	l.audioGateSlider.set(fieldX, row(r), fieldW + actionBtnW + 10.0f, l.rowH);
	r++;
	l.audioMeter.set(fieldX, row(r), fieldW + actionBtnW + 10.0f, l.rowH);
	return l;
}

void ofApp::draw_opciones() {
	const SettingsLayout L = getSettingsLayout();
	const float panelX = L.panel.x;
	const float panelY = L.panel.y;
	const float panelW = L.panel.width;
	const float panelH = L.panel.height;
	const float fieldX = L.fields[0].x;
	const float fieldW = L.fields[0].width;
	const float rowH = L.rowH;
	const float actionBtnW = L.autoTapButton.width;
	// Layout stays here; the look comes from the shared renderer.
	auto drawSettingsButton = [&](const ofRectangle &bounds,
		const string &label, const ofColor &accent, bool active) {
		jp_button::draw(bounds, label, active, true, accent);
	};

	jp_screen::drawFrame(ofRectangle(panelX, panelY, panelW, panelH),
		"SETTINGS", "settings.xml");

	// Field labels & inputs
	string labels[FIELD_OSC_IP_OUT] = {
		"OSC Port In:",
		"OSC Port Out:",
		"Graph Render Width:",
		"Graph Render Height:",
		"BPM:"
	};

	for (int i = 0; i < FIELD_OSC_IP_OUT; i++) {
		const float rowY = L.fields[i].y;

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
		const float rowY = L.spoutToggle.y;
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
		const float rowY = L.ndiToggle.y;
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
		const float rowY = L.fields[FIELD_OSC_IP_OUT].y;
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
		const float rowY = L.fields[FIELD_DEFAULT_COMPO].y;
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
		const float rowY = L.activeCompoRow.y;
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
		const float rowY = L.saveButton.y;
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

	drawAudioSettings(L);

	// Hint text when focused
	if (focusedOptionsField >= 0) {
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("Enter to apply | Click outside to cancel", panelX + 15, panelY + panelH - 10);
	}
	drawLiveOutputSettings();
}

ofRectangle ofApp::getAudioMenuBounds() const
{
	if (!audioMenuOpen) return ofRectangle();
	const SettingsLayout L = getSettingsLayout();
	const int rows = std::max(1, (int)jp_audio::getInputDeviceNames().size() + 1);
	return ofRectangle(L.audioDeviceField.x,
		L.audioDeviceField.getMaxY() + 2.0f,
		L.audioDeviceField.width,
		std::min(240.0f, (float)rows * 24.0f + 4.0f));
}

void ofApp::drawAudioSettings(const SettingsLayout &L)
{
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);

	// Section heading, matching the other SETTINGS groups.
	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString("AUDIO IN", L.labelX, L.audioEnableButton.y + 19.0f);

	const bool live = jp_audio::isRunning();
	jp_button::draw(L.audioEnableButton,
		jp_audio::getEnabled() ? "ON" : "OFF",
		jp_audio::getEnabled(), true,
		live ? COL_ACCENT_GREEN : COL_ACCENT_CYAN);
	jp_tooltip::draw("Turn the audio input on or off",
		L.audioEnableButton.x, L.audioEnableButton.y,
		L.audioEnableButton.width, L.audioEnableButton.height);

	// Device
	ofSetColor(COL_TEXT_SECONDARY);
	font_p.drawString("Device", L.labelX, L.audioDeviceField.y + 19.0f);
	{
		const std::vector<std::string> &names = jp_audio::getInputDeviceNames();
		std::string label = jp_audio::getDeviceName();
		if (label.empty()) label = names.empty() ? "no input device" : "(default)";
		if (font_p.stringWidth(label) > L.audioDeviceField.width - 30.0f)
		{
			while (label.size() > 4 &&
				font_p.stringWidth(label + "...") > L.audioDeviceField.width - 30.0f)
				label = label.substr(0, label.size() - 1);
			label += "...";
		}
		ofSetColor(ofColor(COL_BG_PANEL, 240));
		ofDrawRectRounded(L.audioDeviceField, 4.0f);
		ofNoFill();
		ofSetColor(audioMenuOpen ? COL_ACCENT_CYAN : ofColor(COL_TEXT_MUTED, 180));
		ofDrawRectRounded(L.audioDeviceField, 4.0f);
		ofFill();
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString(label, L.audioDeviceField.x + 8.0f,
			L.audioDeviceField.getMaxY() - 8.0f);
		ofSetColor(COL_ACCENT_CYAN);
		font_p.drawString(audioMenuOpen ? "^" : "v",
			L.audioDeviceField.getMaxX() - 18.0f,
			L.audioDeviceField.getMaxY() - 8.0f);
	}

	// Gain: a drag slider, not a text field.
	ofSetColor(COL_TEXT_SECONDARY);
	font_p.drawString("Gain", L.labelX, L.audioGainSlider.y + 19.0f);
	{
		const float g = jp_audio::getGain();
		const float t = ofClamp((g - 0.05f) / (8.0f - 0.05f), 0.0f, 1.0f);
		ofSetColor(ofColor(COL_BG_INPUT, 220));
		ofDrawRectRounded(L.audioGainSlider, 4.0f);
		ofSetColor(ofColor(COL_ACCENT_CYAN, 170));
		ofDrawRectRounded(L.audioGainSlider.x, L.audioGainSlider.y,
			std::max(6.0f, L.audioGainSlider.width * t),
			L.audioGainSlider.height, 4.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		font_p.drawString("x" + ofToString(g, 2),
			L.audioGainSlider.x + 8.0f, L.audioGainSlider.getMaxY() - 8.0f);
		jp_tooltip::draw("Drag to set input gain",
			L.audioGainSlider.x, L.audioGainSlider.y,
			L.audioGainSlider.width, L.audioGainSlider.height);
	}
	jp_button::draw(L.audioDivButton,
		string("beat /") + jp_audio::divLabel(jp_audio::getShaderDiv()),
		false, true, COL_ACCENT_CYAN);
	jp_tooltip::draw(
		"Beat division for the audio_trigger/express/logic shader uniforms",
		L.audioDivButton.x, L.audioDivButton.y,
		L.audioDivButton.width, L.audioDivButton.height);

	jp_button::draw(L.audioAutoGainButton,
		jp_audio::getAutoGain() ? "AUTO GAIN" : "MANUAL",
		jp_audio::getAutoGain(), true, COL_ACCENT_CYAN);
	jp_button::draw(L.audioChannelButton,
		jp_audio::channelModeLabel(jp_audio::getChannelMode()),
		false, true, COL_ACCENT_CYAN);
	const jp_audio::AudioSnapshot snapshot = jp_audio::getSnapshot();
	jp_button::draw(L.audioCalibrateButton,
		snapshot.calibrating ?
			("LEARN " + ofToString((int)(snapshot.calibrationProgress * 100.0f)) + "%") :
			"CALIBRATE",
		snapshot.calibrating, true, COL_ACCENT_GREEN);

	ofSetColor(COL_TEXT_SECONDARY);
	font_p.drawString("Noise gate", L.labelX, L.audioGateSlider.y + 19.0f);
	const float gateT = ofClamp(jp_audio::getNoiseGate() / 0.25f, 0.0f, 1.0f);
	ofSetColor(ofColor(COL_BG_INPUT, 220));
	ofDrawRectRounded(L.audioGateSlider, 4.0f);
	ofSetColor(ofColor(COL_ACCENT_CYAN, 170));
	ofDrawRectRounded(L.audioGateSlider.x, L.audioGateSlider.y,
		std::max(4.0f, L.audioGateSlider.width * gateT), L.audioGateSlider.height, 4.0f);
	ofSetColor(COL_TEXT_PRIMARY);
	font_p.drawString(ofToString(jp_audio::getNoiseGate(), 3),
		L.audioGateSlider.x + 8.0f, L.audioGateSlider.getMaxY() - 8.0f);

	// Meter: spectrum, kick/snare flashes and the status line. This is the
	// surface that tells you whether anything is actually being heard.
	{
		const ofRectangle m = L.audioMeter;
		ofSetColor(ofColor(COL_BG_INPUT, 220));
		ofDrawRectRounded(m, 4.0f);
		const int bins = 24;
		float spec[24];
		jp_audio::getSpectrum(spec, bins);
		const float bw = m.width / (float)bins;
		for (int i = 0; i < bins; i++)
		{
			const float h = ofClamp(spec[i], 0.0f, 1.0f) * (m.height - 6.0f);
			ofSetColor(ofColor(COL_ACCENT_CYAN, 200));
			ofDrawRectangle(m.x + i * bw + 1.0f, m.getMaxY() - 3.0f - h,
				std::max(1.0f, bw - 2.0f), h);
		}
		// Onset flashes
		const float kAge = jp_audio::secondsSinceKick();
		const float sAge = jp_audio::secondsSinceSnare();
		ofSetColor(COL_ACCENT_GOLD, kAge < 0.12f ? 255 : 40);
		ofDrawCircle(m.getMaxX() - 24.0f, m.getCenter().y, 5.0f);
		ofSetColor(COL_ACCENT_GREEN, sAge < 0.12f ? 255 : 40);
		ofDrawCircle(m.getMaxX() - 10.0f, m.getCenter().y, 5.0f);

		ofSetColor(snapshot.clipping ? COL_ERROR_TEXT :
			(live ? COL_TEXT_MUTED : COL_ERROR_TEXT));
		string diagnostic = jp_audio::getStatus();
		if (snapshot.tempoConfidence > 0.0f)
			diagnostic += "  " + ofToString(snapshot.detectedBpm, 1) + " BPM " +
				ofToString((int)(snapshot.tempoConfidence * 100.0f)) + "%";
		if (snapshot.clipping) diagnostic += "  CLIP";
		font_p.drawString(diagnostic, L.labelX, m.getMaxY() + 16.0f);
	}

	// The dropdown paints last so it covers the rows beneath it.
	if (audioMenuOpen)
	{
		const ofRectangle menu = getAudioMenuBounds();
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(menu, 4.0f);
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 200));
		ofDrawRectRounded(menu, 4.0f);
		ofFill();
		const std::vector<std::string> &names = jp_audio::getInputDeviceNames();
		for (int i = 0; i <= (int)names.size(); i++)
		{
			const float ry = menu.y + 2.0f + i * 24.0f;
			if (ry + 24.0f > menu.getMaxY()) break;
			const bool over = ofRectangle(menu.x, ry, menu.width, 24.0f)
				.inside((float)ofGetMouseX(), (float)ofGetMouseY());
			const string name = i == 0 ? "(system default)" : names[i - 1];
			if (over)
			{
				ofSetColor(ofColor(COL_BG_HOVER, 230));
				ofDrawRectRounded(menu.x + 2.0f, ry, menu.width - 4.0f, 24.0f, 3.0f);
			}
			ofSetColor(name == jp_audio::getDeviceName() ?
				COL_ACCENT_CYAN : COL_TEXT_PRIMARY);
			font_p.drawString(name, menu.x + 8.0f, ry + 17.0f);
		}
	}
	ofPopStyle();
}

bool ofApp::handleAudioSettingsClick(int x, int y)
{
	const SettingsLayout L = getSettingsLayout();
	const ofVec2f m((float)x, (float)y);

	// An open dropdown overlays the rows under it, so it is tested first.
	if (audioMenuOpen)
	{
		const ofRectangle menu = getAudioMenuBounds();
		if (menu.inside(m.x, m.y))
		{
			const std::vector<std::string> &names = jp_audio::getInputDeviceNames();
			for (int i = 0; i <= (int)names.size(); i++)
			{
				const float ry = menu.y + 2.0f + i * 24.0f;
				if (ry + 24.0f > menu.getMaxY()) break;
				if (!ofRectangle(menu.x, ry, menu.width, 24.0f).inside(m.x, m.y))
					continue;
				jp_audio::setDevice(i == 0 ? "" : names[i - 1]);
				audioMenuOpen = false;
				saveSettings();
				return true;
			}
		}
		audioMenuOpen = false;
		return true;   // the dismissing click is consumed, not passed through
	}

	if (L.audioEnableButton.inside(m.x, m.y))
	{
		jp_audio::setEnabled(!jp_audio::getEnabled());
		saveSettings();
		return true;
	}
	if (L.audioDeviceField.inside(m.x, m.y))
	{
		jp_audio::refreshDevices();
		audioMenuOpen = true;
		return true;
	}
	if (L.audioDivButton.inside(m.x, m.y))
	{
		jp_audio::setShaderDiv(
			(jp_audio::getShaderDiv() + 1) % jp_audio::DIV_COUNT);
		saveSettings();
		return true;
	}
	if (L.audioAutoGainButton.inside(m.x, m.y))
	{
		jp_audio::setAutoGain(!jp_audio::getAutoGain());
		saveSettings();
		return true;
	}
	if (L.audioChannelButton.inside(m.x, m.y))
	{
		jp_audio::setChannelMode((jp_audio::getChannelMode() + 1) % jp_audio::CHANNEL_COUNT);
		saveSettings();
		return true;
	}
	if (L.audioCalibrateButton.inside(m.x, m.y))
	{
		jp_audio::beginCalibration();
		return true;
	}
	if (L.audioGateSlider.inside(m.x, m.y))
	{
		audioGateDragging = true;
		jp_audio::setNoiseGate(ofClamp((m.x - L.audioGateSlider.x) /
			L.audioGateSlider.width, 0.0f, 1.0f) * 0.25f);
		return true;
	}
	if (L.audioGainSlider.inside(m.x, m.y))
	{
		audioGainDragging = true;
		const float t = ofClamp(
			(m.x - L.audioGainSlider.x) / L.audioGainSlider.width, 0.0f, 1.0f);
		jp_audio::setGain(ofLerp(0.05f, 8.0f, t));
		return true;
	}
	return false;
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
	const float generalPanelH = getLiveOutputPanelHeight(LO_TAB_OUTPUTS);
	layout.twoColumns = ofGetWidth() >= 1080;
	layout.panel.x = layout.twoColumns ?
		margin + generalPanelW + 16.0f : margin;
	layout.panel.y = layout.twoColumns ?
		44.0f : 44.0f + generalPanelH + 16.0f;
	layout.panel.y -= settingsScroll;
	layout.panel.width = layout.twoColumns ?
		std::max(500.0f, ofGetWidth() - layout.panel.x - margin) :
		std::max(500.0f, ofGetWidth() - margin * 2.0f);
	layout.panel.height = getLiveOutputPanelHeight(liveOutputTab);


	// Content starts below the shared header, not at a hand-picked offset that
	// assumed a shorter one - the tab strip used to be drawn over the subtitle.
	const float contentTop = layout.panel.y + jp_screen::kHeaderH;
	// Tab strip sits between the title and everything else, so every rect below
	// shifts by tabStripH rather than overlapping the first control row.
	const float tabH = 22.0f;
	const float tabStripH = tabH + 8.0f;
	const char *tabLabels[LO_TAB_COUNT] = {"OUTPUTS", "WALL"};
	float tabX = layout.panel.x + 14.0f;
	for (int i = 0; i < LO_TAB_COUNT; i++)
	{
		const float tabW = std::max(70.0f,
			font_p.stringWidth(tabLabels[i]) + 24.0f);
		layout.tabs.push_back(ofRectangle(
			tabX, contentTop, tabW, tabH));
		tabX += tabW + 2.0f;
	}

	const float listW = std::min(184.0f, layout.panel.width * 0.34f);
	// Add/remove sit directly under the list they act on, instead of in the
	// panel header where nothing said which thing they added to.
	const float listButtonH = 26.0f;
	layout.list.set(layout.panel.x + 14.0f,
		contentTop + 12.0f + tabStripH,
		listW, layout.panel.height - jp_screen::kHeaderH - 26.0f - tabStripH
			- (listButtonH + 8.0f));
	const float halfListW = (listW - 6.0f) * 0.5f;
	layout.addButton.set(layout.list.x, layout.list.getBottom() + 8.0f,
		halfListW, listButtonH);
	layout.deleteButton.set(layout.list.x + halfListW + 6.0f,
		layout.list.getBottom() + 8.0f, halfListW, listButtonH);
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
	float rowY = contentTop + 40.0f + tabStripH;
	if (liveOutputTab == LO_TAB_OUTPUTS)
	{
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
	}
	else
	{
		const float halfW = controlW * 0.5f - 3.0f;
		const float rightX = controlX + controlW * 0.5f + 3.0f;

		// --- This output only ---
		layout.tiledToggle.set(controlX, rowY, 70.0f, 28.0f);
		layout.patternToggle.set(controlX + 78.0f, rowY,
			std::max(110.0f, controlW - 78.0f), 28.0f);
		rowY += 43.0f;
		// X / Y then W / H, two per row.
		layout.fieldRects[LO_FIELD_CROP_X].set(controlX, rowY, halfW, 28.0f);
		layout.fieldRects[LO_FIELD_CROP_Y].set(rightX, rowY, halfW, 28.0f);
		rowY += 43.0f;
		layout.fieldRects[LO_FIELD_CROP_W].set(controlX, rowY, halfW, 28.0f);
		layout.fieldRects[LO_FIELD_CROP_H].set(rightX, rowY, halfW, 28.0f);
		rowY += 43.0f;
		layout.fieldRects[LO_FIELD_BEZEL].set(controlX, rowY,
			halfW - 36.0f, 28.0f);
		layout.bezelSignButton.set(controlX + halfW - 32.0f, rowY, 32.0f, 28.0f);
		const float matchW = halfW * 0.5f - 3.0f;
		layout.matchAspectButton.set(rightX, rowY, matchW, 28.0f);
		layout.matchResolutionButton.set(
			rightX + matchW + 6.0f, rowY, matchW, 28.0f);
		rowY += 44.0f;

		// --- Everything below applies to the whole installation ---
		// The wall mode, the split grid and the preview are not per output:
		// mode reinterprets every crop, SPLIT rewrites every enabled output's
		// rect, and the preview shows all of them at once. Sitting in the same
		// column as this output's own fields, they read as if they belonged to
		// the selected output.
		layout.globalSectionY = rowY;
		// Divider at +8, caption baseline at +28, then a full row gap. The
		// caption used to land on the Mode control a pixel below it.
		rowY += 42.0f;
		layout.modeToggle.set(controlX, rowY, controlW, 28.0f);
		rowY += 43.0f;
		// SPLIT fills every enabled output's rect from a cols x rows grid.
		const float thirdW = std::max(44.0f, (controlW - 12.0f) / 3.0f);
		layout.splitColsField.set(controlX, rowY, thirdW, 28.0f);
		layout.splitRowsField.set(controlX + thirdW + 6.0f, rowY,
			thirdW, 28.0f);
		layout.splitButton.set(controlX + (thirdW + 6.0f) * 2.0f, rowY,
			controlW - (thirdW + 6.0f) * 2.0f, 28.0f);
		rowY += 43.0f;
		layout.viewToggle.set(controlX, rowY, halfW, 28.0f);
		rowY += 40.0f;
		// Preview of every tile, in canvas aspect, filling what is left.
		// Reserve the status line that is drawn under the preview; it used to
		// be painted past the panel's bottom edge, over the list buttons.
		const float statusH = 26.0f;
		const float previewH = std::max(60.0f,
			layout.panel.getBottom() - 14.0f - statusH - rowY);
		const float canvasAspect = jp_constants::renderHeight > 0 ?
			(float)jp_constants::renderWidth /
				(float)jp_constants::renderHeight : 1.7778f;
		float previewW = previewH * canvasAspect;
		float finalH = previewH;
		if (previewW > controlW) { previewW = controlW; finalH = previewW / canvasAspect; }
		layout.preview.set(controlX, rowY, previewW, finalH);
	}

	if (liveOutputTab == LO_TAB_OUTPUTS &&
		liveOutputMenu != LIVE_OUTPUT_MENU_NONE)
	{
		const ofRectangle &anchor =
			liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE ?
				layout.sourceButton : layout.monitorButton;
		const int optionCount =
			liveOutputMenu == LIVE_OUTPUT_MENU_SOURCE ?
				(int)getLiveOutputSourceOptions().size() :
				(int)liveOutputMonitors.size() + 1;
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

void ofApp::drawLiveOutputTestPattern(const ofRectangle &bounds,
	int outputIndex)
{
	if (bounds.width < 4.0f || bounds.height < 4.0f) return;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofFill();
	ofSetColor(0);
	ofDrawRectangle(bounds);

	// Grid, so geometry errors and non-square pixels are visible.
	ofSetColor(40, 60, 70);
	ofSetLineWidth(1.0f);
	for (int i = 1; i < 10; i++)
	{
		const float fx = bounds.x + bounds.width * i / 10.0f;
		const float fy = bounds.y + bounds.height * i / 10.0f;
		ofDrawLine(fx, bounds.y, fx, bounds.getBottom());
		ofDrawLine(bounds.x, fy, bounds.getRight(), fy);
	}

	// Labelled rings at 2/4/6/8 percent. Read off the outermost ring you can
	// still see on the tube and type that as the overscan; on a CRT the frame
	// eats the edges, so the answer goes in as a NEGATIVE bezel, which samples
	// wider to compensate.
	const int percents[4] = {2, 4, 6, 8};
	const ofColor ringColors[4] = {
		ofColor(255, 80, 80), ofColor(226, 174, 64),
		ofColor(46, 190, 120), ofColor(0, 175, 190)};
	ofNoFill();
	for (int i = 0; i < 4; i++)
	{
		const float insetX = bounds.width * percents[i] / 100.0f;
		const float insetY = bounds.height * percents[i] / 100.0f;
		ofSetColor(ringColors[i]);
		ofSetLineWidth(1.5f);
		ofDrawRectangle(bounds.x + insetX, bounds.y + insetY,
			bounds.width - insetX * 2.0f, bounds.height - insetY * 2.0f);
		if (bounds.width > 220.0f)
		{
			const string label = ofToString(percents[i]) + "%";
			ofFill();
			font_p.drawString(label, bounds.x + insetX + 3.0f,
				bounds.y + insetY + 13.0f);
			ofNoFill();
		}
	}

	// Corner brackets, so a cut corner is unmistakable.
	ofSetColor(255);
	ofSetLineWidth(2.0f);
	const float arm = std::min(bounds.width, bounds.height) * 0.08f;
	const float cx[2] = {bounds.x, bounds.getRight()};
	const float cy[2] = {bounds.y, bounds.getBottom()};
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			const float sx = i == 0 ? 1.0f : -1.0f;
			const float sy = j == 0 ? 1.0f : -1.0f;
			ofDrawLine(cx[i], cy[j], cx[i] + arm * sx, cy[j]);
			ofDrawLine(cx[i], cy[j], cx[i], cy[j] + arm * sy);
		}
	}

	// Screen identity, centred and large enough to read across a room.
	ofFill();
	const string name = outputIndex >= 0 &&
		outputIndex < (int)liveOutputs.size() ?
		getLiveOutputDisplayName(outputIndex) : "";
	const string number = ofToString(outputIndex + 1);
	if (bounds.width > 120.0f)
	{
		ofSetColor(255);
		jp_constants::h_font.drawString(number,
			bounds.getCenter().x -
				jp_constants::h_font.stringWidth(number) * 0.5f,
			bounds.getCenter().y + 7.0f);
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString(name,
			bounds.getCenter().x - font_p.stringWidth(name) * 0.5f,
			bounds.getCenter().y + 28.0f);
	}
	else
	{
		ofSetColor(255);
		font_p.drawString(number, bounds.getCenter().x - 3.0f,
			bounds.getCenter().y + 4.0f);
	}
	ofPopStyle();
}

ofRectangle ofApp::getInstallationBounds() const
{
	bool any = false;
	float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
	for (const LiveOutputRuntime &output : liveOutputs)
	{
		const LiveOutputConfig &config = output.config;
		// Only measured, tiled screens take part. A screen with no measurement
		// would otherwise drag the bounding box to the origin.
		if (!config.cropEnabled) continue;
		if (config.physW <= 0.0 || config.physH <= 0.0) continue;
		const float x0 = (float)config.physX;
		const float y0 = (float)config.physY;
		const float x1 = (float)(config.physX + config.physW);
		const float y1 = (float)(config.physY + config.physH);
		if (!any) { minX = x0; minY = y0; maxX = x1; maxY = y1; any = true; }
		else
		{
			minX = std::min(minX, x0);
			minY = std::min(minY, y0);
			maxX = std::max(maxX, x1);
			maxY = std::max(maxY, y1);
		}
	}
	if (!any) return ofRectangle();
	return ofRectangle(minX, minY, maxX - minX, maxY - minY);
}

void ofApp::applySpatialLayout()
{
	if (wallMode != WALL_MODE_SPATIAL) return;
	const ofRectangle bounds = getInstallationBounds();
	if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
	// The canvas is stretched across the installation's bounding box, so each
	// screen samples exactly the part of it that falls behind its own glass.
	// Everything between the screens is canvas nobody sees, which is the point.
	for (LiveOutputRuntime &output : liveOutputs)
	{
		LiveOutputConfig &config = output.config;
		if (!config.cropEnabled) continue;
		if (config.physW <= 0.0 || config.physH <= 0.0) continue;
		config.cropX = (config.physX - bounds.x) / bounds.width;
		config.cropY = (config.physY - bounds.y) / bounds.height;
		config.cropW = config.physW / bounds.width;
		config.cropH = config.physH / bounds.height;
	}
}

ofRectangle ofApp::getRoomScreenRect(const LiveOutputSettingsLayout &layout,
	int outputIndex) const
{
	if (outputIndex < 0 || outputIndex >= (int)liveOutputs.size())
		return ofRectangle();
	const LiveOutputConfig &config = liveOutputs[outputIndex].config;
	if (config.physW <= 0.0 || config.physH <= 0.0) return ofRectangle();
	const ofRectangle bounds = getInstallationBounds();
	if (bounds.width <= 0.0f || bounds.height <= 0.0f) return ofRectangle();
	// One scale for both axes so the room is not distorted: the installation is
	// letterboxed inside the preview.
	const float scale = std::min(layout.preview.width / bounds.width,
		layout.preview.height / bounds.height);
	const float originX = layout.preview.x +
		(layout.preview.width - bounds.width * scale) * 0.5f;
	const float originY = layout.preview.y +
		(layout.preview.height - bounds.height * scale) * 0.5f;
	return ofRectangle(
		originX + ((float)config.physX - bounds.x) * scale,
		originY + ((float)config.physY - bounds.y) * scale,
		(float)config.physW * scale, (float)config.physH * scale);
}

void ofApp::drawWallRoomView(const LiveOutputSettingsLayout &layout)
{
	// The room itself, so the empty space between scattered screens reads as
	// wall rather than as missing content.
	ofSetColor(8, 10, 12);
	ofDrawRectRounded(layout.preview, 4.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_BORDER_MUTED, 200));
	ofDrawRectRounded(layout.preview, 4.0f);
	ofFill();

	const ofRectangle bounds = getInstallationBounds();
	if (bounds.width <= 0.0f || bounds.height <= 0.0f)
	{
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("Measure each screen (Pos/Size mm) to see the room",
			layout.preview.x + 10.0f, layout.preview.y + 22.0f);
		return;
	}

	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		const LiveOutputConfig &config = liveOutputs[i].config;
		const ofRectangle screen = getRoomScreenRect(layout, i);
		if (screen.width < 1.0f || screen.height < 1.0f) continue;
		const bool isSelected = i == selectedLiveOutput;

		// Live content, drawn through the same path the real window uses so
		// what you see here is what the screen shows.
		if (config.testPattern)
		{
			drawLiveOutputTestPattern(screen, i);
		}
		else
		{
			ofSetColor(0);
			ofDrawRectangle(screen);
			const ofRectangle crop = config.cropEnabled ?
				ofRectangle(config.cropX, config.cropY,
					config.cropW, config.cropH) :
				ofRectangle(0.0f, 0.0f, 1.0f, 1.0f);
			const float bezel = config.cropEnabled ?
				(float)config.bezelPx : 0.0f;
			ofPushMatrix();
			ofTranslate(screen.x, screen.y);
			ofSetColor(255);
			boxes.drawLiveOutputSource(
				config.sourceMode == LIVE_OUTPUT_MAIN_ACTIVE,
				config.sourceBox, screen.width, screen.height, crop, bezel);
			ofPopMatrix();
		}

		ofNoFill();
		ofSetLineWidth(isSelected ? 2.0f : 1.0f);
		ofSetColor(isSelected ? COL_ACCENT_CYAN : COL_TEXT_MUTED);
		ofDrawRectangle(screen);
		ofFill();
		ofSetLineWidth(1.0f);
		if (screen.width > 24.0f && screen.height > 14.0f)
		{
			ofSetColor(isSelected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
			font_p.drawString(ofToString(i + 1),
				screen.x + 4.0f, screen.getBottom() - 4.0f);
		}
	}

	// Scale readout, so the preview can be related to the real thing.
	const float scale = std::min(layout.preview.width / bounds.width,
		layout.preview.height / bounds.height);
	ofSetColor(COL_TEXT_MUTED);
	font_p.drawString(ofToString((int)std::lround(bounds.width)) + " x " +
		ofToString((int)std::lround(bounds.height)) + " mm   1:" +
		ofToString((int)std::lround(1.0f / std::max(0.0001f, scale))),
		layout.preview.x + 6.0f, layout.preview.getBottom() - 6.0f);
}

ofRectangle ofApp::getWallTileRect(const LiveOutputSettingsLayout &layout,
	int outputIndex) const
{
	if (outputIndex < 0 || outputIndex >= (int)liveOutputs.size())
		return ofRectangle();
	const LiveOutputConfig &config = liveOutputs[outputIndex].config;
	return ofRectangle(
		layout.preview.x + config.cropX * layout.preview.width,
		layout.preview.y + config.cropY * layout.preview.height,
		config.cropW * layout.preview.width,
		config.cropH * layout.preview.height);
}

bool ofApp::handleLiveOutputSettingsDrag(int x, int y, int button)
{
	if (button != OF_MOUSE_BUTTON_LEFT) return false;
	if (wallDragMode == WALL_DRAG_NONE) return false;
	if (wallDragOutput < 0 || wallDragOutput >= (int)liveOutputs.size())
	{
		wallDragMode = WALL_DRAG_NONE;
		wallDragOutput = -1;
		wallDragCorner = -1;
		return false;
	}
	const LiveOutputSettingsLayout layout = getLiveOutputSettingsLayout();
	if (layout.preview.width <= 0.0f || layout.preview.height <= 0.0f)
		return true;
	LiveOutputConfig &config = liveOutputs[wallDragOutput].config;
	const ofVec2f canvas = getLiveOutputCanvasSize(config);

	// Snapshot plus delta, in normalized units.
	const double dx = (x - wallDragStartMouse.x) / layout.preview.width;
	const double dy = (y - wallDragStartMouse.y) / layout.preview.height;
	const ofRectangle &start = wallDragStartCrop;

	// Snap candidates: the canvas edges and every other tile's edges, so tiles
	// can be made to abut exactly instead of leaving a one pixel seam.
	const double snapX = 6.0 / layout.preview.width;
	const double snapY = 6.0 / layout.preview.height;
	vector<double> edgesX{0.0, 1.0};
	vector<double> edgesY{0.0, 1.0};
	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		if (i == wallDragOutput) continue;
		const LiveOutputConfig &other = liveOutputs[i].config;
		if (!other.cropEnabled) continue;
		edgesX.push_back(other.cropX);
		edgesX.push_back(other.cropX + other.cropW);
		edgesY.push_back(other.cropY);
		edgesY.push_back(other.cropY + other.cropH);
	}
	// Reports whether it actually snapped. Without that flag an edge that did
	// not snap looks like a zero distance move and always wins the comparison
	// below against the edge that did.
	auto snap = [](double value, const vector<double> &edges, double tol,
		bool &snapped) {
		double best = value;
		double bestDist = tol;
		snapped = false;
		for (double edge : edges)
		{
			const double dist = std::abs(value - edge);
			if (dist < bestDist)
			{
				bestDist = dist;
				best = edge;
				snapped = true;
			}
		}
		return best;
	};
	// Whole canvas pixels, so the fields stay clean integers.
	auto quantX = [&](double v) {
		return std::lround(v * canvas.x) / (double)canvas.x; };
	auto quantY = [&](double v) {
		return std::lround(v * canvas.y) / (double)canvas.y; };

	if (wallDragMode == WALL_DRAG_MOVE)
	{
		double nx = start.x + dx;
		double ny = start.y + dy;
		// Snap whichever of the two opposing edges landed on a candidate,
		// preferring the smaller correction when both did.
		bool hitLeft = false, hitRight = false;
		const double leftSnap = snap(nx, edgesX, snapX, hitLeft);
		const double rightSnap = snap(nx + start.width, edgesX, snapX,
			hitRight) - start.width;
		if (hitLeft && hitRight)
			nx = std::abs(leftSnap - nx) <= std::abs(rightSnap - nx) ?
				leftSnap : rightSnap;
		else if (hitLeft) nx = leftSnap;
		else if (hitRight) nx = rightSnap;
		bool hitTop = false, hitBottom = false;
		const double topSnap = snap(ny, edgesY, snapY, hitTop);
		const double bottomSnap = snap(ny + start.height, edgesY, snapY,
			hitBottom) - start.height;
		if (hitTop && hitBottom)
			ny = std::abs(topSnap - ny) <= std::abs(bottomSnap - ny) ?
				topSnap : bottomSnap;
		else if (hitTop) ny = topSnap;
		else if (hitBottom) ny = bottomSnap;
		config.cropX = ofClamp(quantX(nx), 0.0, 1.0 - start.width);
		config.cropY = ofClamp(quantY(ny), 0.0, 1.0 - start.height);
	}
	else
	{
		// The opposite corner stays put and the mouse applies one uniform scale,
		// preserving the tile's source-pixel aspect ratio throughout the drag.
		const bool right = wallDragCorner == 1 || wallDragCorner == 3;
		const bool bottom = wallDragCorner == 2 || wallDragCorner == 3;
		const double anchorX = right ? start.x : start.x + start.width;
		const double anchorY = bottom ? start.y : start.y + start.height;
		double movingX = (right ? start.x + start.width : start.x) + dx;
		double movingY = (bottom ? start.y + start.height : start.y) + dy;
		bool snappedX = false, snappedY = false;
		movingX = snap(movingX, edgesX, snapX, snappedX);
		movingY = snap(movingY, edgesY, snapY, snappedY);
		movingX = ofClamp(quantX(movingX), 0.0, 1.0);
		movingY = ofClamp(quantY(movingY), 0.0, 1.0);

		const double startPixelW = std::max(
			1.0, (double)start.width * canvas.x);
		const double startPixelH = std::max(
			1.0, (double)start.height * canvas.y);
		const double rawPixelW = std::abs(movingX - anchorX) * canvas.x;
		const double rawPixelH = std::abs(movingY - anchorY) * canvas.y;
		const double scaleX = rawPixelW / startPixelW;
		const double scaleY = rawPixelH / startPixelH;
		double requestedScale =
			std::abs(scaleX - 1.0) >= std::abs(scaleY - 1.0) ?
				scaleX : scaleY;
		if (snappedX != snappedY)
			requestedScale = snappedX ? scaleX : scaleY;
		const double maxPixelW = (right ? 1.0 - anchorX : anchorX) * canvas.x;
		const double maxPixelH = (bottom ? 1.0 - anchorY : anchorY) * canvas.y;
		const double minimumScale = std::max(
			1.0 / startPixelW, 1.0 / startPixelH);
		const double maximumScale = std::max(minimumScale, std::min(
			maxPixelW / startPixelW, maxPixelH / startPixelH));
		const double uniformScale = ofClamp(
			requestedScale, minimumScale, maximumScale);
		const double targetW = std::min(maxPixelW, std::max(1.0,
			(double)std::lround(startPixelW * uniformScale))) / canvas.x;
		const double targetH = std::min(maxPixelH, std::max(1.0,
			(double)std::lround(startPixelH * uniformScale))) / canvas.y;
		config.cropX = right ? anchorX : anchorX - targetW;
		config.cropY = bottom ? anchorY : anchorY - targetH;
		config.cropW = targetW;
		config.cropH = targetH;
	}
	// Keep the numbers in the fields tracking the drag.
	if (selectedLiveOutput == wallDragOutput && focusedLiveOutputField < 0 &&
		focusedSplitField < 0)
	{
		initLiveOutputFields();
	}
	return true;
}

bool ofApp::handleLiveOutputSettingsRelease(int x, int y, int button)
{
	(void)x; (void)y;
	if (button != OF_MOUSE_BUTTON_LEFT) return false;
	if (wallDragMode == WALL_DRAG_NONE) return false;
	wallDragMode = WALL_DRAG_NONE;
	wallDragOutput = -1;
	wallDragCorner = -1;
	// Saved once here rather than on every drag frame: saveSettings rewrites
	// the whole settings file.
	initLiveOutputFields();
	saveSettings();
	return true;
}

void ofApp::drawLiveOutputWallTab(const LiveOutputSettingsLayout &layout,
	float editorX)
{
	LiveOutputRuntime &output = liveOutputs[selectedLiveOutput];
	LiveOutputConfig &config = output.config;
	const ofVec2f canvas = getLiveOutputCanvasSize(config);
	const ofVec2f targetSize = getLiveOutputTargetSize(output);

	// Same control renderer the outputs tab uses.
	auto drawControl = [&](const ofRectangle &bounds, const string &label,
		bool active, bool disabled = false) {
		const bool hovered = !disabled &&
			bounds.inside(ofGetMouseX(), ofGetMouseY());
		ofSetColor(disabled ? COL_BG_DARK :
			(active ? COL_ACCENT_CYAN_DIM :
				(hovered ? COL_BG_HOVER : COL_BG_INPUT)));
		ofDrawRectRounded(bounds, 4.0f);
		ofNoFill();
		ofSetColor(active ? COL_ACCENT_CYAN :
			(disabled ? COL_TEXT_MUTED : COL_MAPPED_OFF));
		ofDrawRectRounded(bounds, 4.0f);
		ofFill();
		ofSetColor(disabled ? COL_TEXT_MUTED : COL_TEXT_PRIMARY);
		string visible = label;
		if (bounds.width < 76.0f && label == "MATCH AR") visible = "AR";
		if (bounds.width < 76.0f && label == "MATCH RES") visible = "RES";
		const float textX = bounds.width < 40.0f ?
			bounds.getCenter().x - font_p.stringWidth(visible) * 0.5f :
			bounds.x + 7.0f;
		font_p.drawString(visible, textX, bounds.getBottom() - 8.0f);
	};

	ofSetColor(COL_ACCENT_CYAN);
	font_p.drawString(getLiveOutputDisplayName(selectedLiveOutput),
		editorX, layout.panel.y + jp_screen::kHeaderH + 21.0f);
	ofSetColor(COL_TEXT_MUTED);
	font_p.drawString("this output only",
		editorX + font_p.stringWidth(
			getLiveOutputDisplayName(selectedLiveOutput)) + 10.0f,
		layout.panel.y + jp_screen::kHeaderH + 21.0f);

	// Labels, each aligned to the control it belongs to.
	struct WallLabel { const char *text; const ofRectangle *bounds; };
	const bool spatial = wallMode == WALL_MODE_SPATIAL;
	const WallLabel wallLabels[] = {
		{"Tiled",    &layout.tiledToggle},
		{spatial ? "Position mm" : "Position X/Y",
			&layout.fieldRects[LO_FIELD_CROP_X]},
		{spatial ? "Size mm" : "Size W/H",
			&layout.fieldRects[LO_FIELD_CROP_W]},
		{"Overscan", &layout.fieldRects[LO_FIELD_BEZEL]},
		{"Mode",     &layout.modeToggle},
		{"Split",    &layout.splitColsField},
		{"Preview",  &layout.viewToggle}
	};
	for (const WallLabel &label : wallLabels)
	{
		ofSetColor(COL_TEXT_SECONDARY);
		font_p.drawString(label.text, editorX,
			label.bounds->getBottom() - 7.0f);
	}

	// Divider and caption marking where per-output settings end.
	{
		const float lineY = layout.globalSectionY + 8.0f;
		const float captionBaseline = lineY + 20.0f;
		ofSetColor(ofColor(COL_BORDER_MUTED, 150));
		ofSetLineWidth(1.0f);
		ofDrawLine(editorX, lineY, layout.panel.getRight() - 14.0f, lineY);
		ofSetColor(COL_ACCENT_CYAN);
		font_p.drawString("ALL OUTPUTS", editorX, captionBaseline);
		ofSetColor(COL_TEXT_MUTED);
		const string caption = "applies to the whole installation";
		font_p.drawString(caption,
			editorX + font_p.stringWidth("ALL OUTPUTS") + 10.0f,
			captionBaseline);
	}

	drawControl(layout.tiledToggle,
		config.cropEnabled ? "ON" : "OFF", config.cropEnabled);
	drawControl(layout.modeToggle,
		spatial ? "SPATIAL (measured mm)" : "FREEFORM (canvas px)", spatial);
	drawControl(layout.viewToggle, wallRoomView ? "ROOM" : "CANVAS",
		wallRoomView);
	drawControl(layout.patternToggle,
		config.testPattern ? "PATTERN ON" : "PATTERN OFF", config.testPattern);

	// Crop and bezel stay editable while fullscreen, unlike the window size
	// fields: a wall is normally run fullscreen.
	const int cropFields[] = {LO_FIELD_CROP_X, LO_FIELD_CROP_Y,
		LO_FIELD_CROP_W, LO_FIELD_CROP_H, LO_FIELD_BEZEL};
	for (int field : cropFields)
	{
		const ofRectangle &bounds = layout.fieldRects[field];
		drawControl(bounds, liveOutputFieldText[field],
			focusedLiveOutputField == field, !config.cropEnabled);
		if (focusedLiveOutputField == field)
		{
			if (liveOutputFieldSelectAll)
				jp_textfield::drawSelection(font_p, liveOutputFieldText[field],
					bounds.x + 7.0f, bounds.getBottom() - 8.0f,
					bounds.height - 8.0f);
			else
				jp_textfield::drawCaret(font_p, liveOutputFieldText[field],
					liveOutputFieldCursor, bounds.x + 7.0f,
					bounds.getCenter().y, bounds.height - 8.0f);
		}
	}
	// The text field takes digits only, so the sign lives on its own button.
	// Minus samples WIDER, which is how CRT overscan is compensated.
	drawControl(layout.bezelSignButton, config.bezelPx < 0 ? "-" : "+",
		config.bezelPx < 0, !config.cropEnabled);
	drawControl(layout.matchAspectButton, "MATCH AR", false,
		!config.cropEnabled || spatial);
	drawControl(layout.matchResolutionButton, "MATCH RES", false,
		!config.cropEnabled || spatial);

	drawControl(layout.splitColsField, splitFieldText[0],
		focusedSplitField == 0);
	drawControl(layout.splitRowsField, splitFieldText[1],
		focusedSplitField == 1);
	if (focusedSplitField >= 0)
	{
		const ofRectangle &bounds = focusedSplitField == 0 ?
			layout.splitColsField : layout.splitRowsField;
		if (splitFieldSelectAll)
			jp_textfield::drawSelection(font_p, splitFieldText[focusedSplitField],
				bounds.x + 7.0f, bounds.getBottom() - 8.0f,
				bounds.height - 8.0f);
		else
			jp_textfield::drawCaret(font_p, splitFieldText[focusedSplitField],
				splitFieldCursor, bounds.x + 7.0f,
				bounds.getCenter().y, bounds.height - 8.0f);
	}
	drawControl(layout.splitButton, "SPLIT", false);

	if (wallRoomView)
	{
		drawWallRoomView(layout);
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString(spatial ?
			"Room view - screens at their measured positions" :
			"Room view needs measured positions (switch to SPATIAL)",
			editorX, layout.preview.getBottom() + 15.0f);
		jp_tooltip::draw("Drag a tile to move it, drag a corner to resize. "
			"Edges snap to the canvas and to other tiles.",
			layout.viewToggle.x, layout.viewToggle.y,
			layout.viewToggle.width, layout.viewToggle.height);
		jp_tooltip::draw("Replace the content with an alignment pattern: "
			"labelled rings at 2/4/6/8% and the screen number",
			layout.patternToggle.x, layout.patternToggle.y,
			layout.patternToggle.width, layout.patternToggle.height);
		return;
	}

	// Preview: every tile of every output that shares this source, over a box
	// in canvas aspect. This is how gaps and overlaps become visible at all.
	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(layout.preview, 4.0f);
	ofNoFill();
	ofSetColor(ofColor(COL_BORDER_MUTED, 200));
	ofDrawRectRounded(layout.preview, 4.0f);
	ofFill();

	float coverage = 0.0f;
	bool overlaps = false;
	vector<int> peers;
	for (int i = 0; i < (int)liveOutputs.size(); i++)
	{
		const LiveOutputConfig &other = liveOutputs[i].config;
		// Only outputs sharing a source are comparable; tiles of different
		// sources say nothing about each other.
		if (!other.cropEnabled) continue;
		if (other.sourceMode != config.sourceMode) continue;
		if (other.sourceMode == LIVE_OUTPUT_FIXED_BOX &&
			other.sourceBox != config.sourceBox) continue;
		peers.push_back(i);
	}
	for (size_t a = 0; a < peers.size(); a++)
	{
		const LiveOutputConfig &ca = liveOutputs[peers[a]].config;
		coverage += (float)(ca.cropW * ca.cropH);
		for (size_t b = a + 1; b < peers.size(); b++)
		{
			const LiveOutputConfig &cb = liveOutputs[peers[b]].config;
			const bool apart =
				ca.cropX + ca.cropW <= cb.cropX + 0.0001 ||
				cb.cropX + cb.cropW <= ca.cropX + 0.0001 ||
				ca.cropY + ca.cropH <= cb.cropY + 0.0001 ||
				cb.cropY + cb.cropH <= ca.cropY + 0.0001;
			if (!apart) overlaps = true;
		}
	}
	for (int index : peers)
	{
		const ofRectangle tile = getWallTileRect(layout, index);
		const bool isSelected = index == selectedLiveOutput;
		ofSetColor(isSelected ? ofColor(COL_ACCENT_CYAN, 70) :
			ofColor(COL_TEXT_MUTED, 45));
		ofDrawRectangle(tile);
		ofNoFill();
		ofSetLineWidth(isSelected ? 2.0f : 1.0f);
		ofSetColor(isSelected ? COL_ACCENT_CYAN : COL_TEXT_MUTED);
		ofDrawRectangle(tile);
		ofFill();
		ofSetLineWidth(1.0f);
		if (tile.width > 26.0f && tile.height > 14.0f)
		{
			ofSetColor(isSelected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
			font_p.drawString(ofToString(index + 1),
				tile.x + 4.0f, tile.getBottom() - 4.0f);
		}
		if (isSelected)
		{
			// Grab handles, so drag to resize is visible rather than guessed.
			const ofVec2f corners[4] = {
				ofVec2f(tile.x, tile.y),
				ofVec2f(tile.getRight(), tile.y),
				ofVec2f(tile.x, tile.getBottom()),
				ofVec2f(tile.getRight(), tile.getBottom())};
			for (const ofVec2f &corner : corners)
			{
				ofSetColor(COL_BG_DARK);
				ofDrawRectangle(corner.x - 4.0f, corner.y - 4.0f, 8.0f, 8.0f);
				ofSetColor(COL_ACCENT_CYAN);
				ofDrawRectangle(corner.x - 3.0f, corner.y - 3.0f, 6.0f, 6.0f);
			}
		}
	}

	// Advisory only: deliberate overlap is a legitimate edge blend setup.
	string status;
	ofColor statusColor = COL_TEXT_MUTED;
	if (!config.cropEnabled)
	{
		status = "Not tiled - this output shows the whole canvas";
	}
	else if (overlaps)
	{
		status = "Tiles overlap";
		statusColor = COL_ACCENT_GOLD;
	}
	else if (coverage < 0.999f && !spatial)
	{
		// Only a complaint in FREEFORM. In SPATIAL the gaps are the wall
		// between scattered screens, which is exactly what was measured.
		status = "Coverage has gaps (" +
			ofToString((int)std::lround(coverage * 100.0f)) + "%)";
		statusColor = COL_ACCENT_GOLD;
	}
	else if (spatial)
	{
		status = "Tiled from measured layout (" +
			ofToString((int)std::lround(coverage * 100.0f)) + "% of canvas seen)";
		statusColor = COL_ACCENT_GREEN;
		// The canvas is stretched onto the installation's bounding box, so if
		// their aspects differ the content is distorted everywhere. Silent
		// otherwise, and a 33% stretch is very visible, so give the number and
		// the render size that fixes it.
		const ofRectangle bounds = getInstallationBounds();
		if (bounds.width > 0.0f && bounds.height > 0.0f)
		{
			const float installAspect = bounds.width / bounds.height;
			const float canvasAspect = canvas.y > 0.0f ?
				canvas.x / canvas.y : 1.0f;
			const float stretch = installAspect / std::max(0.0001f, canvasAspect);
			if (std::abs(stretch - 1.0f) > 0.01f)
			{
				status = "Content stretched " +
					ofToString(stretch, 2) + " : 1  |  set render to " +
					ofToString((int)std::lround(canvas.y * installAspect)) +
					"x" + ofToString((int)std::lround(canvas.y));
				statusColor = COL_ACCENT_GOLD;
			}
		}
	}
	else
	{
		status = "Tiled";
		statusColor = COL_ACCENT_GREEN;
	}
	// Aspect advisory: the tile is stretched to the window, so a mismatch is a
	// silent geometry error otherwise.
	// Suppressed while the canvas-stretch note is showing: they have the same
	// single cause, and fixing the render size clears both.
	const bool stretchNoteShown = spatial &&
		status.rfind("Content stretched", 0) == 0;
	if (config.cropEnabled && targetSize.y > 0.0f && !stretchNoteShown)
	{
		const ofRectangle src = getLiveOutputSourceRect(config);
		if (src.height > 0.0f)
		{
			const float cropAspect = src.width / src.height;
			const float windowAspect = targetSize.x / targetSize.y;
			if (std::abs(cropAspect - windowAspect) / windowAspect > 0.01f)
			{
				status += "  |  aspect off " + ofToString(
					(int)std::lround((cropAspect / windowAspect - 1.0f) *
						100.0f)) + "%";
				statusColor = COL_ACCENT_GOLD;
			}
		}
	}
	ofSetColor(statusColor);
	font_p.drawString(status, editorX, layout.preview.getBottom() + 15.0f);

	jp_tooltip::draw("Show only a sub-rectangle of the source on this output",
		layout.tiledToggle.x, layout.tiledToggle.y,
		layout.tiledToggle.width, layout.tiledToggle.height);
	jp_tooltip::draw("Canvas pixels hidden by this monitor's frame, inset on "
		"all four sides",
		layout.fieldRects[LO_FIELD_BEZEL].x,
		layout.fieldRects[LO_FIELD_BEZEL].y,
		layout.fieldRects[LO_FIELD_BEZEL].width,
		layout.fieldRects[LO_FIELD_BEZEL].height);
	jp_tooltip::draw("Solve height from width so the tile matches this "
		"output's window aspect",
		layout.matchAspectButton.x, layout.matchAspectButton.y,
		layout.matchAspectButton.width, layout.matchAspectButton.height);
	jp_tooltip::draw("Set the tile's canvas-pixel size from this output's "
		"window resolution",
		layout.matchResolutionButton.x, layout.matchResolutionButton.y,
		layout.matchResolutionButton.width,
		layout.matchResolutionButton.height);
	jp_tooltip::draw("Fill every enabled output from a columns x rows grid",
		layout.splitButton.x, layout.splitButton.y,
		layout.splitButton.width, layout.splitButton.height);
	jp_tooltip::draw("SPATIAL derives every crop from measured mm positions, "
		"so gaps between scattered screens are handled for you. FREEFORM keeps "
		"hand authored crops.",
		layout.modeToggle.x, layout.modeToggle.y,
		layout.modeToggle.width, layout.modeToggle.height);
	jp_tooltip::draw("Switch the preview between the canvas and the room",
		layout.viewToggle.x, layout.viewToggle.y,
		layout.viewToggle.width, layout.viewToggle.height);
	jp_tooltip::draw("Replace the content with an alignment pattern: labelled "
		"rings at 2/4/6/8% and the screen number",
		layout.patternToggle.x, layout.patternToggle.y,
		layout.patternToggle.width, layout.patternToggle.height);
	jp_tooltip::draw("Drag a tile to move it, drag a corner to resize. Edges "
		"snap to the canvas and to other tiles; corner resizing keeps aspect.",
		layout.preview.x, layout.preview.y,
		layout.preview.width, layout.preview.height);
	(void)canvas;
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
		jp_button::draw(bounds, label, active, !disabled);
	};

	jp_screen::drawFrame(layout.panel, "LIVE OUTPUTS",
		"windows, monitors and wall");
	drawControl(layout.addButton, "+ ADD", false);
	drawControl(layout.deleteButton, "REMOVE", false, liveOutputs.empty());

	// Tab strip, following the house pattern used by JPboxgroup::drawTabs.
	const char *tabLabels[LO_TAB_COUNT] = {"OUTPUTS", "WALL"};
	for (int i = 0; i < (int)layout.tabs.size(); i++)
	{
		const ofRectangle &bounds = layout.tabs[i];
		const bool active = liveOutputTab == (LiveOutputSettingsTab)i;
		const bool hovered = bounds.inside(ofGetMouseX(), ofGetMouseY());
		ofSetColor(active ? ofColor(COL_ACCENT_GREEN, 235) :
			(hovered ? ofColor(COL_BG_HOVER, 235) :
				ofColor(COL_TAB_INACTIVE_BG, 225)));
		ofDrawRectRounded(bounds, 3.0f);
		ofNoFill();
		ofSetColor(active ? COL_ACCENT_GREEN_BR :
			ofColor(COL_BORDER_MUTED, 200));
		ofDrawRectRounded(bounds, 3.0f);
		ofFill();
		ofSetColor(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		font_p.drawString(tabLabels[i],
			bounds.getCenter().x - font_p.stringWidth(tabLabels[i]) * 0.5f,
			bounds.getBottom() - 7.0f);
		if (active)
		{
			ofSetColor(COL_ACCENT_GREEN_BR);
			ofDrawRectangle(bounds.x + 6.0f, bounds.getBottom() - 3.0f,
				bounds.width - 12.0f, 2.0f);
		}
		jp_tooltip::draw(i == LO_TAB_WALL ?
			"Tile outputs into one big canvas" :
			"Per output source, monitor and window",
			bounds.x, bounds.y, bounds.width, bounds.height);
	}

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
		ofSetColor(COL_ACCENT_CYAN);
		font_p.drawString(getLiveOutputDisplayName(selectedLiveOutput),
			editorX, layout.panel.y + jp_screen::kHeaderH + 21.0f);
		ofSetColor(COL_TEXT_MUTED);
		font_p.drawString("this output only",
			editorX + font_p.stringWidth(
				getLiveOutputDisplayName(selectedLiveOutput)) + 10.0f,
			layout.panel.y + jp_screen::kHeaderH + 21.0f);

		if (liveOutputTab == LO_TAB_WALL)
		{
			drawLiveOutputWallTab(layout, editorX);
			return;
		}

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
		string monitorLabel = config.virtualMonitor ? "(virtual screen)" :
			(config.monitorName.empty() ? "Select monitor" :
				config.monitorName);
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
				if (liveOutputFieldSelectAll)
					jp_textfield::drawSelection(font_p,
						liveOutputFieldText[focusedLiveOutputField],
						field.x + 7.0f, field.getBottom() - 8.0f,
						field.height - 8.0f);
				else
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
		if (config.enabled && monitorIndex < 0 && !config.virtualMonitor)
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
		// Surfaced here because openRenderWindow force-enables output 0, and a
		// cropped window would otherwise read as a bug.
		if (config.cropEnabled) status += "  |  Tiled";
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
			layout.panel.y + jp_screen::kHeaderH + 44.0f);
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
			else if (optionIndex == 0)
			{
				label = "(virtual screen)";
			}
			else
			{
				const LiveOutputMonitor &monitor =
					liveOutputMonitors[optionIndex - 1];
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

void ofApp::clearLiveOutputInteractionState()
{
	focusedLiveOutputField = -1;
	liveOutputFieldSelectAll = false;
	focusedSplitField = -1;
	splitFieldSelectAll = false;
	lastLiveOutputInputClick = -1;
	lastLiveOutputInputClickMs = 0;
	liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
	liveOutputMenuScroll = 0;
	wallDragMode = WALL_DRAG_NONE;
	wallDragOutput = -1;
	wallDragCorner = -1;
}

void ofApp::focusAdjacentLiveOutputField(bool backwards)
{
	vector<int> order;
	if (liveOutputTab == LO_TAB_OUTPUTS)
	{
		if (selectedLiveOutput < 0 ||
			selectedLiveOutput >= (int)liveOutputs.size() ||
			liveOutputs[selectedLiveOutput].config.fullscreen)
			return;
		order = {LO_FIELD_WIDTH, LO_FIELD_HEIGHT};
	}
	else
	{
		order = {LO_FIELD_CROP_X, LO_FIELD_CROP_Y, LO_FIELD_CROP_W,
			LO_FIELD_CROP_H, LO_FIELD_BEZEL, LO_FIELD_COUNT,
			LO_FIELD_COUNT + 1};
	}

	const int current = focusedSplitField >= 0 ?
		LO_FIELD_COUNT + focusedSplitField : focusedLiveOutputField;
	auto it = std::find(order.begin(), order.end(), current);
	if (it == order.end()) return;
	const int currentIndex = (int)std::distance(order.begin(), it);
	const int direction = backwards ? -1 : 1;
	const int count = (int)order.size();
	const int next = order[(currentIndex + direction + count) % count];

	if (focusedLiveOutputField >= 0) applyLiveOutputField();
	else if (focusedSplitField >= 0) applySplitField();

	if (next < LO_FIELD_COUNT)
	{
		focusedLiveOutputField = next;
		focusedSplitField = -1;
		liveOutputFieldCursor = (int)liveOutputFieldText[next].size();
		liveOutputFieldSelectAll = true;
		splitFieldSelectAll = false;
	}
	else
	{
		focusedSplitField = next - LO_FIELD_COUNT;
		focusedLiveOutputField = -1;
		splitFieldCursor = (int)splitFieldText[focusedSplitField].size();
		splitFieldSelectAll = true;
		liveOutputFieldSelectAll = false;
	}
}

void ofApp::setSelectedLiveOutput(int index)
{
	selectedLiveOutput = liveOutputs.empty() ? -1 :
		ofClamp(index, 0, (int)liveOutputs.size() - 1);
	clearLiveOutputInteractionState();
	initLiveOutputFields();
}

ofVec2f ofApp::getLiveOutputCanvasSize(const LiveOutputConfig &config) const
{
	// Measured against the texture actually sampled, not jp_constants: nothing
	// reallocates render FBOs at runtime, so after a render resize the real
	// canvas and jp_constants disagree.
	if (config.sourceMode == LIVE_OUTPUT_FIXED_BOX)
	{
		const ofVec2f size = boxes.getBoxFboSize(config.sourceBox);
		if (size.x >= 1.0f && size.y >= 1.0f) return size;
	}
	else
	{
		const ofVec2f size = boxes.getMasterCanvasSize();
		if (size.x >= 1.0f && size.y >= 1.0f) return size;
	}
	return ofVec2f(std::max(1, jp_constants::renderWidth),
		std::max(1, jp_constants::renderHeight));
}

ofVec2f ofApp::getLiveOutputTargetSize(
	const LiveOutputRuntime &output) const
{
	if (output.window)
	{
		const float width = output.window->getWidth();
		const float height = output.window->getHeight();
		if (width >= 1.0f && height >= 1.0f)
		{
			return ofVec2f(width, height);
		}
	}

	if (output.config.fullscreen && !output.config.virtualMonitor)
	{
		const int monitorIndex = resolveLiveOutputMonitor(output.config);
		if (monitorIndex >= 0 && monitorIndex < (int)liveOutputMonitors.size())
		{
			const LiveOutputMonitor &monitor = liveOutputMonitors[monitorIndex];
			return ofVec2f(std::max(1, monitor.width),
				std::max(1, monitor.height));
		}
	}

	return ofVec2f(std::max(1, output.config.width),
		std::max(1, output.config.height));
}

ofRectangle ofApp::getLiveOutputSourceRect(const LiveOutputConfig &config) const
{
	const ofVec2f canvas = getLiveOutputCanvasSize(config);
	ofRectangle rect(config.cropX * canvas.x, config.cropY * canvas.y,
		config.cropW * canvas.x, config.cropH * canvas.y);
	const float bezel = (float)config.bezelPx;
	const float left = ofClamp(rect.x + bezel, 0.0f,
		std::max(0.0f, canvas.x - 1.0f));
	const float top = ofClamp(rect.y + bezel, 0.0f,
		std::max(0.0f, canvas.y - 1.0f));
	const float right = ofClamp(rect.getRight() - bezel, left + 1.0f,
		canvas.x);
	const float bottom = ofClamp(rect.getBottom() - bezel, top + 1.0f,
		canvas.y);
	rect.set(left, top, right - left, bottom - top);
	return rect;
}

void ofApp::initLiveOutputFields()
{
	if (selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		for (int i = 0; i < LO_FIELD_COUNT; i++)
			liveOutputFieldText[i].clear();
		focusedLiveOutputField = -1;
		return;
	}
	const LiveOutputConfig &config =
		liveOutputs[selectedLiveOutput].config;
	const ofVec2f canvas = getLiveOutputCanvasSize(config);
	liveOutputFieldText[LO_FIELD_WIDTH] = ofToString(config.width);
	liveOutputFieldText[LO_FIELD_HEIGHT] = ofToString(config.height);
	// Crop is stored normalized but edited in canvas pixels: the numeric text
	// field only accepts digits, so a decimal could not be typed.
	if (wallMode == WALL_MODE_SPATIAL)
	{
		// The four fields become the measured glass rect, in mm.
		liveOutputFieldText[LO_FIELD_CROP_X] =
			ofToString((int)std::lround(config.physX));
		liveOutputFieldText[LO_FIELD_CROP_Y] =
			ofToString((int)std::lround(config.physY));
		liveOutputFieldText[LO_FIELD_CROP_W] =
			ofToString((int)std::lround(config.physW));
		liveOutputFieldText[LO_FIELD_CROP_H] =
			ofToString((int)std::lround(config.physH));
	}
	else
	{
		liveOutputFieldText[LO_FIELD_CROP_X] =
			ofToString((int)std::lround(config.cropX * canvas.x));
		liveOutputFieldText[LO_FIELD_CROP_Y] =
			ofToString((int)std::lround(config.cropY * canvas.y));
		liveOutputFieldText[LO_FIELD_CROP_W] =
			ofToString((int)std::lround(config.cropW * canvas.x));
		liveOutputFieldText[LO_FIELD_CROP_H] =
			ofToString((int)std::lround(config.cropH * canvas.y));
	}
	liveOutputFieldText[LO_FIELD_BEZEL] = ofToString(std::abs(config.bezelPx));
	if (splitFieldText[0].empty()) splitFieldText[0] = "2";
	if (splitFieldText[1].empty()) splitFieldText[1] = "1";
}

void ofApp::applyLiveOutputField()
{
	if (focusedLiveOutputField < 0 ||
		selectedLiveOutput < 0 ||
		selectedLiveOutput >= (int)liveOutputs.size())
	{
		focusedLiveOutputField = -1;
		liveOutputFieldSelectAll = false;
		return;
	}
	LiveOutputRuntime &output = liveOutputs[selectedLiveOutput];
	LiveOutputConfig &config = output.config;
	const int field = focusedLiveOutputField;
	const int raw = ofToInt(liveOutputFieldText[field]);
	const ofVec2f canvas = getLiveOutputCanvasSize(config);
	// Per field clamps. The single 64..16384 clamp this used to apply to every
	// field would have turned a crop of 0 into 64.
	switch (field)
	{
	case LO_FIELD_WIDTH:
		config.width = ofClamp(raw, 64, 16384);
		break;
	case LO_FIELD_HEIGHT:
		config.height = ofClamp(raw, 64, 16384);
		break;
	case LO_FIELD_CROP_W:
		if (wallMode == WALL_MODE_SPATIAL)
		{
			config.physW = std::max(0, raw);
			applySpatialLayout();
			break;
		}
		config.cropW = ofClamp(raw, 1, (int)canvas.x) / (double)canvas.x;
		config.cropX = std::min(config.cropX, 1.0 - config.cropW);
		break;
	case LO_FIELD_CROP_H:
		if (wallMode == WALL_MODE_SPATIAL)
		{
			config.physH = std::max(0, raw);
			applySpatialLayout();
			break;
		}
		config.cropH = ofClamp(raw, 1, (int)canvas.y) / (double)canvas.y;
		config.cropY = std::min(config.cropY, 1.0 - config.cropH);
		break;
	case LO_FIELD_CROP_X:
		if (wallMode == WALL_MODE_SPATIAL)
		{
			// Positions may be negative: the origin is wherever you started
			// measuring from, not necessarily the leftmost screen.
			config.physX = raw;
			applySpatialLayout();
			break;
		}
		config.cropX = ofClamp(raw, 0,
			std::max(0, (int)std::lround(canvas.x * (1.0 - config.cropW)))) /
			(double)canvas.x;
		break;
	case LO_FIELD_CROP_Y:
		if (wallMode == WALL_MODE_SPATIAL)
		{
			config.physY = raw;
			applySpatialLayout();
			break;
		}
		config.cropY = ofClamp(raw, 0,
			std::max(0, (int)std::lround(canvas.y * (1.0 - config.cropH)))) /
			(double)canvas.y;
		break;
	case LO_FIELD_BEZEL:
	{
		// Clamped against the crop, so the number shown is the number applied.
		const int limit = std::max(0, (int)(std::min(
			config.cropW * canvas.x, config.cropH * canvas.y) / 2.0) - 1);
		// The field holds the magnitude; the sign button owns the direction.
		const int magnitude = ofClamp(std::abs(raw), 0, limit);
		config.bezelPx = config.bezelPx < 0 ? -magnitude : magnitude;
		break;
	}
	default:
		break;
	}
	focusedLiveOutputField = -1;
	liveOutputFieldSelectAll = false;
	initLiveOutputFields();
	// Only the window size fields touch the window.
	if ((field == LO_FIELD_WIDTH || field == LO_FIELD_HEIGHT) &&
		output.window && !config.fullscreen)
	{
		output.window->setWindowShape(config.width, config.height);
	}
	saveSettings();
}

void ofApp::applySplitField()
{
	if (focusedSplitField < 0)
	{
		focusedSplitField = -1;
		splitFieldSelectAll = false;
		return;
	}
	splitFieldText[focusedSplitField] = ofToString(
		ofClamp(ofToInt(splitFieldText[focusedSplitField]), 1, 16));
	focusedSplitField = -1;
	splitFieldSelectAll = false;
}

void ofApp::applyWallSplit()
{
	const int cols = ofClamp(ofToInt(splitFieldText[0]), 1, 16);
	const int rows = ofClamp(ofToInt(splitFieldText[1]), 1, 16);
	// Fills every ENABLED output in reading order, left to right then top to
	// bottom, and leaves disabled ones alone.
	int cell = 0;
	for (LiveOutputRuntime &output : liveOutputs)
	{
		if (!output.config.enabled) continue;
		if (cell >= cols * rows) break;
		const int col = cell % cols;
		const int row = cell / cols;
		output.config.cropEnabled = true;
		output.config.cropW = 1.0 / cols;
		output.config.cropH = 1.0 / rows;
		output.config.cropX = col / (double)cols;
		output.config.cropY = row / (double)rows;
		cell++;
	}
	initLiveOutputFields();
	saveSettings();
}

void ofApp::setLiveOutputTab(LiveOutputSettingsTab tab)
{
	if (liveOutputTab == tab) return;
	// Commit rather than discard: losing a typed value on a tab click is the
	// surprising behaviour.
	if (focusedLiveOutputField >= 0) applyLiveOutputField();
	if (focusedSplitField >= 0) applySplitField();
	// Mandatory. The popup rects are anchored to the source/monitor buttons,
	// which do not exist on the wall tab, and the popup hit test runs first in
	// the click handler - a stale popup would swallow the next click.
	clearLiveOutputInteractionState();
	liveOutputTab = tab;
	settingsScroll = 0.0f;
	clampSettingsScroll();
}

bool ofApp::handleLiveOutputSettingsClick(int x, int y, int button)
{
	if (button != OF_MOUSE_BUTTON_LEFT)
	{
		return false;
	}
	LiveOutputSettingsLayout layout = getLiveOutputSettingsLayout();
	auto inputWasDoubleClicked = [&](int inputId) {
		const uint64_t now = ofGetElapsedTimeMillis();
		const bool doubleClicked = inputId == lastLiveOutputInputClick &&
			now - lastLiveOutputInputClickMs <= 350;
		lastLiveOutputInputClick = inputId;
		lastLiveOutputInputClickMs = now;
		return doubleClicked;
	};

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
				clearLiveOutputInteractionState();
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
			else if (optionIndex == 0)
			{
				// Stand-in for hardware that is not attached. monitorName is
				// kept so switching back to the real screen is one click.
				output.config.virtualMonitor = true;
				output.config.hasPosition = false;
				requestLiveOutputRecreate(selectedLiveOutput);
				updateLiveOutputs();
			}
			else if (optionIndex >= 1 &&
				optionIndex <= (int)liveOutputMonitors.size())
			{
				const LiveOutputMonitor &monitor =
					liveOutputMonitors[optionIndex - 1];
				output.config.virtualMonitor = false;
				output.config.monitorName = monitor.name;
				output.config.monitorIndex = monitor.index;
				output.config.hasPosition = false;
				requestLiveOutputRecreate(selectedLiveOutput);
				updateLiveOutputs();
			}
			clearLiveOutputInteractionState();
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (layout.popup.inside(x, y))
		{
			return true;
		}
		clearLiveOutputInteractionState();
		layout = getLiveOutputSettingsLayout();
	}

	for (int i = 0; i < (int)layout.tabs.size(); i++)
	{
		if (!layout.tabs[i].inside(x, y)) continue;
		setLiveOutputTab((LiveOutputSettingsTab)i);
		return true;
	}

	// Commit when leaving the active field, including when moving directly to
	// another input in the same row.
	const bool clickedFocusedSplit = focusedSplitField == 0 ?
		layout.splitColsField.inside(x, y) :
		(focusedSplitField == 1 && layout.splitRowsField.inside(x, y));
	if (focusedSplitField >= 0 && !clickedFocusedSplit)
	{
		applySplitField();
	}
	bool clickedFocusedLive = false;
	if (focusedLiveOutputField >= 0)
	{
		if (liveOutputTab == LO_TAB_OUTPUTS)
			clickedFocusedLive = focusedLiveOutputField == LO_FIELD_WIDTH ?
				layout.widthField.inside(x, y) :
				layout.heightField.inside(x, y);
		else
			clickedFocusedLive =
				layout.fieldRects[focusedLiveOutputField].inside(x, y);
	}
	if (focusedLiveOutputField >= 0 && !clickedFocusedLive)
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
	if (liveOutputTab == LO_TAB_WALL)
	{
		LiveOutputConfig &config = output.config;
		if (layout.tiledToggle.inside(x, y))
		{
			config.cropEnabled = !config.cropEnabled;
			focusedLiveOutputField = -1;
			applySpatialLayout();
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (layout.modeToggle.inside(x, y))
		{
			wallMode = wallMode == WALL_MODE_SPATIAL ?
				WALL_MODE_FREEFORM : WALL_MODE_SPATIAL;
			// Entering SPATIAL seeds unmeasured screens from their current crop
			// so the switch is not destructive: 1 canvas px becomes 1 mm, which
			// you then correct with the real tape measurements.
			if (wallMode == WALL_MODE_SPATIAL)
			{
				for (LiveOutputRuntime &other : liveOutputs)
				{
					LiveOutputConfig &oc = other.config;
					if (!oc.cropEnabled) continue;
					if (oc.physW > 0.0 && oc.physH > 0.0) continue;
					const ofVec2f oCanvas = getLiveOutputCanvasSize(oc);
					oc.physX = oc.cropX * oCanvas.x;
					oc.physY = oc.cropY * oCanvas.y;
					oc.physW = oc.cropW * oCanvas.x;
					oc.physH = oc.cropH * oCanvas.y;
				}
				applySpatialLayout();
			}
			focusedLiveOutputField = -1;
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (layout.viewToggle.inside(x, y))
		{
			wallRoomView = !wallRoomView;
			return true;
		}
		if (layout.patternToggle.inside(x, y))
		{
			config.testPattern = !config.testPattern;
			saveSettings();
			return true;
		}
		if (config.cropEnabled && layout.bezelSignButton.inside(x, y))
		{
			config.bezelPx = -config.bezelPx;
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (layout.splitColsField.inside(x, y) ||
			layout.splitRowsField.inside(x, y))
		{
			focusedSplitField = layout.splitColsField.inside(x, y) ? 0 : 1;
			splitFieldCursor = (int)splitFieldText[focusedSplitField].size();
			splitFieldSelectAll = inputWasDoubleClicked(
				LO_FIELD_COUNT + focusedSplitField);
			focusedLiveOutputField = -1;
			liveOutputFieldSelectAll = false;
			return true;
		}
		if (layout.splitButton.inside(x, y))
		{
			if (focusedSplitField >= 0) applySplitField();
			applyWallSplit();
			return true;
		}
		if (config.cropEnabled && wallMode == WALL_MODE_FREEFORM &&
			layout.matchAspectButton.inside(x, y))
		{
			// Solve crop height from crop width against the window aspect, so
			// the tile stops being stretched.
			const ofVec2f canvas = getLiveOutputCanvasSize(config);
			const ofVec2f targetSize = getLiveOutputTargetSize(output);
			const float windowAspect = targetSize.x / targetSize.y;
			const double wantH = (config.cropW * canvas.x) /
				std::max(0.0001f, windowAspect) / canvas.y;
			config.cropH = ofClamp(wantH, 1.0 / canvas.y, 1.0);
			config.cropY = std::min(config.cropY, 1.0 - config.cropH);
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (config.cropEnabled && wallMode == WALL_MODE_FREEFORM &&
			layout.matchResolutionButton.inside(x, y))
		{
			const ofVec2f canvas = getLiveOutputCanvasSize(config);
			const ofVec2f targetSize = getLiveOutputTargetSize(output);
			// Use an exact 1:1 tile when it fits. If the output resolution is
			// larger than the source canvas, scale both dimensions together.
			const double fitScale = std::min(1.0, std::min(
				(double)canvas.x / targetSize.x,
				(double)canvas.y / targetSize.y));
			const double pixelW = std::max(1.0,
				(double)std::lround(targetSize.x * fitScale));
			const double pixelH = std::max(1.0,
				(double)std::lround(targetSize.y * fitScale));
			config.cropW = pixelW / canvas.x;
			config.cropH = pixelH / canvas.y;
			config.cropX = std::min(config.cropX, 1.0 - config.cropW);
			config.cropY = std::min(config.cropY, 1.0 - config.cropH);
			initLiveOutputFields();
			saveSettings();
			return true;
		}
		if (layout.preview.inside(x, y))
		{
			// In SPATIAL the crops are derived from the measured layout, so a
			// drag would be overwritten on the next recompute. Selecting still
			// works; editing is via the mm fields.
			if (wallMode == WALL_MODE_SPATIAL || wallRoomView)
			{
				for (int i = (int)liveOutputs.size() - 1; i >= 0; i--)
				{
					if (!liveOutputs[i].config.cropEnabled) continue;
					const ofRectangle hitRect = wallRoomView ?
						getRoomScreenRect(layout, i) :
						getWallTileRect(layout, i);
					if (!hitRect.inside(x, y)) continue;
					if (i != selectedLiveOutput) setSelectedLiveOutput(i);
					break;
				}
				return true;
			}
			// Corner handles of the SELECTED tile win, so a handle sitting on
			// top of a neighbouring tile is still grabbable.
			if (config.cropEnabled)
			{
				const ofRectangle tile =
					getWallTileRect(layout, selectedLiveOutput);
				const ofVec2f corners[4] = {
					ofVec2f(tile.x, tile.y),
					ofVec2f(tile.getRight(), tile.y),
					ofVec2f(tile.x, tile.getBottom()),
					ofVec2f(tile.getRight(), tile.getBottom())};
				for (int i = 0; i < 4; i++)
				{
					if (corners[i].distance(ofVec2f(x, y)) > 9.0f) continue;
					wallDragMode = WALL_DRAG_RESIZE;
					wallDragOutput = selectedLiveOutput;
					wallDragCorner = i;
					wallDragStartMouse.set(x, y);
					wallDragStartCrop.set(config.cropX, config.cropY,
						config.cropW, config.cropH);
					return true;
				}
			}
			// Then tile interiors. Backwards so the last drawn wins, except the
			// selected one always gets first refusal.
			int hit = -1;
			if (config.cropEnabled &&
				getWallTileRect(layout, selectedLiveOutput).inside(x, y))
			{
				hit = selectedLiveOutput;
			}
			for (int i = (int)liveOutputs.size() - 1; i >= 0 && hit < 0; i--)
			{
				if (!liveOutputs[i].config.cropEnabled) continue;
				if (getWallTileRect(layout, i).inside(x, y)) hit = i;
			}
			if (hit >= 0)
			{
				// Dragging a tile also selects its output, so the preview is a
				// picker too.
				if (hit != selectedLiveOutput) setSelectedLiveOutput(hit);
				const LiveOutputConfig &hitConfig = liveOutputs[hit].config;
				wallDragMode = WALL_DRAG_MOVE;
				wallDragOutput = hit;
				wallDragCorner = -1;
				wallDragStartMouse.set(x, y);
				wallDragStartCrop.set(hitConfig.cropX, hitConfig.cropY,
					hitConfig.cropW, hitConfig.cropH);
				return true;
			}
			return true;
		}
		if (config.cropEnabled)
		{
			// Deliberately not gated on fullscreen, unlike the window size
			// fields: a wall is normally run fullscreen.
			const int wallFields[] = {LO_FIELD_CROP_X, LO_FIELD_CROP_Y,
				LO_FIELD_CROP_W, LO_FIELD_CROP_H, LO_FIELD_BEZEL};
			for (int field : wallFields)
			{
				if (!layout.fieldRects[field].inside(x, y)) continue;
				focusedLiveOutputField = field;
				liveOutputFieldCursor =
					(int)liveOutputFieldText[field].size();
				liveOutputFieldSelectAll = inputWasDoubleClicked(field);
				focusedSplitField = -1;
				splitFieldSelectAll = false;
				focusedOptionsField = -1;
				return true;
			}
		}
		return layout.panel.inside(x, y);
	}

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
		clearLiveOutputInteractionState();
		liveOutputMenu = LIVE_OUTPUT_MENU_SOURCE;
		return true;
	}
	if (layout.monitorButton.inside(x, y))
	{
		refreshLiveOutputMonitors();
		clearLiveOutputInteractionState();
		liveOutputMenu = LIVE_OUTPUT_MENU_MONITOR;
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
		liveOutputFieldSelectAll =
			inputWasDoubleClicked(focusedLiveOutputField);
		focusedSplitField = -1;
		splitFieldSelectAll = false;
		focusedOptionsField = -1;
		return true;
	}
	return layout.panel.inside(x, y);
}

float ofApp::getLiveOutputPanelHeight(LiveOutputSettingsTab tab) const
{
	// The general settings panel's height, which the outputs tab matches.
	const float generalPanelH =
		jp_screen::kHeaderH + (FIELD_OSC_IP_OUT + 6) * 40.0f + 25.0f;
	if (tab != LO_TAB_WALL) return generalPanelH;
	// The wall tab carries a room/canvas preview under its fields.
	return std::max(generalPanelH, 640.0f);
}

float ofApp::getSettingsContentHeight() const
{
	const float generalPanelH = getLiveOutputPanelHeight(LO_TAB_OUTPUTS);
	const float outputPanelH = getLiveOutputPanelHeight(liveOutputTab);
	if (ofGetWidth() >= 1080)
	{
		// Side by side: the taller of the two panels sets the content height.
		return 44.0f + std::max(generalPanelH, outputPanelH) + 30.0f;
	}
	return 44.0f + generalPanelH + 16.0f + outputPanelH + 30.0f;
}

void ofApp::clampSettingsScroll()
{
	// Gated on whether the content actually overflows, not on the column count:
	// the wall tab is taller than the general panel, so two column mode can
	// overflow too and used to have scrolling hard disabled.
	const float maxScroll = std::max(0.0f,
		getSettingsContentHeight() - ofGetHeight());
	settingsScroll = ofClamp(settingsScroll, 0.0f, maxScroll);
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
	clearLiveOutputInteractionState();
	refreshLiveOutputMonitors();
	bool reopenConnectedOutput = false;
	for (LiveOutputRuntime &output : liveOutputs)
	{
		// A virtual screen needs no monitor to resolve, so it must be allowed
		// to retry here too or it never reopens after a failed attempt.
		if (output.config.enabled && !output.window &&
			(output.config.virtualMonitor ||
				resolveLiveOutputMonitor(output.config) >= 0))
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
	// Render size may have changed and the wall crop fields are displayed in
	// canvas pixels, so they have to be recomputed.
	initLiveOutputFields();
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
	const float searchY = panelY + jp_screen::kHeaderH;
	const float searchH = 26.0f;
	const float buttonGap = 8.0f;
	const float footerW = (contentW - buttonGap * 2.0f) / 3.0f;

	layout.panel.set(panelX, panelY, panelW, panelH);
	layout.titleBaseline = panelY + jp_screen::kTitleBaseline;
	layout.hintBaseline = panelY + jp_screen::kSubtitleBaseline;
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

	// The mouse/frame freezing now lives in jp_shader_globals::apply via
	// ctx.liveMouse, so the preview no longer computes its own copies.
	const int previewFrame = useLiveMouse ? ofGetFrameNum() : 0;

	previewFbo.begin();
	ofClear(0, 0, 0, 255);
	previewShader.begin();
	{
		// Same globals every box gets, from the one shared helper. The preview
		// freezes the mouse and frame counter so a thumbnail does not depend on
		// where the pointer happens to be.
		JPShaderGlobalsCtx ctx;
		ctx.width = previewFbo.getWidth();
		ctx.height = previewFbo.getHeight();
		ctx.boxFrameNum = previewFrame;
		ctx.liveMouse = useLiveMouse;
		jp_shader_globals::apply(previewShader, ctx);
	}
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
							uname == "texture" ||
							// The audio globals too, or they would show up as
							// randomisable sliders in the preview.
							jp_shader_globals::isGlobalName(uname)) continue;
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

	int totalShaders = 0;
	for (const ShaderFolder &folder : shaderFolders) {
		if (!folder.isFavorites) totalShaders += (int)folder.shaders.size();
	}
	const string title = language == 0 ?
		"IMPORT  |  " + ofToString(totalShaders) + " shaders" :
		"IMPORTAR  |  " + ofToString(totalShaders) + " shaders";
	// Half width so the node canvas stays visible behind it, but otherwise the
	// same frame, border, title and subtitle as every other screen.
	jp_screen::drawFrame(layout.panel, title, language == 0 ?
		"Up/Down navigate | double click/Enter to load" :
		"Arriba/Abajo navegar | doble clic/Enter para cargar");

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

	const jp_textfield::Window window = jp_textfield::visibleWindow(
		font_p, shaderSearchText, shaderSearchCursor, textMaxW);
	const int displayStart = window.start;
	const string displayText =
		shaderSearchText.substr(window.start, window.end - window.start);
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
		jp_button::draw(button, label, false, enabled, border);
		return;
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

	// ONE ESC RULE FOR THE WHOLE PROGRAM: dismiss the topmost surface, one
	// layer per press - field edit, then dropdown, then panel, then modal.
	// When nothing is open it does nothing. It never changes screen and never
	// quits, so ESC is always safe. This replaces eight separate handlers whose
	// meaning depended on where they happened to sit in this function.
	if (key == OF_KEY_ESC) {
		surfaces.closeTopmost();
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
		if (key == OF_KEY_TAB)
		{
			focusAdjacentLiveOutputField(ofGetKeyPressed(OF_KEY_SHIFT));
			return;
		}
		if (key == 1 || ((key == 'a' || key == 'A') &&
			ofGetKeyPressed(OF_KEY_CONTROL)))
		{
			liveOutputFieldSelectAll = true;
			return;
		}
		if (key == OF_KEY_RETURN || key == '\r')
		{
			applyLiveOutputField();
			return;
		}
		const bool signedPosition = wallMode == WALL_MODE_SPATIAL &&
			(focusedLiveOutputField == LO_FIELD_CROP_X ||
			 focusedLiveOutputField == LO_FIELD_CROP_Y);
		jp_textfield::handleKey(
			liveOutputFieldText[focusedLiveOutputField],
			liveOutputFieldCursor, key, true, signedPosition,
			&liveOutputFieldSelectAll);
		return;
	}
	if (focusedSplitField >= 0)
	{
		if (key == OF_KEY_TAB)
		{
			focusAdjacentLiveOutputField(ofGetKeyPressed(OF_KEY_SHIFT));
			return;
		}
		if (key == 1 || ((key == 'a' || key == 'A') &&
			ofGetKeyPressed(OF_KEY_CONTROL)))
		{
			splitFieldSelectAll = true;
			return;
		}
		if (key == OF_KEY_RETURN || key == '\r')
		{
			applySplitField();
			return;
		}
		jp_textfield::handleKey(splitFieldText[focusedSplitField],
			splitFieldCursor, key, true, false, &splitFieldSelectAll);
		return;
	}
	// IMPORT used to swallow every key unconditionally, so 1-5 and every other
	// shortcut were dead on this screen and ESC was the only way out. Typing
	// only wins while the search field actually has focus; ESC releases it and
	// the global keymap comes back.
	if (pantallaActiva == SHADER_INDEX) {
		if (key == OF_KEY_UP || key == OF_KEY_DOWN) {
			moveShaderSelection(key == OF_KEY_DOWN ? 1 : -1);
			return;
		}
		if (key == OF_KEY_RETURN || key == '\r') {
			loadSelectedShaderBox();
			return;
		}
		if (shaderSearchFocused &&
			jp_textfield::handleKey(shaderSearchText, shaderSearchCursor, key)) {
			shaderScroll = 0;
			clampShaderScroll(getShaderBrowserLayout());
			return;
		}
	}

	if (key == '6') {
		pantallaActiva = MIDI_KEYMAP;
		focusedOptionsField = -1;
		clearLiveOutputInteractionState();
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '1') {
		pantallaActiva = NODOS;
		focusedOptionsField = -1;
		clearLiveOutputInteractionState();
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
		clearLiveOutputInteractionState();
		if (shaderEditor.isVisible()) shaderEditor.setVisible(false);
	}

	if (key == '4') {
		pantallaActiva = SHADER_INDEX;
		focusedOptionsField = -1;
		clearLiveOutputInteractionState();
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
		clearLiveOutputInteractionState();
		shaderEditor.setVisible(true);
	}

	// H toggles hit-box visualization in shader index
	if (key == 'h' && pantallaActiva == SHADER_INDEX) {
		showShaderHitBoxes = !showShaderHitBoxes;
		return;
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
		if (key == 'C') {
			boxes.addBox("kinect2");
		}
		if (key == 'P') {
			boxes.addBox("pointercloud");
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
	if (pantallaActiva == TUTORIAL && helpScrollbarDragging)
	{
		const HelpLayout L = getHelpLayout();
		if (!L.showScrollbar)
		{
			helpScrollbarDragging = false;
			helpScroll = 0.0f;
			return;
		}
		const float travel = std::max(1.0f,
			L.scrollTrack.height - L.scrollThumb.height);
		const float thumbY = ofClamp(
			y - helpScrollbarDragOffset,
			L.scrollTrack.y, L.scrollTrack.y + travel);
		helpScroll = ((thumbY - L.scrollTrack.y) / travel) * L.maxScroll;
		return;
	}
	midiKeymap.mouseDragged(x, y, button);

	// Forward to shader editor for selection dragging, on its screen only.
	if (pantallaActiva == EDITOR && shaderEditor.isVisible()) {
		shaderEditor.mouseDragged(x, y, button);
	}

	if (pantallaActiva == OPCIONES) {
		if (audioGateDragging) {
			const SettingsLayout L = getSettingsLayout();
			const float t = ofClamp(((float)x - L.audioGateSlider.x) /
				L.audioGateSlider.width, 0.0f, 1.0f);
			jp_audio::setNoiseGate(t * 0.25f);
			return;
		}
		if (audioGainDragging) {
			const SettingsLayout L = getSettingsLayout();
			const float t = ofClamp(
				((float)x - L.audioGainSlider.x) / L.audioGainSlider.width,
				0.0f, 1.0f);
			jp_audio::setGain(ofLerp(0.05f, 8.0f, t));
			return;
		}
		if (handleLiveOutputSettingsDrag(x, y, button)) {
			return;
		}
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
		// A modal has to swallow everything it does not itself handle. Without
		// this, a click inside the box but off a button fell through to the
		// MIDI panel, the top bar and the node canvas - you could drag boxes
		// and switch screens straight through the "modal".
		return;
	}

	// Clicking away commits an inline tab rename. It used to have no exit but
	// ENTER/ESC, so a stray click left it open swallowing every key. Clicks on
	// the tab being renamed are left alone so the field stays editable.
	if (boxes.tabRenaming &&
		boxes.getTabAtScreenPos(x, y) != boxes.tabRenameTabIndex) {
		boxes.commitTabRename();
	}

	if (pantallaActiva == MIDI_KEYMAP &&
		midiKeymap.mousePressed(x, y, button)) {
		return;
	}
	if (midiKeymap.captureFunctionClick(x, y, button)) {
		return;
	}

	// Screen tab click handling
	{
		int tabScreen = getScreenTabAtPos(x, y);
			if (tabScreen == kCuePanelBarItem) {
				if (boxes.getCuePreviewBox() != nullptr) {
					boxes.setCueBoxByIndex(-1);
				} else {
					boxes.setCueBoxByIndex(
						boxes.getCueEntryIndexForCurrentView());
				}
				return;
			}
			if (tabScreen == kMappingPanelBarItem) {
				boxes.toggleMappingEdit();
				return;
			}
			if (tabScreen >= 0) {
				if (pantallaActiva != tabScreen) {
					pantallaActiva = tabScreen;
					focusedOptionsField = -1;
					clearLiveOutputInteractionState();

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
		// Same layout the draw pass used - not a second copy of the numbers.
		// This was a hardcoded rect at panelY+13 with height 22 against a
		// button drawn at f.y+10 with height 24, so its top 3px was dead.
		const HelpLayout L = getHelpLayout();
		if (button == OF_MOUSE_BUTTON_LEFT &&
			L.langBtn.inside((float)x, (float)y)) {
			language = (language == 0) ? 1 : 0;
			helpScroll = 0.0f;
			helpScrollbarDragging = false;
			helpCacheLang = -1;
			return;
		}
		if (button == OF_MOUSE_BUTTON_LEFT && L.showScrollbar &&
			L.scrollThumb.inside((float)x, (float)y)) {
			helpScrollbarDragging = true;
			helpScrollbarDragOffset = y - L.scrollThumb.y;
			return;
		}
		// Click anywhere on the scrollbar track jumps there.
		if (button == OF_MOUSE_BUTTON_LEFT && L.showScrollbar &&
			L.scrollTrack.inside((float)x, (float)y)) {
			const float t = ofClamp(
				(y - L.scrollTrack.y - L.scrollThumb.height * 0.5f) /
				std::max(1.0f, L.scrollTrack.height - L.scrollThumb.height),
				0.0f, 1.0f);
			helpScroll = t * L.maxScroll;
			helpScrollbarDragging = true;
			helpScrollbarDragOffset = L.scrollThumb.height * 0.5f;
			return;
		}
	}
	if (pantallaActiva == OPCIONES) {
		// Audio first: an open device dropdown overlays the rows beneath it.
		if (handleAudioSettingsClick(x, y)) {
			return;
		}
		if (handleLiveOutputSettingsClick(x, y, button)) {
			return;
		}
		// Same layout the draw pass used - not a second copy of the numbers.
		const SettingsLayout L = getSettingsLayout();
		const float fieldX = L.fields[0].x;
		const float fieldW = L.fields[0].width;
		const float rowH = L.rowH;
		const float actionBtnW = L.autoTapButton.width;

		// Check if clicked inside any text field
		focusedOptionsField = -1;
		for (int i = 0; i < FIELD_OSC_IP_OUT; i++) {
			const float rowY = L.fields[i].y;
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
			const float rowY = L.spoutToggle.y;
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
			const float rowY = L.ndiToggle.y;
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
			const float rowY = L.fields[FIELD_OSC_IP_OUT].y;
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
			const float rowY = L.fields[FIELD_DEFAULT_COMPO].y;
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
			const float rowY = L.saveButton.y;
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
		if (!layout.panel.inside(x, y)) return;
		shaderSearchFocused = layout.search.inside(x, y);

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
			const jp_textfield::Window window = jp_textfield::visibleWindow(
				font_p, shaderSearchText, shaderSearchCursor, textMaxW);
			const string displayText =
				shaderSearchText.substr(window.start, window.end - window.start);
			shaderSearchCursor = ofClamp(window.start +
				jp_textfield::cursorFromX(font_p, displayText, textX, (float)x),
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
	helpScrollbarDragging = false;
	helpCacheLang = -1;
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
	if (audioGateDragging) {
		audioGateDragging = false;
		saveSettings();
	}
	if (audioGainDragging) {
		audioGainDragging = false;
		saveSettings();
	}
	if (helpScrollbarDragging)
	{
		helpScrollbarDragging = false;
		return;
	}
	midiKeymap.mouseReleased(x, y, button);
	// Gate on the screen that owns the drag. This ran everywhere, so a drag
	// started on the node canvas still completed - and still wrote settings -
	// after the user had switched to HELP or SETTINGS mid-gesture.
	if (pantallaActiva == OPCIONES && handleLiveOutputSettingsRelease(x, y, button)) {
		return;
	}
	if (pantallaActiva == NODOS) {
		if (boxes.update_mappingMouseReleased(button)) {
			saveSettings();
			return;
		}
		if (boxes.update_cueMouseReleased(button)) {
			saveSettings();
			return;
		}
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
	// Forward to the shader editor only while its screen is actually showing.
	if (pantallaActiva == EDITOR && shaderEditor.isVisible()) {
		shaderEditor.mouseScrolled(x, y, scrollX, scrollY);
		return;
	}
	if (pantallaActiva == TUTORIAL) {
		// Clamp against the layout's own measurement rather than numbers the
		// last draw pass happened to leave behind.
		const HelpLayout L = getHelpLayout();
		helpScroll = ofClamp(helpScroll - scrollY * 34.0f, 0.0f, L.maxScroll);
		return;
	}
	if (pantallaActiva == MIDI_KEYMAP) {
		midiKeymap.mouseScrolled(x, y, scrollX, scrollY);
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
					(int)liveOutputMonitors.size() + 1;
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
	// Before the save, not after: the bounding box just changed, so every
	// derived crop has too and the file must hold the new ones.
	applySpatialLayout();
	initLiveOutputFields();
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
	clearLiveOutputInteractionState();
	applySpatialLayout();
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
	// A virtual screen stands in for hardware that is not here: it opens as an
	// ordinary window at its configured resolution so a whole installation can
	// be built and rehearsed on one machine, then deployed.
	const bool virtualScreen = output.config.virtualMonitor;
	int resolvedMonitor = virtualScreen ? -1 :
		resolveLiveOutputMonitor(output.config);
	if (!virtualScreen && resolvedMonitor < 0)
	{
		return;
	}
	if (virtualScreen)
	{
		// Land it on the primary display, or on whatever exists.
		for (int i = 0; i < (int)liveOutputMonitors.size(); i++)
		{
			if (liveOutputMonitors[i].primary) { resolvedMonitor = i; break; }
		}
		if (resolvedMonitor < 0 && !liveOutputMonitors.empty())
			resolvedMonitor = 0;
		if (resolvedMonitor < 0) return;
	}
	const LiveOutputMonitor &monitor = liveOutputMonitors[resolvedMonitor];
	if (!virtualScreen) output.config.monitorIndex = monitor.index;
	// Never fullscreen a virtual screen: it would swallow a real display.
	const bool wantFullscreen = output.config.fullscreen && !virtualScreen;

	ofGLFWWindowSettings settings;
	settings.setGLVersion(3, 2);
	settings.shareContextWith = mainWindow;
	settings.monitor = monitor.index;
	settings.resizable = !wantFullscreen;
	settings.title = "Guipper - " + getLiveOutputDisplayName(index) +
		(virtualScreen ? " (virtual)" : "");
	if (wantFullscreen)
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
	auto floatValue = [](const ofXml &parent, const string &name,
		double fallback) {
		auto child = parent.getChild(name);
		if (!child) return fallback;
		const double value = child.getDoubleValue();
		// A hand edited or truncated file must not hand a NaN to the crop
		// maths, where it would reach a division and emit NaN vertices.
		return std::isfinite(value) ? value : fallback;
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
	auto wallModeChild = settings.getChild("wall_mode");
	wallMode = (wallModeChild && wallModeChild.getValue() == "spatial") ?
		WALL_MODE_SPATIAL : WALL_MODE_FREEFORM;

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
		// Audio: stored here only; jp_audio::setup() runs after loadSettings and
	// applies them when it opens the stream.
	jp_audio::setEnabled(boolValue(settings, "audio_enabled", true));
	jp_audio::setDevice(stringValue(settings, "audio_device", ""));
	jp_audio::setGain(floatValue(settings, "audio_gain", 1.0f));
	jp_audio::setAutoGain(boolValue(settings, "audio_auto_gain", true));
	jp_audio::setChannelMode(intValue(settings, "audio_channel_mode", jp_audio::CHANNEL_MIX));
	jp_audio::setNoiseGate(floatValue(settings, "audio_noise_gate", 0.015f));
	jp_audio::setShaderDiv(intValue(settings, "audio_shader_div", 0));

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
			// Absent in every settings.xml written before the wall existed,
			// which is exactly when untiled is the right answer.
			output.config.cropEnabled = boolValue(
				outputNode, "crop_enabled", false);
			output.config.cropW = ofClamp(
				floatValue(outputNode, "crop_w", 1.0), 0.0001, 1.0);
			output.config.cropH = ofClamp(
				floatValue(outputNode, "crop_h", 1.0), 0.0001, 1.0);
			output.config.cropX = ofClamp(
				floatValue(outputNode, "crop_x", 0.0), 0.0,
				1.0 - output.config.cropW);
			output.config.cropY = ofClamp(
				floatValue(outputNode, "crop_y", 0.0), 0.0,
				1.0 - output.config.cropH);
			// Deliberately not floored at zero: negative samples wider, which
			// is how CRT overscan is compensated.
			output.config.bezelPx = intValue(outputNode, "bezel_px", 0);
			output.config.physX = floatValue(outputNode, "phys_x", 0.0);
			output.config.physY = floatValue(outputNode, "phys_y", 0.0);
			output.config.physW = std::max(0.0,
				floatValue(outputNode, "phys_w", 0.0));
			output.config.physH = std::max(0.0,
				floatValue(outputNode, "phys_h", 0.0));
			output.config.testPattern = boolValue(
				outputNode, "test_pattern", false);
			output.config.virtualMonitor = boolValue(
				outputNode, "virtual_monitor", false);
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
	// Crops are derived state in SPATIAL mode, so recompute them from the
	// measured layout before anything is drawn or a window is opened.
	applySpatialLayout();
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
	settings.appendChild("audio_enabled").set(toXmlString(jp_audio::getEnabled()));
	settings.appendChild("audio_device").set(jp_audio::getDeviceName());
	settings.appendChild("audio_gain").set(jp_audio::getGain());
	settings.appendChild("audio_auto_gain").set(toXmlString(jp_audio::getAutoGain()));
	settings.appendChild("audio_channel_mode").set(jp_audio::getChannelMode());
	settings.appendChild("audio_noise_gate").set(jp_audio::getNoiseGate());
	settings.appendChild("audio_shader_div").set(jp_audio::getShaderDiv());
	settings.appendChild("favorites_display_mode").set((int)favoritesDisplayMode);

	settings.appendChild("wall_mode").set(
		wallMode == WALL_MODE_SPATIAL ? "spatial" : "freeform");
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
		outputNode.appendChild("crop_enabled").set(
			toXmlString(config.cropEnabled));
		// Explicit precision: the default would round a normalized edge to
		// roughly four thousandths of a pixel at 8K.
		outputNode.appendChild("crop_x").set(ofToString(config.cropX, 9));
		outputNode.appendChild("crop_y").set(ofToString(config.cropY, 9));
		outputNode.appendChild("crop_w").set(ofToString(config.cropW, 9));
		outputNode.appendChild("crop_h").set(ofToString(config.cropH, 9));
		outputNode.appendChild("bezel_px").set(config.bezelPx);
		outputNode.appendChild("phys_x").set(ofToString(config.physX, 4));
		outputNode.appendChild("phys_y").set(ofToString(config.physY, 4));
		outputNode.appendChild("phys_w").set(ofToString(config.physW, 4));
		outputNode.appendChild("phys_h").set(ofToString(config.physH, 4));
		outputNode.appendChild("test_pattern").set(
			toXmlString(config.testPattern));
		outputNode.appendChild("virtual_monitor").set(
			toXmlString(config.virtualMonitor));
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
	jp_audio::shutdown();
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
	if (config.testPattern)
	{
		// Alignment mode: the pattern replaces the content entirely, so what
		// you measure is not confused by whatever the composition is doing.
		drawLiveOutputTestPattern(ofRectangle(0.0f, 0.0f, width, height),
			index);
		return;
	}
	const bool followMain =
		config.sourceMode == LIVE_OUTPUT_MAIN_ACTIVE;
	const ofRectangle crop = config.cropEnabled ?
		ofRectangle(config.cropX, config.cropY, config.cropW, config.cropH) :
		ofRectangle(0.0f, 0.0f, 1.0f, 1.0f);
	const float bezel = config.cropEnabled ? (float)config.bezelPx : 0.0f;
	ofRectangle effective(0.0f, 0.0f, 1.0f, 1.0f);
	const bool sourceAvailable = boxes.drawLiveOutputSource(
		followMain, config.sourceBox, width, height, crop, bezel, &effective);
	if (sourceAvailable)
	{
		// The overlay is authored across the whole canvas, so hand it a virtual
		// rect big enough that the visible tile lands on this window. Derived
		// from the effective rect, not the raw crop: otherwise the bezel inset
		// shows up as a constant misalignment.
		float overlayX = 0.0f, overlayY = 0.0f;
		float overlayW = width, overlayH = height;
		if (effective.width > 0.0f && effective.height > 0.0f)
		{
			overlayW = width / effective.width;
			overlayH = height / effective.height;
			overlayX = -effective.x / effective.width * width;
			overlayY = -effective.y / effective.height * height;
		}
		boxes.drawMappingOverlayForSource(followMain, config.sourceBox,
			overlayX, overlayY, overlayW, overlayH);
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
	// The split fields live outside the per output array, but
	// initLiveOutputFields seeds them, so guard on both focus variables.
	if (selectedLiveOutput == index && focusedLiveOutputField < 0 &&
		focusedSplitField < 0)
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

vector<ofApp::ScreenBarItem> ofApp::buildScreenBar() const {
	const float tabH = 28;
	const float pad = 8;
	const float gap = 2;
	// Panel toggles are a different kind of button from screen switchers, so
	// they read as their own group behind a separator.
	const float groupGap = 20;

	vector<ScreenBarItem> items = {
		{"NODES", NODOS, "Edit the node graph"},
		{"SETTINGS", OPCIONES, "Configure ports, render size, BPM, and output"},
		{"HELP", TUTORIAL, "View keyboard and workflow help"},
		{"IMPORT", SHADER_INDEX, "Browse and preview shader boxes"},
		{"EDITOR", EDITOR, "Edit the selected shader source"},
		{"MIDI", MIDI_KEYMAP, "Bind MIDI controls to boxes and parameters"},
		{"CUE", kCuePanelBarItem, "Cue preview panel"},
		{"MAP", kMappingPanelBarItem, "Projection mapping editor (needs a mapping shader selected)"}
	};

	// Every floating panel now has one discoverable home. Cue was only on 'z'
	// and mapping was buried in a button that appeared inside the inspector.
	ofApp *self = const_cast<ofApp *>(this);
	for (ScreenBarItem &item : items) {
		item.isPanelToggle = item.action < 0;
		switch (item.action) {
		case kCuePanelBarItem:
			item.lit = self->boxes.getCuePreviewBox() != nullptr;
			break;
		case kMappingPanelBarItem:
			item.lit = boxes.isMappingEditActive();
			// Only a mapping shader can be mapped, so say so by greying out
			// rather than by silently ignoring the click.
			item.enabled = item.lit ||
				self->boxes.isMappingShaderBox(self->boxes.getInspectorBox());
			break;
		default:
			item.lit = (pantallaActiva == item.action);
			break;
		}
	}

	float x = pad;
	bool separatorPlaced = false;
	for (int i = 0; i < (int)items.size(); i++) {
		if (items[i].isPanelToggle && !separatorPlaced) {
			x += groupGap - gap;
			separatorPlaced = true;
		}
		const float textW = jp_constants::p_font.stringWidth(items[i].label);
		const float tabW = max(items[i].isPanelToggle ? 66.0f : 90.0f, textW + 24);
		items[i].rect = ofRectangle(x, pad, tabW, tabH);
		x += tabW + gap;
	}
	return items;
}

void ofApp::drawScreenTabs() {
	const vector<ScreenBarItem> items = buildScreenBar();

	// The bar owns its own width, so the MIDI badge asks rather than guesses.
	if (!items.empty()) {
		midiKeymap.setChromeRightEdge(items.back().rect.getMaxX());
	}

	// Separator between the screen switchers and the panel toggles.
	for (int i = 1; i < (int)items.size(); i++) {
		if (!items[i].isPanelToggle || items[i - 1].isPanelToggle) continue;
		ofPushStyle();
		ofSetColor(ofColor(COL_BORDER_MUTED, 170));
		ofSetLineWidth(1);
		const float sx = (items[i - 1].rect.getMaxX() + items[i].rect.x) * 0.5f;
		ofDrawLine(sx, items[i].rect.y + 4, sx, items[i].rect.getMaxY() - 4);
		ofPopStyle();
	}

	for (int i = 0; i < (int)items.size(); i++) {
		const ScreenBarItem &item = items[i];
		const bool active = item.lit;
		// Cyan, not the tabs' green: an open panel is not "the screen you are
		// on", and the two should not read as the same kind of state.
		const ofColor activeFill =
			item.isPanelToggle ? COL_ACCENT_CYAN : COL_ACCENT_GREEN;
		const ofColor activeBorder =
			item.isPanelToggle ? COL_ACCENT_CYAN : COL_ACCENT_GREEN_BR;

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		if (active) {
			ofSetColor(ofColor(activeFill, 235));
		} else {
			ofSetColor(ofColor(COL_TAB_INACTIVE_BG, 225));
		}
		ofDrawRectRounded(item.rect.x, item.rect.y,
			item.rect.width, item.rect.height, 3);

		// Border
		ofNoFill();
		ofSetLineWidth(1);
		if (active) {
			ofSetColor(activeBorder);
		} else {
			ofSetColor(ofColor(COL_BORDER_MUTED, 200));
		}
		ofDrawRectRounded(item.rect.x, item.rect.y,
			item.rect.width, item.rect.height, 3);
		ofFill();

		// Text
		const float textW = jp_constants::p_font.stringWidth(item.label);
		ofSetColor(!item.enabled ? COL_TEXT_MUTED :
			(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		jp_constants::p_font.drawString(item.label,
			item.rect.x + (item.rect.width - textW) * 0.5f,
			item.rect.y + item.rect.height * 0.5f + 5);

		ofPopStyle();
		jp_tooltip::draw(item.tooltip, item.rect.x, item.rect.y,
			item.rect.width, item.rect.height);
	}
}

int ofApp::getScreenTabAtPos(int x, int y) {
	const vector<ScreenBarItem> items = buildScreenBar();
	for (int i = 0; i < (int)items.size(); i++) {
		if (!items[i].enabled) continue;
		if (items[i].rect.inside((float)x, (float)y)) {
			return items[i].action;
		}
	}
	return -1;
}
