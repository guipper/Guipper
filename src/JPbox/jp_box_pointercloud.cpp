#include "jp_box_pointercloud.h"

namespace
{
	// Density maps onto a lattice stride rather than a point count, so the
	// surviving points stay on a regular grid at every setting.
	constexpr int kMinDensityStep = 1;
	constexpr int kMaxDensityStep = 12;

	// The Kinect's usable range. Same bounds the kinect2 box clamps to, so the
	// two boxes agree about what "near" and "far" mean.
	constexpr float kMinRangeMm = 500.0f;
	constexpr float kMaxRangeMm = 4500.0f;
}

JPbox_pointercloud::~JPbox_pointercloud()
{
	clear();
}

void JPbox_pointercloud::setup(string directory, string boxName)
{
	JPbox::setup(directory, boxName);
	// The canonical token is what round-trips through <directory>, so it must
	// not depend on how the box happened to be created.
	dir = "pointercloud";
	name = boxName;
	tipo = POINTERCLOUDBOX;

	parameters.addFloatValue(3.0f, "point size", true);
	parameters.setMin(1.0f, POINT_SIZE);
	parameters.setMax(16.0f, POINT_SIZE);
	parameters.addFloatValue(0.7f, "density");
	parameters.setMin(0.0f, DENSITY);
	parameters.setMax(1.0f, DENSITY);
	parameters.addFloatValue(1.0f, "depth scale", true);
	parameters.setMin(0.0f, DEPTH_SCALE);
	parameters.setMax(3.0f, DEPTH_SCALE);
	parameters.addFloatValue(kMinRangeMm, "near mm");
	parameters.setMin(kMinRangeMm, NEAR_MM);
	parameters.setMax(kMaxRangeMm - 1.0f, NEAR_MM);
	parameters.addFloatValue(kMaxRangeMm, "far mm");
	parameters.setMin(kMinRangeMm + 1.0f, FAR_MM);
	parameters.setMax(kMaxRangeMm, FAR_MM);
	parameters.addFloatValue(0.0f, "rotate X");
	parameters.setMin(-180.0f, ROTATE_X);
	parameters.setMax(180.0f, ROTATE_X);
	parameters.addFloatValue(0.0f, "rotate Y");
	parameters.setMin(-180.0f, ROTATE_Y);
	parameters.setMax(180.0f, ROTATE_Y);
	parameters.addFloatValue(0.0f, "rotate Z");
	parameters.setMin(-180.0f, ROTATE_Z);
	parameters.setMax(180.0f, ROTATE_Z);
	parameters.addFloatValue(1.0f, "zoom", true);
	parameters.setMin(0.1f, ZOOM);
	parameters.setMax(3.0f, ZOOM);
	parameters.addFloatValue(0.5f, "field of view");
	parameters.setMin(0.0f, FIELD_OF_VIEW);
	parameters.setMax(1.0f, FIELD_OF_VIEW);
	parameters.addFloatValue(0.0f, "color cycle", true);
	parameters.setMin(0.0f, COLOR_CYCLE);
	parameters.setMax(1.0f, COLOR_CYCLE);
	parameters.addFloatValue(1.0f, "brightness", true);
	parameters.setMin(0.0f, BRIGHTNESS);
	parameters.setMax(3.0f, BRIGHTNESS);
	parameters.addFloatValue(0.0f, "trails");
	parameters.setMin(0.0f, TRAILS);
	parameters.setMax(0.98f, TRAILS);
	parameters.addBoolValue(false, "additive");
	parameters.addBoolValue(true, "mirror");
	parameters.addBoolValue(false, "flip vertical");

	// Inlet names are written straight out as XML element names, so this has
	// to stay a legal NCName.
	fbohandlergroup.addFbohandler("color");
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setInletPosition();

	shader.load("shaders/private/pointercloud");
	if (!shader.isLoaded())
	{
		ofLogError("JPbox_pointercloud")
			<< "could not load shaders/private/pointercloud - the box will "
			   "render nothing";
	}

	buildLattice();
	ensureRenderTarget();
	capture = JPbox_kinect2::acquireSharedCapture();
	cleared = false;
}

