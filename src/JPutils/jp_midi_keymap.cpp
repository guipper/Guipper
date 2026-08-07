#include "jp_midi_keymap.h"
#include "jp_tooltip.h"
#include "../JPgui/jp_screen.h"
#include "../JPgui/jp_button.h"
#include "jp_pointer.h"
#include <algorithm>
#include <cctype>

namespace
{
	const int MIDI_CC_THRESHOLD = 64;
	// Geometry comes from the shared screen chrome now that MIDI is a screen
	// rather than a floating panel. The frame spans the window; the content
	// stays in a readable column so a wide monitor does not stretch the rows.
	ofRectangle panelFrame()
	{
		return jp_screen::frame();
	}
	const float ROW_H = 24;
	const float PAD = 12;
	const float PARAM_HEADER_H = 28;
	// Measured from the previous section's last row bottom to the next
	// heading's baseline. It must clear SELECT_FIELD_Y_OFFSET, because the
	// heading's Show/Hide button straddles the baseline by that much.
	const float SECTION_VERTICAL_SPACING = 42;
	// The right column is single rows with no straddling Show/Hide button, so
	// it does not need the left column's clearance.
	const float RIGHT_ROW_SPACING = 20;
	const float PANEL_BOTTOM_PAD = 54.0f;
	const float SELECT_LABEL_W = 100.0f;
	const float SELECT_FIELD_Y_OFFSET = 15.0f;
	const float DROPDOWN_GAP = 4.0f;
	const float SELECT_DROPDOWN_MAX_H = 420.0f;

	bool mouseInRect(float x, float y, float w, float h)
	{
		return ofGetMouseX() >= x && ofGetMouseX() <= x + w &&
			   ofGetMouseY() >= y && ofGetMouseY() <= y + h;
	}

	bool pointInRect(float px, float py, float x, float y, float w, float h)
	{
		return px >= x && px <= x + w && py >= y && py <= y + h;
	}

	// Thin wrapper over the shared renderer. The old body drew its own chrome
	// and left-aligned the label at x+8, which put the glyph hard against the
	// edge of a 32px "X" and nearly overflowed a 48px "Learn". jp_button
	// centres on both axes and has hover and disabled states; only the tooltip
	// table below is local to this screen.
	void drawButton(float x, float y, float w, float h, const string &label, bool selected)
	{
		jp_button::draw(ofRectangle(x, y, w, h), label, selected);
		string tooltip;
		// Learn and X are on every row of both lists, so their tooltips fired
		// constantly while reading down a column and said nothing the label did
		// not already say.
		if (label == "Map On" || label == "Map Off") tooltip = "Toggle MIDI mapping mode";
		else if (label == "Rescan") tooltip = "Rescan connected MIDI devices";
		else if (label == "Find") tooltip = "Search for this shader";
		jp_tooltip::draw(tooltip, x, y, w, h);
	}

	void drawSelectField(float x, float y, float w, float h, const string &label, bool open)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(ofColor(COL_BG_PANEL, 240));
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofNoFill();
		if (mouseInRect(x, y, w, h) || open) {
			ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
			ofSetLineWidth(1.5f);
		} else {
			ofSetColor(ofColor(COL_TEXT_MUTED, 180));
			ofSetLineWidth(1.0f);
		}
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(label, x + 8, y + h - 7);
		ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
		jp_constants::p_font.drawString(open ? "^" : "v", x + w - 18, y + h - 7);
	}

	// Right-hand end of the top tab bar, so the badge can sit clear of it.
	void drawMapOnIndicator(float chromeRightEdge)
	{
		const float w = 132.0f;
		const float h = 24.0f;
		// This used to be pinned at 12,12 - directly on top of the NODES tab,
		// which is drawn after it and therefore hid it almost completely.
		// Right-align it in the same row instead, and drop below the group tab
		// bar on a window too narrow for the row to hold both.
		float x = ofGetWidth() - 12.0f - w;
		float y = 12.0f;
		if (x < chromeRightEdge + 12.0f)
		{
			y = 84.0f;
		}
		x = std::max(x, 12.0f);
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(245, 215, 70, 245);
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofNoFill();
		ofSetColor(30, 30, 30, 220);
		ofDrawRectRounded(x, y, w, h, 4.0f);
		ofFill();
		ofSetColor(COL_TEXT_DARK);
		jp_constants::p_font.drawString("MIDI MAP ON", x + 10, y + h - 7);
		jp_tooltip::draw("MIDI mapping mode is active", x, y, w, h);
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

	string portableShaderStem(string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		size_t slash = path.find_last_of('/');
		string filename = slash == string::npos ? path : path.substr(slash + 1);
		size_t dot = filename.find_last_of('.');
		if (dot != string::npos)
		{
			filename = filename.substr(0, dot);
		}
		return normalizeText(filename);
	}

	bool isAbsolutePath(string path)
	{
		return path.find(":/") != string::npos ||
			   path.find(":\\") != string::npos ||
			   path.find("/") == 0 ||
			   path.find("\\") == 0;
	}

	bool shaderPathExists(string path)
	{
		return !path.empty() && (ofFile::doesFileExist(path) || ofFile::doesFileExist(ofToDataPath(path)));
	}
}

