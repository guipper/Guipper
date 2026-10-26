#include "jp_box_pointercloud.h"
#include "../JPutils/jp_app_paths.h"

namespace
{
	// Density maps onto a lattice stride rather than a point count, so the
	// surviving points stay on a regular grid at every setting.
	constexpr int kMinDensityStep = 1;
	constexpr int kMaxDensityStep = 12;
	// Points per depth pixel along each axis. 4 is 16x the vertices: 1.2 M
	// for the phone's 256x192, 3.5 M for the Kinect's 512x424.
	constexpr int kMaxUpsample = 4;

	// Union of both devices' usable ranges: the Kinect resolves 500-4500 and
	// writes 0 outside it, so the wider bounds only matter for the iPhone.
	constexpr float kMinRangeMm = JPbox_iphonelidar::kMinRangeMm;
	constexpr float kMaxRangeMm = JPbox_iphonelidar::kMaxRangeMm;
	// Defaults stay on the Kinect's range, as before the iPhone existed.
	constexpr float kDefaultNearMm = 500.0f;
	constexpr float kDefaultFarMm = 4500.0f;
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
	parameters.addFloatValue(kDefaultNearMm, "near mm");
	parameters.setMin(kMinRangeMm, NEAR_MM);
	parameters.setMax(kMaxRangeMm - 1.0f, NEAR_MM);
	parameters.addFloatValue(kDefaultFarMm, "far mm");
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
	// More points than the sensor has pixels, interpolated along surfaces.
	// Mostly for the iPhone, whose depth is a quarter of the Kinect's.
	parameters.addFloatValue(1.0f, "detail");
	parameters.setMin(1.0f, DETAIL);
	parameters.setMax((float)kMaxUpsample, DETAIL);
	// Same cleanup as the iPhone box, for either device: steadier surfaces
	// at range without trails behind anyone moving.
	parameters.addFloatValue(0.3f, "smooth");
	parameters.setMin(0.0f, SMOOTH);
	parameters.setMax(1.0f, SMOOTH);
	parameters.addBoolValue(true, "fill holes");

	// Inlet names are written straight out as XML element names, so this has
	// to stay a legal NCName.
	fbohandlergroup.addFbohandler("color");
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setInletPosition();

	// From the executable's own resources, as JPbox_kinectparticles does: a
	// migrated development profile keeps its copy of this shader forever, and
	// that copy predates the "detail" upsampling the code now drives.
	const auto shaderPath =
		jp::AppPaths::current().bundle / "shaders/private/pointercloud";
	if (!shader.load(shaderPath.string()))
	{
		ofLogError("JPbox_pointercloud")
			<< "could not load " << shaderPath << " - the box will render "
			   "nothing";
	}

	ensureRenderTarget();
	cleared = false;
	// The device opens on the first update, not here: a saved session sets
	// the device after setup, and opening the Kinect only to close it again
	// would block on its worker thread.
	captureEnabled = true;
}

void JPbox_pointercloud::acquireDevice()
{
	if (cleared) return;
	lastFrameVersion = 0;
	intrinsics = JPbox_kinect2::DepthIntrinsics();
	if (device == DEVICE_IPHONE_LIDAR)
	{
		capture.reset();
		if (!phone) phone = JPbox_iphonelidar::acquireSharedCapture();
	}
	else
	{
		phone.reset();
		if (!capture) capture = JPbox_kinect2::acquireSharedCapture();
	}
}

void JPbox_pointercloud::releaseDevice()
{
	capture.reset();
	phone.reset();
}

void JPbox_pointercloud::buildLattice(int width, int height)
{
	// upsample^2 vertices per depth pixel, each carrying its pixel and its
	// sub-point index. Rebuilt only when the depth size or the detail changes
	// - density and clipping are shader side.
	const int upsample = currentUpsample();
	if (width == latticeWidth && height == latticeHeight &&
		upsample == latticeUpsample && lattice.getNumVertices() > 0)
	{
		return;
	}
	latticeWidth = width;
	latticeHeight = height;
	latticeUpsample = upsample;
	const int subPoints = upsample * upsample;
	lattice.clear();
	lattice.setMode(OF_PRIMITIVE_POINTS);
	lattice.getVertices().reserve((size_t)width * height * subPoints);
	for (int row = 0; row < height; ++row)
	{
		for (int column = 0; column < width; ++column)
		{
			for (int sub = 0; sub < subPoints; ++sub)
			{
				lattice.addVertex(glm::vec3((float)column, (float)row,
					(float)sub));
			}
		}
	}
}