void JPbox_pointercloud::buildLattice()
{
	// One vertex per depth pixel, carrying its own pixel coordinate. Built
	// once and never touched again - density and clipping are shader side.
	const int width = JPbox_kinect2::depthWidth();
	const int height = JPbox_kinect2::depthHeight();
	lattice.clear();
	lattice.setMode(OF_PRIMITIVE_POINTS);
	lattice.getVertices().reserve((size_t)width * height);
	for (int row = 0; row < height; ++row)
	{
		for (int column = 0; column < width; ++column)
		{
			lattice.addVertex(glm::vec3((float)column, (float)row, 0.0f));
		}
	}
}

void JPbox_pointercloud::ensureRenderTarget()
{
	// JPbox::setup allocates the fbo with the 2 arg overload, which attaches
	// no depth buffer - glEnable(GL_DEPTH_TEST) against that is a silent
	// no-op. Render size is also mutable at runtime from the settings screen.
	const int width = jp_constants::renderWidth;
	const int height = jp_constants::renderHeight;
	if (width <= 0 || height <= 0) return;
	if (fbo.isAllocated() && width == targetWidth && height == targetHeight)
	{
		return;
	}

	ofFbo::Settings settings;
	settings.width = width;
	settings.height = height;
	settings.internalformat = GL_RGBA;
	settings.useDepth = true;
	settings.useStencil = false;
	settings.depthStencilAsTexture = false;
	settings.numSamples = 0;
	fbo.allocate(settings);
	fbo.begin();
	ofClear(0, 0, 0, 255);
	fbo.end();
	targetWidth = width;
	targetHeight = height;
}

void JPbox_pointercloud::uploadDepth()
{
	if (!capture) return;
	const bool hadIntrinsics = intrinsics.valid;
	intrinsics = JPbox_kinect2::depthIntrinsics(capture);
	if (intrinsics.valid && !hadIntrinsics)
	{
		// Worth one line: if the cloud ever looks wrongly scaled or skewed,
		// these four numbers are the first thing to check.
		ofLogNotice("JPbox_pointercloud")
			<< "IR intrinsics fx=" << intrinsics.fx
			<< " fy=" << intrinsics.fy
			<< " cx=" << intrinsics.cx
			<< " cy=" << intrinsics.cy;
	}
	// Refcounted handle, not a copy: the depth buffer is 868 KB and this used
	// to be memcpy'd once per box per frame.
	JPbox_kinect2::MonoFrame frame =
		JPbox_kinect2::acquireRawDepth(capture, lastFrameVersion);
	if (!frame) return;

	const int width = JPbox_kinect2::depthWidth();
	const int height = JPbox_kinect2::depthHeight();
	if (frame->size() != (size_t)width * height) return;

	if (!depthTexture.isAllocated() ||
		depthTexture.getWidth() != (float)width ||
		depthTexture.getHeight() != (float)height)
	{
		depthTexture.allocate(width, height, GL_R32F);
		// Never interpolate depth: a filtered sample between a foreground and
		// a background pixel is a distance nothing is actually at, and those
		// in-between points smear the silhouette into the backdrop.
		depthTexture.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
	}
	depthTexture.loadData(frame->data(), width, height, GL_RED);
}

void JPbox_pointercloud::update()
{
	JPbox::update();
	setInletPosition();
	ensureRenderTarget();

	// Keep the range sane and ordered. Both the value and the lerp value have
	// to be written or JPParameter::update stomps it on the next tick.
	const float nearMm = ofClamp(parameters.getFloatValue(NEAR_MM),
		kMinRangeMm, kMaxRangeMm - 1.0f);
	const float farMm = ofClamp(parameters.getFloatValue(FAR_MM),
		nearMm + 1.0f, kMaxRangeMm);
	parameters.setFloatValue(nearMm, NEAR_MM);
	parameters.setFloatLerpValue(nearMm, NEAR_MM);
	parameters.setFloatValue(farMm, FAR_MM);
	parameters.setFloatLerpValue(farMm, FAR_MM);

	uploadDepth();
	updateFBO();
}

