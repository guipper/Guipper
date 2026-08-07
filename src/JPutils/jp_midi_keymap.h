#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include "../JPbox/JPboxgroup.h"
#include "jp_textfield.h"
#include <functional>
#include <map>
#include <mutex>

class JPMidiKeymap : public ofxMidiListener
{
public:
	enum Action
	{
		BYPASS,
		PAUSE,
		SELECT_OPEN_BOX,
		PARAMETER,
		NEXT_SHADER,
		PREV_SHADER,
		SET_CUE_SHADER,
		SET_ACTIVE_SHADER,
		SET_ACTIVE_RENDER,
		NEXT_SHADER_GALLERY,
		PREV_SHADER_GALLERY,
		TOGGLE_GALLERY,
		BPM_TAP,
		ADD_SHADER_BOX
	};

	struct MidiKey
	{
		string deviceName;
		int channel = 0;
		string messageType;
		int number = 0;
		float value = 1.0;
	};

	struct Binding
	{
		MidiKey key;
		string boxName;
		Action action = BYPASS;
		int parameterIndex = 0;
		string shaderQuery;
		string shaderPath;
	};

	struct PanelLayout
	{
		float panelH = 0.0f;
		float contentH = 0.0f;
		// innerX/innerW address the LEFT column; the sections that live on the
		// right carry their own rect. Kept as the names the row helpers and
		// hit tests already use.
		float innerX = 0.0f;
		float innerW = 0.0f;
		ofRectangle leftCol;    // bind-from lists
		ofRectangle rightCol;   // context + live bindings
		float leftContentH = 0.0f;
		float rightContentH = 0.0f;
		float rightX = 0.0f;
		float rightW = 0.0f;
		float headerY = 0.0f;
		float paramY = 0.0f;
		float globalY = 0.0f;
		float addShaderY = 0.0f;
		float mapDeviceY = 0.0f;
		float targetBoxY = 0.0f;
		float actionY = 0.0f;
		float paramPickY = 0.0f;
		float learnY = 0.0f;
		float bindingsY = 0.0f;
	};

	// One source for a list row's geometry. Draw and hit-test used to each
	// carry their own `x + w - 146/92/38` literals - sixteen of them - so any
	// relayout could leave a button painted where it is not clickable.
	struct RowRects
	{
		ofRectangle body;
		ofRectangle find;     // empty unless the row has a Find button
		ofRectangle learn;
		ofRectangle remove;
		float labelX = 0.0f;
		float labelMaxW = 0.0f;
		float bindingX = 0.0f;
		float bindingMaxW = 0.0f;
	};
	static RowRects rowRects(float x, float y, float w, bool withFind);

	struct DropdownLayout
	{
		float x = 0.0f;
		float y = 0.0f;
		float w = 0.0f;
		float h = 0.0f;
		float contentH = 0.0f;
		float maxScrollY = 0.0f;
		bool showScrollbar = false;
	};

