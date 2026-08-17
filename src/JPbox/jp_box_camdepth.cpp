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
	// The two new cues default to ZERO weight on purpose. Loading is by name,
	// so a compo saved before these existed keeps its tuned values for the old
	// parameters and picks up the defaults for these - and a non-zero default
	// would silently change how every already-saved CAM DEPTH box looks. Raise
	// "peso paralaje" to bring in the motion cue.
	parameters.addFloatValue(0.0, "peso paralaje");
	parameters.addFloatValue(0.0, "peso aire");
	// How long motion lingers once it stops. High, because a subject holding
	// still for a moment has not moved to the back of the room.
	parameters.addFloatValue(0.85, "retencion");
	parameters.addFloatValue(0.5, "ganancia mov");
	// Heat ramp OFF by default: grey is the contract that makes this box
	// interchangeable with a Kinect DEPTH box upstream of a displacement
	// shader, and a saved compo must not change what it feeds downstream.
	parameters.addBoolValue(false, "colores");
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
	parallaxWeightParam = index("peso paralaje");
	aerialWeightParam = index("peso aire");
	parallaxHoldParam = index("retencion");
	parallaxGainParam = index("ganancia mov");
	colourParam = index("colores");

	availableDeviceIds = JPbox_cam::availableCameraIds();
	appliedRescanGeneration = JPbox_cam::cameraRescanCount();
	applyCameraIndexFromParameter(true);
}

bool JPbox_camdepth::ensureShader()
{
	// The motion pass is loaded alongside but is NOT required: if only it fails
	// the box still produces depth from the appearance cues, just without
	// parallax. Failing the whole box over an optional cue would be worse than
	// losing the cue.
	if (!showShader.isLoaded())
	{
		if (!showShader.load("shaders/default.vert",
			"shaders/private/camdepth_show.frag"))
		{
			ofLogError("CAMDEPTH")
				<< "could not load shaders/private/camdepth_show.frag - "
				<< "heat ramp disabled";
		}
	}
	if (!motionShader.isLoaded())
	{
		if (!motionShader.load("shaders/default.vert",
			"shaders/private/camdepth_motion.frag"))
		{
			ofLogError("CAMDEPTH")
				<< "could not load shaders/private/camdepth_motion.frag - "
				<< "parallax cue disabled";
		}
	}
	if (shader.isLoaded()) return true;
	if (shader.load("shaders/default.vert", "shaders/private/camdepth.frag"))
		return true;
	// Logged once per attempt rather than silently drawing black, because a
	// missing shader and a missing camera look identical on screen otherwise.
	ofLogError("CAMDEPTH") << "could not load shaders/private/camdepth.frag";
	return false;
}

void JPbox_camdepth::updateMotion()
{
	if (!motionShader.isLoaded() || !cameraSource || !cameraSource->hasTexture())
		return;

	const int w = std::max(1, (int)fbo.getWidth() / kMotionDivisor);
	const int h = std::max(1, (int)fbo.getHeight() / kMotionDivisor);
	for (int i = 0; i < 2; ++i)
	{
		if (motion[i].isAllocated() &&
			(int)motion[i].getWidth() == w && (int)motion[i].getHeight() == h)
		{
			continue;
		}
		// GL_RGBA16F, not the default 8-bit: the accumulator decays by a factor
		// each frame, and at 8 bits a value below 1/255 quantises straight to
		// zero - so the peak-hold would drop out in steps instead of fading.
		motion[i].allocate(w, h, GL_RGBA16F);
		motion[i].begin();
		ofClear(0, 0, 0, 255);
		motion[i].end();
	}

	const int read = motionWrite;
	const int write = 1 - motionWrite;

	auto value = [this](int index, float fallback) {
		return index >= 0 ? parameters.getFloatValue(index) : fallback;
	};
	auto flag = [this](int index, bool fallback) {
		return index >= 0 ? parameters.getBoolValue(index) : fallback;
	};

	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255, 255, 255, 255);
	motion[write].begin();
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	motionShader.begin();
	motionShader.setUniformTexture("camara", cameraSource->getTexture(), 1);
	motionShader.setUniformTexture("anterior", motion[read].getTexture(), 2);
	motionShader.setUniform2f("resolution", (float)w, (float)h);
	motionShader.setUniform1f("espejo", flag(mirrorParam, true) ? 1.0f : 0.0f);
	// 0..1 maps to 0.5..0.99 of the previous energy retained per frame. Below
	// about 0.5 the cue flickers off between frames of slow movement.
	motionShader.setUniform1f("retencion",
		ofLerp(0.5f, 0.99f, ofClamp(value(parallaxHoldParam, 0.85f), 0.0f, 1.0f)));
	// 0..1 maps to 1..30. A person crossing frame moves a few percent of luma
	// per frame, so unity gain would leave the cue almost black.
	motionShader.setUniform1f("ganancia",
		1.0f + value(parallaxGainParam, 0.5f) * 29.0f);
	ofDrawRectangle(0, 0, (float)w, (float)h);
	motionShader.end();
	motion[write].end();
	ofEnableAlphaBlending();

	motionWrite = write;
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
	// Before the depth pass: the depth pass reads what this writes.
	updateMotion();

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
	// Falls back to the history texture when the motion pass never ran (shader
	// missing). Its .g is the depth value rather than motion energy, which
	// would be wrong - so the weight is forced to zero in that case below,
	// and binding something valid just keeps the sampler from reading garbage.
	const bool motionReady = motion[motionWrite].isAllocated();
	shader.setUniformTexture("movimiento", motionReady ?
		motion[motionWrite].getTexture() : history.getTexture(), 3);
	shader.setUniform2f("resolution", fbo.getWidth(), fbo.getHeight());
	shader.setUniform1f("pesoFoco", value(focusWeightParam, 0.5f));
	shader.setUniform1f("pesoBrillo", value(brightWeightParam, 0.5f));
	shader.setUniform1f("pesoVertical", value(verticalWeightParam, 0.5f));
	shader.setUniform1f("pesoParalaje",
		motionReady ? value(parallaxWeightParam, 0.0f) : 0.0f);
	shader.setUniform1f("pesoAire", value(aerialWeightParam, 0.0f));
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

	// Heat ramp LAST, and only if asked.
	//
	// After the history copy on purpose: history must keep the grey depth map,
	// because next frame's temporal smoothing reads it back as a depth value.
	// Colouring before the copy would feed the ramp's red channel - which
	// saturates across the whole near half of the range - into that feedback.
	//
	// Reads history rather than fbo because a pass cannot read the target it is
	// writing, and at this point the two hold the same grey image.
	if (flag(colourParam, false) && showShader.isLoaded())
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255, 255, 255);
		fbo.begin();
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		showShader.begin();
		showShader.setUniformTexture("profundidad", history.getTexture(), 1);
		showShader.setUniform2f("resolution", fbo.getWidth(), fbo.getHeight());
		ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
		showShader.end();
		fbo.end();
		ofEnableAlphaBlending();
	}
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
	motion[0].clear();
	motion[1].clear();
	motionWrite = 0;
	JPbox::clear();
}