void JPbox_pointercloud::updateFBO()
{
	if (tryPassThroughFBO()) return;
	if (!fbo.isAllocated()) return;
	if (!onoff.boolValue)
	{
		JPbox::updateFBO();
		return;
	}

	// JPbox::draw leaves the global rect mode on CENTER, which would put the
	// trails fade rectangle over a quarter of the fbo instead of all of it.
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();
	ofPushStyle();

	const float trails = ofClamp(parameters.getFloatValue(TRAILS), 0.0f, 0.98f);
	if (trails <= 0.0f)
	{
		ofClear(0, 0, 0, 255);
	}
	else
	{
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(0, 0, 0, (1.0f - trails) * 255.0f);
		ofDrawRectangle(0.0f, 0.0f, fbo.getWidth(), fbo.getHeight());
	}
	// Depth always starts clean, trails or not: a stale depth buffer would
	// reject this frame's points against last frame's geometry.
	glClear(GL_DEPTH_BUFFER_BIT);

	const bool ready = shader.isLoaded() && depthTexture.isAllocated() &&
		intrinsics.valid && lattice.getNumVertices() > 0;
	if (ready)
	{
		const ofFbo *colorSource =
			fbohandlergroup.getisPointerSet(COLOR_INLET) ?
			fbohandlergroup.getFboPointerReference(COLOR_INLET) : nullptr;
		const bool hasColor =
			colorSource != nullptr && colorSource->isAllocated();
		const bool additive = parameters.getBoolValue(ADDITIVE);

		ofEnableDepthTest();
		// Additive accumulation is order independent, so let overlapping
		// points sum instead of masking each other out.
		glDepthMask(additive ? GL_FALSE : GL_TRUE);
		ofEnableBlendMode(additive ? OF_BLENDMODE_ADD : OF_BLENDMODE_ALPHA);
		glEnable(GL_PROGRAM_POINT_SIZE);

		const int densityStep = (int)std::round(ofMap(
			ofClamp(parameters.getFloatValue(DENSITY), 0.0f, 1.0f),
			1.0f, 0.0f, (float)kMinDensityStep, (float)kMaxDensityStep));

		shader.begin();
		shader.setUniformTexture("depthTexture", depthTexture, 0);
		shader.setUniform2f("depthSize",
			(float)JPbox_kinect2::depthWidth(),
			(float)JPbox_kinect2::depthHeight());
		shader.setUniform2f("targetSize", fbo.getWidth(), fbo.getHeight());
		shader.setUniform1f("fx", intrinsics.fx);
		shader.setUniform1f("fy", intrinsics.fy);
		shader.setUniform1f("cx", intrinsics.cx);
		shader.setUniform1f("cy", intrinsics.cy);
		shader.setUniform1f("nearMm", parameters.getFloatValue(NEAR_MM));
		shader.setUniform1f("farMm", parameters.getFloatValue(FAR_MM));
		shader.setUniform1i("densityStep", densityStep);
		shader.setUniform1f("pointSize", parameters.getFloatValue(POINT_SIZE));
		shader.setUniform1f("depthScale", parameters.getFloatValue(DEPTH_SCALE));
		shader.setUniform1f("rotateX",
			ofDegToRad(parameters.getFloatValue(ROTATE_X)));
		shader.setUniform1f("rotateY",
			ofDegToRad(parameters.getFloatValue(ROTATE_Y)));
		shader.setUniform1f("rotateZ",
			ofDegToRad(parameters.getFloatValue(ROTATE_Z)));
		shader.setUniform1f("zoom", parameters.getFloatValue(ZOOM));
		shader.setUniform1f("fovScale",
			parameters.getFloatValue(FIELD_OF_VIEW));
		shader.setUniform1i("mirror", parameters.getBoolValue(MIRROR));
		shader.setUniform1i("flipVertical",
			parameters.getBoolValue(FLIP_VERTICAL));
		shader.setUniform1f("colorCycle",
			parameters.getFloatValue(COLOR_CYCLE));
		shader.setUniform1f("brightness",
			parameters.getFloatValue(BRIGHTNESS));
		shader.setUniform1i("hasColor", hasColor ? 1 : 0);
		if (hasColor)
		{
			shader.setUniformTexture("colorTexture",
				colorSource->getTexture(), 1);
		}
		lattice.draw();
		shader.end();

		glDisable(GL_PROGRAM_POINT_SIZE);
		glDepthMask(GL_TRUE);
		ofDisableDepthTest();
	}
	else
	{
		// Without a device there is nothing to draw, so say why rather than
		// leaving a black rectangle the user has to guess about.
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetColor(COL_TEXT_DIM);
		const string message = getCaptureStatus();
		jp_constants::p_font.drawString(message,
			fbo.getWidth() / 2.0f -
				jp_constants::p_font.stringWidth(message) / 2.0f,
			fbo.getHeight() / 2.0f);
	}

	ofPopStyle();
	fbo.end();
}

