#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include "../JPbox/JPboxgroup.h"
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
		float innerX = 0.0f;
		float innerW = 0.0f;
		float headerY = 0.0f;
		float paramY = 0.0f;
		float globalY = 0.0f;
		float addShaderY = 0.0f;
		float targetBoxY = 0.0f;
		float actionY = 0.0f;
		float bindingsY = 0.0f;
	};

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

	void setup(JPboxgroup *_boxes);
	void exit();
	void update();
	void draw();
	void drawMappingTargets();
	bool mousePressed(int x, int y, int button);
	bool keyPressed(int key);
	bool captureFunctionClick(int x, int y, int button);
	bool mouseScrolled(int x, int y, float scrollX, float scrollY);
	void mouseDragged(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void togglePanel();
	bool isPanelOpen() const;
	void save(string path);
	void load(string path);
	void newMidiMessage(ofxMidiMessage &msg) override;

private:
	JPboxgroup *boxes = nullptr;
	vector<ofxMidiIn *> midiInputs;
	vector<Binding> bindings;
	vector<MidiKey> pendingKeys;
	vector<string> pendingShaderAdds;
	map<string, bool> ccHighState;
	std::mutex pendingMutex;
	string globalKeymapPath;

	bool panelOpen = false;
	bool editMode = false;
	bool learning = false;
	bool parameterSectionCollapsed = false;
	bool globalFunctionsCollapsed = true;
	bool addShaderSectionCollapsed = true;
	bool targetBoxSelectOpen = false;
	bool actionSelectOpen = false;
	bool mapDeviceSelectOpen = false;
	int rebindIndex = -1;
	int focusedAddShaderRow = -1;
	string selectedBoxName;
	string addShaderQuery;
	vector<string> addShaderRows;
	vector<string> addShaderResolvedPaths;
	vector<bool> addShaderSearched;
	int selectedParameterIndex = 0;
	Action selectedAction = BYPASS;
	MidiKey lastKey;
	bool hasLastKey = false;
	bool inputsOpen = false;
	vector<string> availableDeviceNames;
	string activeMapDeviceName;

	// Scroll and drag variables
	float targetBoxScrollY = 0.0f;
	bool scrollbarDragging = false;
	float dragStartY = 0.0f;
	float dragStartScrollY = 0.0f;

	void openInputs();
	void closeInputs();
	void setActiveMapDevice(string deviceName);
	bool isActiveMapDevice(string deviceName) const;
	vector<string> getMapDeviceNames() const;
	void ensureActiveMapDevice();
	void processKey(const MidiKey &key);
	void applyBinding(const Binding &binding, float midiValue);
	void processPendingShaderAdds();
	void queueShaderAdd(string shaderPath);
	void learnKey(const MidiKey &key);
	void armLearn(const Binding &binding, int existingIndex = -1);
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
	string getAddShaderRowPath(string query) const;
	string getShaderPathForAddBinding(const Binding &binding) const;
	string resolveShaderQuery(string query) const;
	void collectShaderMatches(string directory, string normalizedQuery, vector<string> &exactMatches, vector<string> &containsMatches) const;
	JPbox *getSelectedParameterBox() const;
	int getGlobalParameterIndexCount() const;
	int getNonParameterBindingCount() const;
	int getParameterRowCount() const;
	int getGlobalActionRowCount() const;
	int getAddShaderRowCount() const;
	PanelLayout getPanelLayout() const;
	DropdownLayout getTargetBoxDropdownLayout(const PanelLayout &layout) const;
	DropdownLayout getActionDropdownLayout(const PanelLayout &layout) const;
	DropdownLayout getMapDeviceDropdownLayout(const PanelLayout &layout) const;
	vector<Action> getGlobalActions() const;
	vector<Action> getBoxActions() const;
	string getKeyId(const MidiKey &key) const;
	string getKeyLabel(const MidiKey &key) const;
	string getActionName(Action action) const;
	string actionToXml(Action action) const;
	Action actionFromXml(string value) const;
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
