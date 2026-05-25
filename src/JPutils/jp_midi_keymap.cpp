#include "jp_midi_keymap.h"
#include <algorithm>
#include <cctype>

namespace
{
	const int MIDI_CC_THRESHOLD = 64;
	const float PANEL_X = 40;
	const float PANEL_Y = 40;
	const float PANEL_W = 560;
	const float ROW_H = 22;
	const float PAD = 12;
	const float PARAM_Y_OFFSET = 82;
	const float PARAM_HEADER_H = 28;
	const float SECTION_GAP = 34;
	const float SECTION_VERTICAL_SPACING = 20;
	const float PANEL_MIN_H = 340.0f;
	const float PANEL_BOTTOM_PAD = 54.0f;
	const float SELECT_LABEL_W = 100.0f;
	const float SELECT_FIELD_Y_OFFSET = 15.0f;
	const float DROPDOWN_GAP = 4.0f;
	const float SELECT_DROPDOWN_MAX_H = 420.0f;
	const int DEFAULT_GLOBAL_PARAMETER_INDEX_COUNT = 16;

	bool mouseInRect(float x, float y, float w, float h)
	{
		return ofGetMouseX() >= x && ofGetMouseX() <= x + w &&
			   ofGetMouseY() >= y && ofGetMouseY() <= y + h;
	}

	bool pointInRect(float px, float py, float x, float y, float w, float h)
	{
		return px >= x && px <= x + w && py >= y && py <= y + h;
	}

	void drawButton(float x, float y, float w, float h, const string &label, bool selected)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(selected ? ofColor(0, 160, 160, 220) : ofColor(25, 30, 35, 230));
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofNoFill();
		if (mouseInRect(x, y, w, h)) {
			ofSetColor(255);
			ofSetLineWidth(1.5f);
		} else {
			ofSetColor(selected ? ofColor(0, 230, 230, 200) : ofColor(70, 80, 90, 150));
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		ofSetColor(255);
		jp_constants::p_font.drawString(label, x + 8, y + h - 7);
	}

	void drawSelectField(float x, float y, float w, float h, const string &label, bool open)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(ofColor(15, 20, 25, 240));
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofNoFill();
		if (mouseInRect(x, y, w, h) || open) {
			ofSetColor(0, 230, 230, 255);
			ofSetLineWidth(1.5f);
		} else {
			ofSetColor(80, 90, 100, 180);
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		ofSetColor(255);
		jp_constants::p_font.drawString(label, x + 8, y + h - 7);
		ofSetColor(0, 230, 230, 255);
		jp_constants::p_font.drawString(open ? "^" : "v", x + w - 18, y + h - 7);
	}

	void drawMapOnIndicator()
	{
		const float x = 12.0f;
		const float y = 12.0f;
		const float w = 132.0f;
		const float h = 24.0f;
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(245, 215, 70, 245);
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofNoFill();
		ofSetColor(30, 30, 30, 220);
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofFill();
		ofSetColor(20, 20, 20, 255);
		jp_constants::p_font.drawString("MIDI MAP ON", x + 10, y + h - 7);
	}

	string fitLabel(string label, float maxWidth)
	{
		while (label.size() > 4 && jp_constants::p_font.stringWidth(label) > maxWidth)
		{
			label = label.substr(0, label.size() - 4) + "...";
		}
		return label;
	}

	string normalizeText(string value)
	{
		value = ofToLower(value);
		value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
			return std::isspace(c);
		}), value.end());
		return value;
	}
}

void JPMidiKeymap::setup(JPboxgroup *_boxes)
{
	boxes = _boxes;
	ensureAddShaderDraftRow();
	openInputs();
}

void JPMidiKeymap::exit()
{
	closeInputs();
}

void JPMidiKeymap::openInputs()
{
	closeInputs();

	ofxMidiIn midiProbe;
	int numPorts = midiProbe.getNumInPorts();
	for (int i = 0; i < numPorts; i++)
	{
		ofxMidiIn *midiIn = new ofxMidiIn();
		midiIn->openPort(i);
		midiIn->ignoreTypes(false, false, false);
		midiIn->addListener(this);
		midiIn->setVerbose(false);
		midiInputs.push_back(midiIn);
	}
}

void JPMidiKeymap::closeInputs()
{
	for (int i = 0; i < midiInputs.size(); i++)
	{
		midiInputs[i]->removeListener(this);
		midiInputs[i]->closePort();
		delete midiInputs[i];
	}
	midiInputs.clear();
}

void JPMidiKeymap::update()
{
	vector<MidiKey> keys;
	{
		std::lock_guard<std::mutex> lock(pendingMutex);
		keys.swap(pendingKeys);
	}

	for (int i = 0; i < keys.size(); i++)
	{
		processKey(keys[i]);
	}
}

void JPMidiKeymap::newMidiMessage(ofxMidiMessage &msg)
{
	MidiKey key;
	key.deviceName = msg.portName.empty() ? "port " + ofToString(msg.portNum) : msg.portName;
	key.channel = msg.channel;

	if (msg.status == MIDI_NOTE_ON)
	{
		if (msg.velocity <= 0)
		{
			return;
		}
		key.messageType = "note";
		key.number = msg.pitch;
		key.value = ofClamp(msg.velocity / 127.0f, 0.0f, 1.0f);
	}
	else if (msg.status == MIDI_CONTROL_CHANGE)
	{
		key.messageType = "cc";
		key.number = msg.control;
		key.value = ofClamp(msg.value / 127.0f, 0.0f, 1.0f);
		string ccId = getKeyId(key);
		int existingIndex = findBindingForKey(key);
		bool isContinuousBinding = !learning && existingIndex >= 0 &&
								   (bindings[existingIndex].action == PARAMETER ||
								    bindings[existingIndex].action == BYPASS ||
								    bindings[existingIndex].action == PAUSE);
		if (!isContinuousBinding)
		{
			bool isHigh = msg.value >= MIDI_CC_THRESHOLD;
			bool wasHigh = ccHighState[ccId];
			ccHighState[ccId] = isHigh;
			if (!isHigh || wasHigh)
			{
				return;
			}
		}
	}
	else
	{
		return;
	}

	std::lock_guard<std::mutex> lock(pendingMutex);
	pendingKeys.push_back(key);
}

void JPMidiKeymap::processKey(const MidiKey &key)
{
	lastKey = key;
	hasLastKey = true;

	if (learning)
	{
		learnKey(key);
		return;
	}

	int index = findBindingForKey(key);
	if (index >= 0)
	{
		applyBinding(bindings[index], key.value);
	}
}

void JPMidiKeymap::learnKey(const MidiKey &key)
{
	removeBindingForKey(key);

	if (boxes == nullptr ||
		(selectedAction != PARAMETER &&
		 selectedAction != ADD_SHADER_BOX &&
		 !isGlobalAction(selectedAction) &&
		 selectedBoxName.empty()) ||
		(selectedAction == ADD_SHADER_BOX && addShaderQuery.empty()))
	{
		learning = false;
		rebindIndex = -1;
		return;
	}

	Binding binding;
	binding.key = key;
	binding.boxName = (isGlobalAction(selectedAction) || selectedAction == ADD_SHADER_BOX) ? "" : selectedBoxName;
	binding.action = selectedAction;
	binding.parameterIndex = selectedParameterIndex;
	binding.shaderQuery = selectedAction == ADD_SHADER_BOX ? addShaderQuery : "";
	binding.shaderPath = selectedAction == ADD_SHADER_BOX ? getAddShaderRowPath(addShaderQuery) : "";

	if (rebindIndex >= 0 && rebindIndex < bindings.size())
	{
		bindings[rebindIndex] = binding;
	}
	else
	{
		bindings.push_back(binding);
	}

	learning = false;
	rebindIndex = -1;
	ensureAddShaderDraftRow();
}

void JPMidiKeymap::armLearn(const Binding &binding, int existingIndex)
{
	selectedBoxName = binding.boxName;
	selectedAction = binding.action;
	selectedParameterIndex = binding.parameterIndex;
	if (binding.action == ADD_SHADER_BOX)
	{
		addShaderQuery = binding.shaderQuery;
	}
	rebindIndex = existingIndex;
	learning = true;
	panelOpen = true;
	editMode = true;
}

