#include "jp_uishot.h"

#include "../ofApp.h"
#include "jp_audio.h"
#include "jp_constants.h"
#include "jp_dragobject.h"
#include "../JPgui/jp_complexslider.h"

#include <cstdlib>
#include <fstream>

namespace
{
	// Fixed window. windowResized() recomputes inspectorwindow_x, so the panel
	// only lands in the same place if the window does.
	struct Resolution { int width; int height; };
	const Resolution kResolutions[] = {{1440, 810}, {1600, 1000}};
	// Captured column: the panel is [W-450, W]; 20px of canvas margin makes any
	// overflow past the panel edge visible instead of cropped.
	constexpr int kShotW = 470;
	// Frames to let FBOs allocate and one possible controller rebuild land.
	constexpr int kSettleFrames = 8;

	const char *kFixtureFloats = "shaders/imageprocessing/feedback_advance.frag";
	const char *kFixtureBools = "shaders/imageprocessing/mirrorquad.frag";
	const char *kFixtureScroll = "shaders/imageprocessing/extruder.frag";

	std::string gTag;
	bool gActive = false;
	int gState = 0;
	int gFrames = 0;
	int gResolution = 0;
	bool gDone = false;

	enum Arm
	{
		ARM_COLLAPSED = 0, ARM_RANGE, ARM_AUTOMATED, ARM_BPM, ARM_AUDIO, ARM_AUDIOSHAPE,
		ARM_MIXED, ARM_BOOL, ARM_INPUTS, ARM_INPUTS_COLLAPSED, ARM_LONG_TITLE,
		ARM_SCROLL_TOP, ARM_SCROLL_MIDDLE, ARM_SCROLL_BOTTOM, ARM_COUNT
	};
	struct StateDef { const char *name; const char *fixture; Arm arm; };
	const StateDef kStates[ARM_COUNT] = {
		{"collapsed",        kFixtureFloats, ARM_COLLAPSED},
		{"custom_range",     kFixtureFloats, ARM_RANGE},
		{"automated",        kFixtureFloats, ARM_AUTOMATED},
		{"bpm",              kFixtureFloats, ARM_BPM},
		{"audio",            kFixtureFloats, ARM_AUDIO},
		{"audioshape",       kFixtureFloats, ARM_AUDIOSHAPE},
		{"mixed",            kFixtureFloats, ARM_MIXED},
		{"bool",             kFixtureBools,  ARM_BOOL},
		{"inputs",           kFixtureFloats, ARM_INPUTS},
		{"inputs_collapsed", kFixtureFloats, ARM_INPUTS_COLLAPSED},
		{"long_title",       kFixtureFloats, ARM_LONG_TITLE},
		{"scroll_top",       kFixtureScroll, ARM_SCROLL_TOP},
		{"scroll_middle",    kFixtureScroll, ARM_SCROLL_MIDDLE},
		{"scroll_bottom",    kFixtureScroll, ARM_SCROLL_BOTTOM},
	};

	std::string shotDir()
	{
		return ofToDataPath("uishots/" + gTag + "/", true);
	}

	// Every float pinned, so the shader's ofRandom seeding cannot change the
	// slider fills or the numeric labels between runs. floatLerpValue too, or
	// the value eases toward the target over the settle frames.
	void pinParameters(JPbox *box)
	{
		if (box == nullptr) return;
		for (int i = 0; i < box->parameters.getSize(); i++)
		{
			JPParameter *p = box->parameters.getJParameter(i);
			if (p == nullptr) continue;
			p->movtype = JPParameter::STANDART;
			p->speed = 0.35f;
			p->min = 0.0f;
			p->max = 1.0f;
			p->nativeMin = 0.0f;
			p->nativeMax = 1.0f;
			p->rangeEnabled = false;
			p->floatValue = 0.5f;
			p->floatLerpValue = 0.5f;
			// audioBase is seeded from the shader's ofRandom default, and the
			// AUDIO branch eases floatLerpValue toward it when the analyser is
			// off - so leaving it unpinned made every audio state differ
			// between runs.
			p->audioBase = 0.5f;
			p->audioAmount = 1.0f;
			p->audioInvert = false;
			p->audioThreshold = 0.0f;
			p->audioCurve = 1.0f;
			p->audioAttackMs = 8.0f;
			p->audioReleaseMs = 250.0f;
			p->boolValue = false;
			p->audioShapingOpen = false;
			p->audioSource = jp_audio::SRC_LOW;
			p->audioDiv = jp_audio::DIV_1;
			p->bpmRate = JPParameter::BPM_RATE_ONE;
		}
	}

