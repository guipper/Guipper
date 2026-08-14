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
	// The layout regression needs one width below the measured two-column
	// minimum and one above it. Keeping this separate avoids changing the
	// established inspector baselines.
	const Resolution kLayoutResolutions[] = {{1024, 768}, {1440, 810}};
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
	float gArmTime = 0.0f;
	int gResolution = 0;
	bool gDone = false;
	bool gLayoutOnly = false;
	int gFirstState = 0;
	int gStateLimit = 0;

	enum Arm
	{
		ARM_COLLAPSED = 0, ARM_RANGE, ARM_AUTOMATED, ARM_BPM, ARM_AUDIO, ARM_AUDIOSHAPE,
		ARM_MIXED, ARM_BOOL, ARM_INPUTS, ARM_INPUTS_COLLAPSED, ARM_LONG_TITLE,
		ARM_MEDIA, ARM_MEDIA_TRANSPORT, ARM_SCROLL_TOP, ARM_SCROLL_MIDDLE, ARM_SCROLL_BOTTOM,
		ARM_WINDOW_SELECTED, ARM_WINDOW_UNSELECTED, ARM_SETTINGS_TOP,
		ARM_SETTINGS_BOTTOM, ARM_SCREEN_HELP, ARM_SCREEN_MIDI, ARM_COUNT
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
		{"media",            "",             ARM_MEDIA},
		{"media_transport",  "",             ARM_MEDIA_TRANSPORT},
		{"scroll_top",       kFixtureScroll, ARM_SCROLL_TOP},
		{"scroll_middle",    kFixtureScroll, ARM_SCROLL_MIDDLE},
		{"scroll_bottom",    kFixtureScroll, ARM_SCROLL_BOTTOM},
		{"window_selected",  "",             ARM_WINDOW_SELECTED},
		{"window_unselected","",             ARM_WINDOW_UNSELECTED},
		{"settings_top",     "",             ARM_SETTINGS_TOP},
		{"settings_bottom",  "",             ARM_SETTINGS_BOTTOM},
		{"screen_help",      "",             ARM_SCREEN_HELP},
		{"screen_midi",      "",             ARM_SCREEN_MIDI},
	};
	constexpr int kLayoutFirstState = ARM_WINDOW_SELECTED;

	bool isWindowLayoutState(Arm arm)
	{
		return arm >= ARM_WINDOW_SELECTED;
	}

	bool isSettingsState(Arm arm)
	{
		return arm == ARM_SETTINGS_TOP || arm == ARM_SETTINGS_BOTTOM;
	}

	const Resolution &currentResolution()
	{
		return gLayoutOnly ? kLayoutResolutions[gResolution] :
			kResolutions[gResolution];
	}

	int resolutionCount()
	{
		return gLayoutOnly ?
			(int)(sizeof(kLayoutResolutions) / sizeof(kLayoutResolutions[0])) :
			(int)(sizeof(kResolutions) / sizeof(kResolutions[0]));
	}

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
		gArmTime = ofGetElapsedTimef();
		app.boxes.clear();
		if (isWindowLayoutState(def.arm))
		{
			// An opaque, non-black fixture makes a stale viewport/scissor visible
			// at a glance and lets the sidecar verify uncovered edge pixels.
			const string path = shotDir() + "window_fixture.frag";
			ofBuffer fixtureShader;
			fixtureShader.set(
				"#version 330\n"
				"out vec4 fragColor;\n"
				"void main(){fragColor=vec4(37.0/255.0,91.0/255.0,"
				"143.0/255.0,1.0);}\n");
			ofBufferToFile(path, fixtureShader);
			app.boxes.addBox(path, 120.0f, 200.0f);
			if (!app.boxes.boxes.empty())
			{
				app.boxes.boxes[0]->setonoff(true);
			}
		}
		else if(def.arm==ARM_MEDIA || def.arm==ARM_MEDIA_TRANSPORT)
		{
			string path;
			if(def.arm==ARM_MEDIA_TRANSPORT && std::getenv("GUIPPER_UISHOT_VIDEO"))
				path=std::getenv("GUIPPER_UISHOT_VIDEO");
			else
			{
				ofPixels pixels;pixels.allocate(96,54,OF_PIXELS_RGBA);
				for(int y=0;y<54;++y)for(int x=0;x<96;++x)
					pixels.setColor(x,y,ofColor(0,175,190,(x+y)%24<12?255:80));
				path=shotDir()+"media_fixture.png";ofSaveImage(pixels,path);
			}
			app.boxes.addBox(path,120.0f,200.0f);
		}
		else app.boxes.addBox(def.fixture, 120.0f, 200.0f);
		if (app.boxes.boxes.empty()) return;
		JPbox *box = app.boxes.boxes[0];
		if(def.arm!=ARM_MEDIA && def.arm!=ARM_MEDIA_TRANSPORT)pinParameters(box);

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

		app.pantallaActiva = def.arm == ARM_SCREEN_HELP ? ofApp::TUTORIAL :
			(def.arm == ARM_SCREEN_MIDI ? ofApp::MIDI_KEYMAP :
				(isSettingsState(def.arm) ? ofApp::OPCIONES : ofApp::NODOS));
		if (def.arm == ARM_WINDOW_UNSELECTED || isSettingsState(def.arm) ||
			def.arm == ARM_SCREEN_HELP || def.arm == ARM_SCREEN_MIDI)
		{
			app.boxes.openguinumber = -1;
		}
		else
		{
			app.boxes.selectOpenBoxForCurrentView(0);
		}
		app.settingsScroll = 0.0f;
		if (def.arm == ARM_SETTINGS_BOTTOM)
		{
			app.settingsScroll = 100000.0f;
			app.clampSettingsScroll();
		}
		if (def.arm == ARM_SCROLL_MIDDLE)
			app.boxes.setInspectorScrollNormalized(0.5f);
		else if (def.arm == ARM_SCROLL_BOTTOM)
			app.boxes.setInspectorScrollNormalized(1.0f);
	}

	void writeSidecar(ofApp &app, const std::string &state,
		const ofImage &shot, bool fullWindow)
	{
		std::ofstream out(shotDir() + state + ".txt");
		if (!out) return;
		auto r = [&](const char *key, const ofRectangle &v) {
			out << key << "=" << ofToString(v.x, 1) << "," << ofToString(v.y, 1)
				<< "," << ofToString(v.width, 1) << "," << ofToString(v.height, 1)
				<< "\n";
		};

		out << "window=" << ofGetWidth() << "," << ofGetHeight() << "\n";
		if (state.find("window_") != std::string::npos)
		{
			GLint viewport[4] = {0, 0, 0, 0};
			glGetIntegerv(GL_VIEWPORT, viewport);
			out << "viewport=" << viewport[0] << "," << viewport[1] << ","
				<< viewport[2] << "," << viewport[3] << "\n";
			out << "scissorEnabled=" << (int)glIsEnabled(GL_SCISSOR_TEST) << "\n";
			out << "assert.fullViewport=" <<
				(viewport[0] == 0 && viewport[1] == 0 &&
				 viewport[2] == ofGetWidth() && viewport[3] == ofGetHeight()) << "\n";
			auto fixtureAt = [&](int x, int y) {
				if (!fullWindow || !shot.isAllocated()) return false;
				const ofColor pixel = shot.getColor(
					ofClamp(x, 0, shot.getWidth() - 1),
					ofClamp(y, 0, shot.getHeight() - 1));
				return std::abs((int)pixel.r - 37) <= 2 &&
					std::abs((int)pixel.g - 91) <= 2 &&
					std::abs((int)pixel.b - 143) <= 2;
			};
			const int width = shot.getWidth();
			const int height = shot.getHeight();
			const bool selected = state.find("window_selected") != std::string::npos;
			const bool reachesVisibleEdges =
				fixtureAt(2, height - 3) &&
				fixtureAt(width / 2, height - 3) &&
				fixtureAt(width / 2, 70) &&
				(selected || fixtureAt(width - 3, height / 2));
			out << "assert.renderReachesVisibleEdges=" << reachesVisibleEdges << "\n";
			return;
		}
		if (state.find("screen_") != std::string::npos)
		{
			GLint viewport[4] = {0, 0, 0, 0};
			glGetIntegerv(GL_VIEWPORT, viewport);
			out << "scissorEnabled=" << (int)glIsEnabled(GL_SCISSOR_TEST) << "\n";
			out << "assert.noClipOrViewportLeak=" <<
				(!glIsEnabled(GL_SCISSOR_TEST) && viewport[0] == 0 &&
				 viewport[1] == 0 && viewport[2] == ofGetWidth() &&
				 viewport[3] == ofGetHeight()) << "\n";
			return;
		}
		if (state.find("settings_") != std::string::npos)
		{
			const ofApp::SettingsLayout settings = app.getSettingsLayout();
			const ofApp::LiveOutputSettingsLayout outputs =
				app.getLiveOutputSettingsLayout();
			r("settings.panel", settings.panel);
			r("outputs.panel", outputs.panel);
			out << "twoColumns=" << outputs.twoColumns << "\n";
			out << "settingsScroll=" << app.settingsScroll << "\n";
			const bool labelClear = settings.fields[0].x >=
				settings.labelX + settings.labelWidth + 13.9f;
			const bool separated = outputs.twoColumns ?
				outputs.panel.x >= settings.panel.getRight() + 15.9f :
				outputs.panel.y >= settings.panel.getBottom() + 15.9f;
			const bool horizontalContainment =
				settings.panel.getRight() <= ofGetWidth() - jp_screen::kMarginX + 0.1f &&
				outputs.panel.getRight() <= ofGetWidth() - jp_screen::kMarginX + 0.1f;
			const ofRectangle settingsControls[] = {
				settings.fields[0], settings.fields[1], settings.fields[2],
				settings.fields[3], settings.fields[4], settings.fields[5],
				settings.fields[6], settings.autoTapButton, settings.browseButton,
				settings.saveButton, settings.audioEnableButton,
				settings.audioDeviceField, settings.audioGainSlider,
				settings.audioDivButton, settings.audioAutoGainButton,
				settings.audioChannelButton, settings.audioCalibrateButton,
				settings.audioGateSlider, settings.audioMeter
			};
			bool controlsContained = true;
			for (const ofRectangle &control : settingsControls)
			{
				if (control.width <= 0.0f || control.height <= 0.0f) continue;
				controlsContained = controlsContained &&
					settings.panel.inside(control.getTopLeft()) &&
					settings.panel.inside(control.getBottomRight());
			}
			const ofRectangle outputControls[] = {
				outputs.list, outputs.addButton, outputs.deleteButton,
				outputs.enabledToggle, outputs.sourceButton, outputs.monitorButton,
				outputs.windowModeButton, outputs.fullscreenModeButton,
				outputs.widthField, outputs.heightField, outputs.tiledToggle,
				outputs.modeToggle, outputs.viewToggle, outputs.patternToggle,
				outputs.splitColsField, outputs.splitRowsField, outputs.splitButton,
				outputs.bezelSignButton, outputs.matchAspectButton,
				outputs.matchResolutionButton, outputs.preview
			};
			for (const ofRectangle &control : outputControls)
			{
				if (control.width <= 0.0f || control.height <= 0.0f) continue;
				controlsContained = controlsContained &&
					outputs.panel.inside(control.getTopLeft()) &&
					outputs.panel.inside(control.getBottomRight());
			}
			for (const ofRectangle &control : outputs.tabs)
				controlsContained = controlsContained &&
					outputs.panel.inside(control.getTopLeft()) &&
					outputs.panel.inside(control.getBottomRight());
			for (const ofRectangle &control : outputs.rows)
				controlsContained = controlsContained &&
					outputs.panel.inside(control.getTopLeft()) &&
					outputs.panel.inside(control.getBottomRight());
			for (int i = 0; i < ofApp::LO_FIELD_COUNT; ++i)
			{
				const ofRectangle &control = outputs.fieldRects[i];
				if (control.width <= 0.0f || control.height <= 0.0f) continue;
				controlsContained = controlsContained &&
					outputs.panel.inside(control.getTopLeft()) &&
					outputs.panel.inside(control.getBottomRight());
			}
			const float maxScroll = std::max(0.0f,
				app.getSettingsContentHeight() - ofGetHeight());
			const bool correctScroll = state.find("settings_bottom") == std::string::npos ?
				std::abs(app.settingsScroll) < 0.1f :
				maxScroll > 0.0f && std::abs(app.settingsScroll - maxScroll) < 0.1f;
			out << "assert.labelsClearFields=" << labelClear << "\n";
			out << "assert.panelsSeparated=" << separated << "\n";
			out << "assert.horizontalContainment=" << horizontalContainment << "\n";
			out << "assert.settingsControlsContained=" << controlsContained << "\n";
			out << "assert.scrollPosition=" << correctScroll << "\n";
			return;
		}
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
	gLayoutOnly = gTag == "layout";
	gFirstState = gLayoutOnly ? kLayoutFirstState : 0;
	gStateLimit = gLayoutOnly ? ARM_COUNT : kLayoutFirstState;
	gState = gFirstState;

	ofSetWindowShape(currentResolution().width, currentResolution().height);
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
	const Resolution resolution = currentResolution();
	if (ofGetWidth() != resolution.width || ofGetHeight() != resolution.height)
	{
		ofSetWindowShape(resolution.width, resolution.height);
		return;
	}

	const Arm arm = kStates[gState].arm;
	app.pantallaActiva = arm == ARM_SCREEN_HELP ? ofApp::TUTORIAL :
		(arm == ARM_SCREEN_MIDI ? ofApp::MIDI_KEYMAP :
			(isSettingsState(arm) ? ofApp::OPCIONES : ofApp::NODOS));

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

void jp_uishot::poisonWindowStateForTest()
{
	if (!gActive || gDone || !isWindowLayoutState(kStates[gState].arm)) return;
	// Reproduce the render-sized state that originally escaped an offscreen
	// pass. This runs after box updates, so the fixture FBO itself stays valid.
	glViewport(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, std::max(1, jp_constants::renderWidth / 2),
		std::max(1, jp_constants::renderHeight / 2));
}

void jp_uishot::draw(ofApp &app)
{
	if (!gActive || gDone) return;

	const StateDef &def = kStates[gState];
	// Async video discovery needs enough time for duration/frame metadata to
	// reach the inspector. Other cards remain fast and deterministic.
	const int settleFrames = def.arm == ARM_MEDIA_TRANSPORT ? 240 : kSettleFrames;
	if (gFrames <= settleFrames) return;
	// The graph transition object can retain its allocated canvas after the
	// harness clears the loaded composition. Wait past its normal transition
	// duration so the edge assertion measures the active FBO, not that fixture
	// teardown artifact.
	if (isWindowLayoutState(def.arm) &&
		ofGetElapsedTimef() - gArmTime < 1.5f) return;
	ofImage shot;
	const bool fullWindow = isWindowLayoutState(def.arm);
	shot.grabScreen(fullWindow ? 0 : ofGetWidth() - kShotW, 0,
		fullWindow ? ofGetWidth() : kShotW, ofGetHeight());
	const std::string prefix = ofToString(ofGetWidth()) + "x" +
		ofToString(ofGetHeight()) + "_" + def.name;
	shot.save(shotDir() + prefix + ".png");
	writeSidecar(app, prefix, shot, fullWindow);
	ofLogNotice("jp_uishot") << "  " << def.name;

	gState++;
	gFrames = 0;
	if (gState >= gStateLimit)
	{
		gState = gFirstState;
		gResolution++;
		if (gResolution < resolutionCount())
		{
			const Resolution next = currentResolution();
			ofSetWindowShape(next.width, next.height);
			return;
		}
		gDone = true;
		JPdragobject::clearMouseOverride();
		ofLogNotice("jp_uishot") << "done: " <<
			(gStateLimit - gFirstState) * resolutionCount() << " captures";
		ofExit();
	}
}