	void setup(JPboxgroup *_boxes, std::function<void()> _bpmTapCallback);
	void exit();
	void update();
	void draw();
	void drawMappingTargets();
	// Where the top tab bar ends, so the "MIDI MAP ON" badge can be placed
	// clear of it instead of guessing at the bar's width.
	void setChromeRightEdge(float x) { chromeRightEdge = x; }
	bool mousePressed(int x, int y, int button);
	bool keyPressed(int key);
	bool captureFunctionClick(int x, int y, int button);
	bool mouseScrolled(int x, int y, float scrollX, float scrollY);
	void mouseDragged(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void togglePanel();
	bool isPanelOpen() const;
	// MIDI is a screen, so ofApp owns whether it is showing and tells us.
	void setPanelVisible(bool visible);
	// The surface stack drives dismissal, so the panel and its dropdowns each
	// have to be closable from outside and describe where they are.
	void closePanel();
	// armLearn used to set panelOpen directly, but ofApp drives that from the
	// active screen every frame and immediately cancelled the learn. It now
	// raises a request the app consumes to switch screen.
	bool consumeShowRequest();
	bool hasConflictPrompt() const { return conflictPromptOpen; }
	ofRectangle conflictPromptRect() const;
	ofRectangle conflictButtonRect(int index) const;
	void resolveConflict(bool keepBoth);
	void cancelConflict();
	bool hasOpenDropdown() const;
	void closeDropdowns();
	ofRectangle getPanelBounds() const;
	// Union of whichever dropdown is open, so the surface stack can block the
	// controls underneath it. SURFACE_DROPDOWN was registered with an empty
	// rect, so it never blocked anything by position.
	ofRectangle getOpenDropdownBounds() const;
	void save(string path);
	void load(string path);
	void newMidiMessage(ofxMidiMessage &msg) override;

	// Inline MIDI-learn for an ADD_SHADER_BOX binding, driven from the Import
	// page (no MIDI panel required): arm learn for `shaderQuery`; the next MIDI
	// message received binds it. `isLearning()` lets the caller show a
	// "waiting for MIDI" state; `cancelInlineLearn()` aborts.
	void beginAddShaderLearn(const string &shaderQuery);
	bool isLearning() const { return learning; }
	void cancelInlineLearn() { cancelLearning(); }
	string getAddShaderBindingLabel(const string &shaderQuery,
		const string &shaderPath = "") const;

private:
	JPboxgroup *boxes = nullptr;
	vector<ofxMidiIn *> midiInputs;
	vector<Binding> bindings;
	vector<MidiKey> pendingKeys;
	vector<string> pendingShaderAdds;
	map<string, bool> ccHighState;
	std::mutex pendingMutex;
	string globalKeymapPath;
	std::function<void()> bpmTapCallback;

	bool panelOpen = false;
	bool editMode = false;
	bool learning = false;
	bool parameterSectionCollapsed = false;
	bool globalFunctionsCollapsed = true;
	bool addShaderSectionCollapsed = true;
	bool targetBoxSelectOpen = false;
	bool actionSelectOpen = false;
	bool mapDeviceSelectOpen = false;
	bool paramSelectOpen = false;
	// The first focus concept that spans sections. Before this, keyboard input
	// existed only for add-shader text rows and swallowed every other key.
	enum FocusSection
	{
		FOCUS_NONE = 0,
		FOCUS_PARAM,
		FOCUS_GLOBAL,
		FOCUS_ADDSHADER,
		FOCUS_BINDING
	};
	FocusSection focusSection = FOCUS_NONE;
	int focusRow = -1;
	int focusSectionRowCount(FocusSection section) const;
	void moveFocus(int delta);
	void cycleFocusSection(bool backwards);
	void activateFocusedRow();
	void unbindFocusedRow();
	void ensureFocusVisible();
	bool isRowFocused(FocusSection section, int row) const;

	int rebindIndex = -1;
	int focusedAddShaderRow = -1;
	string selectedBoxName;
	string addShaderQuery;
	vector<string> addShaderRows;
	vector<string> addShaderResolvedPaths;
	vector<bool> addShaderSearched;
	// Per-row insertion index, so the rows edit like every other text field in
	// the app instead of being append-only.
	vector<int> addShaderCursors;
	int selectedParameterIndex = 0;
	Action selectedAction = BYPASS;
	MidiKey lastKey;
	bool hasLastKey = false;
	bool inputsOpen = false;
	vector<string> availableDeviceNames;
	string activeMapDeviceName;

	// Scroll and drag variables
	float targetBoxScrollY = 0.0f;
	// The body never scrolled: contentH was computed and never read, so with
	// the parameter list expanded the bindings sat below the frame, unreachable.
	float leftScroll = 0.0f;
	float rightScroll = 0.0f;
	bool scrollbarDragging = false;
	float dragStartY = 0.0f;
	float dragStartScrollY = 0.0f;

	void openInputs();
	void closeInputs();
	void setActiveMapDevice(string deviceName);
	bool isActiveMapDevice(const string &deviceName) const;
	// activeMapDeviceName normalised once. isActiveMapDevice is called inside
	// two nested loops and used to re-normalise BOTH sides every time, which
	// is where the O(N^2) string allocations came from.
	void refreshActiveDeviceCache();
	string activeMapDeviceNormalized;
	string normalizeDeviceName(string deviceName) const;
	vector<string> getMapDeviceNames() const;
	void ensureActiveMapDevice();
	void processKey(const MidiKey &key);
	void applyBinding(const Binding &binding, float midiValue);
	void processPendingShaderAdds();
	void queueShaderAdd(string shaderPath);
	void learnKey(const MidiKey &key);
	// A learned key that is already bound raises a prompt instead of silently
	// stealing it, which is what removeBindingForKey(key,false) used to do.
	void commitLearnedBinding(const Binding &binding, bool replaceExisting);
	bool conflictPromptOpen = false;
	Binding pendingBinding;
	string conflictExistingLabel;
	int conflictCount = 0;
	void armLearn(const Binding &binding, int existingIndex = -1);
	bool showRequested = false;
	void cancelLearning();
	void saveGlobal();
	bool hasLearnTarget() const;
	bool isBindingLoadable(const Binding &binding) const;
	void removeBindingForKey(const MidiKey &key, bool saveChange = true);
	int findBindingForKey(const MidiKey &key) const;
	bool hasBindingForAction(Action action, string boxName = "", int parameterIndex = -1) const;
	int findParameterBindingForIndex(int parameterIndex) const;
	int findGlobalActionBinding(Action action) const;
	int findAddShaderBinding(string query) const;
	bool isGlobalAction(Action action) const;
	int getCurrentBoxIndex() const;
	void selectRelativeBox(int offset, bool galleryMode);
	void setSelectedBoxActive();
	void toggleGalleryMode();
	void syncAddShaderRowsFromBindings();
	void ensureAddShaderDraftRow();
	bool resolveAddShaderRow(int rowIndex);
	// Filename stem with its original case. portableShaderStem() normalises
	// (lowercases, strips spaces), so it cannot supply a name to type back.
	static string rawShaderStem(const string &path);
	// Rewrites the row with the resolved shader's real filename.
	void completeAddShaderRow(int rowIndex);
	string getAddShaderRowPath(string query) const;
	string getShaderPathForAddBinding(const Binding &binding) const;
	string resolveShaderQuery(string query) const;
	void collectShaderMatches(string directory, string normalizedQuery, vector<string> &exactMatches, vector<string> &containsMatches) const;
	JPbox *getSelectedParameterBox() const;
	float chromeRightEdge = 0.0f;
	int getGlobalParameterIndexCount() const;
	int getNonParameterBindingCount() const;
	// Indices of active-device bindings that no left-column row is already
	// showing. Filtering by action KIND would be wrong: the finders below
	// return only the FIRST match, so a second key bound to the same action is
	// displayed nowhere on the left and would become invisible here too.
	// Cached per frame. Computing it costs a full pass over the bindings with a
	// nested find per binding, and getPanelLayout / drawBindings / mousePressed
	// each used to ask for it independently.
	const vector<int> &getCustomBindingIndices() const;
	void invalidateBindingCache();
	mutable vector<int> customBindingCache;
	mutable bool customBindingCacheValid = false;
	bool isBindingShownInLeftColumn(int index) const;
	int getParameterRowCount() const;
	int getGlobalActionRowCount() const;
	int getAddShaderRowCount() const;
	PanelLayout getPanelLayout() const;
	DropdownLayout getTargetBoxDropdownLayout(const PanelLayout &layout) const;
	DropdownLayout getActionDropdownLayout(const PanelLayout &layout) const;
	DropdownLayout getParamDropdownLayout(const PanelLayout &layout) const;
	DropdownLayout getMapDeviceDropdownLayout(const PanelLayout &layout) const;
	static const vector<Action> &globalActionList();
	vector<Action> getGlobalActions() const;
	vector<Action> getBoxActions() const;
	string getKeyId(const MidiKey &key) const;
	string getKeyLabel(const MidiKey &key) const;
	string getCompactKeyLabel(const MidiKey &key) const;
	string describeBinding(const Binding &binding) const;
	string getActionName(Action action) const;
	string actionToXml(Action action) const;
	Action actionFromXml(string value) const;
	static void beginColumnClip(const ofRectangle &r);
	static void endColumnClip();
	static const vector<float> &headerSlotWidths();
	void drawPanelHeader(float x, float y, float w);
	void drawBoxSelector(float x, float y, float w);
	void drawParameterIndexSelector(float x, float y, float w);
	void drawGlobalFunctionsSelector(float x, float y, float w);
	void drawAddShaderSelector(float x, float y, float w);
	void drawActionSelector(float x, float y, float w);
	void drawBindings(float x, float y, float w);
	bool tryCaptureBoxFunctionClick(int x, int y);
	bool tryCaptureInspectorFunctionClick(int x, int y);
	void drawBoxMappingTargets();
	void drawInspectorMappingTargets();
};