	void armState(ofApp &app, const StateDef &def)
	{
		app.boxes.clear();
		app.boxes.addBox(def.fixture, 120.0f, 200.0f);
		if (app.boxes.boxes.empty()) return;
		JPbox *box = app.boxes.boxes[0];
		pinParameters(box);

		auto param = [&](int i) -> JPParameter * {
			return i < box->parameters.getSize() ?
				box->parameters.getJParameter(i) : nullptr;
		};
		auto setMode = [&](int i, int mode) {
			if (JPParameter *p = param(i)) p->movtype = mode;
		};

		switch (def.arm)
		{
		case ARM_RANGE:
			if (JPParameter *p = param(0))
			{
				p->min = 0.25f;
				p->max = 0.75f;
				p->rangeEnabled = true;
			}
			break;
		case ARM_AUTOMATED: setMode(0, JPParameter::OSC); break;
		case ARM_BPM:       setMode(0, JPParameter::BPM); break;
		case ARM_AUDIO:     setMode(0, JPParameter::AUDIO); break;
		case ARM_AUDIOSHAPE:
			setMode(0, JPParameter::AUDIO);
			if (JPParameter *p = param(0)) p->audioShapingOpen = true;
			break;
		case ARM_MIXED:
			// The load-bearing state: the only one that shows the gaps between
			// UNLIKE row heights, which is where the height defects live.
			setMode(1, JPParameter::OSC);
			setMode(2, JPParameter::BPM);
			setMode(3, JPParameter::AUDIO);
			setMode(4, JPParameter::AUDIO);
			if (JPParameter *p = param(4)) p->audioShapingOpen = true;
			break;
		case ARM_LONG_TITLE:
			box->name = "an_extremely_long_inspector_title_that_must_truncate_cleanly";
			break;
		case ARM_SCROLL_TOP:
		case ARM_SCROLL_MIDDLE:
		case ARM_SCROLL_BOTTOM:
			// Force overflow at both test resolutions. Plain rows fit inside the
			// 1000px viewport and previously made middle/bottom false coverage.
			for (int i = 0; i < std::min(6, box->parameters.getSize()); ++i)
			{
				setMode(i, JPParameter::AUDIO);
				if (JPParameter *p = param(i)) p->audioShapingOpen = (i % 2 == 0);
			}
			break;
		default: break;
		}

		app.boxes.selectOpenBoxForCurrentView(0);
		if (def.arm == ARM_SCROLL_MIDDLE)
			app.boxes.setInspectorScrollNormalized(0.5f);
		else if (def.arm == ARM_SCROLL_BOTTOM)
			app.boxes.setInspectorScrollNormalized(1.0f);
	}