void JPMidiKeymap::setup(
	JPboxgroup *_boxes, std::function<void()> _bpmTapCallback)
{
	boxes = _boxes;
	bpmTapCallback = _bpmTapCallback;
	globalKeymapPath = ofToDataPath("midi_keymap.xml");
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
	inputsOpen = true;
	ccHighState.clear();
	availableDeviceNames.clear();

	ofxMidiIn midiProbe;
	int numPorts = midiProbe.getNumInPorts();
	for (int i = 0; i < numPorts; i++)
	{
		string portName = midiProbe.getInPortName(i);
		if (portName.empty())
		{
			portName = "port " + ofToString(i);
		}
		portName = normalizeDeviceName(portName);
		if (std::find(availableDeviceNames.begin(), availableDeviceNames.end(), portName) == availableDeviceNames.end())
		{
			availableDeviceNames.push_back(portName);
		}
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
	if (!inputsOpen && midiInputs.empty())
	{
		return;
	}

	for (int i = 0; i < midiInputs.size(); i++)
	{
		if (midiInputs[i] != nullptr)
		{
			midiInputs[i]->removeListener(this);
			midiInputs[i]->closePort();
			delete midiInputs[i];
		}
	}
	midiInputs.clear();
	inputsOpen = false;
}

void JPMidiKeymap::setActiveMapDevice(string deviceName)
{
	activeMapDeviceName = deviceName;
	refreshActiveDeviceCache();
	mapDeviceSelectOpen = false;
	cancelLearning();
	rebindIndex = -1;
	syncAddShaderRowsFromBindings();
}

bool JPMidiKeymap::isActiveMapDevice(const string &deviceName) const
{
	if (activeMapDeviceName.empty()) return true;
	return normalizeDeviceName(deviceName) == activeMapDeviceNormalized;
}

void JPMidiKeymap::refreshActiveDeviceCache()
{
	activeMapDeviceNormalized = normalizeDeviceName(activeMapDeviceName);
}

// Strip a trailing ALSA "client:port" id so a controller keeps the same profile
// across replug/reboot (its id can change, e.g. "Impulse  24:0" -> "Impulse  28:0").
// Leaves names without such a suffix untouched (e.g. the "port N" fallback).
string JPMidiKeymap::normalizeDeviceName(string deviceName) const
{
	auto rtrim = [](string &s)
	{
		while (!s.empty() && std::isspace((unsigned char)s.back()))
		{
			s.pop_back();
		}
	};

	rtrim(deviceName);
	size_t spacePos = deviceName.find_last_of(' ');
	if (spacePos == string::npos)
	{
		return deviceName;
	}

	string tail = deviceName.substr(spacePos + 1);
	size_t colonPos = tail.find(':');
	if (colonPos == string::npos || colonPos == 0 || colonPos == tail.size() - 1)
	{
		return deviceName;
	}
	for (size_t i = 0; i < tail.size(); i++)
	{
		if (i == colonPos)
		{
			continue;
		}
		if (!std::isdigit((unsigned char)tail[i]))
		{
			return deviceName;
		}
	}

	deviceName = deviceName.substr(0, spacePos);
	rtrim(deviceName);
	return deviceName;
}

vector<string> JPMidiKeymap::getMapDeviceNames() const
{
	vector<string> names = availableDeviceNames;
	for (int i = 0; i < bindings.size(); i++)
	{
		if (!bindings[i].key.deviceName.empty() &&
			std::find(names.begin(), names.end(), bindings[i].key.deviceName) == names.end())
		{
			names.push_back(bindings[i].key.deviceName);
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

void JPMidiKeymap::ensureActiveMapDevice()
{
	vector<string> names = getMapDeviceNames();
	if (!activeMapDeviceName.empty() &&
		std::find(names.begin(), names.end(), activeMapDeviceName) != names.end())
	{
		return;
	}
	// Prefer a currently-connected device so the panel opens on a controller
	// you can actually press; fall back to any device that has bindings.
	if (!availableDeviceNames.empty())
	{
		activeMapDeviceName = availableDeviceNames[0];
		refreshActiveDeviceCache();
	}
	else
	{
		activeMapDeviceName = names.empty() ? "" : names[0];
		refreshActiveDeviceCache();
	}
}

void JPMidiKeymap::update()
{
	invalidateBindingCache();
	ensureActiveMapDevice();
	processPendingShaderAdds();

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
	key.deviceName = normalizeDeviceName(msg.portName.empty() ? "port " + ofToString(msg.portNum) : msg.portName);
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
		// Any continuous binding on this key makes the whole key continuous,
		// otherwise a keep-both pair would edge-trigger its knob.
		bool isContinuousBinding = false;
		if (!learning)
		{
			for (int i = 0; i < (int)bindings.size() && !isContinuousBinding; i++)
			{
				if (getKeyId(bindings[i].key) != ccId) continue;
				isContinuousBinding = bindings[i].action == PARAMETER ||
					bindings[i].action == BYPASS ||
					bindings[i].action == PAUSE;
			}
		}
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

	// All connected devices are live simultaneously: apply the binding for the
	// device that actually sent this message, regardless of which profile the
	// panel is currently editing (activeMapDeviceName).
	// Every binding on this key fires, not just the first. One pad can drive
	// two actions - which is what the "keep both" answer to a conflict means.
	const string keyId = getKeyId(key);
	for (int i = 0; i < (int)bindings.size(); i++)
	{
		if (getKeyId(bindings[i].key) == keyId)
		{
			applyBinding(bindings[i], key.value);
		}
	}
}

void JPMidiKeymap::learnKey(const MidiKey &key)
{
	if (!hasLearnTarget())
	{
		cancelLearning();
		return;
	}

	activeMapDeviceName = key.deviceName;
	refreshActiveDeviceCache();
	mapDeviceSelectOpen = false;

	Binding binding;
	binding.key = key;
	binding.boxName = (isGlobalAction(selectedAction) || selectedAction == ADD_SHADER_BOX) ? "" : selectedBoxName;
	binding.action = selectedAction;
	binding.parameterIndex = selectedParameterIndex;
	binding.shaderQuery = selectedAction == ADD_SHADER_BOX ? addShaderQuery : "";
	binding.shaderPath = selectedAction == ADD_SHADER_BOX ? getAddShaderRowPath(addShaderQuery) : "";
	if (selectedAction == ADD_SHADER_BOX && binding.shaderPath.empty())
	{
		binding.shaderPath = resolveShaderQuery(addShaderQuery);
	}

	// Rebinding an existing row is not a conflict - that row IS the target.
	const int clash = findBindingForKey(key);
	if (rebindIndex < 0 && clash >= 0)
	{
		pendingBinding = binding;
		conflictCount = 0;
		const string keyId = getKeyId(key);
		for (int i = 0; i < (int)bindings.size(); i++)
			if (getKeyId(bindings[i].key) == keyId) conflictCount++;
		conflictExistingLabel = describeBinding(bindings[clash]);
		conflictPromptOpen = true;
		learning = false;
		return;
	}

	commitLearnedBinding(binding, true);
}

void JPMidiKeymap::commitLearnedBinding(const Binding &binding,
	bool replaceExisting)
{
	if (replaceExisting && rebindIndex < 0)
	{
		removeBindingForKey(binding.key, false);
	}
	if (rebindIndex >= 0 && rebindIndex < (int)bindings.size())
	{
		bindings[rebindIndex] = binding;
	}
	else
	{
		bindings.push_back(binding);
	}
	invalidateBindingCache();
	learning = false;
	rebindIndex = -1;
	ensureAddShaderDraftRow();
	saveGlobal();
}

ofRectangle JPMidiKeymap::conflictPromptRect() const
{
	const ofRectangle f = panelFrame();
	const float bw = 460.0f, bh = 150.0f;
	return ofRectangle(f.getCenter().x - bw * 0.5f,
		f.getCenter().y - bh * 0.5f, bw, bh);
}

ofRectangle JPMidiKeymap::conflictButtonRect(int index) const
{
	const ofRectangle box = conflictPromptRect();
	const float h = 26.0f;
	const float y = box.getMaxY() - h - 14.0f;
	if (index == 2) return ofRectangle(box.getMaxX() - 16.0f - 90.0f, y, 90.0f, h);
	const float w = 130.0f;
	return ofRectangle(box.x + 16.0f + (float)index * (w + 8.0f), y, w, h);
}

void JPMidiKeymap::resolveConflict(bool keepBoth)
{
	if (!conflictPromptOpen) return;
	conflictPromptOpen = false;
	commitLearnedBinding(pendingBinding, !keepBoth);
}

void JPMidiKeymap::cancelConflict()
{
	conflictPromptOpen = false;
	learning = false;
	rebindIndex = -1;
}

void JPMidiKeymap::beginAddShaderLearn(const string &shaderQuery)
{
	// Arm an ADD_SHADER_BOX learn without opening the MIDI panel, so the Import
	// page can capture the next MIDI message inline. Rebinds if this shader is
	// already mapped. learnKey() resolves the path from the query on capture.
	if (shaderQuery.empty())
	{
		return;
	}
	selectedAction = ADD_SHADER_BOX;
	selectedBoxName = "";
	addShaderQuery = shaderQuery;
	rebindIndex = findAddShaderBinding(shaderQuery);
	learning = true;
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
	// Deliberately does NOT turn editMode on. Map mode paints overlays on every
	// box and inspector slider across the whole app, so arming a Learn button
	// used to switch the entire UI into a mode nobody asked for. Only the
	// "Key bind map on/off" button owns that mode now. The canvas capture paths
	// come through here too, but they already require editMode to be on.
	// Ask the app to bring this screen up. Setting panelOpen here desynced
	// from pantallaActiva, and setPanelVisible(false) then cancelled the learn
	// on the very next frame.
	showRequested = true;
}

bool JPMidiKeymap::consumeShowRequest()
{
	const bool requested = showRequested;
	showRequested = false;
	return requested;
}

void JPMidiKeymap::cancelLearning()
{
	learning = false;
	rebindIndex = -1;
}

void JPMidiKeymap::saveGlobal()
{
	if (globalKeymapPath.empty())
	{
		globalKeymapPath = ofToDataPath("midi_keymap.xml");
	}
	save(globalKeymapPath);
}

bool JPMidiKeymap::hasLearnTarget() const
{
	if (boxes == nullptr)
	{
		return false;
	}
	if (selectedAction == PARAMETER)
	{
		return selectedParameterIndex >= 0 && selectedParameterIndex < getGlobalParameterIndexCount();
	}
	if (selectedAction == ADD_SHADER_BOX)
	{
		return !addShaderQuery.empty();
	}
	if (isGlobalAction(selectedAction))
	{
		return true;
	}
	return !selectedBoxName.empty();
}

bool JPMidiKeymap::isBindingLoadable(const Binding &binding) const
{
	if (binding.key.messageType != "note" && binding.key.messageType != "cc")
	{
		return false;
	}
	if (binding.key.channel < 0 || binding.key.number < 0)
	{
		return false;
	}
	if (binding.action == PARAMETER)
	{
		return binding.parameterIndex >= 0 &&
			binding.parameterIndex < JPboxgroup::maxBindableParameters;
	}
	if (binding.action == ADD_SHADER_BOX)
	{
		return !binding.shaderQuery.empty() || !binding.shaderPath.empty();
	}
	if (isGlobalAction(binding.action))
	{
		return true;
	}
	return !binding.boxName.empty();
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
		// Follows the open inspector, as before. With nothing open it falls
		// back to the box the binding was made against, so a bind still does
		// what its row says instead of nothing at all.
		if (!boxes->setOpenBoxParameterAtIndex(binding.parameterIndex, midiValue))
		{
			boxes->setBoxParameterAtIndex(binding.boxName,
				binding.parameterIndex, midiValue);
		}
	}
	else if (binding.action == NEXT_SHADER)
	{
		selectRelativeBox(1, false);
	}
	else if (binding.action == PREV_SHADER)
	{
		selectRelativeBox(-1, false);
	}
	else if (binding.action == SET_CUE_SHADER)
	{
		// Mirror the 'z' key: toggle the cue on the current selection for the
		// graph on screen, falling back to its active render (main or group).
		boxes->toggleCueBoxByIndex(boxes->getCueEntryIndexForCurrentView());
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
	else if (binding.action == BPM_TAP)
	{
		if (bpmTapCallback) bpmTapCallback();
	}
	else if (binding.action == ADD_SHADER_BOX)
	{
		string shaderPath = getShaderPathForAddBinding(binding);
		if (!shaderPath.empty())
		{
			queueShaderAdd(shaderPath);
		}
	}
}

void JPMidiKeymap::processPendingShaderAdds()
{
	if (boxes == nullptr || pendingShaderAdds.empty())
	{
		return;
	}

	string shaderPath = pendingShaderAdds.front();
	pendingShaderAdds.erase(pendingShaderAdds.begin());
	boxes->addBox(shaderPath);
	boxes->setLastBoxOnOff(true);
}

void JPMidiKeymap::queueShaderAdd(string shaderPath)
{
	if (!shaderPathExists(shaderPath))
	{
		return;
	}
	if (std::find(pendingShaderAdds.begin(), pendingShaderAdds.end(), shaderPath) != pendingShaderAdds.end())
	{
		return;
	}
	pendingShaderAdds.push_back(shaderPath);
}

void JPMidiKeymap::removeBindingForKey(const MidiKey &key, bool saveChange)
{
	// Loop: duplicates on one key are now legal, so removing "the" binding has
	// to mean all of them or Replace would leave strays behind.
	bool removedAny = false;
	for (int index = findBindingForKey(key); index >= 0;
		index = findBindingForKey(key))
	{
		bindings.erase(bindings.begin() + index);
		invalidateBindingCache();
		removedAny = true;
		if (rebindIndex == index) rebindIndex = -1;
		else if (rebindIndex > index) rebindIndex--;
	}
	if (removedAny && saveChange) saveGlobal();
}

int JPMidiKeymap::findBindingForKey(const MidiKey &key) const
{
	// Device-specific match via getKeyId (which includes the normalized device
	// name), but NOT restricted to the active profile, so every connected
	// device drives its own bindings.
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
		if (!isActiveMapDevice(bindings[i].key.deviceName))
		{
			continue;
		}
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
		if (!isActiveMapDevice(bindings[i].key.deviceName))
		{
			continue;
		}
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
		if (!isActiveMapDevice(bindings[i].key.deviceName))
		{
			continue;
		}
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
		if (!isActiveMapDevice(bindings[i].key.deviceName))
		{
			continue;
		}
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
	const vector<Action> &actions = globalActionList();
	return std::find(actions.begin(), actions.end(), action) != actions.end();
}

int JPMidiKeymap::getCurrentBoxIndex() const
{
	if (boxes == nullptr || boxes->getCurrentViewBoxCount() == 0)
	{
		return -1;
	}
	int selectedIndex = boxes->getCurrentViewSelectedIndex();
	if (selectedIndex >= 0 && selectedIndex < boxes->getCurrentViewBoxCount())
	{
		return selectedIndex;
	}
	int activeIndex = boxes->getCurrentViewActiveRenderIndex();
	if (activeIndex >= 0 && activeIndex < boxes->getCurrentViewBoxCount())
	{
		return activeIndex;
	}
	return 0;
}

void JPMidiKeymap::selectRelativeBox(int offset, bool galleryMode)
{
	if (boxes == nullptr || boxes->getCurrentViewBoxCount() == 0)
	{
		return;
	}

	int index = getCurrentBoxIndex();
	if (index < 0)
	{
		return;
	}

	int count = boxes->getCurrentViewBoxCount();
	index = (index + offset + count) % count;
	if (!boxes->selectOpenBoxForCurrentView(index))
	{
		return;
	}

	if (galleryMode)
	{
		boxes->requestSetActiveRenderForCurrentView(index, true);
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
		boxes->selectOpenBoxForCurrentView(index);
		// Set the selected box as the active render. When a cue is open this is
		// STAGED into the cue state (and shows in the CUE OUTPUT preview) instead
		// of switching the live output; with no cue it switches the live render.
		boxes->requestSetActiveRenderForCurrentView(index);
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
		if (!isActiveMapDevice(bindings[i].key.deviceName))
		{
			continue;
		}
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
	while (addShaderCursors.size() < addShaderRows.size())
	{
		addShaderCursors.push_back(0);
	}
	while (addShaderResolvedPaths.size() > addShaderRows.size())
	{
		addShaderResolvedPaths.pop_back();
	}
	while (addShaderSearched.size() > addShaderRows.size())
	{
		addShaderSearched.pop_back();
	}
	while (addShaderCursors.size() > addShaderRows.size())
	{
		addShaderCursors.pop_back();
	}
	while (addShaderRows.size() > 1 &&
		   addShaderRows[addShaderRows.size() - 1].empty() &&
		   addShaderRows[addShaderRows.size() - 2].empty())
	{
		addShaderRows.erase(addShaderRows.end() - 1);
		addShaderResolvedPaths.erase(addShaderResolvedPaths.end() - 1);
		addShaderSearched.erase(addShaderSearched.end() - 1);
		addShaderCursors.erase(addShaderCursors.end() - 1);
	}
	if (focusedAddShaderRow >= addShaderRows.size())
	{
		focusedAddShaderRow = -1;
	}
}

string JPMidiKeymap::rawShaderStem(const string &path)
{
	if (path.empty()) return "";
	size_t slash = path.find_last_of("/\\");
	string name = slash == string::npos ? path : path.substr(slash + 1);
	size_t dot = name.find_last_of('.');
	if (dot != string::npos && dot > 0) name = name.substr(0, dot);
	return name;
}

void JPMidiKeymap::completeAddShaderRow(int rowIndex)
{
	if (rowIndex < 0 || rowIndex >= (int)addShaderRows.size()) return;
	if (rowIndex >= (int)addShaderResolvedPaths.size()) return;
	const string name = rawShaderStem(addShaderResolvedPaths[rowIndex]);
	if (name.empty() || addShaderRows[rowIndex] == name) return;
	// Only on an explicit Find/Enter, never while typing: rewriting on every
	// keystroke would make shortening or correcting a query impossible.
	addShaderRows[rowIndex] = name;
	if (rowIndex < (int)addShaderCursors.size())
	{
		addShaderCursors[rowIndex] = (int)name.size();
	}
	addShaderQuery = name;
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
		saveGlobal();
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

string JPMidiKeymap::getShaderPathForAddBinding(const Binding &binding) const
{
	if (shaderPathExists(binding.shaderPath))
	{
		return binding.shaderPath;
	}
	if (shaderPathExists(binding.shaderQuery))
	{
		return binding.shaderQuery;
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
	return JPboxgroup::maxBindableParameters;
}

bool JPMidiKeymap::isBindingShownInLeftColumn(int index) const
{
	if (index < 0 || index >= (int)bindings.size()) return false;
	const Binding &b = bindings[index];
	if (b.action == PARAMETER)
	{
		return findParameterBindingForIndex(b.parameterIndex) == index;
	}
	if (b.action == ADD_SHADER_BOX)
	{
		return findAddShaderBinding(b.shaderQuery) == index;
	}
	if (isGlobalAction(b.action))
	{
		return findGlobalActionBinding(b.action) == index;
	}
	// Box actions - BYPASS, PAUSE, SELECT_OPEN_BOX, and legacy
	// SET_ACTIVE_SHADER - have no left-column home at all. These are the
	// custom binds made from Target box + Action.
	return false;
}

const vector<int> &JPMidiKeymap::getCustomBindingIndices() const
{
	if (customBindingCacheValid) return customBindingCache;
	customBindingCache.clear();
	for (int i = 0; i < (int)bindings.size(); i++)
	{
		if (!isActiveMapDevice(bindings[i].key.deviceName)) continue;
		if (isBindingShownInLeftColumn(i)) continue;
		customBindingCache.push_back(i);
	}
	customBindingCacheValid = true;
	return customBindingCache;
}

void JPMidiKeymap::invalidateBindingCache()
{
	customBindingCacheValid = false;
}

int JPMidiKeymap::getNonParameterBindingCount() const
{
	// Kept as the name every caller already uses; it now means "rows in the
	// custom binds list", which is what the right column actually renders.
	return (int)getCustomBindingIndices().size();
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

JPMidiKeymap::RowRects JPMidiKeymap::rowRects(float x, float y, float w,
	bool withFind)
{
	RowRects r;
	const float btnH = ROW_H - 4.0f;
	const float btnY = y + 2.0f;
	r.body.set(x, y, w, ROW_H);
	r.remove.set(x + w - 38.0f, btnY, 32.0f, btnH);
	r.learn.set(x + w - 92.0f, btnY, 48.0f, btnH);
	if (withFind) r.find.set(x + w - 146.0f, btnY, 48.0f, btnH);

	// A fixed label gutter with the binding column right-anchored before the
	// buttons. The old split was a w*0.34 fraction, which reads fine at 620px
	// and leaves a 600px hole at full width.
	const float buttonsLeft = withFind ? r.find.x : r.learn.x;
	r.labelX = x + 8.0f;
	r.bindingX = x + std::min(220.0f, w * 0.34f);
	r.labelMaxW = std::max(40.0f, r.bindingX - r.labelX - 10.0f);
	r.bindingMaxW = std::max(40.0f, buttonsLeft - 8.0f - r.bindingX);
	return r;
}

JPMidiKeymap::PanelLayout JPMidiKeymap::getPanelLayout() const
{
	PanelLayout layout;
	const ofRectangle f = panelFrame();
	const ofRectangle body = jp_screen::body(f);
	layout.headerY = f.y + PAD;
	layout.panelH = f.height;

	// Two columns. Left: the three lists you bind FROM. Right: the context
	// that applies, then the live bindings - which used to be stacked below
	// everything else and off the bottom of the window.
	const float gutter = 24.0f;
	const float leftW = std::max(360.0f, (body.width - gutter) * 0.58f);
	const float rightW = std::max(280.0f, body.width - gutter - leftW);
	layout.leftCol.set(body.x, body.y, leftW, body.height);
	layout.rightCol.set(body.x + leftW + gutter, body.y, rightW, body.height);
	layout.innerX = layout.leftCol.x;
	layout.innerW = layout.leftCol.width;
	layout.rightX = layout.rightCol.x;
	layout.rightW = layout.rightCol.width;

	// Left column, scrolled.
	const float leftTop = layout.leftCol.y - leftScroll;
	layout.paramY = leftTop + PARAM_HEADER_H;
	layout.globalY = layout.paramY + PARAM_HEADER_H +
		getParameterRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING;
	layout.addShaderY = layout.globalY + PARAM_HEADER_H +
		getGlobalActionRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING;
	layout.leftContentH = (layout.addShaderY + PARAM_HEADER_H +
		getAddShaderRowCount() * (ROW_H + 4) + SECTION_VERTICAL_SPACING) -
		leftTop;

	// Right column: device / target / action pinned, bindings scrolled.
	layout.mapDeviceY = layout.rightCol.y + ROW_H;
	layout.targetBoxY = layout.mapDeviceY + ROW_H + RIGHT_ROW_SPACING;
	layout.actionY = layout.targetBoxY + ROW_H + RIGHT_ROW_SPACING;
	// Parameter row appears only for Action = Parameter, then the Learn button
	// that arms exactly these fields, then the list.
	layout.paramPickY = layout.actionY + ROW_H + RIGHT_ROW_SPACING;
	const bool showsParamPick = selectedAction == PARAMETER;
	layout.learnY = (showsParamPick ? layout.paramPickY : layout.actionY) +
		ROW_H + RIGHT_ROW_SPACING;
	layout.bindingsY = layout.learnY + ROW_H + RIGHT_ROW_SPACING + 8 -
		rightScroll;
	layout.rightContentH = 16.0f +
		getNonParameterBindingCount() * (ROW_H + 4) + PANEL_BOTTOM_PAD;
	layout.contentH = std::max(layout.leftContentH, layout.rightContentH);
	return layout;
}

JPMidiKeymap::DropdownLayout JPMidiKeymap::getTargetBoxDropdownLayout(const PanelLayout &layout) const
{
	DropdownLayout dropdown;
	dropdown.x = layout.rightX + SELECT_LABEL_W;
	dropdown.y = layout.targetBoxY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = layout.rightW - SELECT_LABEL_W;
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
	dropdown.x = layout.rightX + SELECT_LABEL_W;
	dropdown.y = layout.actionY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = 200.0f;
	dropdown.contentH = getBoxActions().size() * (ROW_H + 2) + 4;
	dropdown.h = dropdown.contentH;
	return dropdown;
}

JPMidiKeymap::DropdownLayout JPMidiKeymap::getParamDropdownLayout(const PanelLayout &layout) const
{
	DropdownLayout dropdown;
	dropdown.x = layout.rightX + SELECT_LABEL_W;
	dropdown.y = layout.paramPickY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = 200.0f;
	dropdown.contentH = getGlobalParameterIndexCount() * (ROW_H + 2) + 4;
	const float roomBelow = panelFrame().getMaxY() - dropdown.y - 8.0f;
	dropdown.h = std::min(std::min(SELECT_DROPDOWN_MAX_H, roomBelow),
		dropdown.contentH);
	dropdown.showScrollbar = false;
	dropdown.maxScrollY = 0.0f;
	return dropdown;
}

JPMidiKeymap::DropdownLayout JPMidiKeymap::getMapDeviceDropdownLayout(const PanelLayout &layout) const
{
	DropdownLayout dropdown;
	dropdown.x = layout.rightX + SELECT_LABEL_W;
	dropdown.y = layout.mapDeviceY - SELECT_FIELD_Y_OFFSET + ROW_H + DROPDOWN_GAP;
	dropdown.w = layout.rightW - SELECT_LABEL_W;
	vector<string> names = getMapDeviceNames();
	dropdown.contentH = names.size() * (ROW_H + 2) + 2;
	// Clamp to the frame, not just to a constant, and allow scrolling: this
	// was pinned to showScrollbar=false, so beyond ~17 devices the rest of the
	// list was drawn past the clip and could never be picked.
	const float roomBelow = panelFrame().getMaxY() - dropdown.y - 8.0f;
	dropdown.h = std::min(std::min(SELECT_DROPDOWN_MAX_H, roomBelow),
		dropdown.contentH);
	dropdown.showScrollbar = dropdown.contentH > dropdown.h;
	dropdown.maxScrollY = std::max(0.0f, dropdown.contentH - dropdown.h);
	return dropdown;
}

const vector<JPMidiKeymap::Action> &JPMidiKeymap::globalActionList()
{
	// One source of truth, built once. This used to be rebuilt on the heap by
	// every isGlobalAction() call, which runs inside the binding loops.
	// SET_ACTIVE_SHADER is folded into SET_ACTIVE_RENDER (they did the same
	// thing); only offer one action. Legacy set_active_shader bindings still
	// load and behave identically (handled in applyBinding /
	// setSelectedBoxActive).
	static const vector<Action> actions = {
		NEXT_SHADER, PREV_SHADER, SET_CUE_SHADER, SET_ACTIVE_RENDER,
		NEXT_SHADER_GALLERY, PREV_SHADER_GALLERY, TOGGLE_GALLERY, BPM_TAP};
	return actions;
}

vector<JPMidiKeymap::Action> JPMidiKeymap::getGlobalActions() const
{
	return globalActionList();
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
	return normalizeDeviceName(key.deviceName) + "|" + ofToString(key.channel) + "|" + key.messageType + "|" + ofToString(key.number);
}

string JPMidiKeymap::getKeyLabel(const MidiKey &key) const
{
	return key.deviceName + " ch" + ofToString(key.channel) + " " + key.messageType + " " + ofToString(key.number);
}

string JPMidiKeymap::getCompactKeyLabel(const MidiKey &key) const
{
	const string type = ofToLower(key.messageType) == "cc" ? "CC" : "Note";
	return "Ch" + ofToString(key.channel) + " " + type + " " + ofToString(key.number);
}

string JPMidiKeymap::getAddShaderBindingLabel(
	const string &shaderQuery,
	const string &shaderPath) const
{
	const string normalizedQuery = normalizeText(shaderQuery);
	const string queryStem = portableShaderStem(shaderQuery);
	const string pathStem = portableShaderStem(shaderPath);
	string firstLabel;
	int matchCount = 0;

	for (const Binding &binding : bindings)
	{
		if (binding.action != ADD_SHADER_BOX)
		{
			continue;
		}
		const string bindingQuery = normalizeText(binding.shaderQuery);
		const string bindingQueryStem = portableShaderStem(binding.shaderQuery);
		const string bindingPathStem = portableShaderStem(binding.shaderPath);
		const bool matches =
			(!normalizedQuery.empty() && bindingQuery == normalizedQuery) ||
			(!queryStem.empty() &&
				(bindingQueryStem == queryStem || bindingPathStem == queryStem)) ||
			(!pathStem.empty() &&
				(bindingQueryStem == pathStem || bindingPathStem == pathStem));
		if (!matches)
		{
			continue;
		}
		if (firstLabel.empty())
		{
			firstLabel = getCompactKeyLabel(binding.key);
		}
		matchCount++;
	}

	if (matchCount > 1)
	{
		firstLabel += " +" + ofToString(matchCount - 1);
	}
	return firstLabel;
}

string JPMidiKeymap::describeBinding(const Binding &binding) const
{
	string target = isGlobalAction(binding.action) ? "Global" : binding.boxName;
	if (binding.action == ADD_SHADER_BOX) target = binding.shaderQuery;
	if (binding.action == PARAMETER)
	{
		target = "p" + ofToString(binding.parameterIndex);
	}
	if (target.empty()) target = "Global";
	return target + " / " + getActionName(binding.action);
}

string JPMidiKeymap::getActionName(Action action) const
{
	if (action == BYPASS) return "Bypass";
	if (action == PAUSE) return "Pause";
	if (action == PARAMETER) return "Parameter";
	if (action == SELECT_OPEN_BOX) return "Select/Open Box";
	if (action == NEXT_SHADER) return "Next Shader";
	if (action == PREV_SHADER) return "Prev Shader";
	if (action == SET_CUE_SHADER) return "Set Cue Shader";
	if (action == SET_ACTIVE_SHADER) return "Set Active Shader";
	if (action == SET_ACTIVE_RENDER) return "Set Active Render";
	if (action == NEXT_SHADER_GALLERY) return "Next Shader Gallery";
	if (action == PREV_SHADER_GALLERY) return "Prev Shader Gallery";
	if (action == TOGGLE_GALLERY) return "Toggle Gallery Mode";
	if (action == BPM_TAP) return "BPM Tap";
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
	if (action == SET_CUE_SHADER) return "set_cue_shader";
	if (action == SET_ACTIVE_SHADER) return "set_active_shader";
	if (action == SET_ACTIVE_RENDER) return "set_active_render";
	if (action == NEXT_SHADER_GALLERY) return "next_shader_gallery";
	if (action == PREV_SHADER_GALLERY) return "prev_shader_gallery";
	if (action == TOGGLE_GALLERY) return "toggle_gallery";
	if (action == BPM_TAP) return "bpm_tap";
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
	if (value == "set_cue_shader") return SET_CUE_SHADER;
	if (value == "set_active_shader") return SET_ACTIVE_SHADER;
	if (value == "set_active_render") return SET_ACTIVE_RENDER;
	if (value == "next_shader_gallery") return NEXT_SHADER_GALLERY;
	if (value == "prev_shader_gallery") return PREV_SHADER_GALLERY;
	if (value == "toggle_gallery") return TOGGLE_GALLERY;
	if (value == "bpm_tap") return BPM_TAP;
	if (value == "add_shader_box") return ADD_SHADER_BOX;
	return BYPASS;
}

void JPMidiKeymap::beginColumnClip(const ofRectangle &r)
{
	// Same primitive the target-box dropdown already uses. Window coords, so
	// it assumes points == pixels, which holds everywhere this app runs.
	glEnable(GL_SCISSOR_TEST);
	glScissor((int)r.x, (int)(ofGetHeight() - (r.y + r.height)),
		(int)r.width, (int)r.height);
}

void JPMidiKeymap::endColumnClip()
{
	glDisable(GL_SCISSOR_TEST);
}

void JPMidiKeymap::draw()
{
	jp_pointer::Scope pointerScope(jp_pointer::kMidiBody);
	if (!panelOpen)
	{
		return;
	}
	// Never pick a box on the user's behalf. This defaulted to boxes[0] every
	// frame that nothing was chosen, so the Target box field always named some
	// box, and a Learn pressed without choosing one silently bound whichever
	// box happened to be first in the graph. An empty selection now stays
	// empty - it reads "None", and hasLearnTarget() keeps Learn disabled until
	// a box is actually picked. Only a name whose box is gone gets cleared.
	if (boxes != nullptr && !selectedBoxName.empty() &&
		!boxes->hasBoxName(selectedBoxName))
	{
		selectedBoxName = "";
	}
	selectedParameterIndex = ofClamp(selectedParameterIndex, 0, getGlobalParameterIndexCount() - 1);

	PanelLayout layout = getPanelLayout();
	ofSetRectMode(OF_RECTMODE_CORNER);

	// Shared screen chrome: same margins, radius, border and title style as
	// SETTINGS, HELP, IMPORT and EDITOR.
	const string subtitle = "Inputs: " + ofToString(midiInputs.size()) +
		"   Last MIDI: " + (hasLastKey ? getKeyLabel(lastKey) : "none");
	jp_screen::drawFrame(panelFrame(), "MIDI KEYMAP", subtitle);

	drawPanelHeader(layout.innerX, layout.headerY, layout.innerW);

	// LEFT COLUMN - the lists you bind from. Clipped so a scrolled row cannot
	// paint over the header or spill past the frame.
	beginColumnClip(layout.leftCol);
	drawParameterIndexSelector(layout.innerX, layout.paramY, layout.innerW);
	drawGlobalFunctionsSelector(layout.innerX, layout.globalY, layout.innerW);
	drawAddShaderSelector(layout.innerX, layout.addShaderY, layout.innerW);
	endColumnClip();

	// RIGHT COLUMN - context stays pinned, the bindings list scrolls.
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p_font.drawString("Device", layout.rightX, layout.mapDeviceY);
	{
		string mapLabel = activeMapDeviceName.empty() ?
			"No MIDI device" : activeMapDeviceName;
		mapLabel = fitLabel(mapLabel, layout.rightW - SELECT_LABEL_W - 20.0f);
		drawSelectField(layout.rightX + SELECT_LABEL_W,
			layout.mapDeviceY - SELECT_FIELD_Y_OFFSET,
			layout.rightW - SELECT_LABEL_W, ROW_H, mapLabel, mapDeviceSelectOpen);
	}
	drawBoxSelector(layout.rightX, layout.targetBoxY, layout.rightW);
	drawActionSelector(layout.rightX, layout.actionY, layout.rightW);
	if (selectedAction == PARAMETER)
	{
		// Picking Action = Parameter used to leave the index unchooseable from
		// here, so it silently bound p0 or whatever the left column last set.
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString("Parameter",
			layout.rightX, layout.paramPickY);
		drawSelectField(layout.rightX + SELECT_LABEL_W,
			layout.paramPickY - SELECT_FIELD_Y_OFFSET, 200.0f, ROW_H,
			"p" + ofToString(selectedParameterIndex), paramSelectOpen);
	}
	{
		const string learnLabel = learning ? "Listening..." : "Learn";
		jp_button::draw(ofRectangle(layout.rightX + SELECT_LABEL_W,
			layout.learnY - SELECT_FIELD_Y_OFFSET, 200.0f, ROW_H),
			learnLabel, learning, hasLearnTarget(), COL_ACCENT_GOLD);
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString("Bind", layout.rightX, layout.learnY);
		jp_tooltip::draw("Arm the fields above, then move a MIDI control",
			layout.rightX + SELECT_LABEL_W,
			layout.learnY - SELECT_FIELD_Y_OFFSET, 200.0f, ROW_H);
	}
	beginColumnClip(ofRectangle(layout.rightCol.x,
		layout.actionY + ROW_H + 8.0f, layout.rightCol.width,
		layout.rightCol.getMaxY() - (layout.actionY + ROW_H + 8.0f)));
	drawBindings(layout.rightX, layout.bindingsY, layout.rightW);
	endColumnClip();

	if (conflictPromptOpen)
	{
		// The prompt owns the pointer while it is up. Without this its own
		// buttons are drawn at the panel body's layer, and the modal rule -
		// "a modal blocks the whole window for everything below it" - then
		// blocks the prompt's own Replace/Keep both/Cancel, so they never
		// highlighted and read as dead.
		jp_pointer::Scope promptScope(jp_pointer::kPrompt);
		// Centred over the frame. The previous behaviour stole the key
		// silently, so the old binding vanished from wherever it was shown.
		const ofRectangle box = conflictPromptRect();
		ofSetColor(0, 0, 0, 150);
		ofDrawRectangle(panelFrame());
		ofSetColor(ofColor(COL_BG_PANEL, 250));
		ofDrawRectRounded(box, 6.0f);
		ofNoFill();
		ofSetColor(COL_ACCENT_GOLD);
		ofSetLineWidth(2.0f);
		ofDrawRectRounded(box, 6.0f);
		ofFill();
		ofSetLineWidth(1.0f);
		ofSetColor(COL_ACCENT_GOLD);
		jp_constants::p_font.drawString(
			getCompactKeyLabel(pendingBinding.key) + " is already bound",
			box.x + 16.0f, box.y + 28.0f);
		ofSetColor(COL_TEXT_SECONDARY);
		jp_constants::p_font.drawString("Existing:  " + conflictExistingLabel,
			box.x + 16.0f, box.y + 54.0f);
		jp_constants::p_font.drawString("New:       " +
			describeBinding(pendingBinding), box.x + 16.0f, box.y + 74.0f);
		ofSetColor(COL_TEXT_MUTED);
		jp_constants::p_font.drawString(
			"Keep both fires every binding on this key.",
			box.x + 16.0f, box.y + 96.0f);
		jp_button::draw(conflictButtonRect(0), "Replace", false, true,
			COL_ACCENT_RED);
		jp_button::draw(conflictButtonRect(1), "Keep both", false, true,
			COL_ACCENT_GREEN);
		jp_button::draw(conflictButtonRect(2), "Cancel", false);
	}

	// Learning no longer implies map mode, so the "listening" hint has to be
	// driven by `learning` itself or a plain Learn would arm with no feedback.
	if (editMode || learning)
	{
		ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
		string hint = learning ? "Press a MIDI key/control..." : "MIDI map edit: click a box button or inspector slider, then press MIDI";
		jp_constants::p_font.drawString(hint, layout.innerX,
			panelFrame().getMaxY() - 16);
	}

	// DRAW OVERLAYS FOR DROPDOWNS
	if (mapDeviceSelectOpen)
	{
		vector<string> names = getMapDeviceNames();
		DropdownLayout dropdown = getMapDeviceDropdownLayout(layout);
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
		ofSetLineWidth(1.5f);
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		ofFill();
		ofSetLineWidth(1.0f);

		for (int i = 0; i < names.size(); i++)
		{
			float optionY = dropdown.y + 2 + i * (ROW_H + 2);
			bool isSelected = names[i] == activeMapDeviceName;
			bool isHovered = pointInRect(ofGetMouseX(), ofGetMouseY(), dropdown.x + 2, optionY, dropdown.w - 4, ROW_H);
			ofSetColor(isSelected ? ofColor(COL_ACCENT_CYAN_DIM, 220) : (isHovered ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_INPUT, 200)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			ofNoFill();
			ofSetColor(isHovered ? COL_TEXT_PRIMARY : (isSelected ? ofColor(COL_ACCENT_CYAN, 200) : ofColor(COL_MAPPED_OFF, 100)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			ofFill();
			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p_font.drawString(fitLabel(names[i], dropdown.w - 20), dropdown.x + 10, optionY + ROW_H - 7);
		}
	}

	if (targetBoxSelectOpen && boxes != nullptr && !boxes->boxes.empty())
	{
		vector<string> names = boxes->getBoxNames();
		DropdownLayout dropdown = getTargetBoxDropdownLayout(layout);
		float scrollbarW = 12.0f;

		// Dropdown container background
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);

		// Dropdown border
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
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

			ofSetColor(isSelected ? ofColor(COL_ACCENT_CYAN_DIM, 220) : (isHovered ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_INPUT, 200)));
			ofDrawRectRounded(dropdown.x + 2, optionY, btnW, ROW_H, 3.0f);
			
			ofNoFill();
			ofSetColor(isHovered ? COL_TEXT_PRIMARY : (isSelected ? ofColor(COL_ACCENT_CYAN, 200) : ofColor(COL_MAPPED_OFF, 100)));
			ofDrawRectRounded(dropdown.x + 2, optionY, btnW, ROW_H, 3.0f);
			ofFill();

			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p_font.drawString(optionLabel, dropdown.x + 10, optionY + ROW_H - 7);
		}

		glDisable(GL_SCISSOR_TEST);

		// Draw scrollbar
		if (dropdown.showScrollbar)
		{
			float trackX = dropdown.x + dropdown.w - scrollbarW - 2;
			float trackY = dropdown.y + 2;
			float trackH = dropdown.h - 4;
			
			ofSetColor(ofColor(COL_BG_SLIDER, 200));
			ofDrawRectRounded(trackX, trackY, scrollbarW, trackH, 4.0f);

			float thumbH = (dropdown.h / dropdown.contentH) * trackH;
			thumbH = std::max(thumbH, 20.0f);
			float thumbY = trackY + (targetBoxScrollY / dropdown.maxScrollY) * (trackH - thumbH);

			bool thumbHover = ofGetMouseX() >= trackX && ofGetMouseX() <= trackX + scrollbarW &&
							  ofGetMouseY() >= thumbY && ofGetMouseY() <= thumbY + thumbH;

			ofSetColor((scrollbarDragging || thumbHover) ? ofColor(COL_ACCENT_CYAN, 255) : ofColor(COL_BG_SCROLLBAR, 200));
			ofDrawRectRounded(trackX + 2, thumbY, scrollbarW - 4, thumbH, 3.0f);
		}
	}
	
	if (paramSelectOpen)
	{
		const DropdownLayout dropdown = getParamDropdownLayout(layout);
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 4.0f);
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 200));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 4.0f);
		ofFill();
		for (int i = 0; i < getGlobalParameterIndexCount(); i++)
		{
			const float optionY = dropdown.y + 2 + i * (ROW_H + 2);
			if (optionY + ROW_H > dropdown.y + dropdown.h) break;
			drawButton(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H,
				"p" + ofToString(i), selectedParameterIndex == i);
		}
	}

	if (actionSelectOpen)
	{
		DropdownLayout dropdown = getActionDropdownLayout(layout);
		
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
		ofDrawRectRounded(dropdown.x, dropdown.y, dropdown.w, dropdown.h, 6.0f);
		ofFill();
		
		vector<Action> actions = getBoxActions();
		for (int i = 0; i < actions.size(); i++)
		{
			float optionY = dropdown.y + 2 + i * (ROW_H + 2);
			bool isSelected = (selectedAction == actions[i]);
			bool isHovered = ofGetMouseX() >= dropdown.x && ofGetMouseX() <= dropdown.x + dropdown.w &&
							 ofGetMouseY() >= optionY && ofGetMouseY() <= optionY + ROW_H;
							 
			ofSetColor(isSelected ? ofColor(COL_ACCENT_CYAN_DIM, 220) : (isHovered ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_INPUT, 200)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			
			ofNoFill();
			ofSetColor(isHovered ? COL_TEXT_PRIMARY : (isSelected ? ofColor(COL_ACCENT_CYAN, 200) : ofColor(COL_MAPPED_OFF, 100)));
			ofDrawRectRounded(dropdown.x + 2, optionY, dropdown.w - 4, ROW_H, 3.0f);
			ofFill();
			
			ofSetColor(COL_TEXT_PRIMARY);
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
	drawMapOnIndicator(chromeRightEdge);
	if (boxes == nullptr)
	{
		return;
	}
	drawBoxMappingTargets();
	drawInspectorMappingTargets();
}

void JPMidiKeymap::drawBoxMappingTargets()
{
	JPdragobject::setMouseOverride(boxes->screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY())));
	for (int i = 0; i < boxes->boxes.size(); i++)
	{
		JPbox *box = boxes->boxes[i];
		bool boxOver = box->mouseOver();
		bool bypassOver = box->bypass.mouseOver();
		bool pauseOver = box->onoff.mouseOver();
		bool boxBound = hasBindingForAction(SELECT_OPEN_BOX, box->name);
		bool bypassBound = hasBindingForAction(BYPASS, box->name);
		bool pauseBound = hasBindingForAction(PAUSE, box->name);
		ofVec2f boxPos = boxes->canvasToScreen(ofVec2f(box->x, box->y));
		ofVec2f bypassPos = boxes->canvasToScreen(ofVec2f(box->bypass.x, box->bypass.y));
		ofVec2f pausePos = boxes->canvasToScreen(ofVec2f(box->onoff.x, box->onoff.y));
		float zoom = boxes->viewportZoom;

		ofNoFill();
		ofSetLineWidth((boxOver || boxBound) ? 3 : 2);
		ofSetColor(boxBound ? ofColor(COL_MAPPED_ON, boxOver ? 255 : 220) :
							  (boxOver ? ofColor(255, 255, 0, 230) : ofColor(255, 255, 0, 120)));
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofDrawRectRounded(boxPos.x, boxPos.y, (box->width + 8) * zoom, (box->height + 8) * zoom, 8.0f * zoom);
		jp_tooltip::draw("Map box selection", boxPos.x - (box->width + 8) * zoom / 2,
			boxPos.y - (box->height + 8) * zoom / 2,
			(box->width + 8) * zoom, (box->height + 8) * zoom);

		ofSetLineWidth((bypassOver || bypassBound) ? 3 : 2);
		ofSetColor(bypassBound ? ofColor(COL_MAPPED_ON, bypassOver ? 255 : 220) :
								(bypassOver ? COL_ACCENT_RED : ofColor(COL_ACCENT_RED, 170)));
		ofDrawRectRounded(bypassPos.x, bypassPos.y, (box->bypass.width + 8) * zoom, (box->bypass.height + 8) * zoom, 4.0f * zoom);
		jp_tooltip::draw("Map bypass", bypassPos.x - (box->bypass.width + 8) * zoom / 2,
			bypassPos.y - (box->bypass.height + 8) * zoom / 2,
			(box->bypass.width + 8) * zoom, (box->bypass.height + 8) * zoom);

		ofSetLineWidth((pauseOver || pauseBound) ? 3 : 2);
		ofSetColor(pauseBound ? ofColor(COL_MAPPED_ON, pauseOver ? 255 : 220) :
							   (pauseOver ? COL_TEXT_PRIMARY : ofColor(COL_TEXT_PRIMARY, 170)));
		ofDrawRectRounded(pausePos.x, pausePos.y, (box->onoff.width + 8) * zoom, (box->onoff.height + 8) * zoom, 4.0f * zoom);
		jp_tooltip::draw("Map pause or resume", pausePos.x - (box->onoff.width + 8) * zoom / 2,
			pausePos.y - (box->onoff.height + 8) * zoom / 2,
			(box->onoff.width + 8) * zoom, (box->onoff.height + 8) * zoom);
		ofFill();
		ofSetRectMode(OF_RECTMODE_CORNER); // restore
	}
	JPdragobject::clearMouseOverride();
}

void JPMidiKeymap::drawInspectorMappingTargets()
{
	if (boxes == nullptr || boxes->getInspectorBox() == nullptr)
	{
		return;
	}

	for (int i = 0;
		 i < boxes->getOpenParameterCount();
		 i++)
	{
		JPParameter *parameter =
			boxes->getOpenParameterAtIndex(i);
		if (parameter == nullptr ||
			(parameter->variabletype != JPParameter::FLOAT &&
			 parameter->variabletype != JPParameter::BOOL))
		{
			continue;
		}

		JPcontroller *controller = boxes->controllers[i];
		if (controller == nullptr)
		{
			continue;
		}
		bool over = controller->mouseOver();
		bool bound = hasBindingForAction(PARAMETER, "", i);
		bool mapsAutomationSpeed =
			parameter->variabletype == JPParameter::FLOAT &&
			parameter->movtype != JPParameter::STANDART;
		ofNoFill();
		ofSetLineWidth((over || bound) ? 3 : 2);
		ofSetColor(bound ? ofColor(COL_MAPPED_ON, over ? 255 : 220) :
					   (over ? ofColor(COL_ACCENT_CYAN, 255) : ofColor(COL_ACCENT_CYAN, 160)));
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofDrawRectRounded(controller->x, controller->y, controller->width + 8, controller->height + 8, 4.0f);
		jp_tooltip::draw(
			(mapsAutomationSpeed ? "Map automation speed p" :
				"Map parameter p") + ofToString(i),
			controller->x - (controller->width + 8) / 2,
			controller->y - (controller->height + 8) / 2,
			controller->width + 8, controller->height + 8);
		ofFill();
		ofSetRectMode(OF_RECTMODE_CORNER); // restore

		ofSetColor(bound ? ofColor(COL_MAPPED_ON, over ? 255 : 220) :
					   ofColor(COL_ACCENT_CYAN, over ? 255 : 190));
		jp_constants::p_font.drawString(
										"p" + ofToString(i) +
											(mapsAutomationSpeed ? " SPD" : ""),
										controller->x + controller->width / 2 + 8,
										controller->y + 4);
	}
}

// Header slot widths, right to left: Rescan devices, Key bind map on/off.
// Learn is no longer here - it moved next to the fields it actually arms.
const vector<float> &JPMidiKeymap::headerSlotWidths()
{
	static const vector<float> widths = {132.0f, 148.0f};
	return widths;
}

void JPMidiKeymap::drawPanelHeader(float x, float y, float w)
{
	// Title and subtitle belong to the shared frame; this only lays out the
	// header actions. "Map Off" read as if it disabled the MAP top-bar button,
	// which is the projection mapping editor - hence the longer labels.
	const ofRectangle f = panelFrame();
	jp_button::draw(jp_screen::actionSlotRun(f, headerSlotWidths(), 1),
		editMode ? "Key bind map on" : "Key bind map off", editMode);
	jp_button::draw(jp_screen::actionSlotRun(f, headerSlotWidths(), 0),
		"Rescan devices", false);
}

void JPMidiKeymap::drawBoxSelector(float x, float y, float w)
{
	ofSetColor(COL_TEXT_PRIMARY);
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
	ofSetColor(COL_TEXT_PRIMARY);
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

	ofSetColor(COL_TEXT_DIM);
	jp_constants::p_font.drawString("MIDI binding",
		rowRects(x, y, w, false).bindingX, y);

	float rowY = y + PARAM_HEADER_H;
	for (int i = 0; i < getGlobalParameterIndexCount(); i++)
	{
		const RowRects R = rowRects(x, rowY, w, false);
		const bool keyFocused = isRowFocused(FOCUS_PARAM, i);
		bool selected = selectedAction == PARAMETER && selectedParameterIndex == i;
		int bindingIndex = findParameterBindingForIndex(i);
		bool mapped = bindingIndex >= 0;
		bool over = mouseInRect(x, rowY, w, ROW_H);
		JPParameter *parameter =
			boxes->getOpenParameterAtIndex(i);
		bool isBoolParameter = parameter != nullptr &&
			parameter->variabletype == JPParameter::BOOL;
		bool mapsAutomationSpeed = parameter != nullptr &&
			parameter->variabletype == JPParameter::FLOAT &&
			parameter->movtype != JPParameter::STANDART;
		bool boolValue = isBoolParameter &&
			parameter->boolValue;

		ofSetRectMode(OF_RECTMODE_CORNER);
		if (isBoolParameter)
		{
			ofSetColor(boolValue ? ofColor(245, 245, 245, 235) :
								   (over ? ofColor(COL_TEXT_MUTED, 230) : ofColor(COL_TEXT_MUTED, 220)));
		}
		else
		{
			ofSetColor(selected ? ofColor(COL_ACCENT_CYAN_DIM, 220) :
								 (over ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_BUTTON, 210)));
		}
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		
		ofNoFill();
		if (selected)
		{
			ofSetColor(ofColor(COL_ACCENT_CYAN, 255));
		}
		else
		{
			ofSetColor(mapped ? ofColor(COL_MAPPED_ON, over ? 255 : 210) :
								(over ? COL_TEXT_PRIMARY : ofColor(COL_TEXT_MUTED, 150)));
		}
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();
		if (keyFocused)
		{
			ofNoFill();
			ofSetColor(COL_ACCENT_GOLD);
			ofSetLineWidth(2.0f);
			ofDrawRectRounded(R.body.x - 2.0f, R.body.y - 2.0f,
				R.body.width + 4.0f, R.body.height + 4.0f, 5.0f);
			ofSetLineWidth(1.0f);
			ofFill();
		}

		ofColor textColor = boolValue ? COL_TEXT_DARK : COL_TEXT_PRIMARY;
		ofSetColor(textColor);
		const string parameterLabel = "p" + ofToString(i) +
			(mapsAutomationSpeed ? " speed" : "");
		jp_constants::p_font.drawString(
			parameterLabel, x + 8, rowY + ROW_H - 7);
		if (isBoolParameter)
		{
			ofSetColor(boolValue ? COL_TEXT_DARK : COL_TEXT_SECONDARY);
		}
		else
		{
			ofSetColor(mapped ? COL_MAPPED_ON : COL_TEXT_DIM);
		}
		string keyLabel = mapped ? getCompactKeyLabel(bindings[bindingIndex].key) : "Unmapped";
		keyLabel = fitLabel(keyLabel, R.bindingMaxW);
		jp_constants::p_font.drawString(keyLabel, R.bindingX, rowY + ROW_H - 7);

		bool learningThisRow = learning &&
							   selectedAction == PARAMETER &&
							   selectedParameterIndex == i;
		drawButton(R.learn.x, R.learn.y, R.learn.width, R.learn.height,
			"Learn", learningThisRow);
		drawButton(R.remove.x, R.remove.y, R.remove.width, R.remove.height,
			"X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawGlobalFunctionsSelector(float x, float y, float w)
{
	ofSetColor(COL_TEXT_PRIMARY);
	{
		// Section divider, same hairline the wall panel uses.
		ofPushStyle();
		ofSetColor(ofColor(COL_BORDER_MUTED, 150));
		ofSetLineWidth(1.0f);
		const float ruleY = y - SELECT_FIELD_Y_OFFSET - 12.0f;
		ofDrawLine(x, ruleY, x + w, ruleY);
		ofPopStyle();
	}
	ofSetColor(COL_TEXT_PRIMARY);
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

	ofSetColor(COL_TEXT_DIM);
	jp_constants::p_font.drawString("MIDI binding",
		rowRects(x, y, w, false).bindingX, y);

	vector<Action> globalActions = getGlobalActions();

	float rowY = y + PARAM_HEADER_H;
	for (int i = 0; i < globalActions.size(); i++)
	{
		const RowRects R = rowRects(x, rowY, w, false);
		const bool keyFocused = isRowFocused(FOCUS_GLOBAL, i);
		bool selected = selectedAction == globalActions[i];
		int bindingIndex = findGlobalActionBinding(globalActions[i]);
		bool mapped = bindingIndex >= 0;
		bool over = mouseInRect(x, rowY, w, ROW_H);

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(selected ? ofColor(COL_ACCENT_CYAN_DIM, 220) :
							 (over ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_BUTTON, 210)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		
		ofNoFill();
		ofSetColor(mapped ? ofColor(COL_MAPPED_ON, over ? 255 : 210) :
							(over ? COL_TEXT_PRIMARY : ofColor(COL_TEXT_MUTED, 150)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();
		if (keyFocused)
		{
			ofNoFill();
			ofSetColor(COL_ACCENT_GOLD);
			ofSetLineWidth(2.0f);
			ofDrawRectRounded(R.body.x - 2.0f, R.body.y - 2.0f,
				R.body.width + 4.0f, R.body.height + 4.0f, 5.0f);
			ofSetLineWidth(1.0f);
			ofFill();
		}

		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(getActionName(globalActions[i]), x + 8, rowY + ROW_H - 7);
		ofSetColor(mapped ? COL_MAPPED_ON : COL_TEXT_DIM);
		string keyLabel = mapped ? getCompactKeyLabel(bindings[bindingIndex].key) : "Unmapped";
		keyLabel = fitLabel(keyLabel, R.bindingMaxW);
		jp_constants::p_font.drawString(keyLabel, R.bindingX, rowY + ROW_H - 7);

		bool learningThisRow = learning && selectedAction == globalActions[i];
		drawButton(R.learn.x, R.learn.y, R.learn.width, R.learn.height, "Learn", learningThisRow);
		drawButton(R.remove.x, R.remove.y, R.remove.width, R.remove.height, "X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawAddShaderSelector(float x, float y, float w)
{
	ensureAddShaderDraftRow();
	ofSetColor(COL_TEXT_PRIMARY);
	{
		// Section divider, same hairline the wall panel uses.
		ofPushStyle();
		ofSetColor(ofColor(COL_BORDER_MUTED, 150));
		ofSetLineWidth(1.0f);
		const float ruleY = y - SELECT_FIELD_Y_OFFSET - 12.0f;
		ofDrawLine(x, ruleY, x + w, ruleY);
		ofPopStyle();
	}
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p_font.drawString("Add shader box", x, y);
	if (!addShaderSectionCollapsed)
	{
		ofSetColor(COL_TEXT_DIM);
		jp_constants::p_font.drawString("MIDI binding",
			rowRects(x, y, w, true).bindingX, y);
	}
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
		const RowRects R = rowRects(x, rowY, w, true);
		const bool keyFocused = isRowFocused(FOCUS_ADDSHADER, i);
		string query = addShaderRows[i];
		int bindingIndex = findAddShaderBinding(query);
		bool mapped = bindingIndex >= 0;
		bool searched = i < addShaderSearched.size() && addShaderSearched[i];
		bool resolved = i < addShaderResolvedPaths.size() && !addShaderResolvedPaths[i].empty();
		bool focused = focusedAddShaderRow == i;

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(focused ? ofColor(COL_ACCENT_CYAN_DIM, 220) :
							 (mouseInRect(x, rowY, w, ROW_H) ? ofColor(COL_BG_HOVER, 230) : ofColor(COL_BG_BUTTON, 210)));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);

		ofNoFill();
		ofSetColor(!query.empty() && !resolved ? COL_ERROR_BR :
					(mapped ? ofColor(COL_MAPPED_ON, 230) :
							  (focused ? ofColor(COL_ACCENT_CYAN, 255) : ofColor(COL_TEXT_MUTED, 150))));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();
		if (keyFocused)
		{
			ofNoFill();
			ofSetColor(COL_ACCENT_GOLD);
			ofSetLineWidth(2.0f);
			ofDrawRectRounded(R.body.x - 2.0f, R.body.y - 2.0f,
				R.body.width + 4.0f, R.body.height + 4.0f, 5.0f);
			ofSetLineWidth(1.0f);
			ofFill();
		}

		// Same windowing and caret as the IMPORT search box: the visible slice
		// follows the cursor instead of the text being truncated at the end.
		if (query.empty())
		{
			ofSetColor(COL_TEXT_DIM);
			jp_constants::p_font.drawString("type shader name",
				R.labelX, rowY + ROW_H - 7);
		}
		else
		{
			const int cursor = i < (int)addShaderCursors.size() ?
				addShaderCursors[i] : (int)query.size();
			const jp_textfield::Window win = jp_textfield::visibleWindow(
				jp_constants::p_font, query, cursor, R.labelMaxW);
			const string shown = query.substr(win.start, win.end - win.start);
			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p_font.drawString(shown, R.labelX, rowY + ROW_H - 7);
			if (focused)
			{
				jp_textfield::drawCaret(jp_constants::p_font, shown,
					cursor - win.start, R.labelX,
					rowY + ROW_H * 0.5f, ROW_H - 8.0f);
			}
		}
		if (focused && query.empty())
		{
			jp_textfield::drawCaret(jp_constants::p_font, "", 0, R.labelX,
				rowY + ROW_H * 0.5f, ROW_H - 8.0f);
		}

		string keyLabel = mapped ? getCompactKeyLabel(bindings[bindingIndex].key) :
						  (!query.empty() && searched && !resolved ? "Not found" :
						   (!query.empty() && resolved ? "Found" : "Unmapped"));
		keyLabel = fitLabel(keyLabel, R.bindingMaxW);
		ofSetColor(mapped ? COL_MAPPED_ON :
					(!query.empty() && !resolved ? COL_ACCENT_RED_DIM : COL_TEXT_DIM));
		jp_constants::p_font.drawString(keyLabel, R.bindingX, rowY + ROW_H - 7);

		bool learningThisRow = learning && selectedAction == ADD_SHADER_BOX && addShaderQuery == query;
		drawButton(R.find.x, R.find.y, R.find.width, R.find.height, "Find", resolved);
		drawButton(R.learn.x, R.learn.y, R.learn.width, R.learn.height, "Learn", learningThisRow);
		drawButton(R.remove.x, R.remove.y, R.remove.width, R.remove.height, "X", false);
		rowY += ROW_H + 4;
	}
}

void JPMidiKeymap::drawActionSelector(float x, float y, float w)
{
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p_font.drawString("Action", x, y);
	drawSelectField(x + SELECT_LABEL_W, y - SELECT_FIELD_Y_OFFSET, 200, ROW_H, getActionName(selectedAction), actionSelectOpen);
}

void JPMidiKeymap::drawBindings(float x, float y, float w)
{
	ofSetColor(COL_ACCENT_CYAN);
	jp_constants::p_font.drawString("CUSTOM BINDS", x, y);
	ofSetColor(COL_TEXT_MUTED);
	jp_constants::p_font.drawString("made with Target box + Action",
		x + jp_constants::p_font.stringWidth("CUSTOM BINDS") + 10.0f, y);
	const vector<int> customIndices = getCustomBindingIndices();
	if (customIndices.empty())
	{
		// Otherwise an empty column reads as a bug rather than as "you have
		// not made one yet".
		ofSetColor(COL_TEXT_MUTED);
		jp_constants::p_font.drawString(
			"No custom binds yet.", x, y + 28.0f);
		jp_constants::p_font.drawString(
			"Pick a Target box and an Action above, then Learn.",
			x, y + 46.0f);
	}
	int rowIndex = 0;
	for (int listPos = 0; listPos < (int)customIndices.size(); listPos++)
	{
		const int i = customIndices[listPos];

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
		const RowRects R = rowRects(x, rowY, w, false);
		const bool keyFocused = isRowFocused(FOCUS_BINDING, rowIndex);
		ofSetColor(unresolved ? ofColor(COL_ERROR_BG, 190) : ofColor(COL_BG_BUTTON, 210));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);

		ofNoFill();
		ofSetColor(unresolved ? ofColor(COL_ERROR_TEXT, 200) : ofColor(COL_ACCENT_CYAN, 80));
		ofDrawRectRounded(x, rowY, w, ROW_H, 4.0f);
		ofFill();
		if (keyFocused)
		{
			ofNoFill();
			ofSetColor(COL_ACCENT_GOLD);
			ofSetLineWidth(2.0f);
			ofDrawRectRounded(R.body.x - 2.0f, R.body.y - 2.0f,
				R.body.width + 4.0f, R.body.height + 4.0f, 5.0f);
			ofSetLineWidth(1.0f);
			ofFill();
		}

		// Key in one column, target/action in the other. This was a single
		// concatenated string truncated to w-112, so a wider panel bought
		// nothing: the middle of every binding stayed hidden behind "..".
		string targetLabel = isGlobalAction(bindings[i].action) ? "Global" : bindings[i].boxName;
		if (bindings[i].action == ADD_SHADER_BOX)
		{
			targetLabel = bindings[i].shaderQuery;
		}
		targetLabel += "  /  " + getActionName(bindings[i].action);
		if (unresolved)
		{
			targetLabel += " (missing)";
		}
		ofSetColor(unresolved ? COL_ERROR_TEXT : COL_MAPPED_ON);
		jp_constants::p_font.drawString(
			fitLabel(getCompactKeyLabel(bindings[i].key), R.labelMaxW),
			R.labelX, rowY + ROW_H - 7);
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(fitLabel(targetLabel, R.bindingMaxW),
			R.bindingX, rowY + ROW_H - 7);
		drawButton(R.learn.x, R.learn.y, R.learn.width, R.learn.height, "Learn", rebindIndex == i && learning);
		drawButton(R.remove.x, R.remove.y, R.remove.width, R.remove.height, "X", false);
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
	if (conflictPromptOpen)
	{
		if (conflictButtonRect(0).inside((float)x, (float)y))
		{
			resolveConflict(false);
			return true;
		}
		if (conflictButtonRect(1).inside((float)x, (float)y))
		{
			resolveConflict(true);
			return true;
		}
		if (conflictButtonRect(2).inside((float)x, (float)y))
		{
			cancelConflict();
			return true;
		}
		return true;   // modal: swallow everything else while it is up
	}
	if (!panelFrame().inside((float)x, (float)y))
	{
		return false;
	}

	if (button != OF_MOUSE_BUTTON_LEFT)
	{
		return true;
	}

	// One guard for the whole function: a point inside an open dropdown belongs
	// to the dropdown. The dropdowns are drawn LAST (on top) but three of their
	// four hit tests sat AFTER the Learn button and the select fields, so
	// clicking a list row that covered Learn armed a learn instead of picking.
	const ofRectangle openDropdown = getOpenDropdownBounds();
	const bool overDropdown = openDropdown.getWidth() > 0.0f &&
		openDropdown.inside((float)x, (float)y);

	// A click anywhere outside an open dropdown just dismisses it. This has to
	// run before every other test: the header buttons are not covered by the
	// list, so a per-test guard would still let them fire on the same press
	// that closes it.
	if (hasOpenDropdown() && !overDropdown)
	{
		closeDropdowns();
		return true;
	}

	if (mapDeviceSelectOpen)
	{
		vector<string> names = getMapDeviceNames();
		DropdownLayout dropdown = getMapDeviceDropdownLayout(layout);
		if (pointInRect(x, y, dropdown.x, dropdown.y, dropdown.w, dropdown.h))
		{
			float clickY = y - dropdown.y - 2;
			int clickedIndex = clickY / (ROW_H + 2);
			if (clickedIndex >= 0 && clickedIndex < names.size())
			{
				setActiveMapDevice(names[clickedIndex]);
				saveGlobal();
			}
			return true;
		}
		mapDeviceSelectOpen = false;
	}

	// Check header buttons
	const ofRectangle headerFrame = panelFrame();
	if (!overDropdown && jp_screen::actionSlotRun(headerFrame, headerSlotWidths(), 1).inside((float)x, (float)y))
	{
		editMode = !editMode;
		cancelLearning();
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}
	if (!overDropdown && jp_screen::actionSlotRun(headerFrame, headerSlotWidths(), 0).inside((float)x, (float)y))
	{
		openInputs();
		cancelLearning();
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}
	// Learn now sits under the fields it arms, not in the header where it
	// silently bound "Bypass on the first box" from a stale selection.
	if (!overDropdown && ofRectangle(layout.rightX + SELECT_LABEL_W,
			layout.learnY - SELECT_FIELD_Y_OFFSET,
			200.0f, ROW_H).inside((float)x, (float)y))
	{
		if (learning || !hasLearnTarget())
		{
			cancelLearning();
		}
		else
		{
			learning = true;
			rebindIndex = -1;
		}
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}
	if (!overDropdown && pointInRect(x, y, layout.rightX + SELECT_LABEL_W,
		layout.mapDeviceY - SELECT_FIELD_Y_OFFSET,
		layout.rightW - SELECT_LABEL_W, ROW_H))
	{
		mapDeviceSelectOpen = !mapDeviceSelectOpen;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		focusedAddShaderRow = -1;
		return true;
	}

	if (!overDropdown && pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.paramY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		parameterSectionCollapsed = !parameterSectionCollapsed;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}

	if (!overDropdown && pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.globalY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		globalFunctionsCollapsed = !globalFunctionsCollapsed;
		focusedAddShaderRow = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}

	if (!overDropdown && pointInRect(x, y, layout.innerX + layout.innerW - 78, layout.addShaderY - SELECT_FIELD_Y_OFFSET, 78, ROW_H))
	{
		addShaderSectionCollapsed = !addShaderSectionCollapsed;
		focusedAddShaderRow = -1;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
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
			targetBoxSelectOpen = false;
			return true;   // dismiss only
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
			actionSelectOpen = false;
			return true;   // dismiss only, do not also actuate underneath
		}
	}

	// Check Target Box select field box click
	if (boxes != nullptr)
	{
		if (!overDropdown && pointInRect(x, y, layout.rightX + SELECT_LABEL_W, layout.targetBoxY - SELECT_FIELD_Y_OFFSET, layout.rightW - SELECT_LABEL_W, ROW_H))
		{
			focusedAddShaderRow = -1;
			targetBoxSelectOpen = !targetBoxSelectOpen;
			actionSelectOpen = false;
			mapDeviceSelectOpen = false;
			return true;
		}
	}

	// Check Action select field box click
	if (paramSelectOpen)
	{
		const DropdownLayout dropdown = getParamDropdownLayout(layout);
		if (pointInRect(x, y, dropdown.x, dropdown.y, dropdown.w, dropdown.h))
		{
			const int picked = (int)((y - dropdown.y - 2) / (ROW_H + 2));
			if (picked >= 0 && picked < getGlobalParameterIndexCount())
			{
				// Same field the left column's Parameter-index rows write, so
				// the two stay in sync and the left row highlight follows.
				selectedParameterIndex = picked;
				selectedAction = PARAMETER;
			}
			paramSelectOpen = false;
			return true;
		}
		paramSelectOpen = false;
		return true;   // dismiss only
	}
	if (!overDropdown && selectedAction == PARAMETER &&
		pointInRect(x, y, layout.rightX + SELECT_LABEL_W,
			layout.paramPickY - SELECT_FIELD_Y_OFFSET, 200.0f, ROW_H))
	{
		paramSelectOpen = !paramSelectOpen;
		targetBoxSelectOpen = false;
		actionSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}
	if (!overDropdown && pointInRect(x, y, layout.rightX + SELECT_LABEL_W,
		layout.actionY - SELECT_FIELD_Y_OFFSET, 200.0f, ROW_H))
	{
		focusedAddShaderRow = -1;
		actionSelectOpen = !actionSelectOpen;
		targetBoxSelectOpen = false;
		mapDeviceSelectOpen = false;
		return true;
	}

	// Rows scrolled out of a column are invisible; they must not answer clicks
	// either. Drawing is clipped by beginColumnClip, hit-testing was not, so a
	// scrolled-away row still fired from wherever it used to be.
	const bool inLeftCol = !overDropdown &&
		layout.leftCol.inside((float)x, (float)y);
	const bool inRightCol = !overDropdown &&
		layout.rightCol.inside((float)x, (float)y);

	// Check parameter list clicks
	if (boxes != nullptr && !parameterSectionCollapsed && inLeftCol)
	{
		float rowY = layout.paramY + PARAM_HEADER_H;
		float rowW = layout.innerW;
		for (int i = 0; i < getGlobalParameterIndexCount(); i++)
		{
			if (rowRects(layout.innerX, rowY, rowW, false).learn.inside((float)x, (float)y))
			{
				Binding binding;
				binding.action = PARAMETER;
				binding.parameterIndex = i;
				focusedAddShaderRow = -1;
				armLearn(binding, findParameterBindingForIndex(i));
				return true;
			}
			if (rowRects(layout.innerX, rowY, rowW, false).remove.inside((float)x, (float)y))
			{
				int bindingIndex = findParameterBindingForIndex(i);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
					invalidateBindingCache();
					saveGlobal();
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

	if (boxes != nullptr && !globalFunctionsCollapsed && inLeftCol)
	{
		vector<Action> globalActions = getGlobalActions();
		float rowY = layout.globalY + PARAM_HEADER_H;
		for (int i = 0; i < globalActions.size(); i++)
		{
			if (rowRects(layout.innerX, rowY, layout.innerW, false).learn.inside((float)x, (float)y))
			{
				Binding binding;
				binding.action = globalActions[i];
				focusedAddShaderRow = -1;
				armLearn(binding, findGlobalActionBinding(globalActions[i]));
				return true;
			}
			if (rowRects(layout.innerX, rowY, layout.innerW, false).remove.inside((float)x, (float)y))
			{
				int bindingIndex = findGlobalActionBinding(globalActions[i]);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
					invalidateBindingCache();
					saveGlobal();
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

	if (!addShaderSectionCollapsed && inLeftCol)
	{
		float rowY = layout.addShaderY + PARAM_HEADER_H;
		ensureAddShaderDraftRow();
		for (int i = 0; i < addShaderRows.size(); i++)
		{
			if (rowRects(layout.innerX, rowY, layout.innerW, true).find.inside((float)x, (float)y))
			{
				resolveAddShaderRow(i);
				completeAddShaderRow(i);
				return true;
			}
			if (rowRects(layout.innerX, rowY, layout.innerW, true).learn.inside((float)x, (float)y))
			{
				if (!addShaderRows[i].empty())
				{
					if (i >= addShaderResolvedPaths.size() || addShaderResolvedPaths[i].empty())
					{
						resolveAddShaderRow(i);
						completeAddShaderRow(i);
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
			if (rowRects(layout.innerX, rowY, layout.innerW, true).remove.inside((float)x, (float)y))
			{
				int bindingIndex = findAddShaderBinding(addShaderRows[i]);
				if (bindingIndex >= 0)
				{
					bindings.erase(bindings.begin() + bindingIndex);
					invalidateBindingCache();
					saveGlobal();
				}
				addShaderRows.erase(addShaderRows.begin() + i);
				// The cursor vector is parallel to the rows; leaving it alone
				// shifted every later row's caret by one.
				if (i < (int)addShaderCursors.size())
				{
					addShaderCursors.erase(addShaderCursors.begin() + i);
				}
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
			{
				// Body is everything left of the buttons. This was
				// `innerW - 98` while Find starts at `w - 146`, so a 48px band
				// answered to neither the body nor any button.
				const RowRects RR = rowRects(layout.innerX, rowY,
					layout.innerW, true);
				const ofRectangle bodyRect(RR.body.x, RR.body.y,
					RR.find.x - RR.body.x - 6.0f, RR.body.height);
				if (bodyRect.inside((float)x, (float)y))
				{
					focusedAddShaderRow = i;
					addShaderQuery = addShaderRows[i];
					selectedAction = ADD_SHADER_BOX;
					targetBoxSelectOpen = false;
					actionSelectOpen = false;
					// Place the caret where it was clicked, like every other
					// text field in the app.
					if (i < (int)addShaderCursors.size())
					{
						const string &text = addShaderRows[i];
						const jp_textfield::Window win =
							jp_textfield::visibleWindow(jp_constants::p_font,
								text, addShaderCursors[i], RR.labelMaxW);
						const string shown =
							text.substr(win.start, win.end - win.start);
						addShaderCursors[i] = ofClamp(win.start +
							jp_textfield::cursorFromX(jp_constants::p_font,
								shown, RR.labelX, (float)x),
							0, (int)text.size());
					}
					return true;
				}
			}
			rowY += ROW_H + 4;
		}
		focusedAddShaderRow = -1;
	}

	// Check Bindings list clicks
	// Only below the pinned context fields, and only inside the column.
	const vector<int> customHitIndices =
		(inRightCol && (float)y > layout.learnY + ROW_H) ?
		getCustomBindingIndices() : vector<int>();
	int bindingRow = 0;
	for (int hitPos = 0; hitPos < (int)customHitIndices.size(); hitPos++)
	{
		const int i = customHitIndices[hitPos];

		float rowY = layout.bindingsY + 16 + bindingRow * (ROW_H + 4);
		if (rowRects(layout.rightX, rowY, layout.rightW, false).learn.inside((float)x, (float)y))
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
		if (rowRects(layout.rightX, rowY, layout.rightW, false).remove.inside((float)x, (float)y))
		{
			bindings.erase(bindings.begin() + i);
			invalidateBindingCache();
			saveGlobal();
			learning = false;
			rebindIndex = -1;
			return true;
		}
		bindingRow++;
	}

	return true;
}

int JPMidiKeymap::focusSectionRowCount(FocusSection section) const
{
	switch (section)
	{
	case FOCUS_PARAM:     return getParameterRowCount();
	case FOCUS_GLOBAL:    return getGlobalActionRowCount();
	case FOCUS_ADDSHADER: return getAddShaderRowCount();
	case FOCUS_BINDING:   return getNonParameterBindingCount();
	default:              return 0;
	}
}

bool JPMidiKeymap::isRowFocused(FocusSection section, int row) const
{
	return focusSection == section && focusRow == row;
}

void JPMidiKeymap::cycleFocusSection(bool backwards)
{
	// Skip collapsed or empty sections so Tab never lands nowhere.
	const FocusSection order[] = {FOCUS_PARAM, FOCUS_GLOBAL, FOCUS_ADDSHADER,
		FOCUS_BINDING};
	int start = 0;
	for (int i = 0; i < 4; i++) if (order[i] == focusSection) start = i;
	for (int step = 1; step <= 4; step++)
	{
		const int i = ((start + (backwards ? -step : step)) % 4 + 4) % 4;
		if (focusSectionRowCount(order[i]) > 0)
		{
			focusSection = order[i];
			focusRow = 0;
			ensureFocusVisible();
			return;
		}
	}
}

void JPMidiKeymap::moveFocus(int delta)
{
	if (focusSection == FOCUS_NONE)
	{
		focusSection = FOCUS_PARAM;
		focusRow = 0;
		if (focusSectionRowCount(focusSection) == 0) cycleFocusSection(false);
		ensureFocusVisible();
		return;
	}
	const int count = focusSectionRowCount(focusSection);
	if (count <= 0) { cycleFocusSection(delta < 0); return; }
	const int next = focusRow + delta;
	if (next < 0 || next >= count)
	{
		// Roll into the neighbouring section rather than stopping dead.
		cycleFocusSection(delta < 0);
		if (delta < 0)
		{
			focusRow = std::max(0, focusSectionRowCount(focusSection) - 1);
		}
	}
	else
	{
		focusRow = next;
	}
	ensureFocusVisible();
}

void JPMidiKeymap::ensureFocusVisible()
{
	if (focusSection == FOCUS_NONE || focusRow < 0) return;
	const PanelLayout L = getPanelLayout();
	float rowY = 0.0f;
	bool right = false;
	switch (focusSection)
	{
	case FOCUS_PARAM:     rowY = L.paramY + PARAM_HEADER_H; break;
	case FOCUS_GLOBAL:    rowY = L.globalY + PARAM_HEADER_H; break;
	case FOCUS_ADDSHADER: rowY = L.addShaderY + PARAM_HEADER_H; break;
	case FOCUS_BINDING:   rowY = L.bindingsY + 16.0f; right = true; break;
	default: return;
	}
	rowY += focusRow * (ROW_H + 4);
	const ofRectangle &col = right ? L.rightCol : L.leftCol;
	float &scroll = right ? rightScroll : leftScroll;
	const float contentH = right ? L.rightContentH : L.leftContentH;
	const float maxScroll = std::max(0.0f, contentH - col.height);
	if (rowY < col.y) scroll = ofClamp(scroll - (col.y - rowY), 0.0f, maxScroll);
	else if (rowY + ROW_H > col.getMaxY())
		scroll = ofClamp(scroll + (rowY + ROW_H - col.getMaxY()), 0.0f, maxScroll);
}

void JPMidiKeymap::activateFocusedRow()
{
	if (focusSection == FOCUS_NONE || focusRow < 0) return;
	Binding binding;
	binding.key = MidiKey();
	switch (focusSection)
	{
	case FOCUS_PARAM:
		binding.boxName = selectedBoxName;
		binding.action = PARAMETER;
		binding.parameterIndex = focusRow;
		selectedAction = PARAMETER;
		selectedParameterIndex = focusRow;
		armLearn(binding, findParameterBindingForIndex(focusRow));
		return;
	case FOCUS_GLOBAL:
	{
		const vector<Action> actions = getGlobalActions();
		if (focusRow >= (int)actions.size()) return;
		binding.action = actions[focusRow];
		selectedAction = actions[focusRow];
		armLearn(binding, findGlobalActionBinding(actions[focusRow]));
		return;
	}
	case FOCUS_ADDSHADER:
		focusedAddShaderRow = focusRow;   // hand over to text entry
		return;
	case FOCUS_BINDING:
	{
		// Same list the right column renders, so keyboard focus and the drawn
		// rows can never address different bindings.
		const vector<int> custom = getCustomBindingIndices();
		if (focusRow < (int)custom.size())
		{
			const int i = custom[focusRow];
			selectedBoxName = bindings[i].boxName;
			selectedAction = bindings[i].action;
			selectedParameterIndex = bindings[i].parameterIndex;
			learning = true;
			rebindIndex = i;
			return;
		}
		return;
	}
	default: return;
	}
}

void JPMidiKeymap::unbindFocusedRow()
{
	if (focusSection == FOCUS_NONE || focusRow < 0) return;
	int index = -1;
	if (focusSection == FOCUS_PARAM)
	{
		index = findParameterBindingForIndex(focusRow);
	}
	else if (focusSection == FOCUS_GLOBAL)
	{
		const vector<Action> actions = getGlobalActions();
		if (focusRow < (int)actions.size())
			index = findGlobalActionBinding(actions[focusRow]);
	}
	else if (focusSection == FOCUS_BINDING)
	{
		const vector<int> custom = getCustomBindingIndices();
		if (focusRow < (int)custom.size()) index = custom[focusRow];
	}
	if (index < 0) return;
	bindings.erase(bindings.begin() + index);
	invalidateBindingCache();
	saveGlobal();
	learning = false;
	rebindIndex = -1;
	focusRow = std::min(focusRow,
		std::max(0, focusSectionRowCount(focusSection) - 1));
}

bool JPMidiKeymap::keyPressed(int key)
{
	if (!panelOpen) return false;

	// Row navigation, when a text row is NOT being typed into. Anything not
	// handled here falls through, so the global 1-6 and ESC keep working -
	// this function used to swallow every key while a row had focus.
	if (focusedAddShaderRow < 0 || focusedAddShaderRow >= (int)addShaderRows.size())
	{
		switch (key)
		{
		case OF_KEY_UP:    moveFocus(-1); return true;
		case OF_KEY_DOWN:  moveFocus(1);  return true;
		case OF_KEY_TAB:
			cycleFocusSection(ofGetKeyPressed(OF_KEY_SHIFT));
			return true;
		case OF_KEY_RETURN:   // == '\r' in openFrameworks
			if (focusSection == FOCUS_NONE) return false;
			activateFocusedRow();
			return true;
		case OF_KEY_DEL: case OF_KEY_BACKSPACE:
			if (focusSection == FOCUS_NONE) return false;
			unbindFocusedRow();
			return true;
		case OF_KEY_ESC:
			// Clear row focus first; a second press falls through to the
			// app-wide "close the topmost surface" rule.
			if (focusSection == FOCUS_NONE) return false;
			focusSection = FOCUS_NONE;
			focusRow = -1;
			return true;
		default:
			return false;
		}
	}

	if (key == OF_KEY_ESC || key == 27)
	{
		focusedAddShaderRow = -1;
		return true;
	}
	if (key == OF_KEY_RETURN)
	{
		if (!addShaderRows[focusedAddShaderRow].empty())
		{
			if (addShaderResolvedPaths[focusedAddShaderRow].empty())
			{
				resolveAddShaderRow(focusedAddShaderRow);
			}
			completeAddShaderRow(focusedAddShaderRow);
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

	// Full editing - arrows, HOME/END, DEL, insert at the cursor. This used to
	// be append-plus-backspace-at-end with no cursor at all.
	if (focusedAddShaderRow >= (int)addShaderCursors.size())
	{
		ensureAddShaderDraftRow();
	}
	int &cursor = addShaderCursors[focusedAddShaderRow];
	string &text = addShaderRows[focusedAddShaderRow];
	const string before = text;
	if (jp_textfield::handleKey(text, cursor, key))
	{
		if (text != before)
		{
			addShaderQuery = text;
			addShaderResolvedPaths[focusedAddShaderRow] = "";
			addShaderSearched[focusedAddShaderRow] = false;
			ensureAddShaderDraftRow();
		}
		return true;
	}
	// Anything the field did not consume falls through, so the global
	// shortcuts keep working while a row is focused.
	return false;
}

bool JPMidiKeymap::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
	if (!panelOpen)
	{
		return false;
	}

	if (!targetBoxSelectOpen)
	{
		// The body had no scrolling at all, so the wheel fell through to the
		// node canvas behind the screen and panned it instead.
		const PanelLayout L = getPanelLayout();
		const float step = scrollY * 40.0f;
		if (L.rightCol.inside((float)x, (float)y))
		{
			rightScroll = ofClamp(rightScroll - step, 0.0f,
				std::max(0.0f, L.rightContentH - L.rightCol.height));
			return true;
		}
		if (L.leftCol.inside((float)x, (float)y))
		{
			leftScroll = ofClamp(leftScroll - step, 0.0f,
				std::max(0.0f, L.leftContentH - L.leftCol.height));
			return true;
		}
		// Consume anywhere on the screen so nothing leaks to the canvas.
		return panelFrame().inside((float)x, (float)y);
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
	JPdragobject::setMouseOverride(boxes->screenToCanvas(ofVec2f(x, y)));
	for (int i = int(boxes->boxes.size()) - 1; i >= 0; i--)
	{
		JPbox *box = boxes->boxes[i];
		Binding binding;
		binding.boxName = box->name;
		if (box->bypass.mouseOver())
		{
			JPdragobject::clearMouseOverride();
			binding.action = BYPASS;
			armLearn(binding);
			return true;
		}
		if (box->onoff.mouseOver())
		{
			JPdragobject::clearMouseOverride();
			binding.action = PAUSE;
			armLearn(binding);
			return true;
		}
		if (box->mouseOver())
		{
			JPdragobject::clearMouseOverride();
			binding.action = SELECT_OPEN_BOX;
			armLearn(binding);
			return true;
		}
	}
	JPdragobject::clearMouseOverride();
	return false;
}

bool JPMidiKeymap::tryCaptureInspectorFunctionClick(int x, int y)
{
	if (boxes == nullptr || boxes->getInspectorBox() == nullptr)
	{
		return false;
	}
	for (int i = 0; i < boxes->getOpenParameterCount(); i++)
	{
		JPParameter *parameter =
			boxes->getOpenParameterAtIndex(i);
		JPcontroller *controller = boxes->controllers[i];
		if (parameter != nullptr && controller != nullptr &&
			controller->mouseOver() &&
			(parameter->variabletype == JPParameter::FLOAT ||
			 parameter->variabletype == JPParameter::BOOL))
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

void JPMidiKeymap::setPanelVisible(bool visible)
{
	if (panelOpen == visible) return;
	panelOpen = visible;
	// Leaving the screen must not strand a dropdown or a half-armed learn.
	if (!visible)
	{
		closeDropdowns();
		if (learning) cancelLearning();
		// Nor a conflict prompt: draw() stops rendering it off-screen, but the
		// surface stays open and modal, so every click in the whole app was
		// blocked by a prompt nothing was drawing.
		cancelConflict();
		focusedAddShaderRow = -1;
	}
}

void JPMidiKeymap::closePanel()
{
	// Leaving a dropdown or a half-finished learn armed behind a closed panel
	// would keep swallowing clicks with nothing on screen to explain why.
	closeDropdowns();
	if (learning) cancelLearning();
	focusedAddShaderRow = -1;
	panelOpen = false;
}

bool JPMidiKeymap::hasOpenDropdown() const
{
	return targetBoxSelectOpen || actionSelectOpen || mapDeviceSelectOpen ||
		paramSelectOpen;
}

void JPMidiKeymap::closeDropdowns()
{
	targetBoxSelectOpen = false;
	actionSelectOpen = false;
	mapDeviceSelectOpen = false;
	paramSelectOpen = false;
}

ofRectangle JPMidiKeymap::getOpenDropdownBounds() const
{
	if (!panelOpen) return ofRectangle();
	const PanelLayout layout = getPanelLayout();
	DropdownLayout d;
	if (mapDeviceSelectOpen) d = getMapDeviceDropdownLayout(layout);
	else if (targetBoxSelectOpen) d = getTargetBoxDropdownLayout(layout);
	else if (actionSelectOpen) d = getActionDropdownLayout(layout);
	else if (paramSelectOpen) d = getParamDropdownLayout(layout);
	else return ofRectangle();
	return ofRectangle(d.x, d.y, d.w, d.h);
}

ofRectangle JPMidiKeymap::getPanelBounds() const
{
	if (!panelOpen) return ofRectangle();
	return panelFrame();
}

void JPMidiKeymap::save(string path)
{
	ofXml xml;

	auto keymap = xml.appendChild("midikeymap");
	keymap.appendChild("active_device").set(activeMapDeviceName);
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
	ofFilePath::createEnclosingDirectory(path);
	xml.save(path);
}

void JPMidiKeymap::load(string path)
{
	bindings.clear();
	invalidateBindingCache();
	addShaderRows.clear();
	addShaderResolvedPaths.clear();
	addShaderSearched.clear();
	focusedAddShaderRow = -1;
	addShaderQuery = "";
	cancelLearning();

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
	auto activeDevice = keymap.getChild("active_device");
	if (activeDevice)
	{
		activeMapDeviceName = normalizeDeviceName(activeDevice.getValue());
		refreshActiveDeviceCache();
	}

	for (auto &bindingNode : keymap.getChildren("binding"))
	{
		Binding binding;
		binding.key.deviceName = normalizeDeviceName(bindingNode.getChild("device").getValue());
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
		if (binding.action == ADD_SHADER_BOX && binding.shaderPath.empty() && !binding.shaderQuery.empty())
		{
			binding.shaderPath = resolveShaderQuery(binding.shaderQuery);
		}
		if (isBindingLoadable(binding))
		{
			bindings.push_back(binding);
			invalidateBindingCache();
		invalidateBindingCache();
		}
	}
	ensureActiveMapDevice();
	syncAddShaderRowsFromBindings();
}