void JPMidiKeymap::applyBinding(const Binding &binding, float midiValue)
{
	if (boxes == nullptr)
	{
		return;
	}

	if (binding.action == BYPASS)
	{
		if (binding.key.messageType == "cc")
		{
			boxes->setBypassForBox(binding.boxName, midiValue > 0.5f);
		}
		else
		{
			boxes->toggleBypassForBox(binding.boxName);
		}
	}
	else if (binding.action == PAUSE)
	{
		if (binding.key.messageType == "cc")
		{
			boxes->setPauseForBox(binding.boxName, midiValue > 0.5f);
		}
		else
		{
			boxes->togglePauseForBox(binding.boxName);
		}
	}
	else if (binding.action == SELECT_OPEN_BOX)
	{
		boxes->selectOpenBoxByName(binding.boxName);
	}
	else if (binding.action == PARAMETER)
	{
		boxes->setOpenBoxParameterAtIndex(binding.parameterIndex, midiValue);
	}
	else if (binding.action == NEXT_SHADER)
	{
		selectRelativeBox(1, false);
	}
	else if (binding.action == PREV_SHADER)
	{
		selectRelativeBox(-1, false);
	}
	else if (binding.action == SET_ACTIVE_SHADER || binding.action == SET_ACTIVE_RENDER)
	{
		setSelectedBoxActive();
	}
	else if (binding.action == NEXT_SHADER_GALLERY)
	{
		selectRelativeBox(1, true);
	}
	else if (binding.action == PREV_SHADER_GALLERY)
	{
		selectRelativeBox(-1, true);
	}
	else if (binding.action == TOGGLE_GALLERY)
	{
		toggleGalleryMode();
	}
	else if (binding.action == ADD_SHADER_BOX)
	{
		string shaderPath = binding.shaderPath.empty() ? resolveShaderQuery(binding.shaderQuery) : binding.shaderPath;
		if (!shaderPath.empty() &&
			((shaderPath.find(":") == string::npos &&
			  shaderPath.find("/") != 0 &&
			  shaderPath.find("\\") != 0) ||
			 shaderPath.find(":/") != string::npos))
		{
			shaderPath = resolveShaderQuery(binding.shaderQuery);
		}
		if (!shaderPath.empty())
		{
			boxes->addBox(shaderPath);
			boxes->setLastBoxOnOff(true);
		}
	}
}

void JPMidiKeymap::removeBindingForKey(const MidiKey &key)
{
	int index = findBindingForKey(key);
	if (index >= 0)
	{
		bindings.erase(bindings.begin() + index);
		if (rebindIndex == index)
		{
			rebindIndex = -1;
		}
		else if (rebindIndex > index)
		{
			rebindIndex--;
		}
	}
}

int JPMidiKeymap::findBindingForKey(const MidiKey &key) const
{
	string keyId = getKeyId(key);
	for (int i = 0; i < bindings.size(); i++)
	{
		if (getKeyId(bindings[i].key) == keyId)
		{
			return i;
		}
	}
	return -1;
}

bool JPMidiKeymap::hasBindingForAction(Action action, string boxName, int parameterIndex) const
{
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action != action)
		{
			continue;
		}
		if (action == PARAMETER)
		{
			if (bindings[i].parameterIndex == parameterIndex)
			{
				return true;
			}
		}
		else if (isGlobalAction(action))
		{
			return true;
		}
		else if (bindings[i].boxName == boxName)
		{
			return true;
		}
	}
	return false;
}

int JPMidiKeymap::findParameterBindingForIndex(int parameterIndex) const
{
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == PARAMETER &&
			bindings[i].parameterIndex == parameterIndex)
		{
			return i;
		}
	}
	return -1;
}

int JPMidiKeymap::findGlobalActionBinding(Action action) const
{
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == action)
		{
			return i;
		}
	}
	return -1;
}

int JPMidiKeymap::findAddShaderBinding(string query) const
{
	string normalizedQuery = normalizeText(query);
	if (normalizedQuery.empty())
	{
		return -1;
	}
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == ADD_SHADER_BOX &&
			normalizeText(bindings[i].shaderQuery) == normalizedQuery)
		{
			return i;
		}
	}
	return -1;
}

bool JPMidiKeymap::isGlobalAction(Action action) const
{
	vector<Action> actions = getGlobalActions();
	return std::find(actions.begin(), actions.end(), action) != actions.end();
}

int JPMidiKeymap::getCurrentBoxIndex() const
{
	if (boxes == nullptr || boxes->boxes.empty())
	{
		return -1;
	}
	if (boxes->openguinumber >= 0 && boxes->openguinumber < boxes->boxes.size())
	{
		return boxes->openguinumber;
	}
	int activeIndex = boxes->getActiverenderNum();
	if (activeIndex >= 0 && activeIndex < boxes->boxes.size())
	{
		return activeIndex;
	}
	return 0;
}

void JPMidiKeymap::selectRelativeBox(int offset, bool galleryMode)
{
	if (boxes == nullptr || boxes->boxes.empty())
	{
		return;
	}

	int index = getCurrentBoxIndex();
	if (index < 0)
	{
		return;
	}

	int count = boxes->boxes.size();
	index = (index + offset + count) % count;
	if (!boxes->selectOpenBoxByIndex(index))
	{
		return;
	}

	if (galleryMode)
	{
		boxes->updateTransition(index);
		boxes->setActiveOnlyBox(index);
	}
}

void JPMidiKeymap::setSelectedBoxActive()
{
	if (boxes == nullptr)
	{
		return;
	}

	int index = getCurrentBoxIndex();
	if (index >= 0)
	{
		boxes->selectOpenBoxByIndex(index);
		boxes->updateTransition(index);
	}
}

void JPMidiKeymap::toggleGalleryMode()
{
	if (boxes == nullptr)
	{
		return;
	}

	boxes->activeSequence = !boxes->activeSequence;
	if (boxes->activeSequence)
	{
		for (int i = 0; i < boxes->boxes.size(); i++)
		{
			boxes->boxes[i]->setonoff(true);
		}
	}
}

void JPMidiKeymap::syncAddShaderRowsFromBindings()
{
	addShaderRows.clear();
	addShaderResolvedPaths.clear();
	addShaderSearched.clear();
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == ADD_SHADER_BOX && !bindings[i].shaderQuery.empty())
		{
			addShaderRows.push_back(bindings[i].shaderQuery);
			addShaderResolvedPaths.push_back(bindings[i].shaderPath);
			addShaderSearched.push_back(!bindings[i].shaderPath.empty());
		}
	}
	ensureAddShaderDraftRow();
}

void JPMidiKeymap::ensureAddShaderDraftRow()
{
	if (addShaderRows.empty() || !addShaderRows.back().empty())
	{
		addShaderRows.push_back("");
	}
	while (addShaderResolvedPaths.size() < addShaderRows.size())
	{
		addShaderResolvedPaths.push_back("");
	}
	while (addShaderSearched.size() < addShaderRows.size())
	{
		addShaderSearched.push_back(false);
	}
	while (addShaderResolvedPaths.size() > addShaderRows.size())
	{
		addShaderResolvedPaths.pop_back();
	}
	while (addShaderSearched.size() > addShaderRows.size())
	{
		addShaderSearched.pop_back();
	}
	while (addShaderRows.size() > 1 &&
		   addShaderRows[addShaderRows.size() - 1].empty() &&
		   addShaderRows[addShaderRows.size() - 2].empty())
	{
		addShaderRows.erase(addShaderRows.end() - 1);
		addShaderResolvedPaths.erase(addShaderResolvedPaths.end() - 1);
		addShaderSearched.erase(addShaderSearched.end() - 1);
	}
	if (focusedAddShaderRow >= addShaderRows.size())
	{
		focusedAddShaderRow = -1;
	}
}

bool JPMidiKeymap::resolveAddShaderRow(int rowIndex)
{
	ensureAddShaderDraftRow();
	if (rowIndex < 0 || rowIndex >= addShaderRows.size())
	{
		return false;
	}
	addShaderResolvedPaths[rowIndex] = resolveShaderQuery(addShaderRows[rowIndex]);
	addShaderSearched[rowIndex] = true;
	int bindingIndex = findAddShaderBinding(addShaderRows[rowIndex]);
	if (bindingIndex >= 0)
	{
		bindings[bindingIndex].shaderQuery = addShaderRows[rowIndex];
		bindings[bindingIndex].shaderPath = addShaderResolvedPaths[rowIndex];
	}
	return !addShaderResolvedPaths[rowIndex].empty();
}