int JPbox_pointercloud::currentUpsample()
{
	return (int)std::round(ofClamp(parameters.getFloatValue(DETAIL), 1.0f,
		(float)kMaxUpsample));
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
	if (device == DEVICE_IPHONE_LIDAR)
	{
		if (!phone) return;
		JPbox_iphonelidar::DepthHandle frame =
			JPbox_iphonelidar::acquireDepth(phone, lastFrameVersion);
		if (!frame) return;
		// Per frame, not per device: the phone's resolution is only known
		// once it streams.
		intrinsics = frame->intrinsics;
		uploadMillimetres(frame->millimetres.data(), frame->width,
			frame->height);
		return;
	}

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
	uploadMillimetres(frame->data(), width, height);
}

bool JPbox_pointercloud::uploadMillimetres(const float *millimetres,
	int width, int height)
{
	if (width <= 0 || height <= 0) return false;
	jp_depth_filter::Settings filter;
	filter.smooth = parameters.getFloatValue(SMOOTH);
	filter.fillHoles = parameters.getBoolValue(FILL_HOLES);
	depthFilter.apply(millimetres, width, height, filter, filteredDepth);
	millimetres = filteredDepth.data();
	buildLattice(width, height);
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
	depthTexture.loadData(millimetres, width, height, GL_RED);
	return true;
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

	if (captureEnabled && !capture && !phone) acquireDevice();
	uploadDepth();
	// The detail slider rebuilds the lattice even while no new frame comes.
	if (latticeWidth > 0 && currentUpsample() != latticeUpsample)
	{
		buildLattice(latticeWidth, latticeHeight);
	}
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
		shader.setUniform2f("depthSize", (float)latticeWidth,
			(float)latticeHeight);
		shader.setUniform2f("targetSize", fbo.getWidth(), fbo.getHeight());
		shader.setUniform1f("fx", intrinsics.fx);
		shader.setUniform1f("fy", intrinsics.fy);
		shader.setUniform1f("cx", intrinsics.cx);
		shader.setUniform1f("cy", intrinsics.cy);
		shader.setUniform1f("nearMm", parameters.getFloatValue(NEAR_MM));
		shader.setUniform1f("farMm", parameters.getFloatValue(FAR_MM));
		shader.setUniform1i("densityStep", densityStep);
		shader.setUniform1i("upsample", latticeUpsample);
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
	const string status = device == DEVICE_IPHONE_LIDAR ?
		JPbox_iphonelidar::captureStatus(phone) :
		JPbox_kinect2::captureStatus(capture);
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
		draw_inlet(inlet);
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

void JPbox_pointercloud::setCaptureActive(bool active)
{
	captureEnabled = active;
	if (!active) releaseDevice();
}

void JPbox_pointercloud::saveCustomState(ofXml &boxNode) const
{
	auto state = boxNode.appendChild("pointercloud");
	state.appendChild("device").set((int)device);
}

void JPbox_pointercloud::loadCustomState(const ofXml &boxNode)
{
	// Sessions saved before the iPhone existed have no node: Kinect.
	auto state = boxNode.getChild("pointercloud");
	if (state)
	{
		setDevice((Device)ofClamp(state.getChild("device").getIntValue(), 0, 1));
	}
}

void JPbox_pointercloud::copyCustomStateFrom(const JPbox *source)
{
	const auto *cloud = dynamic_cast<const JPbox_pointercloud *>(source);
	if (cloud != nullptr) setDevice(cloud->getDevice());
}

vector<string> JPbox_pointercloud::inspectorChoiceLabels() const
{
	return {"KINECT V2", "IPHONE LIDAR"};
}

int JPbox_pointercloud::inspectorChoice() const
{
	return (int)device;
}

void JPbox_pointercloud::setInspectorChoice(int choice)
{
	setDevice((Device)choice);
}

string JPbox_pointercloud::inspectorChoiceStatus() const
{
	return getCaptureStatus();
}

JPbox_pointercloud::Device JPbox_pointercloud::getDevice() const
{
	return device;
}

void JPbox_pointercloud::setDevice(Device value)
{
	value = (Device)ofClamp((int)value, 0, 1);
	if (value == device) return;
	device = value;
	// update() opens the new one, unless capture is paused.
	releaseDevice();
	if (depthTexture.isAllocated()) depthTexture.clear();
	depthFilter.reset();
	intrinsics = JPbox_kinect2::DepthIntrinsics();
	lastFrameVersion = 0;
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
	releaseDevice();
	fbohandlergroup.clear();
	JPbox::clear();
}