	void writeSidecar(ofApp &app, const std::string &state)
	{
		std::ofstream out(shotDir() + state + ".txt");
		if (!out) return;
		auto r = [&](const char *key, const ofRectangle &v) {
			out << key << "=" << ofToString(v.x, 1) << "," << ofToString(v.y, 1)
				<< "," << ofToString(v.width, 1) << "," << ofToString(v.height, 1)
				<< "\n";
		};

		out << "window=" << ofGetWidth() << "," << ofGetHeight() << "\n";
		r("panel", app.boxes.getInspectorBounds());
		r("header", app.boxes.getInspectorHeaderBounds());
		r("bodyViewport", app.boxes.getInspectorBodyViewport());
		const JPboxgroup::InspectorLayoutMetrics &metrics =
			app.boxes.getInspectorLayoutMetrics();
		out << "metrics=" << metrics.outerInset << ","
			<< metrics.contentPadding << "," << metrics.columnGap << ","
			<< metrics.rowGap << "," << metrics.minControlHeight << ","
			<< metrics.headerHeight << "\n";
		out << "contentHeight=" << ofToString(
			app.boxes.getInspectorContentHeight(), 1) << "\n";
		out << "scrollY=" << ofToString(app.boxes.getInspectorScrollY(), 1)
			<< "\n";
		out << "scrollMax=" << ofToString(app.boxes.getInspectorMaxScrollY(), 1)
			<< "\n";
		bool noRowIntersections = true;
		bool minimumRowGap = true;
		bool audioChildrenContained = true;
		float previousBottom = -100000.0f;
		float firstTop = 100000.0f;
		float lastBottom = -100000.0f;

		JPbox *box = app.boxes.getInspectorBox();
		if (box != nullptr)
		{
			out << "name=" << box->name << "\n";
			// Step 5 needs these two: the title band is seeded from the 11pt
			// height while the title is drawn at 20pt.
			out << "name.stringHeight.p_font="
				<< ofToString(jp_constants::p_font.stringHeight(box->name), 1) << "\n";
			out << "name.stringHeight.h_font="
				<< ofToString(jp_constants::h_font.stringHeight(box->name), 1) << "\n";
		}

		for (std::size_t i = 0; i < app.boxes.controllers.size(); i++)
		{
			JPcontroller *c = app.boxes.controllers[i];
			if (c == nullptr) continue;
			const std::string k = "control[" + ofToString((int)i) + "]";
			const ofRectangle bounds(c->x - c->width / 2.0f,
				c->y - c->height / 2.0f, c->width, c->height);
			if (c->width > 0.0f && c->height > 0.0f)
			{
				firstTop = std::min(firstTop, bounds.y);
				lastBottom = std::max(lastBottom, bounds.getBottom());
				const float gap = bounds.y - previousBottom;
				if (previousBottom > -99999.0f)
				{
					noRowIntersections = noRowIntersections && gap >= -0.01f;
					minimumRowGap = minimumRowGap && gap >= 5.99f;
				}
				previousBottom = bounds.getBottom();
			}
			r(k.c_str(), bounds);
			if (bounds.intersects(app.boxes.getInspectorBodyViewport()))
				r((k + ".visible").c_str(), bounds.getIntersection(
					app.boxes.getInspectorBodyViewport()));

			JPComplexSlider *sl = dynamic_cast<JPComplexSlider *>(c);
			if (sl == nullptr || sl->parameters == nullptr) continue;
			out << k << ".movtype=" << sl->parameters->movtype << "\n";
			out << k << ".primaryRowY=" << ofToString(sl->primaryRowY, 1) << "\n";
			r((k + ".slider").c_str(), ofRectangle(
				sl->slider_value.x - sl->slider_value.width / 2.0f,
				sl->slider_value.y - sl->slider_value.height / 2.0f,
				sl->slider_value.width, sl->slider_value.height));
			if (sl->parameters->movtype == JPParameter::AUDIO)
			{
				const ofRectangle sourceBounds(
					sl->audio_source_button.x - sl->audio_source_button.width / 2.0f,
					sl->audio_source_button.y - sl->audio_source_button.height / 2.0f,
					sl->audio_source_button.width, sl->audio_source_button.height);
				r((k + ".srcchip").c_str(), sourceBounds);
				audioChildrenContained = audioChildrenContained &&
					bounds.inside(sourceBounds.getTopLeft()) &&
					bounds.inside(sourceBounds.getBottomRight());
				if (sl->parameters->audioShapingOpen)
				{
					const ofRectangle releaseBounds(
						sl->audio_release_button.x - sl->audio_release_button.width / 2.0f,
						sl->audio_release_button.y - sl->audio_release_button.height / 2.0f,
						sl->audio_release_button.width, sl->audio_release_button.height);
					r((k + ".release").c_str(), releaseBounds);
					audioChildrenContained = audioChildrenContained &&
						bounds.inside(releaseBounds.getTopLeft()) &&
						bounds.inside(releaseBounds.getBottomRight());
				}
			}
		}
		out << "assert.noRowIntersections=" << noRowIntersections << "\n";
		out << "assert.minimumRowGap=" << minimumRowGap << "\n";
		out << "assert.audioChildrenContained=" << audioChildrenContained << "\n";
		out << "assert.bodyBelowHeader=" <<
			(app.boxes.getInspectorBodyViewport().y >=
			 app.boxes.getInspectorHeaderBounds().getBottom()) << "\n";
		const bool scrollState = state.find("scroll_") != std::string::npos;
		bool scrollPosition = true;
		if (scrollState)
		{
			const float scroll = app.boxes.getInspectorScrollY();
			const float maximum = app.boxes.getInspectorMaxScrollY();
			scrollPosition = maximum > 0.0f;
			if (state.find("scroll_top") != std::string::npos)
				scrollPosition = scrollPosition && std::abs(scroll) < 0.1f &&
					std::abs(firstTop - (app.boxes.getInspectorBodyViewport().y +
						metrics.contentPadding)) < 0.1f;
			else if (state.find("scroll_middle") != std::string::npos)
				scrollPosition = scrollPosition &&
					std::abs(scroll - maximum * 0.5f) < 0.1f;
			else if (state.find("scroll_bottom") != std::string::npos)
				scrollPosition = scrollPosition &&
					std::abs(scroll - maximum) < 0.1f &&
					std::abs(lastBottom - (app.boxes.getInspectorBodyViewport().getBottom() -
						metrics.contentPadding)) < 0.1f;
		}
		out << "assert.scrollPosition=" << scrollPosition << "\n";
	}
}