string JPMidiKeymap::getAddShaderRowPath(string query) const
{
	string normalizedQuery = normalizeText(query);
	for (int i = 0; i < addShaderRows.size(); i++)
	{
		if (normalizeText(addShaderRows[i]) == normalizedQuery &&
			i < addShaderResolvedPaths.size())
		{
			return addShaderResolvedPaths[i];
		}
	}
	return "";
}

string JPMidiKeymap::resolveShaderQuery(string query) const
{
	string normalizedQuery = normalizeText(query);
	if (normalizedQuery.empty())
	{
		return "";
	}

	vector<string> exactMatches;
	vector<string> containsMatches;
	collectShaderMatches(ofToDataPath("shaders", true), normalizedQuery, exactMatches, containsMatches);
	std::sort(exactMatches.begin(), exactMatches.end());
	std::sort(containsMatches.begin(), containsMatches.end());

	if (!exactMatches.empty())
	{
		return exactMatches[0];
	}
	if (!containsMatches.empty())
	{
		return containsMatches[0];
	}
	return "";
}

void JPMidiKeymap::collectShaderMatches(string directory, string normalizedQuery, vector<string> &exactMatches, vector<string> &containsMatches) const
{
	ofDirectory dir(directory);
	if (!dir.exists())
	{
		return;
	}
	dir.listDir();
	dir.sort();

	for (int i = 0; i < dir.size(); i++)
	{
		ofFile file = dir.getFile(i);
		if (file.isDirectory())
		{
			collectShaderMatches(file.getAbsolutePath(), normalizedQuery, exactMatches, containsMatches);
			continue;
		}
		string fileName = file.getFileName();
		size_t extensionDot = fileName.find_last_of('.');
		string extension = extensionDot == string::npos ? "" : fileName.substr(extensionDot + 1);
		if (ofToLower(extension) != "frag")
		{
			continue;
		}
		size_t dot = fileName.find_last_of('.');
		string stem = normalizeText(dot == string::npos ? fileName : fileName.substr(0, dot));
		if (stem == normalizedQuery)
		{
			exactMatches.push_back(file.getAbsolutePath());
		}
		else if (stem.find(normalizedQuery) != string::npos)
		{
			containsMatches.push_back(file.getAbsolutePath());
		}
	}
}

JPbox *JPMidiKeymap::getSelectedParameterBox() const
{
	if (boxes == nullptr)
	{
		return nullptr;
	}
	for (int i = 0; i < boxes->boxes.size(); i++)
	{
		if (boxes->boxes[i]->name == selectedBoxName)
		{
			return boxes->boxes[i];
		}
	}
	if (boxes->openguinumber >= 0 && boxes->openguinumber < boxes->boxes.size())
	{
		return boxes->boxes[boxes->openguinumber];
	}
	return nullptr;
}

int JPMidiKeymap::getGlobalParameterIndexCount() const
{
	int maxCount = boxes != nullptr ? boxes->getMaxParameterCount() : 0;
	return std::max(DEFAULT_GLOBAL_PARAMETER_INDEX_COUNT, maxCount);
}

int JPMidiKeymap::getNonParameterBindingCount() const
{
	int count = 0;
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action != PARAMETER)
		{
			count++;
		}
	}
	return count;
}

int JPMidiKeymap::getParameterRowCount() const
{
	return parameterSectionCollapsed ? 0 : getGlobalParameterIndexCount();
}

int JPMidiKeymap::getGlobalActionRowCount() const
{
	return globalFunctionsCollapsed ? 0 : int(getGlobalActions().size());
}

int JPMidiKeymap::getAddShaderRowCount() const
{
	return addShaderSectionCollapsed ? 0 : std::max(1, int(addShaderRows.size()));
}

JPMidiKeymap::PanelLayout JPMidiKeymap::getPanelLayout() const
{
	PanelLayout layout;
	layout.innerX = PANEL_X + PAD;
	layout.innerW = PANEL_W - PAD * 2;
	layout.headerY = PANEL_Y + PAD;
	layout.paramY = PANEL_Y + PARAM_Y_OFFSET;
	layout.globalY = layout.paramY + PARAM_HEADER_H + getParameterRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING;
	layout.addShaderY = layout.globalY + PARAM_HEADER_H + getGlobalActionRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING;
	layout.targetBoxY = layout.addShaderY + PARAM_HEADER_H + getAddShaderRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING;
	layout.actionY = layout.targetBoxY + ROW_H + SECTION_VERTICAL_SPACING;
	layout.bindingsY = layout.actionY + ROW_H + SECTION_VERTICAL_SPACING + 4;

	float contentBottom = layout.bindingsY + 16 + getNonParameterBindingCount() * (ROW_H + 4) + PANEL_BOTTOM_PAD;
	layout.panelH = ofClamp(contentBottom - PANEL_Y, PANEL_MIN_H, ofGetHeight() - PANEL_Y * 2);
	return layout;
}

JPMidiKeymap::DropdownLayout JPMidiKeymap::getTargetBoxDropdownLayout(const PanelLayout &layout) const
{
	DropdownLayout dropdown;
	dropdown.x = layout.innerX + SELECT_LABEL_W;
	dropdown.y = layout.targetBoxY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = layout.innerW - SELECT_LABEL_W;
	int boxCount = boxes != nullptr ? boxes->boxes.size() : 0;
	dropdown.contentH = boxCount * (ROW_H + 2) + 2;
	dropdown.h = std::min(SELECT_DROPDOWN_MAX_H, dropdown.contentH);
	dropdown.showScrollbar = dropdown.contentH > dropdown.h;
	dropdown.maxScrollY = std::max(0.0f, dropdown.contentH - dropdown.h);
	return dropdown;
}

JPMidiKeymap::DropdownLayout JPMidiKeymap::getActionDropdownLayout(const PanelLayout &layout) const
{
	DropdownLayout dropdown;
	dropdown.x = layout.innerX + SELECT_LABEL_W;
	dropdown.y = layout.actionY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = 200.0f;
	dropdown.contentH = getBoxActions().size() * (ROW_H + 2) + 4;
	dropdown.h = dropdown.contentH;
	return dropdown;
}

vector<JPMidiKeymap::Action> JPMidiKeymap::getGlobalActions() const
{
	vector<Action> actions;
	actions.push_back(NEXT_SHADER);
	actions.push_back(PREV_SHADER);
	actions.push_back(SET_ACTIVE_SHADER);
	actions.push_back(SET_ACTIVE_RENDER);
	actions.push_back(NEXT_SHADER_GALLERY);
	actions.push_back(PREV_SHADER_GALLERY);
	actions.push_back(TOGGLE_GALLERY);
	return actions;
}

vector<JPMidiKeymap::Action> JPMidiKeymap::getBoxActions() const
{
	vector<Action> actions;
	actions.push_back(BYPASS);
	actions.push_back(PAUSE);
	actions.push_back(SELECT_OPEN_BOX);
	actions.push_back(PARAMETER);
	return actions;
}

string JPMidiKeymap::getKeyId(const MidiKey &key) const
{
	return key.deviceName + "|" + ofToString(key.channel) + "|" + key.messageType + "|" + ofToString(key.number);
}

string JPMidiKeymap::getKeyLabel(const MidiKey &key) const
{
	return key.deviceName + " ch" + ofToString(key.channel) + " " + key.messageType + " " + ofToString(key.number);
}

string JPMidiKeymap::getActionName(Action action) const
{
	if (action == BYPASS) return "Bypass";
	if (action == PAUSE) return "Pause";
	if (action == PARAMETER) return "Parameter";
	if (action == SELECT_OPEN_BOX) return "Select/Open Box";
	if (action == NEXT_SHADER) return "Next Shader";
	if (action == PREV_SHADER) return "Prev Shader";
	if (action == SET_ACTIVE_SHADER) return "Set Active Shader";
	if (action == SET_ACTIVE_RENDER) return "Set Active Render";
	if (action == NEXT_SHADER_GALLERY) return "Next Shader Gallery";
	if (action == PREV_SHADER_GALLERY) return "Prev Shader Gallery";
	if (action == TOGGLE_GALLERY) return "Toggle Gallery Mode";
	if (action == ADD_SHADER_BOX) return "Add Shader Box";
	return "Unknown";
}