string JPbox_pointercloud::getCaptureStatus() const
{
	if (!shader.isLoaded()) return "SHADER FAILED";
	const string status = JPbox_kinect2::captureStatus(capture);
	// The stream can be up a frame or two before the intrinsics are read.
	if (status == "STREAMING" && !intrinsics.valid) return "WAITING FOR DEPTH";
	return status;
}

void JPbox_pointercloud::setInletPosition()
{
	if (fbohandlergroup.getSize() > COLOR_INLET)
	{
		fbohandlergroup.setPos(x - width / 2.0f, y, COLOR_INLET);
	}
}

void JPbox_pointercloud::setPos(float x, float y)
{
	JPdragobject::setPos(x, y);
	setInletPosition();
}

void JPbox_pointercloud::draw()
{
	ofSetColor(255);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2.0f - 3.0f, fbowidth, fboheight);
	JPbox::draw_outlet();

	for (int inlet = 0; inlet < fbohandlergroup.getSize(); ++inlet)
	{
		const float inletX = fbohandlergroup.getPosX(inlet);
		const float inletY = fbohandlergroup.getPosY(inlet);
		const bool over = fbohandlergroup.mouseOver(inlet);
		ofColor inletColor = fbohandlergroup.getisPointerSet(inlet) ?
			COL_ACCENT_GREEN : COL_ACCENT_RED;
		if (over) inletColor = inletColor.getLerped(COL_TEXT_PRIMARY, 0.5f);
		ofNoFill();
		ofSetColor(0);
		ofDrawCircle(inletX, inletY, inlet_size * 0.5f);
		ofFill();
		ofSetColor(inletColor);
		ofDrawCircle(inletX, inletY, inlet_size * 0.5f);
		if (over)
		{
			const string label = fbohandlergroup.getName(inlet);
			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p_font.drawString(label,
				inletX - inlet_size * 1.2f -
					jp_constants::p_font.stringWidth(label),
				inletY + inlet_size * 0.25f);
		}
	}
	ofSetColor(COL_TEXT_PRIMARY);
}

void JPbox_pointercloud::clear()
{
	if (cleared) return;
	cleared = true;
	lattice.clear();
	if (depthTexture.isAllocated()) depthTexture.clear();
	shader.unload();
	// Dropping the handle lets the shared device close once the last consumer
	// is gone.
	capture.reset();
	fbohandlergroup.clear();
	JPbox::clear();
}
