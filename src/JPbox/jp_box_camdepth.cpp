#include "jp_box_camdepth.h"

#include <cmath>

JPbox_camdepth::JPbox_camdepth() {}
JPbox_camdepth::~JPbox_camdepth() {}

void JPbox_camdepth::setup(string _dir, string _name)
{
	JPbox::setup(_dir, _name);
	tipo = CAMDEPTHBOX;
	name = _name;
	dir = _dir;

	// Weights start balanced: which cue works depends entirely on the footage,
	// and there is no default that is right for both a lit subject on a dark
	// stage and a flat evenly-lit room.
	parameters.addFloatValue(0.0, "camaraindex");
	parameters.addFloatValue(0.5, "peso foco");
	parameters.addFloatValue(0.5, "peso brillo");
	parameters.addFloatValue(0.5, "peso vertical");
	parameters.addFloatValue(0.25, "radio");
	parameters.addFloatValue(0.5, "contraste");
	parameters.addFloatValue(0.0, "cerca");
	parameters.addFloatValue(1.0, "lejos");
	// Not zero: a raw per-frame estimate off a camera flickers, and every
	// displacement shader downstream amplifies it. This is the control that
	// decides whether the box is usable.
	parameters.addFloatValue(0.6, "suavizado");
	// Edge-aware by default. A plain blur smears object boundaries across the
	// background, which is the artefact Depth Anything V2 is specifically good
	// at avoiding - and the thing that makes a depth map look convincing.
	parameters.addFloatValue(0.7, "bordes");
	// Some disparity bias by default, matching V2's inverse-depth output: most
	// of the range describes what is close.
	parameters.addFloatValue(0.4, "curva");
	parameters.addBoolValue(false, "invertir");
	parameters.addBoolValue(true, "piso abajo");
	parameters.addBoolValue(true, "espejo");

	auto index = [this](const char *n) { return parameters.indexOfName(n); };
	cameraIndexParam = index("camaraindex");
	focusWeightParam = index("peso foco");
	brightWeightParam = index("peso brillo");
	verticalWeightParam = index("peso vertical");
	radiusParam = index("radio");
	contrastParam = index("contraste");
	nearParam = index("cerca");
	farParam = index("lejos");
	smoothParam = index("suavizado");
	edgeParam = index("bordes");
	curveParam = index("curva");
	invertParam = index("invertir");
	floorParam = index("piso abajo");
	mirrorParam = index("espejo");

	availableDeviceIds = JPbox_cam::availableCameraIds();
	appliedRescanGeneration = JPbox_cam::cameraRescanCount();
	applyCameraIndexFromParameter(true);
}

bool JPbox_camdepth::ensureShader()
{
	if (shader.isLoaded()) return true;
	if (shader.load("shaders/default.vert", "shaders/private/camdepth.frag"))
		return true;
	// Logged once per attempt rather than silently drawing black, because a
	// missing shader and a missing camera look identical on screen otherwise.
	ofLogError("CAMDEPTH") << "could not load shaders/private/camdepth.frag";
	return false;
}

void JPbox_camdepth::ensureHistory()
{
	if (history.isAllocated() &&
		(int)history.getWidth() == (int)fbo.getWidth() &&
		(int)history.getHeight() == (int)fbo.getHeight())
	{
		return;
	}
	history.allocate(fbo.getWidth(), fbo.getHeight());
	history.begin();
	ofClear(0, 0, 0, 255);
	history.end();
}

void JPbox_camdepth::applyCameraIndexFromParameter(bool force)
{
	if (cameraIndexParam < 0) return;
	if (availableDeviceIds.empty())
	{
		availableDeviceIds = JPbox_cam::availableCameraIds();
	}
	if (availableDeviceIds.empty()) return;

	// Same 0..1 -> device mapping CAMARITA uses, so the slider means the same
	// thing on both boxes.
	int targetListIndex = int(std::round(ofMap(
		parameters.getFloatValue(cameraIndexParam), 0.0f, 1.0f,
		0.0f, float(availableDeviceIds.size() - 1), true)));
	targetListIndex = ofClamp(targetListIndex, 0,
		int(availableDeviceIds.size()) - 1);
	const int targetDeviceId = availableDeviceIds[targetListIndex];

	if (!force && targetDeviceId == currentDeviceId && cameraSource) return;

	// Released before acquiring: if this box already held the device, dropping
	// it first lets the refcounted source be reused rather than the map briefly
	// holding two entries for one camera.
	cameraSource.reset();
	cameraSource = JPbox_cam::acquireSharedCamera(
		targetDeviceId, camWidth, camHeight);
	currentDeviceId = targetDeviceId;
	currentCameraListIndex = targetListIndex;
}