string JPMidiKeymap::actionToXml(Action action) const
{
	if (action == BYPASS) return "bypass";
	if (action == PAUSE) return "pause";
	if (action == PARAMETER) return "parameter";
	if (action == SELECT_OPEN_BOX) return "select_open_box";
	if (action == NEXT_SHADER) return "next_shader";
	if (action == PREV_SHADER) return "prev_shader";
	if (action == SET_ACTIVE_SHADER) return "set_active_shader";
	if (action == SET_ACTIVE_RENDER) return "set_active_render";
	if (action == NEXT_SHADER_GALLERY) return "next_shader_gallery";
	if (action == PREV_SHADER_GALLERY) return "prev_shader_gallery";
	if (action == TOGGLE_GALLERY) return "toggle_gallery";
	if (action == ADD_SHADER_BOX) return "add_shader_box";
	return "bypass";
}

JPMidiKeymap::Action JPMidiKeymap::actionFromXml(string value) const
{
	if (value == "pause") return PAUSE;
	if (value == "select_open_box") return SELECT_OPEN_BOX;
	if (value == "parameter") return PARAMETER;
	if (value == "next_shader") return NEXT_SHADER;
	if (value == "prev_shader") return PREV_SHADER;
	if (value == "set_active_shader") return SET_ACTIVE_SHADER;
	if (value == "set_active_render") return SET_ACTIVE_RENDER;
	if (value == "next_shader_gallery") return NEXT_SHADER_GALLERY;
	if (value == "prev_shader_gallery") return PREV_SHADER_GALLERY;
	if (value == "toggle_gallery") return TOGGLE_GALLERY;
	if (value == "add_shader_box") return ADD_SHADER_BOX;
	return BYPASS;
}

