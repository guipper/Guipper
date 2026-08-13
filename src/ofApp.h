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
#include "JPutils/jp_pointer.h"
#include "JPgui/jp_surfacestack.h"
#include "JPgui/jp_screen.h"
//#include "JPbox/Shaderrender.h"
#include "JPutils/jp_fileloader.h"
#include "JPutils/jp_constants.h"
#include "JPutils/jp_tooltip.h"
#include "JPutils/jp_help_content.h"
#include "JPutils/jp_audio.h"
#include "JPutils/jp_shader_globals.h"
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
	void shutdownApp();
	bool appShutdownDone = false;
	// ofxKFW2::Device kinect;

	ofTrueTypeFont font_p; // Titulo de compo
	// JPGui gui;

	JPboxgroup boxes;
	float a;
	int activerender = 0;
	ofFbo output;
	bool isDebug = false;
	struct FrameProfile
	{
		float updateMs = 0.0f;
		float audioMs = 0.0f;
		float boxesMs = 0.0f;
		float midiMs = 0.0f;
		float outputsMs = 0.0f;
		float oscMs = 0.0f;
		float drawMs = 0.0f;
		float updatePeakMs = 0.0f;
		float drawPeakMs = 0.0f;
	};
	FrameProfile frameProfile;
	float lastProfileLogTime = -1.0f;
	void recordProfileValue(float &average, float &peak, float sampleMs);
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

	// LIVE OUTPUT WINDOW MANAGEMENT
	enum LiveOutputSourceMode
	{
		LIVE_OUTPUT_MAIN_ACTIVE = 0,
		LIVE_OUTPUT_FIXED_BOX = 1
	};

	struct LiveOutputMonitor
	{
		string name;
		int index = 0;
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		bool primary = false;
	};

	struct LiveOutputConfig
	{
		string id;
		bool enabled = false;
		LiveOutputSourceMode sourceMode = LIVE_OUTPUT_MAIN_ACTIVE;
		string sourceBox;
		string monitorName;
		int monitorIndex = 0;
		int width = 1280;
		int height = 720;
		int x = 0;
		int y = 0;
		bool hasPosition = false;
		bool fullscreen = false;
		// Screen wall. cropEnabled stays false by default so an untiled output
		// keeps using the plain full frame draw, which is the known good path.
		// The rect is normalized so it survives a render resolution change; it
		// is edited in canvas pixels and converted against the texture actually
		// being sampled, never against jp_constants.
		bool cropEnabled = false;
		double cropX = 0.0;
		double cropY = 0.0;
		double cropW = 1.0;
		double cropH = 1.0;
		// Canvas pixels inset on all four sides. Positive shrinks the sampled
		// rect, which is what two abutting screens need so the image stays
		// continuous across the seam their frames cover. NEGATIVE samples wider,
		// which is what a CRT needs: the tube overscans and cuts the edges, so
		// the signal has to be bigger than the region you want to see.
		int bezelPx = 0;
		// Where this screen's VISIBLE GLASS sits in the installation, in mm.
		// Measured with a tape, glass only - never the plastic case, so bezels
		// and the gaps between scattered screens are handled implicitly. Drives
		// the crop in SPATIAL mode; ignored in FREEFORM.
		double physX = 0.0;
		double physY = 0.0;
		double physW = 0.0;
		double physH = 0.0;
		// Alignment aid: replaces the content with a calibration pattern.
		bool testPattern = false;
		// Opens as an ordinary window at width x height with no matching
		// display attached, so a whole installation can be rehearsed on one
		// machine. Skips monitor resolution entirely.
		bool virtualMonitor = false;
	};

	// SPATIAL derives every crop from the measured physical layout; FREEFORM
	// keeps hand authored crop rects. Installation wide, not per output.
	enum WallLayoutMode
	{
		WALL_MODE_FREEFORM = 0,
		WALL_MODE_SPATIAL
	};

	struct LiveOutputRuntime
	{
		LiveOutputConfig config;
		shared_ptr<ofAppBaseWindow> window;
		bool recreatePending = false;
		bool closePending = false;
		bool createAttempted = false;
	};

	struct RetiredLiveOutputWindow
	{
		shared_ptr<ofAppBaseWindow> window;
		int releaseCountdown = 2;
	};

	vector<LiveOutputMonitor> liveOutputMonitors;
	vector<LiveOutputRuntime> liveOutputs;
	vector<RetiredLiveOutputWindow> retiredLiveOutputWindows;
	float lastLiveOutputMonitorRefresh = -1.0f;
	std::shared_ptr<ofAppBaseWindow> mainWindow;
	int selectedLiveOutput = 0;
	int nextLiveOutputId = 1;
	void window_drawRender(ofEventArgs &args);
	void window_resized(ofResizeEventArgs &args);
	void window_moved(ofWindowPosEventArgs &args);
	void window_mouseMove(ofMouseEventArgs &e);
	void window_keyPressed(ofKeyEventArgs &e);
	void refreshLiveOutputMonitors();
	void updateLiveOutputs();
	void updateRetiredLiveOutputWindows();
	void createLiveOutputWindow(int index);
	void closeLiveOutputWindow(int index, bool intentional);
	void closeAllLiveOutputWindows();
	void requestLiveOutputRecreate(int index);
	int findLiveOutputByWindow(ofAppBaseWindow *window) const;
	int resolveLiveOutputMonitor(const LiveOutputConfig &config) const;
	void addLiveOutput();
	void removeSelectedLiveOutput();
	void initializeDefaultLiveOutput();
	string makeLiveOutputId();
	string getLiveOutputDisplayName(int index) const;

	void loadSettings();
	void saveSettings();
	void saveSession(string path);
	void loadSession(string path);

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

	enum FavoritesDisplayMode {
		FAVORITES_TOP = 0,
		FAVORITES_IN_FOLDERS = 1
	};

	struct ShaderBrowserLayout {
		ofRectangle panel;
		ofRectangle favoritesModeButton;
		ofRectangle search;
		ofRectangle searchClear;
		ofRectangle list;
		ofRectangle preview;
		ofRectangle loadButton;
		ofRectangle bindButton;
		ofRectangle editButton;
		float titleBaseline = 0.0f;
		float hintBaseline = 0.0f;
	};

	struct ShaderBrowserRow {
		bool folderHeader = false;
		int folderIndex = -1;
		int shaderIndex = -1;
		float height = 0.0f;
		ofRectangle bounds;
	};

	enum MENUACTIVO
	{
		NODOS,       // NODES
		OPCIONES,    // SETTINGS
		TUTORIAL,    // HELP
		SHADER_INDEX,// IMPORT
		EDITOR,      // SHADER EDITOR
		MIDI_KEYMAP  // MIDI keymap. Appended: the ordinals are not persisted,
		             // but keeping them stable keeps '1'-'5' meaning what they
		             // always meant.
	};
	int pantallaActiva;
	// HELP and MIDI never scrolled; both now have a full-height frame to fill.
	// helpContentH / helpViewH used to live here and were measured as a side
	// effect of drawing, so the scroll clamp always ran a frame late. HelpLayout
	// owns them now.
	float helpScroll = 0.0f;
	bool helpScrollbarDragging = false;
	float helpScrollbarDragOffset = 0.0f;
	float midiScroll = 0.0f;

	// Every floating thing is a surface with a declared z-order, so "who is on
	// top", "what does ESC close" and "what blocks the click" are answered in
	// one place instead of by the order of early returns in the input handlers.
	// Values are the z-order; ids are the same numbers.
	enum SurfaceId
	{
		SURFACE_INSPECTOR = jp_pointer::kInspector,
		SURFACE_CUE_PANEL = jp_pointer::kCuePanel,
		SURFACE_MAPPING_PANEL = jp_pointer::kMappingPanel,
		SURFACE_SHADER_EDITOR = jp_pointer::kShaderEditor,
		SURFACE_FIELD_EDIT = jp_pointer::kFieldEdit,
		SURFACE_DROPDOWN = jp_pointer::kDropdown,
		SURFACE_MIDI_CONFLICT = jp_pointer::kPrompt,
		SURFACE_SAVE_MODAL = jp_pointer::kModal
	};
	JPSurfaceStack surfaces;
	void registerSurfaces();
	// Drops focus from whichever text field currently has it.
	void clearFieldFocus();
	bool anyFieldFocused() const;

	// Top bar buttons. Most switch screen and carry their MENUACTIVO id;
	// kMidiPanelBarItem toggles the MIDI panel instead, so it is negative and
	// callers testing `>= 0` keep ignoring it.
	static constexpr int kMidiPanelBarItem = -2;
	static constexpr int kCuePanelBarItem = -3;
	static constexpr int kMappingPanelBarItem = -4;
	struct ScreenBarItem
	{
		string label;
		int action = -1;
		string tooltip;
		ofRectangle rect;
		// Panel toggles read as a separate group: cyan when open rather than
		// green, and greyed out when the panel cannot apply right now.
		bool isPanelToggle = false;
		bool lit = false;
		bool enabled = true;
	};
	// Single source of the bar layout - drawing and hit testing used to keep
	// their own copies of the list and the geometry.
	vector<ScreenBarItem> buildScreenBar() const;
	void drawScreenTabs();
	int getScreenTabAtPos(int x, int y);
	void closeShaderEditorToMain();
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
	string previewShaderPath;
	float lastPreviewRenderTime = -1.0f;
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
	// shaders; display placement is controlled separately in settings.xml.
	vector<string> favoritePaths;
	FavoritesDisplayMode favoritesDisplayMode = FAVORITES_TOP;
	bool favoritesFolderExpanded = true;
	vector<vector<int>> shaderFolderOrder;
	void loadFavorites();
	void saveFavorites();
	bool isFavorite(const string &path) const;
	void toggleFavorite(const string &path);
	void rebuildFavoritesFolder();
	void rebuildShaderFolderOrder();
	void toggleFavoritesDisplayMode();

	ShaderBrowserLayout getShaderBrowserLayout() const;
	const vector<int> &getOrderedShaderIndices(int folderIndex) const;
	vector<ShaderBrowserRow> buildShaderBrowserRows() const;
	vector<ShaderBrowserRow> getVisibleShaderBrowserRows(const ShaderBrowserLayout &layout) const;
	vector<ShaderBrowserRow> getVisibleShaderBrowserRows(
		const ShaderBrowserLayout &layout,
		const vector<ShaderBrowserRow> &rows) const;
	int getMaxShaderScroll(const vector<ShaderBrowserRow> &rows, float viewportHeight) const;
	void clampShaderScroll(const ShaderBrowserLayout &layout);
	void clearShaderSearch();
	string getSelectedShaderPath() const;

	// Import-page inline MIDI bind: true while waiting for a MIDI message to
	// bind the selected shader's add-shader command.
	bool importBindWaiting = false;

	// Shared preview-selection + keyboard navigation for the shader index.
	void ensurePreviewFbo();
	void renderShaderPreview(bool useLiveMouse);
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

	// One layout for the settings screen, computed once and read by both the
	// draw pass and the click handler. They used to keep private copies of the
	// same constants under a comment saying they had to match by hand, which is
	// how a control ends up painted somewhere other than where it is clickable.
	struct SettingsLayout
	{
		ofRectangle panel;
		ofRectangle fields[OPTIONS_FIELD_COUNT];
		ofRectangle autoTapButton;
		ofRectangle spoutToggle;   // empty when built without SPOUT
		ofRectangle ndiToggle;     // empty when built without NDI
		ofRectangle browseButton;
		ofRectangle saveButton;
		ofRectangle activeCompoRow;
		// Audio input. Deliberately NOT text fields: keeping them out of the
		// FIELD_* enum avoids touching labels[], fieldTooltips[], the
		// initOptionsFields/applyOptionsField switch and the numeric-field
		// hit-test loop, all of which key off FIELD_OSC_IP_OUT as a count.
		ofRectangle audioEnableButton;
		ofRectangle audioDeviceField;
		ofRectangle audioGainSlider;
		ofRectangle audioDivButton;
		ofRectangle audioAutoGainButton;
		ofRectangle audioChannelButton;
		ofRectangle audioCalibrateButton;
		ofRectangle audioGateSlider;
		ofRectangle audioMeter;
		float labelX = 0.0f;
		float rowH = 28.0f;
	};
	SettingsLayout getSettingsLayout() const;
	// Audio device dropdown state (SETTINGS). Registered with SURFACE_DROPDOWN
	// so it blocks the rows it covers.
	bool audioMenuOpen = false;
	bool audioGainDragging = false;
	bool audioGateDragging = false;
	ofRectangle getAudioMenuBounds() const;
	void drawAudioSettings(const SettingsLayout &L);
	bool handleAudioSettingsClick(int x, int y);


	// HELP, same idea. One layout feeds the draw pass, the language button's
	// hit test and the wheel clamp, so those cannot drift apart again - the
	// button used to be drawn from jp_screen::actionSlot and hit-tested from a
	// second hardcoded rect three pixels away.
	struct HelpRow
	{
		jp_help::Kind kind = jp_help::Kind::Entry;
		jp_help::Scope scope = jp_help::Scope::Global;
		string keys;
		vector<string> desc;      // already word-wrapped to the content width
		float y = 0.0f;           // top of the row, before scrolling
		float h = 0.0f;
		// Key label wider than the gutter: description drops to the next line
		// at full width instead of colliding with it.
		bool keysOwnLine = false;
	};
	struct HelpLayout
	{
		ofRectangle frame, body, langBtn, scrollTrack, scrollThumb;
		float contentX = 0.0f, contentW = 0.0f, keysW = 0.0f, descX = 0.0f;
		float contentH = 0.0f, viewH = 0.0f, maxScroll = 0.0f;
		bool showScrollbar = false;
		vector<HelpRow> rows;
	};
	HelpLayout getHelpLayout() const;
	// Wrapping every description measures a lot of strings; rebuild only when
	// the language or the available width actually changes.
	mutable HelpLayout helpLayoutCache;
	mutable int helpCacheLang = -1;
	string optionsFieldText[OPTIONS_FIELD_COUNT];
	int focusedOptionsField = -1;
	int optionsFieldCursor = 0;
	void applyOptionsField();
	void initOptionsFields();

	enum LiveOutputMenu
	{
		LIVE_OUTPUT_MENU_NONE = 0,
		LIVE_OUTPUT_MENU_SOURCE,
		LIVE_OUTPUT_MENU_MONITOR
	};

	enum LiveOutputSettingsTab
	{
		LO_TAB_OUTPUTS = 0,
		LO_TAB_WALL,
		LO_TAB_COUNT
	};

	// Per output numeric fields. The commit path switches on these, so a new
	// entry must be added to applyLiveOutputField's switch as well - it used to
	// end in an else that wrote height, which would silently swallow new ids.
	enum LiveOutputField
	{
		LO_FIELD_WIDTH = 0,
		LO_FIELD_HEIGHT,
		LO_FIELD_CROP_X,
		LO_FIELD_CROP_Y,
		LO_FIELD_CROP_W,
		LO_FIELD_CROP_H,
		LO_FIELD_BEZEL,
		LO_FIELD_COUNT
	};

	struct LiveOutputSettingsLayout
	{
		ofRectangle panel;
		vector<ofRectangle> tabs;
		ofRectangle list;
		vector<ofRectangle> rows;
		vector<int> rowIndices;
		// Y of the divider that separates this output's own settings from the
		// ones that apply to the whole installation.
		float globalSectionY = 0.0f;
		ofRectangle addButton;
		ofRectangle deleteButton;
		ofRectangle enabledToggle;
		ofRectangle sourceButton;
		ofRectangle monitorButton;
		ofRectangle windowModeButton;
		ofRectangle fullscreenModeButton;
		ofRectangle widthField;
		ofRectangle heightField;
		// Wall tab. fieldRects is indexed by LiveOutputField so the draw code,
		// the hit test and the caret all read the same geometry.
		ofRectangle tiledToggle;
		ofRectangle modeToggle;
		ofRectangle viewToggle;
		ofRectangle patternToggle;
		ofRectangle fieldRects[LO_FIELD_COUNT];
		ofRectangle splitColsField;
		ofRectangle splitRowsField;
		ofRectangle splitButton;
		ofRectangle bezelSignButton;
		ofRectangle matchAspectButton;
		ofRectangle matchResolutionButton;
		ofRectangle preview;
		ofRectangle popup;
		vector<ofRectangle> popupRows;
		vector<int> popupOptionIndices;
		bool twoColumns = false;
	};

	LiveOutputMenu liveOutputMenu = LIVE_OUTPUT_MENU_NONE;
	LiveOutputSettingsTab liveOutputTab = LO_TAB_OUTPUTS;
	int focusedLiveOutputField = -1;
	string liveOutputFieldText[LO_FIELD_COUNT];
	int liveOutputFieldCursor = 0;
	bool liveOutputFieldSelectAll = false;
	// The split cols/rows live outside the per output array on purpose:
	// initLiveOutputFields runs on every selection change and on every output
	// window resize, so anything in that array is wiped mid edit.
	string splitFieldText[2];
	int focusedSplitField = -1;
	int splitFieldCursor = 0;
	bool splitFieldSelectAll = false;
	int lastLiveOutputInputClick = -1;
	uint64_t lastLiveOutputInputClickMs = 0;

	// Dragging tiles in the wall preview. Snapshot based like the mapping MOVE
	// tool: the crop is written as snapshot + delta so a dropped mouse frame
	// cannot accumulate drift.
	enum WallDragMode
	{
		WALL_DRAG_NONE = 0,
		WALL_DRAG_MOVE,
		WALL_DRAG_RESIZE
	};
	WallLayoutMode wallMode = WALL_MODE_FREEFORM;
	// The wall preview shows either the canvas and its crops, or the room and
	// the screens standing in it.
	bool wallRoomView = false;
	WallDragMode wallDragMode = WALL_DRAG_NONE;
	int wallDragOutput = -1;
	int wallDragCorner = -1;   // 0 TL, 1 TR, 2 BL, 3 BR; opposite is 3 - i
	ofVec2f wallDragStartMouse;
	ofRectangle wallDragStartCrop;
	float settingsScroll = 0.0f;
	int liveOutputListScroll = 0;
	int liveOutputMenuScroll = 0;
	LiveOutputSettingsLayout getLiveOutputSettingsLayout() const;
	void drawLiveOutputSettings();
	void drawLiveOutputWallTab(const LiveOutputSettingsLayout &layout,
		float editorX);
	bool handleLiveOutputSettingsClick(int x, int y, int button);
	bool handleLiveOutputSettingsDrag(int x, int y, int button);
	bool handleLiveOutputSettingsRelease(int x, int y, int button);
	// A tile's rect inside the wall preview, shared by the draw code, the hit
	// test and the drag so they cannot disagree.
	ofRectangle getWallTileRect(const LiveOutputSettingsLayout &layout,
		int outputIndex) const;
	// Bounding box of every tiled output's measured glass, in mm. Empty when
	// nothing has been measured yet.
	ofRectangle getInstallationBounds() const;
	// Rewrites every tiled output's crop from its measured position. No-op in
	// FREEFORM mode.
	void applySpatialLayout();
	// Screen rect inside the room view, scaled to fit the preview.
	ofRectangle getRoomScreenRect(const LiveOutputSettingsLayout &layout,
		int outputIndex) const;
	void drawWallCanvasView(const LiveOutputSettingsLayout &layout);
	void drawWallRoomView(const LiveOutputSettingsLayout &layout);
	// Alignment pattern: labelled rings at 2/4/6/8 percent, a grid and the
	// screen number, drawn into an arbitrary rect so the output window and the
	// room view can share it.
	void drawLiveOutputTestPattern(const ofRectangle &bounds,
		int outputIndex);
	void initLiveOutputFields();
	void applyLiveOutputField();
	void applySplitField();
	void applyWallSplit();
	void clearLiveOutputInteractionState();
	void focusAdjacentLiveOutputField(bool backwards);
	void setSelectedLiveOutput(int index);
	void setLiveOutputTab(LiveOutputSettingsTab tab);
	vector<string> getLiveOutputSourceOptions() const;
	void clampSettingsScroll();
	// One definition of the panel height, shared by the layout and the scroll
	// clamp; they used to carry duplicate copies of the same expression.
	float getLiveOutputPanelHeight(LiveOutputSettingsTab tab) const;
	float getSettingsContentHeight() const;
	// Canvas size a crop is measured against. Falls back to jp_constants when
	// the source texture is not resolvable yet.
	ofVec2f getLiveOutputCanvasSize(const LiveOutputConfig &config) const;
	// Current output size. Fullscreen outputs use their live window or monitor
	// dimensions without overwriting the saved windowed size.
	ofVec2f getLiveOutputTargetSize(const LiveOutputRuntime &output) const;
	ofRectangle getLiveOutputSourceRect(const LiveOutputConfig &config) const;

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
