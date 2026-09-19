#include "ofApp.h"
#include <algorithm>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef NDI
bool ofApp::sendNDIOutput(ofFbo &source)
{
	if (!source.isAllocated() || !ndiSender.SenderCreated()) return false;
	const auto width = ndiSender.GetWidth();
	const auto height = ndiSender.GetHeight();
	if (width == 0 || height == 0) return false;

	// Keep the announced output size across node/transition FBOs and adaptive
	// render quality. ofxNDIsender::SendImage(texture) changes the NDI frame
	// dimensions without resizing its CPU buffers or draining its PBO ring.
	// A larger input can therefore overwrite memory during async readback.
	if (!ndiFbo.isAllocated() || ndiFbo.getWidth() != width || ndiFbo.getHeight() != height)
		ndiFbo.allocate(width, height, GL_RGBA);
	if (!ndiFbo.isAllocated()) return false;

	ndiFbo.begin();
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	ofSetColor(255);
	ofClear(0, 0, 0, 0);
	source.draw(0, 0, width, height);
	ofPopStyle();
	ndiFbo.end();
	return ndiSender.SendImage(ndiFbo);
}
#endif

// Output window lifecycle, source resolution and window callbacks.
// Settings controls and XML persistence stay in ofApp. Events still target
// the same ofApp instance so listener registration and retirement are paired.
JPbox *ofApp::resolveLiveOutputSource(LiveOutputConfig &config)
{
	if (config.sourceMode != LIVE_OUTPUT_FIXED_BOX) return nullptr;
	if (!config.sourceUid.empty())
	{
		JPbox *box = boxes.findBoxByUid(config.sourceUid);
		if (box != nullptr) return box;
	}
	// Legacy binding: settings written before uids existed name the box. Heal
	// it the first time it resolves, so the binding survives the next rename
	// without anyone having to re-pick it. Top-level only, which is all a name
	// could ever address anyway.
	if (!config.sourceBox.empty())
	{
		JPbox *box = boxes.findTopLevelBoxByName(config.sourceBox);
		if (box != nullptr)
		{
			config.sourceUid = box->uid;
			return box;
		}
	}
	return nullptr;
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
void ofApp::window_drawRender(ofEventArgs & args) {
	const int index = findLiveOutputByWindow(ofGetWindowPtr());
	if (index < 0 || !liveOutputs[index].window)
	{
		return;
	}

	const LiveOutputConfig &config = liveOutputs[index].config;
	const float windowW = liveOutputs[index].window->getWidth();
	const float windowH = liveOutputs[index].window->getHeight();
	jp_gl::resetWindowDrawState(windowW, windowH);
	ofClear(0, 0, 0, 255);
	ofSetColor(255);

	// Everything below draws into a WIDTH x HEIGHT frame that is then turned
	// into the window. On a quarter turn the two swap, so a portrait window
	// gets the whole landscape canvas rather than a cropped middle. The
	// rotation wraps the mapping overlay and the test pattern too: a guide that
	// did not turn with its image would be worse than none.
	const int rotation = ((config.rotationDegrees / 90) % 4 + 4) % 4 * 90;
	const bool quarterTurn = rotation == 90 || rotation == 270;
	const float width = quarterTurn ? windowH : windowW;
	const float height = quarterTurn ? windowW : windowH;
	ofPushMatrix();
	if (rotation != 0)
	{
		ofTranslate(windowW * 0.5f, windowH * 0.5f);
		ofRotateDeg((float)rotation);
		ofTranslate(-width * 0.5f, -height * 0.5f);
	}
	struct MatrixGuard { ~MatrixGuard() { ofPopMatrix(); } } matrixGuard;
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
		followMain, config.sourceUid, width, height, crop, bezel, &effective);
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
		boxes.drawMappingOverlayForSource(followMain, config.sourceUid,
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