void JPMidiKeymap::draw()
{
	if (!panelOpen)
	{
		return;
	}
	if (boxes != nullptr && !boxes->boxes.empty() &&
		(selectedBoxName.empty() || !boxes->hasBoxName(selectedBoxName)))
	{
		selectedBoxName = boxes->boxes[0]->name;
	}
	selectedParameterIndex = ofClamp(selectedParameterIndex, 0, getGlobalParameterIndexCount() - 1);

	PanelLayout layout = getPanelLayout();
	ofSetRectMode(OF_RECTMODE_CORNER);
	
	// Glassmorphism background panel
	ofSetColor(12, 16, 20, 235);
	ofDrawRectRounded(PANEL_X, PANEL_Y, PANEL_W, layout.panelH, 12.0f);
	
	// Neon cyan border
	ofNoFill();
	ofSetColor(0, 230, 230, 255);
	ofSetLineWidth(2.0f);
	ofDrawRectRounded(PANEL_X, PANEL_Y, PANEL_W, layout.panelH, 12.0f);
	ofFill();
	ofSetLineWidth(1.0f);

	drawPanelHeader(layout.innerX, layout.headerY, layout.innerW);
	drawParameterIndexSelector(layout.innerX, layout.paramY, layout.innerW);
	drawGlobalFunctionsSelector(layout.innerX, layout.globalY, layout.innerW);
	drawAddShaderSelector(layout.innerX, layout.addShaderY, layout.innerW);
	drawBoxSelector(layout.innerX, layout.targetBoxY, layout.innerW);
	drawActionSelector(layout.innerX, layout.actionY, layout.innerW);
	drawBindings(layout.innerX, layout.bindingsY, layout.innerW);

	if (editMode)
	{
		ofSetColor(0, 230, 230, 255);
		string hint = learning ? "Press a MIDI key/control..." : "MIDI map edit: click a box button or inspector slider, then press MIDI";
		jp_constants::p_font.drawString(hint, layout.innerX, PANEL_Y + layout.panelH - 16);
	}

	// DRAW OVERLAYS FOR DROPDOWNS
	if (targetBoxSelectOpen && boxes != nullptr && !boxes->boxes.empty())
	{
		vector<string> names = boxes->getBoxNames();
		DropdownLayout dropdown = getTargetBoxDropdownLayout(layout);
		float scrollbarW = 12.0f;

		// Dropdown container background
		ofSetColor(15, 20, 25, 245);
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);

		// Dropdown border
		ofNoFill();
		ofSetColor(0, 230, 230, 255);
		ofSetLineWidth(1.5f);
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		ofFill();
		ofSetLineWidth(1.0f);

		// Clamp scroll offset
		targetBoxScrollY = ofClamp(targetBoxScrollY, 0.0f, dropdown.maxScrollY);

		// Draw option buttons inside scissor viewport
		glEnable(GL_SCISSOR_TEST);
		int scissorX = dropdown.x;
		int scissorY = ofGetHeight() - (dropdown.y + dropdown.h);
		int scissorW = dropdown.w;
		int scissorH = dropdown.h;
		glScissor(scissorX, scissorY, scissorW, scissorH);

		for (int i = 0; i < names.size(); i++)
		{
			float optionY = dropdown.y + 2 + i * (ROW_H + 2) - targetBoxScrollY;
			
			if (optionY + ROW_H < dropdown.y || optionY > dropdown.y + dropdown.h)
			{
				continue;
			}

			float btnW = dropdown.showScrollbar ? (dropdown.w - scrollbarW - 4) : (dropdown.w - 4);
			string optionLabel = fitLabel(names[i], btnW - 16);
			
			bool isSelected = (selectedBoxName == names[i]);
			bool isHovered = false;
			if (ofGetMouseX() >= dropdown.x && ofGetMouseX() <= dropdown.x + btnW &&
				ofGetMouseY() >= dropdown.y && ofGetMouseY() <= dropdown.y + dropdown.h &&
				ofGetMouseY() >= optionY && ofGetMouseY() <= optionY + ROW_H)
			{
				isHovered = true;
			}

			ofSetColor(isSelected ? ofColor(0, 160, 160, 220) : (isHovered ? ofColor(40, 50, 60, 230) : ofColor(20, 25, 30, 200)));
			ofDrawRectRounded(dropdown.x + 2, optionY, btnW, ROW_H, 3.0f);
			
			ofNoFill();
			ofSetColor(isHovered ? ofColor(255) : (isSelected ? ofColor(0, 230, 230, 200) : ofColor(60, 70, 80, 100)));
			ofDrawRectRounded(dropdown.x + 2, optionY, btnW, ROW_H, 3.0f);
			ofFill();

			ofSetColor(255);
			jp_constants::p_font.drawString(optionLabel, dropdown.x + 10, optionY + ROW_H - 7);
		}

		glDisable(GL_SCISSOR_TEST);

		// Draw scrollbar
		if (dropdown.showScrollbar)
		{
			float trackX = dropdown.x + dropdown.w - scrollbarW - 2;
			float trackY = dropdown.y + 2;
			float trackH = dropdown.h - 4;
			
			ofSetColor(30, 35, 40, 200);
			ofDrawRectRounded(trackX, trackY, scrollbarW, trackH, 4.0f);

			float thumbH = (dropdown.h / dropdown.contentH) * trackH;
			thumbH = std::max(thumbH, 20.0f);
			float thumbY = trackY + (targetBoxScrollY / dropdown.maxScrollY) * (trackH - thumbH);

			bool thumbHover = ofGetMouseX() >= trackX && ofGetMouseX() <= trackX + scrollbarW &&
							  ofGetMouseY() >= thumbY && ofGetMouseY() <= thumbY + thumbH;

			ofSetColor((scrollbarDragging || thumbHover) ? ofColor(0, 230, 230, 255) : ofColor(90, 100, 110, 200));
			ofDrawRectRounded(trackX + 2, thumbY, scrollbarW - 4, thumbH, 3.0f);
		}
	}
	
	if (actionSelectOpen)
	{
		DropdownLayout dropdown = getActionDropdownLayout(layout);
		
		ofSetColor(15, 20, 25, 245);
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		
		ofNoFill();
		ofSetColor(0, 230, 230, 255);
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		ofFill();
		
		vector<Action> actions = getBoxActions();
		for (int i = 0; i < actions.size(); i++)
		{
			float optionY = dropdown.y + 2 + i * (ROW_H + 2);
			bool isSelected = (selectedAction == actions[i]);
			bool isHovered = ofGetMouseX() >= dropdown.x && ofGetMouseX() <= dropdown.x + dropdown.w &&
							 ofGetMouseY() >= optionY && ofGetMouseY() <= optionY + ROW_H;
							 
			ofSetColor(isSelected ? ofColor(0, 160, 160, 220) : (isHovered ? ofColor(40, 50, 60, 230) : ofColor(20, 25, 30, 200)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			
			ofNoFill();
			ofSetColor(isHovered ? ofColor(255) : (isSelected ? ofColor(0, 230, 230, 200) : ofColor(60, 70, 80, 100)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			ofFill();
			
			ofSetColor(255);
			jp_constants::p_font.drawString(getActionName(actions[i]), dropdown.x + 10, optionY + ROW_H - 7);
		}
	}
}

void JPMidiKeymap::drawMappingTargets()
{
	if (!editMode)
	{
		return;
	}
	drawMapOnIndicator();
	if (boxes == nullptr)
	{
		return;
	}
	drawBoxMappingTargets();
	drawInspectorMappingTargets();
}

void JPMidiKeymap::drawBoxMappingTargets()
{
	for (int i = 0; i < boxes->boxes.size(); i++)
	{
		JPbox *box = boxes->boxes[i];
		bool boxOver = box->mouseOver();
		bool bypassOver = box->bypass.mouseOver();
		bool pauseOver = box->onoff.mouseOver();
		bool boxBound = hasBindingForAction(SELECT_OPEN_BOX, box->name);
		bool bypassBound = hasBindingForAction(BYPASS, box->name);
		bool pauseBound = hasBindingForAction(PAUSE, box->name);

		ofNoFill();
		ofSetLineWidth((boxOver || boxBound) ? 3 : 2);
		ofSetColor(boxBound ? ofColor(0, 255, 120, boxOver ? 255 : 220) :
							  (boxOver ? ofColor(255, 255, 0, 230) : ofColor(255, 255, 0, 120)));
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofDrawRectRounded(box->x, box->y, box->width + 8, box->height + 8, 8.0f);

		ofSetLineWidth((bypassOver || bypassBound) ? 3 : 2);
		ofSetColor(bypassBound ? ofColor(0, 255, 120, bypassOver ? 255 : 220) :
								 (bypassOver ? ofColor(255, 80, 80, 255) : ofColor(255, 80, 80, 170)));
		ofDrawRectRounded(box->bypass.x, box->bypass.y, box->bypass.width + 8, box->bypass.height + 8, 4.0f);

		ofSetLineWidth((pauseOver || pauseBound) ? 3 : 2);
		ofSetColor(pauseBound ? ofColor(0, 255, 120, pauseOver ? 255 : 220) :
							   (pauseOver ? ofColor(255, 255, 255, 255) : ofColor(255, 255, 255, 170)));
		ofDrawRectRounded(box->onoff.x, box->onoff.y, box->onoff.width + 8, box->onoff.height + 8, 4.0f);
		ofFill();
	}
}

void JPMidiKeymap::drawInspectorMappingTargets()
{
	if (boxes->openguinumber < 0 || boxes->openguinumber >= boxes->boxes.size())
	{
		return;
	}

	for (int i = 0; i < boxes->controllers.size(); i++)
	{
		int type = boxes->boxes[boxes->openguinumber]->parameters.getType(i);
		if (type != boxes->boxes[boxes->openguinumber]->parameters.FLOAT &&
			type != boxes->boxes[boxes->openguinumber]->parameters.BOOL)
		{
			continue;
		}

		JPcontroller *controller = boxes->controllers[i];
		bool over = controller->mouseOver();
		bool bound = hasBindingForAction(PARAMETER, "", i);
		ofNoFill();
		ofSetLineWidth((over || bound) ? 3 : 2);
		ofSetColor(bound ? ofColor(0, 255, 120, over ? 255 : 220) :
						   (over ? ofColor(0, 255, 255, 255) : ofColor(0, 255, 255, 160)));
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofDrawRectRounded(controller->x, controller->y, controller->width + 8, controller->height + 8, 4.0f);
		ofFill();

		ofSetColor(bound ? ofColor(0, 255, 120, over ? 255 : 220) :
						   ofColor(0, 255, 255, over ? 255 : 190));
		jp_constants::p_font.drawString("p" + ofToString(i),
										controller->x + controller->width / 2 + 8,
										controller->y + 4);
	}
}

void JPMidiKeymap::drawPanelHeader(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::h_font.drawString("MIDI Keymap", x, y + 18);
	drawButton(x + w - 174, y + 4, 82, 24, editMode ? "Map On" : "Map Off", editMode);
	drawButton(x + w - 84, y + 4, 84, 24, learning ? "Learning" : "Learn", learning);

	jp_constants::p_font.drawString("Inputs: " + ofToString(midiInputs.size()), x, y + 46);
	string last = hasLastKey ? getKeyLabel(lastKey) : "none";
	last = fitLabel("Last MIDI: " + last, w - 126);
	jp_constants::p_font.drawString(last, x + 110, y + 46);
}

void JPMidiKeymap::drawBoxSelector(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::p_font.drawString("Target box", x, y);
	if (boxes == nullptr || boxes->boxes.empty())
	{
		drawSelectField(x + SELECT_LABEL_W, y - SELECT_FIELD_Y_OFFSET, w - SELECT_LABEL_W, ROW_H, "No boxes available", false);
		return;
	}

	string label = selectedBoxName.empty() ? "None" : selectedBoxName;
	label = fitLabel(label, w - 140);
	drawSelectField(x + SELECT_LABEL_W, y - SELECT_FIELD_Y_OFFSET, w - SELECT_LABEL_W, ROW_H, label, targetBoxSelectOpen);
}

void JPMidiKeymap::drawParameterIndexSelector(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::p_font.drawString("Parameter index", x, y);
	drawButton(x + w - 78, y - SELECT_FIELD_Y_OFFSET, 78, ROW_H,
			   parameterSectionCollapsed ? "Show" : "Hide",
			   !parameterSectionCollapsed);
	if (boxes == nullptr)
	{
		jp_constants::p_font.drawString("No box group", x, y + 24);
		return;
	}

	if (parameterSectionCollapsed)
	{
		return;
	}

	ofSetColor(160);
	jp_constants::p_font.drawString("MIDI binding", x + w * 0.34, y);

	JPbox *parameterBox = getSelectedParameterBox();
	float rowY = y + PARAM_HEADER_H;
	for (int i = 0; i < getGlobalParameterIndexCount(); i++)
	{
		bool selected = selectedAction == PARAMETER && selectedParameterIndex == i;
		int bindingIndex = findParameterBindingForIndex(i);
		bool mapped = bindingIndex >= 0;
		bool over = mouseInRect(x, rowY, w, ROW_H);
		bool isBoolParameter = parameterBox != nullptr &&
							   i < parameterBox->parameters.getSize() &&
							   parameterBox->parameters.getType(i) == parameterBox->parameters.BOOL;
		bool boolValue = isBoolParameter &&
						 parameterBox->parameters.getBoolValue(i);

		ofSetRectMode(OF_RECTMODE_CORNER);
		if (isBoolParameter)
		{
			ofSetColor(boolValue ? ofColor(245, 245, 245, 235) :
								   (over ? ofColor(110, 110, 110, 230) : ofColor(85, 85, 85, 220)));
		}
		else
		{
			ofSetColor(selected ? ofColor(0, 160, 160, 220) :
								 (over ? ofColor(40, 48, 56, 230) : ofColor(25, 30, 35, 210)));
		}
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		
		ofNoFill();
		if (selected)
		{
			ofSetColor(0, 230, 230, 255);
		}
		else
		{
			ofSetColor(mapped ? ofColor(0, 230, 120, over ? 255 : 210) :
								(over ? ofColor(255) : ofColor(80, 90, 100, 150)));
		}
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();

		ofColor textColor = boolValue ? ofColor(20, 20, 20, 255) : ofColor(255);
		ofSetColor(textColor);
		jp_constants::p_font.drawString("p" + ofToString(i), x + 8, rowY + ROW_H - 7);
		if (isBoolParameter)
		{
			ofSetColor(boolValue ? ofColor(20, 20, 20, 255) : ofColor(230));
		}
		else
		{
			ofSetColor(mapped ? ofColor(0, 230, 120, 255) : ofColor(170));
		}
		string keyLabel = mapped ? getKeyLabel(bindings[bindingIndex].key) : "Unmapped";
		float keyMaxW = w - w * 0.34 - 116;
		keyLabel = fitLabel(keyLabel, keyMaxW);
		jp_constants::p_font.drawString(keyLabel, x + w * 0.34, rowY + ROW_H - 7);

		bool learningThisRow = learning &&
							   selectedAction == PARAMETER &&
							   selectedParameterIndex == i;
		drawButton(x + w - 92, rowY + 2, 48, ROW_H - 4, "Learn", learningThisRow);
		drawButton(x + w - 38, rowY + 2, 32, ROW_H - 4, "X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawGlobalFunctionsSelector(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::p_font.drawString("Global functions", x, y);
	drawButton(x + w - 78, y - SELECT_FIELD_Y_OFFSET, 78, ROW_H,
			   globalFunctionsCollapsed ? "Show" : "Hide",
			   !globalFunctionsCollapsed);
	if (boxes == nullptr)
	{
		return;
	}

	if (globalFunctionsCollapsed)
	{
		return;
	}

	ofSetColor(160);
	jp_constants::p_font.drawString("MIDI binding", x + w * 0.34, y);

	vector<Action> globalActions = getGlobalActions();

	float rowY = y + PARAM_HEADER_H;
	for (int i = 0; i < globalActions.size(); i++)
	{
		bool selected = selectedAction == globalActions[i];
		int bindingIndex = findGlobalActionBinding(globalActions[i]);
		bool mapped = bindingIndex >= 0;
		bool over = mouseInRect(x, rowY, w, ROW_H);

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(selected ? ofColor(0, 160, 160, 220) :
							 (over ? ofColor(40, 48, 56, 230) : ofColor(25, 30, 35, 210)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		
		ofNoFill();
		ofSetColor(mapped ? ofColor(0, 230, 120, over ? 255 : 210) :
							(over ? ofColor(255) : ofColor(80, 90, 100, 150)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();

		ofSetColor(255);
		jp_constants::p_font.drawString(getActionName(globalActions[i]), x + 8, rowY + ROW_H - 7);
		ofSetColor(mapped ? ofColor(0, 230, 120, 255) : ofColor(170));
		string keyLabel = mapped ? getKeyLabel(bindings[bindingIndex].key) : "Unmapped";
		float keyMaxW = w - w * 0.34 - 116;
		keyLabel = fitLabel(keyLabel, keyMaxW);
		jp_constants::p_font.drawString(keyLabel, x + w * 0.34, rowY + ROW_H - 7);

		bool learningThisRow = learning && selectedAction == globalActions[i];
		drawButton(x + w - 92, rowY + 2, 48, ROW_H - 4, "Learn", learningThisRow);
		drawButton(x + w - 38, rowY + 2, 32, ROW_H - 4, "X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawAddShaderSelector(float x, float y, float w)
{
	ensureAddShaderDraftRow();
	ofSetColor(255);
	jp_constants::p_font.drawString("Add shader box", x, y);
	drawButton(x + w - 78, y - SELECT_FIELD_Y_OFFSET, 78, ROW_H,
			   addShaderSectionCollapsed ? "Show" : "Hide",
			   !addShaderSectionCollapsed);

	if (addShaderSectionCollapsed)
	{
		return;
	}

	float rowY = y + PARAM_HEADER_H;
	for (int i = 0; i < addShaderRows.size(); i++)
	{
		string query = addShaderRows[i];
		int bindingIndex = findAddShaderBinding(query);
		bool mapped = bindingIndex >= 0;
		bool searched = i < addShaderSearched.size() && addShaderSearched[i];
		bool resolved = i < addShaderResolvedPaths.size() && !addShaderResolvedPaths[i].empty();
		bool focused = focusedAddShaderRow == i;

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(focused ? ofColor(0, 160, 160, 220) :
							 (mouseInRect(x, rowY, w, ROW_H) ? ofColor(40, 48, 56, 230) : ofColor(25, 30, 35, 210)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);

		ofNoFill();
		ofSetColor(!query.empty() && !resolved ? ofColor(230, 70, 70, 220) :
					(mapped ? ofColor(0, 230, 120, 230) :
							  (focused ? ofColor(0, 230, 230, 255) : ofColor(80, 90, 100, 150))));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();

		string queryLabel = query.empty() ? "type shader name" : query;
		queryLabel = fitLabel(queryLabel, w * 0.34 - 16);
		ofSetColor(query.empty() ? ofColor(150) : ofColor(255));
		jp_constants::p_font.drawString(queryLabel, x + 8, rowY + ROW_H - 7);

		string keyLabel = mapped ? getKeyLabel(bindings[bindingIndex].key) :
						  (!query.empty() && searched && !resolved ? "Not found" :
						   (!query.empty() && resolved ? "Found" : "Unmapped"));
		keyLabel = fitLabel(keyLabel, w - w * 0.34 - 166);
		ofSetColor(mapped ? ofColor(0, 230, 120, 255) :
					(!query.empty() && !resolved ? ofColor(255, 120, 120, 255) : ofColor(170)));
		jp_constants::p_font.drawString(keyLabel, x + w * 0.34, rowY + ROW_H - 7);

		bool learningThisRow = learning && selectedAction == ADD_SHADER_BOX && addShaderQuery == query;
		drawButton(x + w - 146, rowY + 2, 48, ROW_H - 4, "Find", resolved);
		drawButton(x + w - 92, rowY + 2, 48, ROW_H - 4, "Learn", learningThisRow);
		drawButton(x + w - 38, rowY + 2, 32, ROW_H - 4, "X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawActionSelector(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::p_font.drawString("Action", x, y);
	drawSelectField(x + SELECT_LABEL_W, y - SELECT_FIELD_Y_OFFSET, 200, ROW_H, getActionName(selectedAction), actionSelectOpen);
}

void JPMidiKeymap::drawBindings(float x, float y, float w)
{
	ofSetColor(255);
	jp_constants::p_font.drawString("Bindings", x, y);
	int rowIndex = 0;
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == PARAMETER)
		{
			continue;
		}

		float rowY = y + 16 + rowIndex * (ROW_H + 4);
		bool unresolved = boxes != nullptr &&
						  bindings[i].action != PARAMETER &&
						  bindings[i].action != ADD_SHADER_BOX &&
						  !isGlobalAction(bindings[i].action) &&
						  !boxes->hasBoxName(bindings[i].boxName);
		if (bindings[i].action == ADD_SHADER_BOX)
		{
			unresolved = bindings[i].shaderPath.empty();
		}
		ofSetColor(unresolved ? ofColor(120, 30, 30, 190) : ofColor(25, 30, 35, 210));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		
		ofNoFill();
		ofSetColor(unresolved ? ofColor(230, 70, 70, 200) : ofColor(0, 230, 230, 80));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();

		ofSetColor(255);
		string targetLabel = isGlobalAction(bindings[i].action) ? "Global" : bindings[i].boxName;
		if (bindings[i].action == ADD_SHADER_BOX)
		{
			targetLabel = bindings[i].shaderQuery;
		}
		string label = getKeyLabel(bindings[i].key) + " -> " + targetLabel + " / " + getActionName(bindings[i].action);
		if (unresolved)
		{
			label += " (missing)";
		}
		jp_constants::p_font.drawString(label, x + 8, rowY + ROW_H - 7);
		drawButton(x + w - 92, rowY + 2, 48, ROW_H - 4, "Learn", rebindIndex == i && learning);
		drawButton(x + w - 38, rowY + 2, 32, ROW_H - 4, "X", false);
		rowIndex++;
	}
}

bool JPMidiKeymap::mousePressed(int x, int y, int button)
{
	if (!panelOpen)
	{
		return false;
	}

	PanelLayout layout = getPanelLayout();
	if (!pointInRect(x, y, PANEL_X, PANEL_Y, PANEL_W, layout.panelH))
	{
		return false;
	}

	if (button != OF_MOUSE_BUTTON_LEFT)
	{
		return true;
	}

	// Check header buttons
	if (pointInRect(x, y, layout.innerX + layout.innerW - 174, layout.headerY + 4, 82, 24))
	{
		editMode = !editMode;
		learning = false;
		rebindIndex = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		return true;
	}
	if (pointInRect(x, y, layout.innerX + layout.innerW - 84, layout.headerY + 4, 84, 24))
	{
		learning = !learning;
		rebindIndex = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		return true;
	}

	if (pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.paramY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		parameterSectionCollapsed = !parameterSectionCollapsed;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		return true;
	}

	if (pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.globalY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		globalFunctionsCollapsed = !globalFunctionsCollapsed;
		focusedAddShaderRow = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		return true;
	}

	if (pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.addShaderY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		addShaderSectionCollapsed = !addShaderSectionCollapsed;
		focusedAddShaderRow = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		return true;
	}

	// If Target Box dropdown overlay is open, handle its clicks
	if (targetBoxSelectOpen && boxes != nullptr && !boxes->boxes.empty())
	{
		vector<string> names = boxes->getBoxNames();
		DropdownLayout dropdown = getTargetBoxDropdownLayout(layout);
		float scrollbarW = 12.0f;

		if (pointInRect(x, y, dropdown.x, dropdown.y, dropdown.w, dropdown.h))
		{
			if (dropdown.showScrollbar && x >= dropdown.x + dropdown.w - scrollbarW - 2)
			{
				// Clicked scrollbar
				float trackY = dropdown.y + 2;
				float trackH = dropdown.h - 4;
				float thumbH = (dropdown.h / dropdown.contentH) * trackH;
				thumbH = std::max(thumbH, 20.0f);
				float thumbY = trackY + (targetBoxScrollY / dropdown.maxScrollY) * (trackH - thumbH);

				if (y >= thumbY && y <= thumbY + thumbH)
				{
					scrollbarDragging = true;
					dragStartY = y;
					dragStartScrollY = targetBoxScrollY;
				}
				else
				{
					// Clicked track outside thumb
					float relativeY = (y - trackY - thumbH / 2.0f) / (trackH - thumbH);
					targetBoxScrollY = ofClamp(relativeY * dropdown.maxScrollY, 0.0f, dropdown.maxScrollY);
				}
			}
			else
			{
				// Clicked option item
				float clickY = y - dropdown.y + targetBoxScrollY - 2;
				int clickedIndex = clickY / (ROW_H + 2);
				if (clickedIndex >= 0 && clickedIndex < names.size())
				{
					selectedBoxName = names[clickedIndex];
					targetBoxSelectOpen = false;
				}
			}
			return true;
		}
		else
		{
			// Clicked outside dropdown
			targetBoxSelectOpen = false;
		}
	}

	// If Action dropdown overlay is open, handle its clicks
	if (actionSelectOpen)
	{
		DropdownLayout dropdown = getActionDropdownLayout(layout);

		if (pointInRect(x, y, dropdown.x, dropdown.y, dropdown.w, dropdown.h))
		{
			float clickY = y - dropdown.y - 2;
			int clickedIndex = clickY / (ROW_H + 2);
			vector<Action> actions = getBoxActions();
			if (clickedIndex >= 0 && clickedIndex < actions.size())
			{
				selectedAction = actions[clickedIndex];
				actionSelectOpen = false;
			}
			return true;
		}
		else
		{
			// Clicked outside dropdown
			actionSelectOpen = false;
		}
	}

	// Check Target Box select field box click
	if (boxes != nullptr)
	{
		if (pointInRect(x, y, layout.innerX + SELECT_LABEL_W, layout.targetBoxY - SELECT_FIELD_Y_OFFSET, layout.innerW - SELECT_LABEL_W, ROW_H))
		{
			focusedAddShaderRow = -1;
			targetBoxSelectOpen = !targetBoxSelectOpen;
			actionSelectOpen = false;
			return true;
		}
	}

	// Check Action select field box click
	if (pointInRect(x, y, layout.innerX + SELECT_LABEL_W, layout.actionY - SELECT_FIELD_Y_OFFSET, 200, ROW_H))
	{
		focusedAddShaderRow = -1;
		actionSelectOpen = !actionSelectOpen;
		targetBoxSelectOpen = false;
		return true;
	}

	// Check parameter list clicks
	if (boxes != nullptr && !parameterSectionCollapsed)
	{
		float rowY = layout.paramY + PARAM_HEADER_H;
		float rowW = layout.innerW;
		for (int i = 0; i < getGlobalParameterIndexCount(); i++)
		{
			if (pointInRect(x, y, layout.innerX + rowW - 92, rowY + 2, 48, ROW_H - 4))
			{
				Binding binding;
				binding.action = PARAMETER;
				binding.parameterIndex = i;
				focusedAddShaderRow = -1;
				armLearn(binding, findParameterBindingForIndex(i));
				return true;
			}
			if (pointInRect(x, y, layout.innerX + rowW - 38, rowY + 2, 32, ROW_H - 4))
			{
				int bindingIndex = findParameterBindingForIndex(i);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
				}
				learning = false;
				rebindIndex = -1;
				return true;
			}
			if (pointInRect(x, y, layout.innerX, rowY, rowW, ROW_H))
			{
				focusedAddShaderRow = -1;
				selectedParameterIndex = i;
				selectedAction = PARAMETER;
				return true;
			}
			rowY += ROW_H + 4;
		}
	}

	if (boxes != nullptr && !globalFunctionsCollapsed)
	{
		vector<Action> globalActions = getGlobalActions();
		float rowY = layout.globalY + PARAM_HEADER_H;
		for (int i = 0; i < globalActions.size(); i++)
		{
			if (pointInRect(x, y, layout.innerX + layout.innerW - 92, rowY + 2, 48, ROW_H - 4))
			{
				Binding binding;
				binding.action = globalActions[i];
				focusedAddShaderRow = -1;
				armLearn(binding, findGlobalActionBinding(globalActions[i]));
				return true;
			}
			if (pointInRect(x, y, layout.innerX + layout.innerW - 38, rowY + 2, 32, ROW_H - 4))
			{
				int bindingIndex = findGlobalActionBinding(globalActions[i]);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
				}
				learning = false;
				rebindIndex = -1;
				return true;
			}
			if (pointInRect(x, y, layout.innerX, rowY, layout.innerW, ROW_H))
			{
				focusedAddShaderRow = -1;
				selectedAction = globalActions[i];
				return true;
			}
			rowY += ROW_H + 4;
		}
	}

	if (!addShaderSectionCollapsed)
	{
		float rowY = layout.addShaderY + PARAM_HEADER_H;
		ensureAddShaderDraftRow();
		for (int i = 0; i < addShaderRows.size(); i++)
		{
			if (pointInRect(x, y, layout.innerX + layout.innerW - 146, rowY + 2, 48, ROW_H - 4))
			{
				resolveAddShaderRow(i);
				return true;
			}
			if (pointInRect(x, y, layout.innerX + layout.innerW - 92, rowY + 2, 48, ROW_H - 4))
			{
				if (!addShaderRows[i].empty())
				{
					if (i >= addShaderResolvedPaths.size() || addShaderResolvedPaths[i].empty())
					{
						resolveAddShaderRow(i);
					}
					Binding binding;
					binding.action = ADD_SHADER_BOX;
					binding.shaderQuery = addShaderRows[i];
					binding.shaderPath = i < addShaderResolvedPaths.size() ? addShaderResolvedPaths[i] : "";
					addShaderQuery = addShaderRows[i];
					focusedAddShaderRow = -1;
					armLearn(binding, findAddShaderBinding(addShaderRows[i]));
				}
				return true;
			}
			if (pointInRect(x, y, layout.innerX + layout.innerW - 38, rowY + 2, 32, ROW_H - 4))
			{
				int bindingIndex = findAddShaderBinding(addShaderRows[i]);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
				}
				addShaderRows.erase(addShaderRows.begin() + i);
				if (i < addShaderResolvedPaths.size())
				{
					addShaderResolvedPaths.erase(addShaderResolvedPaths.begin() + i);
				}
				if (i < addShaderSearched.size())
				{
					addShaderSearched.erase(addShaderSearched.begin() + i);
				}
				focusedAddShaderRow = -1;
				ensureAddShaderDraftRow();
				learning = false;
				rebindIndex = -1;
				return true;
			}
			if (pointInRect(x, y, layout.innerX, rowY, layout.innerW - 98, ROW_H))
			{
				focusedAddShaderRow = i;
				addShaderQuery = addShaderRows[i];
				selectedAction = ADD_SHADER_BOX;
				targetBoxSelectOpen = false;
				actionSelectOpen = false;
				return true;
			}
			rowY += ROW_H + 4;
		}
		focusedAddShaderRow = -1;
	}

	// Check Bindings list clicks
	int bindingRow = 0;
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].action == PARAMETER)
		{
			continue;
		}

		float rowY = layout.bindingsY + 16 + bindingRow * (ROW_H + 4);
		if (pointInRect(x, y, layout.innerX + layout.innerW - 92, rowY + 2, 48, ROW_H - 4))
		{
			selectedBoxName = bindings[i].boxName;
			selectedAction = bindings[i].action;
			selectedParameterIndex = bindings[i].parameterIndex;
			if (bindings[i].action == ADD_SHADER_BOX)
			{
				addShaderQuery = bindings[i].shaderQuery;
			}
			learning = true;
			rebindIndex = i;
			return true;
		}
		if (pointInRect(x, y, layout.innerX + layout.innerW - 38, rowY + 2, 32, ROW_H - 4))
		{
			bindings.erase(bindings.begin() + i);
			learning = false;
			rebindIndex = -1;
			return true;
		}
		bindingRow++;
	}

	return true;
}

bool JPMidiKeymap::keyPressed(int key)
{
	if (!panelOpen || focusedAddShaderRow < 0 || focusedAddShaderRow >= addShaderRows.size())
	{
		return false;
	}

	if (key == OF_KEY_ESC || key == 27)
	{
		focusedAddShaderRow = -1;
		return true;
	}
	if (key == OF_KEY_BACKSPACE || key == 8 || key == 127)
	{
		if (!addShaderRows[focusedAddShaderRow].empty())
		{
			addShaderRows[focusedAddShaderRow].erase(addShaderRows[focusedAddShaderRow].size() - 1);
			addShaderQuery = addShaderRows[focusedAddShaderRow];
			addShaderResolvedPaths[focusedAddShaderRow] = "";
			addShaderSearched[focusedAddShaderRow] = false;
		}
		return true;
	}
	if (key == OF_KEY_RETURN || key == '\r' || key == '\n')
	{
		if (!addShaderRows[focusedAddShaderRow].empty())
		{
			if (addShaderResolvedPaths[focusedAddShaderRow].empty())
			{
				resolveAddShaderRow(focusedAddShaderRow);
			}
			Binding binding;
			binding.action = ADD_SHADER_BOX;
			binding.shaderQuery = addShaderRows[focusedAddShaderRow];
			binding.shaderPath = addShaderResolvedPaths[focusedAddShaderRow];
			addShaderQuery = addShaderRows[focusedAddShaderRow];
			focusedAddShaderRow = -1;
			armLearn(binding, findAddShaderBinding(addShaderQuery));
		}
		return true;
	}
	if (key >= 32 && key <= 126)
	{
		addShaderRows[focusedAddShaderRow] += char(key);
		addShaderQuery = addShaderRows[focusedAddShaderRow];
		addShaderResolvedPaths[focusedAddShaderRow] = "";
		addShaderSearched[focusedAddShaderRow] = false;
		ensureAddShaderDraftRow();
		return true;
	}
	return true;
}

bool JPMidiKeymap::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
	if (!panelOpen)
	{
		return false;
	}

	if (targetBoxSelectOpen && boxes != nullptr && !boxes->boxes.empty())
	{
		PanelLayout layout = getPanelLayout();
		DropdownLayout dropdown = getTargetBoxDropdownLayout(layout);

		if (pointInRect(x, y, dropdown.x, dropdown.y, dropdown.w, dropdown.h))
		{
			targetBoxScrollY = ofClamp(targetBoxScrollY - scrollY * 24.0f, 0.0f, dropdown.maxScrollY);
			return true;
		}
	}

	return false;
}

void JPMidiKeymap::mouseDragged(int x, int y, int button)
{
	if (scrollbarDragging && targetBoxSelectOpen && boxes != nullptr && !boxes->boxes.empty())
	{
		PanelLayout layout = getPanelLayout();
		DropdownLayout dropdown = getTargetBoxDropdownLayout(layout);

		float trackH = dropdown.h - 4;
		float thumbH = (dropdown.h / dropdown.contentH) * trackH;
		thumbH = std::max(thumbH, 20.0f);

		float deltaY = y - dragStartY;
		float scrollDelta = (deltaY / (trackH - thumbH)) * dropdown.maxScrollY;
		targetBoxScrollY = ofClamp(dragStartScrollY + scrollDelta, 0.0f, dropdown.maxScrollY);
	}
}

void JPMidiKeymap::mouseReleased(int x, int y, int button)
{
	scrollbarDragging = false;
}

bool JPMidiKeymap::captureFunctionClick(int x, int y, int button)
{
	if (!editMode || button != OF_MOUSE_BUTTON_LEFT || boxes == nullptr)
	{
		return false;
	}
	if (tryCaptureBoxFunctionClick(x, y))
	{
		return true;
	}
	if (tryCaptureInspectorFunctionClick(x, y))
	{
		return true;
	}
	return false;
}

bool JPMidiKeymap::tryCaptureBoxFunctionClick(int x, int y)
{
	for (int i = int(boxes->boxes.size()) - 1; i >= 0; i--)
	{
		JPbox *box = boxes->boxes[i];
		Binding binding;
		binding.boxName = box->name;
		if (box->bypass.mouseOver())
		{
			binding.action = BYPASS;
			armLearn(binding);
			return true;
		}
		if (box->onoff.mouseOver())
		{
			binding.action = PAUSE;
			armLearn(binding);
			return true;
		}
		if (box->mouseOver())
		{
			binding.action = SELECT_OPEN_BOX;
			armLearn(binding);
			return true;
		}
	}
	return false;
}

bool JPMidiKeymap::tryCaptureInspectorFunctionClick(int x, int y)
{
	if (boxes->openguinumber < 0 || boxes->openguinumber >= boxes->boxes.size())
	{
		return false;
	}
	for (int i = 0; i < boxes->controllers.size(); i++)
	{
		int type = boxes->boxes[boxes->openguinumber]->parameters.getType(i);
		if (boxes->controllers[i]->mouseOver() &&
			(type == boxes->boxes[boxes->openguinumber]->parameters.FLOAT ||
			 type == boxes->boxes[boxes->openguinumber]->parameters.BOOL))
		{
			Binding binding;
			binding.action = PARAMETER;
			binding.parameterIndex = i;
			armLearn(binding);
			return true;
		}
	}
	return false;
}

void JPMidiKeymap::togglePanel()
{
	panelOpen = !panelOpen;
}

bool JPMidiKeymap::isPanelOpen() const
{
	return panelOpen;
}

void JPMidiKeymap::saveToSession(string path)
{
	ofXml xml;
	if (!xml.load(path))
	{
		return;
	}

	xml.removeChild("midikeymap");
	auto keymap = xml.appendChild("midikeymap");
	for (int i = 0; i < bindings.size(); i++)
	{
		auto binding = keymap.appendChild("binding");
		binding.appendChild("device").set(bindings[i].key.deviceName);
		binding.appendChild("channel").set(bindings[i].key.channel);
		binding.appendChild("type").set(bindings[i].key.messageType);
		binding.appendChild("number").set(bindings[i].key.number);
		binding.appendChild("box").set(bindings[i].boxName);
		binding.appendChild("action").set(actionToXml(bindings[i].action));
		binding.appendChild("parameterindex").set(bindings[i].parameterIndex);
		if (bindings[i].action == ADD_SHADER_BOX)
		{
			binding.appendChild("shaderquery").set(bindings[i].shaderQuery);
			binding.appendChild("shaderpath").set(bindings[i].shaderPath);
		}
	}
	xml.save(path);
}

void JPMidiKeymap::loadFromSession(string path)
{
	bindings.clear();
	learning = false;
	rebindIndex = -1;

	ofXml xml;
	if (!xml.load(path))
	{
		ensureAddShaderDraftRow();
		return;
	}

	auto keymap = xml.getChild("midikeymap");
	if (!keymap)
	{
		ensureAddShaderDraftRow();
		return;
	}

	for (auto &bindingNode : keymap.getChildren("binding"))
	{
		Binding binding;
		binding.key.deviceName = bindingNode.getChild("device").getValue();
		binding.key.channel = bindingNode.getChild("channel").getIntValue();
		binding.key.messageType = bindingNode.getChild("type").getValue();
		binding.key.number = bindingNode.getChild("number").getIntValue();
		binding.boxName = bindingNode.getChild("box").getValue();
		binding.action = actionFromXml(bindingNode.getChild("action").getValue());
		binding.parameterIndex = bindingNode.getChild("parameterindex").getIntValue();
		auto shaderQuery = bindingNode.getChild("shaderquery");
		binding.shaderQuery = shaderQuery ? shaderQuery.getValue() : "";
		auto shaderPath = bindingNode.getChild("shaderpath");
		binding.shaderPath = shaderPath ? shaderPath.getValue() : "";
		bindings.push_back(binding);
	}
	syncAddShaderRowsFromBindings();
}