bool jp_uishot::active()
{
	return gActive;
}

void jp_uishot::setup(ofApp &app)
{
	const char *tag = std::getenv("GUIPPER_UISHOT");
	if (tag == nullptr) return;
	gActive = true;
	gTag = tag;

	ofSetWindowShape(kResolutions[0].width, kResolutions[0].height);
	ofSetWindowPosition(0, 0);
	ofDirectory::createDirectory(shotDir(), true, true);

	// A live analyser rewrites every audio-driven value each frame, and
	// isRunning() also picks the muted vs live chip palette.
	jp_audio::setEnabled(false);
	jp_constants::setBpm(120.0f);
	jp_constants::syncBeat(0.0f);

	app.pantallaActiva = ofApp::NODOS;
	ofLogNotice("jp_uishot") << "capturing to " << shotDir();
}

void jp_uishot::update(ofApp &app)
{
	if (!gActive || gDone) return;

	// X11 applies the window shape asynchronously, and windowResized()
	// recomputes inspectorwindow_x. Wait for it rather than capturing a panel
	// positioned for the old width.
	const Resolution resolution = kResolutions[gResolution];
	if (ofGetWidth() != resolution.width || ofGetHeight() != resolution.height)
	{
		ofSetWindowShape(resolution.width, resolution.height);
		return;
	}

	app.pantallaActiva = ofApp::NODOS;

	// BPM rows read getBeatPhase(), which is elapsed-time based, so each state
	// was captured at a different point in the beat. Re-origin every frame:
	// phase is then always ~0 and the envelope is the same in every run.
	jp_constants::syncBeat(ofGetElapsedTimef());

	if (gFrames == 0) armState(app, kStates[gState]);
	gFrames++;

	// The panel hit-tests through two paths: JPdragobject::mouseOver() honours
	// this override, but drawInspectorInputRows and the kinect strip read
	// ofGetMouseX() directly. Park the pointer both ways or a hover fill or a
	// tooltip lands in the capture.
	JPdragobject::setMouseOverride(ofVec2f(-10000.0f, -10000.0f));
}

void jp_uishot::draw(ofApp &app)
{
	if (!gActive || gDone) return;
	if (gFrames <= kSettleFrames) return;

	const StateDef &def = kStates[gState];
	ofImage shot;
	shot.grabScreen(ofGetWidth() - kShotW, 0, kShotW, ofGetHeight());
	const std::string prefix = ofToString(ofGetWidth()) + "x" +
		ofToString(ofGetHeight()) + "_" + def.name;
	shot.save(shotDir() + prefix + ".png");
	writeSidecar(app, prefix);
	ofLogNotice("jp_uishot") << "  " << def.name;

	gState++;
	gFrames = 0;
	if (gState >= ARM_COUNT)
	{
		gState = 0;
		gResolution++;
		if (gResolution < 2)
		{
			ofSetWindowShape(kResolutions[gResolution].width,
				kResolutions[gResolution].height);
			return;
		}
		gDone = true;
		JPdragobject::clearMouseOverride();
		ofLogNotice("jp_uishot") << "done: " << ARM_COUNT * 2 << " captures";
		ofExit();
	}
}