void JPbox_camdepth::update()
{
	JPbox::update();
	// A camera plugged in after startup shows up on the next rescan, the same
	// way it does for CAMARITA.
	const uint64_t generation = JPbox_cam::cameraRescanCount();
	if (generation != appliedRescanGeneration)
	{
		appliedRescanGeneration = generation;
		availableDeviceIds = JPbox_cam::availableCameraIds();
		applyCameraIndexFromParameter(true);
	}
	else
	{
		applyCameraIndexFromParameter(false);
	}
	updateFBO();
}

void JPbox_camdepth::updateFBO()
{
	if (tryPassThroughFBO()) return;
	if (!onoff.boolValue)
	{
		JPbox::updateFBO();
		return;
	}
	if (cameraSource) cameraSource->updateOnce();
	if (!ensureShader() || !cameraSource || !cameraSource->hasTexture())
	{
		// Nothing to estimate from. Leave the last good frame rather than
		// flashing black while a camera reconnects.
		return;
	}
	ensureHistory();

	auto value = [this](int index, float fallback) {
		return index >= 0 ? parameters.getFloatValue(index) : fallback;
	};
	auto flag = [this](int index, bool fallback) {
		return index >= 0 ? parameters.getBoolValue(index) : fallback;
	};

	// Rect mode is global and this runs during update(), so it inherits
	// whatever drew last - the same trap that quartered the group and kinect
	// boxes. Set it before the pass, not after.
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255, 255, 255, 255);
	fbo.begin();
	ofClear(0, 0, 0, 255);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	shader.begin();
	shader.setUniformTexture("camara", cameraSource->getTexture(), 1);
	shader.setUniformTexture("anterior", history.getTexture(), 2);
	shader.setUniform2f("resolution", fbo.getWidth(), fbo.getHeight());
	shader.setUniform1f("pesoFoco", value(focusWeightParam, 0.5f));
	shader.setUniform1f("pesoBrillo", value(brightWeightParam, 0.5f));
	shader.setUniform1f("pesoVertical", value(verticalWeightParam, 0.5f));
	// 0..1 maps to a 1..16 pixel ring; below 1 the taps collapse onto the
	// centre and the focus cue reads a constant zero.
	shader.setUniform1f("radio", 1.0f + value(radiusParam, 0.25f) * 15.0f);
	// 0..1 maps to gamma 0.25..4, with the midpoint at 1 (no change).
	shader.setUniform1f("contraste",
		std::pow(4.0f, value(contrastParam, 0.5f) * 2.0f - 1.0f));
	shader.setUniform1f("cerca", value(nearParam, 0.0f));
	shader.setUniform1f("lejos", value(farParam, 1.0f));
	shader.setUniform1f("suavizado", value(smoothParam, 0.6f));
	shader.setUniform1f("bordes", value(edgeParam, 0.7f));
	shader.setUniform1f("curva", value(curveParam, 0.4f));
	shader.setUniform1f("invertir", flag(invertParam, false) ? 1.0f : 0.0f);
	shader.setUniform1f("pisoAbajo", flag(floorParam, true) ? 1.0f : 0.0f);
	shader.setUniform1f("espejo", flag(mirrorParam, true) ? 1.0f : 0.0f);
	ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
	shader.end();
	fbo.end();
	ofEnableAlphaBlending();

	// Keep a copy for the next frame's smoothing. A pass cannot read the target
	// it is writing, so this costs one full-resolution blit - the price of the
	// temporal filter that stops the map flickering.
	history.begin();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255, 255, 255, 255);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	fbo.draw(0, 0, history.getWidth(), history.getHeight());
	history.end();
	ofEnableAlphaBlending();
}

void JPbox_camdepth::draw()
{
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}

void JPbox_camdepth::clear()
{
	cameraSource.reset();
	currentDeviceId = -1;
	history.clear();
	JPbox::clear();
}
