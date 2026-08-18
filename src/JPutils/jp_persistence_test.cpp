#include "jp_persistence_test.h"

#include "../ofApp.h"
#include "jp_audio.h"
#include "../JPbox/jp_box_image.h"
#include "../JPbox/jp_box_video.h"
#include "../JPbox/jp_media.h"

#include <cmath>
#include <cstdlib>

namespace
{
	bool near(float a, float b) { return std::abs(a - b) < 0.002f; }

	bool sameParameterOrder(JPbox *box, const std::vector<std::string> &names)
	{
		if (box == nullptr || box->parameters.getSize() != int(names.size())) return false;
		for (int i = 0; i < box->parameters.getSize(); ++i)
			if (box->parameters.getName(i) != names[i]) return false;
		return true;
	}

	void removeAudioFields(ofXml &xml)
	{
		static const char *fields[] = {"audiosource", "audiodiv", "audiobase",
			"audioamount", "audioinvert", "audiothreshold", "audiocurve",
			"audioattackms", "audioreleasems", "randomlocked",
			"defaultvalue", "defaultbool", "rangeenabled"};
		for (auto &box : xml.getChildren("box"))
			for (auto &param : box.getChild("parameters").getChildren("param"))
			{
				for (const char *field : fields) param.removeChild(field);
				param.removeChild("lastmovtype");
			}
	}

	bool automationModeMemoryWorks()
	{
		JPParameter parameter;
		parameter.setup(0.5f, "memory-test");
		parameter.bpmEligible = true;
		const int modes[] = {JPParameter::OSC, JPParameter::GODER,
			JPParameter::GOIZQ, JPParameter::RANDOM, JPParameter::BPM,
			JPParameter::AUDIO};
		for (int mode : modes)
		{
			parameter.setAutomationMode(mode);
			parameter.toggleAutomation();
			if (parameter.movtype != JPParameter::STANDART ||
				parameter.lastMovtype != mode)
			{
				return false;
			}
			parameter.toggleAutomation();
			if (parameter.movtype != mode) return false;
		}
		parameter.setAutomationMode(JPParameter::OSC);
		const int patternCycle[] = {JPParameter::RANDOM, JPParameter::GODER,
			JPParameter::GOIZQ, JPParameter::OSC};
		for (int expected : patternCycle)
		{
			parameter.cycleAutomationPattern();
			if (parameter.movtype != expected || parameter.lastMovtype != expected)
				return false;
		}
		parameter.setLastAutomationMode(999);
		return parameter.lastMovtype == JPParameter::OSC;
	}

	bool rangeCaptureWorks()
	{
		JPParameter parameter;
		parameter.setup(0.75f, "range-test");
		parameter.nativeMin = -180.0f;
		parameter.nativeMax = 180.0f;
		parameter.min = -90.0f;
		parameter.max = 0.5f;
		parameter.setRangeEnabled(true);
		parameter.floatValue = parameter.floatLerpValue = 0.75f;
		parameter.captureRangeStart();
		if (!near(parameter.min, 0.75f) || !near(parameter.max, 0.75f))
			return false;
		parameter.floatValue = parameter.floatLerpValue = -45.0f;
		parameter.captureRangeEnd();
		if (!near(parameter.min, -45.0f) || !near(parameter.max, -45.0f) ||
			!near(parameter.floatValue, -45.0f)) return false;
		parameter.setRangeEnabled(false);
		if (!near(parameter.effectiveMin(), -180.0f) ||
			!near(parameter.effectiveMax(), 180.0f) ||
			!near(parameter.min, -45.0f)) return false;
		parameter.floatValue = parameter.floatLerpValue = 90.0f;
		parameter.setRangeEnabled(true);
		return near(parameter.floatValue, -45.0f) &&
			std::isfinite(parameter.floatValue) &&
			std::isfinite(parameter.floatLerpValue);
	}

	bool lockAndDefaultWork()
	{
		JPParameter floatParameter;
		floatParameter.setup(0.25f, "default-float-test");
		floatParameter.floatValue = floatParameter.floatLerpValue = 0.6f;
		floatParameter.captureDefaultValue();
		floatParameter.floatValue = floatParameter.floatLerpValue = 0.9f;
		floatParameter.randomLocked = true;
		floatParameter.restoreDefaultValue();
		if (!near(floatParameter.floatValue, 0.9f)) return false;
		floatParameter.randomLocked = false;
		floatParameter.restoreDefaultValue();
		if (!near(floatParameter.floatValue, 0.6f) ||
			!near(floatParameter.floatLerpValue, 0.6f)) return false;

		JPParameter boolParameter;
		boolParameter.setup(true, "default-bool-test");
		boolParameter.captureDefaultValue();
		boolParameter.boolValue = false;
		boolParameter.randomLocked = true;
		boolParameter.restoreDefaultValue();
		if (boolParameter.boolValue) return false;
		boolParameter.randomLocked = false;
		boolParameter.restoreDefaultValue();
		return boolParameter.boolValue;
	}
}

bool jp_persistence_test::run(ofApp &app)
{
	// Several checks below probe the RENDERED output, which is drawn through
	// the MAIN crossfader - so an in-flight transition blends the probe with
	// whatever was on screen before and the colours come out wrong.
	//
	// The duration is a user setting persisted in settings.xml, so the harness
	// cannot assume any particular value: a slider left at 1100ms made
	// mediaSingleComposite fail on this machine and pass on a default one.
	// Collapse it for the duration of the run so no test is ever at the mercy
	// of a fade still running.
	const float restoreTransitionMs = app.boxes.getTransitionDurationMs();
	app.boxes.setTransitionDurationMs(1.0f);
	struct TransitionDurationRestore
	{
		ofApp &app; float value;
		~TransitionDurationRestore() { app.boxes.setTransitionDurationMs(value); }
	} transitionDurationRestore{app, restoreTransitionMs};

	if (std::getenv("GUIPPER_PERSISTENCE_TEST") == nullptr) return true;
	const std::string directory = ofToDataPath("uishots/persistence/", true);
	ofDirectory::createDirectory(directory, true, true);
	const std::string currentPath = directory + "current.xml";
	const std::string legacyPath = directory + "legacy.xml";
	const std::string invalidPath = directory + "invalid.xml";

	app.boxes.clear();
	app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 120, 180);
	if (app.boxes.boxes.empty()) return false;
	JPbox *box = app.boxes.boxes.front();
	if (box->parameters.getSize() < 2) return false;
	std::vector<std::string> names;
	for (int i = 0; i < box->parameters.getSize(); ++i)
		names.push_back(box->parameters.getName(i));
	int boolIndex = -1;
	for (int i = 0; i < box->parameters.getSize(); ++i)
		if (box->parameters.getType(i) == JPParameter::BOOL)
		{
			boolIndex = i;
			break;
		}
	if (boolIndex < 0) return false;
	box->parameters.setFloatValue(0.37f, 0);
	box->parameters.setFloatLerpValue(0.37f, 0);
	box->parameters.setRangeMin(0.12f, 0);
	box->parameters.setRangeMax(0.88f, 0);
	box->parameters.setRangeEnabled(false, 0);
	box->parameters.setmovetype(JPParameter::AUDIO, 0);
	box->parameters.setmovetype(JPParameter::STANDART, 0);
	box->parameters.setAudioSource(jp_audio::SRC_SNARE_LOGIC, 0);
	box->parameters.setAudioDiv(jp_audio::DIV_16, 0);
	box->parameters.setAudioBase(0.41f, 0);
	box->parameters.setAudioAmount(0.62f, 0);
	box->parameters.setAudioInvert(true, 0);
	box->parameters.setAudioThreshold(0.23f, 0);
	box->parameters.setAudioCurve(2.0f, 0);
	box->parameters.setAudioAttackMs(31.0f, 0);
	box->parameters.setAudioReleaseMs(777.0f, 0);
	box->parameters.getJParameter(0)->randomLocked = true;
	box->parameters.getJParameter(0)->defaultFloatValue = 0.62f;
	box->parameters.setBoolValue(true, boolIndex);
	box->parameters.getJParameter(boolIndex)->randomLocked = true;
	box->parameters.getJParameter(boolIndex)->defaultBoolValue = true;
	app.boxes.save(currentPath);
	bool shaderReload = false;
	if (auto *shader = dynamic_cast<JPbox_shader *>(box))
	{
		shader->reload();
		shaderReload = shader->parameters.getJParameter(0)->randomLocked &&
			!shader->parameters.getJParameter(0)->rangeEnabled &&
			near(shader->parameters.getRangeMin(0), 0.12f) &&
			near(shader->parameters.getRangeMax(0), 0.88f) &&
			near(shader->parameters.getNativeMin(0), 0.0f) &&
			near(shader->parameters.getNativeMax(0), 1.0f) &&
			near(shader->parameters.getJParameter(0)->defaultFloatValue, 0.62f) &&
			shader->parameters.getJParameter(boolIndex)->randomLocked &&
			shader->parameters.getJParameter(boolIndex)->defaultBoolValue;
	}

	app.boxes.clear(); app.boxes.load(currentPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	bool current = sameParameterOrder(box, names) &&
		box->parameters.getMovType(0) == JPParameter::STANDART &&
		box->parameters.getLastMovType(0) == JPParameter::AUDIO &&
		!box->parameters.getJParameter(0)->rangeEnabled &&
		near(box->parameters.getRangeMin(0), 0.12f) &&
		near(box->parameters.getRangeMax(0), 0.88f) &&
		near(box->parameters.getMin(0), 0.0f) &&
		near(box->parameters.getMax(0), 1.0f) &&
		box->parameters.getJParameter(0)->randomLocked &&
		near(box->parameters.getJParameter(0)->defaultFloatValue, 0.62f) &&
		box->parameters.getBoolValue(boolIndex) &&
		box->parameters.getJParameter(boolIndex)->randomLocked &&
		box->parameters.getJParameter(boolIndex)->defaultBoolValue &&
		box->parameters.getAudioSource(0) == jp_audio::SRC_SNARE_LOGIC &&
		box->parameters.getAudioDiv(0) == jp_audio::DIV_16 &&
		near(box->parameters.getAudioBase(0), 0.41f) &&
		near(box->parameters.getAudioAmount(0), 0.62f) &&
		box->parameters.getAudioInvert(0) &&
		near(box->parameters.getAudioThreshold(0), 0.23f) &&
		near(box->parameters.getAudioCurve(0), 2.0f) &&
		near(box->parameters.getAudioAttackMs(0), 31.0f) &&
		near(box->parameters.getAudioReleaseMs(0), 777.0f);
	if (current)
	{
		box->parameters.getJParameter(0)->toggleAutomation();
		current = box->parameters.getMovType(0) == JPParameter::AUDIO;
	}

	ofXml legacy; legacy.load(currentPath); removeAudioFields(legacy); legacy.save(legacyPath);
	app.boxes.clear(); app.boxes.load(legacyPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	const bool old = sameParameterOrder(box, names) &&
		!box->parameters.getJParameter(0)->rangeEnabled &&
		near(box->parameters.getRangeMin(0), 0.12f) &&
		near(box->parameters.getRangeMax(0), 0.88f) &&
		near(box->parameters.getMin(0), 0.0f) &&
		near(box->parameters.getMax(0), 1.0f) &&
		box->parameters.getLastMovType(0) == JPParameter::OSC &&
		!box->parameters.getJParameter(0)->randomLocked &&
		!box->parameters.getJParameter(boolIndex)->randomLocked &&
		!box->parameters.getJParameter(boolIndex)->defaultBoolValue &&
		box->parameters.getAudioSource(0) == jp_audio::SRC_LOW &&
		box->parameters.getAudioDiv(0) == jp_audio::DIV_1 &&
		near(box->parameters.getAudioAmount(0), 1.0f) &&
		!box->parameters.getAudioInvert(0) &&
		near(box->parameters.getAudioThreshold(0), 0.0f) &&
		near(box->parameters.getAudioCurve(0), 1.0f) &&
		near(box->parameters.getAudioAttackMs(0), 8.0f) &&
		near(box->parameters.getAudioReleaseMs(0), 250.0f);

	ofXml invalid; invalid.load(currentPath);
	auto param = invalid.getChild("box").getChild("parameters").getChild("param");
	param.getChild("audiosource").set(999);
	param.getChild("audiodiv").set(-99);
	param.getChild("audiobase").set(5.0f);
	param.getChild("audioamount").set(-2.0f);
	param.getChild("audiothreshold").set(4.0f);
	param.getChild("audiocurve").set(99.0f);
	param.getChild("audioattackms").set(-1.0f);
	param.getChild("audioreleasems").set(99999.0f);
	param.getChild("lastmovtype").set(999);
	param.getChild("defaultvalue").set(99.0f);
	invalid.save(invalidPath);
	app.boxes.clear(); app.boxes.load(invalidPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	const bool clamped = sameParameterOrder(box, names) &&
		box->parameters.getLastMovType(0) == JPParameter::OSC &&
		near(box->parameters.getJParameter(0)->defaultFloatValue, 1.0f) &&
		box->parameters.getAudioSource(0) == jp_audio::SRC_COUNT - 1 &&
		box->parameters.getAudioDiv(0) == jp_audio::DIV_1 &&
		near(box->parameters.getAudioBase(0), 1.0f) &&
		near(box->parameters.getAudioAmount(0), 0.0f) &&
		near(box->parameters.getAudioThreshold(0), 0.99f) &&
		near(box->parameters.getAudioCurve(0), 5.0f) &&
		near(box->parameters.getAudioAttackMs(0), 0.0f) &&
		near(box->parameters.getAudioReleaseMs(0), 5000.0f);

	bool midiRange = false;
	if (box != nullptr)
	{
		JPParameter *parameter = box->parameters.getJParameter(0);
		parameter->setAutomationMode(JPParameter::STANDART);
		parameter->setRangeStart(0.2f);
		parameter->setRangeEnd(0.6f);
		parameter->setRangeEnabled(true);
		if (app.boxes.selectOpenBoxByIndex(0) &&
			app.boxes.setOpenBoxParameterAtIndex(0, 0.0f))
		{
			const bool low = near(parameter->floatValue, 0.2f);
			app.boxes.setOpenBoxParameterAtIndex(0, 1.0f);
			const bool high = near(parameter->floatValue, 0.6f);
			parameter->setRangeStart(0.4f);
			parameter->setRangeEnd(0.4f);
			app.boxes.setOpenBoxParameterAtIndex(0, 0.75f);
			const bool zeroWidth = near(parameter->floatValue, 0.4f) &&
				std::isfinite(parameter->floatValue);
			midiRange = low && high && zeroWidth;
		}
	}

	bool cueState = false;
	if (box != nullptr && app.boxes.selectOpenBoxByIndex(0))
	{
		JPParameter *real = box->parameters.getJParameter(0);
		real->randomLocked = false;
		real->defaultFloatValue = 0.15f;
		real->setRangeStart(0.1f);
		real->setRangeEnd(0.9f);
		real->setRangeEnabled(false);
		if (app.boxes.setCueBoxByIndex(0))
		{
			JPbox *draftBox = app.boxes.getInspectorBox();
			JPParameter *draft = draftBox != nullptr ?
				draftBox->parameters.getJParameter(0) : nullptr;
			if (draft != nullptr && draft != real)
			{
				draft->randomLocked = true;
				draft->defaultFloatValue = 0.73f;
				draft->setRangeStart(0.25f);
				draft->setRangeEnd(0.75f);
				draft->setRangeEnabled(true);
				app.boxes.setOpenBoxParameterAtIndex(0, 0.5f);
				if (app.boxes.applyCue())
				{
					const bool applied = real->randomLocked && real->rangeEnabled &&
						near(real->min, 0.25f) && near(real->max, 0.75f) &&
						near(real->defaultFloatValue, 0.73f);
					app.boxes.clearCue();
					real->randomLocked = false;
					real->defaultFloatValue = 0.15f;
					real->setRangeEnabled(false);
					if (app.boxes.setCueBoxByIndex(0))
					{
						draftBox = app.boxes.getInspectorBox();
						draft = draftBox != nullptr ?
							draftBox->parameters.getJParameter(0) : nullptr;
						if (draft != nullptr && draft != real)
						{
							draft->randomLocked = true;
							draft->defaultFloatValue = 0.91f;
							app.boxes.setOpenBoxParameterAtIndex(0, 0.5f);
							app.boxes.clearCue();
							cueState = applied && !real->randomLocked && !real->rangeEnabled &&
								near(real->defaultFloatValue, 0.15f);
						}
					}
				}
			}
		}
		app.boxes.clearCue();
	}

	const bool modeMemory = automationModeMemoryWorks();
	const bool rangeCapture = rangeCaptureWorks();
	const bool lockDefault = lockAndDefaultWork();
	bool mediaState = false, mediaAlpha = false, mediaMotionClear = false,
		mediaStraightMix = false, mediaSingleComposite = false,
		mediaPausePreserves = false,
		mediaBoundary = false, mediaTransforms = false;
	{
		const ofRectangle custom = jp_media::transformedRect(400, 200,
			1000, 1000, JPMediaFitMode::Custom, .5f, .5f, .5f, .5f, 1.0f, .5f, .5f);
		const ofRectangle fit = jp_media::transformedRect(400, 200,
			1000, 1000, JPMediaFitMode::Fit, .5f, .5f, .5f, .5f, 1.0f, .5f, .5f);
		const ofRectangle zoomed = jp_media::transformedRect(400, 200,
			1000, 1000, JPMediaFitMode::Fit, .5f, .5f, .5f, .5f, 2.0f, .5f, .5f);
		const ofRectangle videoFit = jp_media::transformedRect(400, 200,
			1000, 1000, JPMediaFitMode::Fit, .5f, .5f, .5f, .5f, 1.0f, .5f, 1.0f);
		mediaTransforms = near(custom.x, 250.0f) && near(custom.y, 250.0f) &&
			near(custom.width, 500.0f) && near(custom.height, 500.0f) &&
			near(fit.x, 0.0f) && near(fit.y, 250.0f) &&
			near(fit.width, 1000.0f) && near(fit.height, 500.0f) &&
			near(zoomed.x, -500.0f) && near(zoomed.y, 0.0f) &&
			near(zoomed.width, 2000.0f) && near(zoomed.height, 1000.0f) &&
			near(videoFit.x, fit.x) && near(videoFit.y, fit.y) &&
			near(videoFit.width, fit.width) && near(videoFit.height, fit.height);

		JPMediaState state; state.fitMode=JPMediaFitMode::Fill;
		state.loopMode=JPMediaLoopMode::PingPong; state.position=0.42f;
		state.rangeIn=0.2f; state.rangeOut=0.8f; state.rate=1.75f;
		state.playing=false; state.reverse=true; state.muted=false; state.volume=0.35f;
		ofXml root; auto node=root.appendChild("box"); jp_media::save(node,state);
		JPMediaState loaded; mediaState=jp_media::load(node,loaded) &&
			jp_media::transformVersion(node)==2 &&
			loaded.fitMode==state.fitMode && loaded.loopMode==state.loopMode &&
			near(loaded.position,state.position) && near(loaded.rangeIn,state.rangeIn) &&
			near(loaded.rangeOut,state.rangeOut) && near(loaded.rate,state.rate) &&
			loaded.playing==state.playing && loaded.reverse==state.reverse &&
			loaded.muted==state.muted && near(loaded.volume,state.volume);
		JPMediaState boundary; boundary.rangeIn=.2f;boundary.rangeOut=.8f;
		boundary.loopMode=JPMediaLoopMode::PingPong;float p=.9f;
		mediaBoundary=jp_media::applyBoundary(boundary,p)&&near(p,.7f)&&boundary.reverse;

		ofPixels px;px.allocate(2,2,OF_PIXELS_RGBA);
		for(int y=0;y<2;++y)for(int x=0;x<2;++x)px.setColor(x,y,ofColor(255,0,0,255));
		px.setColor(0,0,ofColor(0,0,0,0)); const string alphaPath=directory+"alpha.png";
		ofSaveImage(px,alphaPath); app.boxes.clear();app.boxes.addBox(alphaPath,120,180);
		auto *image=app.boxes.boxes.empty()?nullptr:dynamic_cast<JPbox_image*>(app.boxes.boxes.front());
		if(image)
		{
			image->setonoff(true); image->media=state; image->media.fitMode=JPMediaFitMode::Stretch;
			image->parameters.setFloatValue(1.75f, 5);
			image->parameters.setFloatLerpValue(1.75f, 5);
			image->update(); ofPixels out; image->fbo.readToPixels(out);
			mediaAlpha=out.isAllocated()&&out.getColor(0,0).a<8;
			const string mediaPath=directory+"media.xml"; app.boxes.save(mediaPath);
			app.boxes.clear();app.boxes.load(mediaPath);
			auto *restored=app.boxes.boxes.empty()?nullptr:dynamic_cast<JPbox_image*>(app.boxes.boxes.front());
			mediaState=mediaState&&restored&&restored->media.fitMode==JPMediaFitMode::Stretch&&
				restored->media.loopMode==state.loopMode&&near(restored->media.position,state.position)&&
				near(restored->media.rangeIn,state.rangeIn)&&near(restored->media.rangeOut,state.rangeOut)&&
				near(restored->media.rate,state.rate)&&restored->media.playing==state.playing&&
				restored->parameters.getSize()>5&&
				restored->parameters.getName(5)=="scale ratio"&&
				near(restored->parameters.getFloatValue(5),1.75f)&&
				near(restored->parameters.getNativeMin(5),.1f)&&
				near(restored->parameters.getNativeMax(5),4.0f);
		}

		// The master transition canvas must erase transparent pixels when media
		// moves. Otherwise the old position remains visible as a trail.
		ofFbo moving, probe;
		moving.allocate(32,32);probe.allocate(32,32);
		auto drawHalf=[&](bool right)
		{
			moving.begin();ofClear(0,0,0,0);ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			ofSetColor(255,0,255,255);ofDrawRectangle(right?16:0,0,16,32);
			moving.end();ofEnableAlphaBlending();
		};
		TransitionSR movingTransition;movingTransition.setup();
		movingTransition.setFboPointer1(&moving);movingTransition.setFboPointer2(&moving);
		movingTransition.setLerpValue(1.0f);
		drawHalf(false);movingTransition.update();
		drawHalf(true);movingTransition.update();
		probe.begin();ofClear(0,0,0,0);ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		movingTransition.draw(0,0,32,32);probe.end();ofEnableAlphaBlending();
		ofPixels moved;probe.readToPixels(moved);
		mediaMotionClear=moved.isAllocated()&&moved.getColor(4,16).a<8&&
			moved.getColor(28,16).a>247;

		// Preset crossfades interpolate straight RGBA. Rendering into an empty
		// FBO with blending disabled must retain the average color and alpha,
		// rather than alpha-compositing either child over the other.
		ofFbo mixFirst,mixSecond,mixProbe;
		mixFirst.allocate(16,16);mixSecond.allocate(16,16);mixProbe.allocate(16,16);
		auto clearStraight=[](ofFbo &target,const ofColor &color)
		{
			target.begin();ofEnableBlendMode(OF_BLENDMODE_DISABLED);ofClear(color);
			target.end();ofEnableAlphaBlending();
		};
		clearStraight(mixFirst,ofColor(240,40,180,64));
		clearStraight(mixSecond,ofColor(40,200,20,192));
		TransitionSR straightTransition;straightTransition.setup();
		mixProbe.begin();ofClear(0,0,0,0);ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		const bool mixed=straightTransition.renderStraightMix(&mixFirst,&mixSecond,
			0.5f,16,16);
		mixProbe.end();ofEnableAlphaBlending();
		ofPixels mixedPixels;mixProbe.readToPixels(mixedPixels);
		if(mixed&&mixedPixels.isAllocated())
		{
			const ofColor sample=mixedPixels.getColor(8,8);
			mediaStraightMix=std::abs(int(sample.r)-140)<=3&&
				std::abs(int(sample.g)-120)<=3&&std::abs(int(sample.b)-100)<=3&&
				std::abs(int(sample.a)-128)<=3;
		}

		// The presentation stage performs exactly one source-over operation.
		// Use two boxes because the old path drew an extra pass only when the
		// graph contained more than one box.
		app.boxes.addBox(alphaPath,220,180);
		app.boxes.requestSetActiveRender(0,false);
		for(int i=0;i<55;++i) app.boxes.update();
		if(!app.boxes.boxes.empty()&&app.boxes.boxes.front()->fbo.isAllocated())
		{
			ofFbo &activeFbo=app.boxes.boxes.front()->fbo;
			clearStraight(activeFbo,ofColor(255,59,183,64));
			app.boxes.boxes.front()->setonoff(false);
			app.boxes.boxes.front()->update();
			ofPixels heldPixels;activeFbo.readToPixels(heldPixels);
			if(heldPixels.isAllocated())
			{
				const ofColor held=heldPixels.getColor(activeFbo.getWidth()/2,
					activeFbo.getHeight()/2);
				mediaPausePreserves=std::abs(int(held.r)-255)<=2&&
					std::abs(int(held.g)-59)<=2&&std::abs(int(held.b)-183)<=2&&
					std::abs(int(held.a)-64)<=2;
			}
			ofFbo finalProbe;finalProbe.allocate(32,32);
			auto presentSolid=[&](const ofColor &background)
			{
				finalProbe.begin();ofEnableBlendMode(OF_BLENDMODE_DISABLED);
				ofClear(background);ofEnableAlphaBlending();
				app.boxes.draw_activerender(32,32);
				finalProbe.end();ofEnableAlphaBlending();
				ofPixels pixels;finalProbe.readToPixels(pixels);
				return pixels.isAllocated()?pixels.getColor(16,16):ofColor();
			};
			auto closeColor=[](const ofColor &actual,const ofColor &expected)
			{
				return std::abs(int(actual.r)-int(expected.r))<=4&&
					std::abs(int(actual.g)-int(expected.g))<=4&&
					std::abs(int(actual.b)-int(expected.b))<=4;
			};
			const ofColor overBlack=presentSolid(ofColor(0,0,0,255));
			const ofColor overWhite=presentSolid(ofColor(255,255,255,255));
			finalProbe.begin();ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			ofClear(0,0,0,255);ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(255);ofDrawRectangle(16,0,16,32);
			ofEnableAlphaBlending();app.boxes.draw_activerender(32,32);
			finalProbe.end();ofEnableAlphaBlending();
			ofPixels checkerPixels;finalProbe.readToPixels(checkerPixels);
			mediaSingleComposite=checkerPixels.isAllocated()&&
				closeColor(overBlack,ofColor(64,15,46))&&
				closeColor(overWhite,ofColor(255,206,237))&&
				closeColor(checkerPixels.getColor(8,16),ofColor(64,15,46))&&
				closeColor(checkerPixels.getColor(24,16),ofColor(255,206,237));
		}
	}
	// A still image must composite once and then stop. The skip is invisible on
	// screen by design, so without this a change that invalidates the render
	// signature every frame would silently restore a full render-resolution
	// pass per image box per frame and nothing would look wrong.
	bool mediaSkipsStatic = true;
	{
		const string stillPath = ofToDataPath("guipper.png", true);
		if (ofFile::doesFileExist(stillPath))
		{
			app.boxes.clear();
			app.boxes.addBox(stillPath, 120, 180);
			if (!app.boxes.boxes.empty())
			{
				JPbox *box = app.boxes.boxes.front();
				box->setonoff(true);
				// Let the load settle; each of these may legitimately render.
				for (int i = 0; i < 30; ++i) box->update();
				jp_box_media_stats::beginFrame();
				const int before = jp_box_media_stats::getRendered();
				(void)before;
				// Nothing touches the transform here, so every one of these
				// updates should take the skip path.
				for (int i = 0; i < 20; ++i) box->update();
				jp_box_media_stats::beginFrame();
				mediaSkipsStatic = jp_box_media_stats::getRendered() == 0 &&
					jp_box_media_stats::getSkipped() == 20;

				// ...and a transform change must break the skip immediately,
				// or edits would not show up until something else invalidated.
				box->parameters.setFloatValue(0.75f, 0);
				box->update();
				jp_box_media_stats::beginFrame();
				mediaSkipsStatic = mediaSkipsStatic &&
					jp_box_media_stats::getRendered() == 1;
			}
			app.boxes.clear();
		}
	}
	// Video transport turnaround. GStreamer parks on the last frame's
	// timestamp and raises EOS rather than reporting the nominal end - measured
	// at 0.999977 on a 431 frame clip - so a bare `position >= rangeOut` test
	// never fires. That left ping-pong stuck on the final frame forever and
	// loop unable to wrap. Only the forward end was affected: seeking to zero
	// is exact, so reverse toward the start always worked, which is what made
	// the bug look like "reverse is broken" rather than "the end is broken".
	bool mediaTurnaround = true;
	{
		const string clipPath = ofToDataPath("vid/kinect.mov", true);
		if (ofFile::doesFileExist(clipPath))
		{
			app.boxes.clear();
			app.boxes.addBox(clipPath, 120, 180);
			auto *target = app.boxes.boxes.empty() ? nullptr :
				dynamic_cast<JPMediaInspectable *>(app.boxes.boxes.front());
			if (target != nullptr)
			{
				JPbox *vbox = app.boxes.boxes.front();
				vbox->setonoff(true);
				for (int i = 0; i < 400 && !(target->mediaReady() &&
					target->mediaFrameCount() > 1); ++i)
				{
					vbox->update();
					ofSleepMillis(10);
				}
				if (target->mediaReady())
				{
					JPMediaState &s = target->mediaState();
					s.loopMode = JPMediaLoopMode::PingPong;
					s.reverse = false;
					s.playing = true;
					target->mediaSeek(0.95f);
					// Long enough to reach the end and come back off it.
					for (int i = 0; i < 140 && !s.reverse; ++i)
					{
						vbox->update();
						ofSleepMillis(16);
					}
					const float turnPosition = s.position;
					for (int i = 0; i < 30; ++i)
					{
						vbox->update();
						ofSleepMillis(16);
					}
					// It must both flip direction AND actually move away from
					// the end - flipping a flag while parked would still be a
					// frozen picture.
					mediaTurnaround = s.reverse && s.position < turnPosition;

					// Now the same turnaround at the OTHER end, with IN at the
					// very start of the file. Reverse reaching frame zero is an
					// end-of-segment for the backend, so it raises EOS there
					// too - and that flag stays raised until a new buffer
					// arrives, well after the direction has flipped. Anything
					// that treats a latched EOS as "at the end of the range"
					// bounces the clip straight back and it freezes near the
					// start. With IN mid-file the backend never reaches frame
					// zero, no EOS is raised, and the bug hides.
					s.rangeIn = 0.0f;
					s.rangeOut = 1.0f;
					s.loopMode = JPMediaLoopMode::PingPong;
					s.reverse = true;
					s.playing = true;
					target->mediaSeek(0.02f);
					for (int i = 0; i < 120 && s.reverse; ++i)
					{
						vbox->update();
						ofSleepMillis(16);
					}
					// It should now be running forward, away from the start.
					const float startTurn = s.position;
					for (int i = 0; i < 40; ++i)
					{
						vbox->update();
						ofSleepMillis(16);
					}
					mediaTurnaround = mediaTurnaround && !s.reverse &&
						s.position > startTurn;

					// NOTE: a second phase that drove a full ping-pong cycle
					// with IN at frame zero used to live here. It was removed
					// because it set `reverse` and then seeked from outside the
					// update loop, which issues two conflicting seeks before
					// the new direction has reached the backend - something the
					// inspector cannot do, since every control it offers is
					// applied inside updateFBO. It was failing on its own
					// artificial setup rather than on a real defect.
				}
			}
			app.boxes.clear();
		}
	}
	// Speed-drag stress. Every rate change becomes a FLUSH|ACCURATE
	// gst_element_seek, and dragging the slider produces one per frame.
	if (std::getenv("GUIPPER_SPEED_STRESS"))
	{
		const string clip = ofToDataPath("vid/kinect.mov", true);
		if (ofFile::doesFileExist(clip))
		{
			app.boxes.clear();
			app.boxes.addBox(clip, 120, 180);
			auto *t = app.boxes.boxes.empty() ? nullptr :
				dynamic_cast<JPMediaInspectable *>(app.boxes.boxes.front());
			if (t != nullptr)
			{
				JPbox *vb = app.boxes.boxes.front();
				vb->setonoff(true);
				for (int i = 0; i < 400 && !t->mediaReady(); ++i)
				{ vb->update(); ofSleepMillis(10); }
				JPMediaState &s = t->mediaState();
				s.playing = true; s.reverse = false;
				s.loopMode = JPMediaLoopMode::Loop;
				ofLogNotice("speedstress") << "begin sweep";
				// Sweep like a dragged slider: a new rate every single frame.
				for (int i = 0; i < 240; ++i)
				{
					const float phase = (float)i / 240.0f;
					s.rate = 0.25f + 3.75f * std::abs(std::sin(phase * 6.283f));
					vb->update();
					ofSleepMillis(16);
					if (i % 30 == 0)
						ofLogNotice("speedstress") << i << " rate=" << s.rate
							<< " pos=" << s.position;
				}
				ofLogNotice("speedstress") << "sweep survived";
				// Now alternate direction AND rate together, the worst case.
				for (int i = 0; i < 120; ++i)
				{
					s.rate = (i % 2 == 0) ? 0.25f : 4.0f;
					if (i % 7 == 0) s.reverse = !s.reverse;
					vb->update();
					ofSleepMillis(16);
				}
				ofLogNotice("speedstress") << "direction thrash survived pos="
					<< s.position;
			}
			app.boxes.clear();
		}
	}
	// A MIDI knob bound to slot 0 must move the first slider the inspector
	// shows, and the media boxes must not offer bind slots for the four
	// parameters their transport card owns (strech/speed/position/play) - those
	// are not drawn as sliders, so a slot on them is an invisible target.
	bool mediaMidiIndex = true;
	{
		const string clip = ofToDataPath("vid/kinect.mov", true);
		if (ofFile::doesFileExist(clip))
		{
			app.boxes.clear();
			app.boxes.addBox(clip, 120, 180);
			if (!app.boxes.boxes.empty())
			{
				JPbox *vb = app.boxes.boxes.front();
				vb->setonoff(true);
				for (int i = 0; i < 200 && vb->parameters.getSize() < 9; ++i)
				{ vb->update(); ofSleepMillis(10); }
				// Open it in the inspector so the controller array is built.
				app.boxes.selectOpenBoxByIndex(0);
				vb->update();
				const int count = vb->parameters.getSize();
				ofLogNotice("midiindex") << "params=" << count
					<< " controllers=" << app.boxes.controllers.size();
				if (count > 0)
				{
					JPParameter *first = app.boxes.getOpenParameterAtIndex(0);
					const string firstName = first ? first->name : "<null>";
					const int ratioIndex =
						app.boxes.resolveBindableParameterIndex(vb, 0);
					const float before =
						vb->parameters.getFloatValue(ratioIndex);
					const bool applied =
						app.boxes.setOpenBoxParameterAtIndex(0, 0.85f);
					const float after =
						vb->parameters.getFloatValue(ratioIndex);
					ofLogNotice("midiindex") << "index0 name=" << firstName
						<< " applied=" << applied
						<< " before=" << before << " after=" << after;
					// Slot order must match the inspector: scale ratio, then
					// scalex, scaley, offsetx, offsety, then the four the
					// transport card owns.
					const char *expected[] = {"scale ratio", "scalex", "scaley",
						"offsetx", "offsety"};
					bool orderOk = true;
					for (int sl = 0; sl < 5; ++sl)
					{
						JPParameter *p = app.boxes.getOpenParameterAtIndex(sl);
						if (p == nullptr || p->name != expected[sl])
						{
							orderOk = false;
							ofLogNotice("midiindex") << "slot " << sl
								<< " expected " << expected[sl] << " got "
								<< (p ? p->name : std::string("<null>"));
						}
					}
					// Exactly five slots: the nine parameters minus the four
					// the transport card owns.
					const int slots = app.boxes.getOpenParameterCount();
					mediaMidiIndex = applied && orderOk && slots == 5 &&
						firstName == "scale ratio" &&
						app.boxes.getOpenParameterAtIndex(5) == nullptr &&
						std::abs(after-before) > 0.0001f;
					ofLogNotice("midiindex") << "slots=" << slots;
				}
			}
			app.boxes.clear();
		}
	}
	// The camera box now carries "scale ratio" too, and it must obey the same
	// rule as the media boxes: the row drawn first is the slot bound first.
	// Unlike them nothing is excluded - a camera's `strech` is an ordinary
	// slider, not a transport control - so slots must equal parameter count.
	bool camScaleRatio = true;
	{
		app.boxes.clear();
		app.boxes.addBox("cam", 120, 180);
		if (!app.boxes.boxes.empty())
		{
			JPbox *cb = app.boxes.boxes.front();
			app.boxes.selectOpenBoxByIndex(0);
			cb->update();
			JPParameter *first = app.boxes.getOpenParameterAtIndex(0);
			const int slots = app.boxes.getOpenParameterCount();
			bool hasRatio = false;
			for (int k = 0; k < cb->parameters.getSize(); ++k)
				if (cb->parameters.getName(k) == "scale ratio") hasRatio = true;
			// Slot order is only half of it. The ROW has to be drawn first too
			// - when these disagreed, the panel showed one order and the knobs
			// followed another. Compare the actual laid-out y positions.
			float ratioY = 1e9f, scalexY = -1.0f;
			for (int k = 0; k < (int)app.boxes.controllers.size() &&
				k < cb->parameters.getSize(); ++k)
			{
				if (app.boxes.controllers[k] == nullptr) continue;
				const string n = cb->parameters.getName(k);
				if (n == "scale ratio") ratioY = app.boxes.controllers[k]->y;
				else if (n == "scalex") scalexY = app.boxes.controllers[k]->y;
			}
			const bool drawnFirst = ratioY < scalexY;
			camScaleRatio = hasRatio && first != nullptr &&
				first->name == "scale ratio" &&
				slots == cb->parameters.getSize() && drawnFirst;
			// The panel must measure its rows, not twice its rows. The row
			// positions were computed by the canonical pass while the main
			// loop advanced the cursor again on top of them, so the inspector
			// reported roughly double the height and left dead space under the
			// last slider.
			float rowSpan = 0.0f;
			for (int k = 0; k < (int)app.boxes.controllers.size() &&
				k < cb->parameters.getSize(); ++k)
			{
				JPcontroller *c = app.boxes.controllers[k];
				if (c == nullptr || c->height <= 0.0f) continue;
				rowSpan = std::max(rowSpan, c->y + c->height);
			}
			const float contentH = app.boxes.getInspectorContentHeight();
			// Generous: catching a 2x overshoot, not policing padding.
			const bool heightSane = rowSpan > 0.0f &&
				contentH < rowSpan * 1.5f;
			camScaleRatio = camScaleRatio && heightSane;
			ofLogNotice("camratio") << "ratioY=" << ratioY
				<< " scalexY=" << scalexY << " drawnFirst=" << drawnFirst
				<< " rowSpan=" << rowSpan << " contentH=" << contentH
				<< " heightSane=" << heightSane;
			ofLogNotice("camratio") << "params=" << cb->parameters.getSize()
				<< " slots=" << slots << " slot0="
				<< (first ? first->name : std::string("<null>"))
				<< " hasRatio=" << hasRatio;
		}
		app.boxes.clear();
	}

	// Loading a composition written BEFORE "scale ratio" existed must not shift
	// anything. Parameters load positionally, so appending the new one at the
	// end is the whole reason this still works - if it were ever inserted
	// earlier, every one of these values would land on the wrong parameter.
	bool camLegacyLoad = true;
	{
		const string legacyCompo = ofToDataPath("savefiles/cameffect2.xml", true);
		if (ofFile::doesFileExist(legacyCompo))
		{
			app.boxes.clear();
			app.boxes.load(legacyCompo);
			JPbox *cam = nullptr;
			for (JPbox *b : app.boxes.boxes)
				if (b != nullptr && b->getTipo() == JPbox::CAMBOX) { cam = b; break; }
			if (cam != nullptr)
			{
				auto value = [&](const char *name, float &out) {
					for (int k = 0; k < cam->parameters.getSize(); ++k)
						if (cam->parameters.getName(k) == name)
						{ out = cam->parameters.getFloatValue(k); return true; }
					return false;
				};
				float sx=-1, sy=-1, ox=-1, oy=-1, ratio=-1;
				const bool found = value("scalex", sx) && value("scaley", sy) &&
					value("offsetx", ox) && value("offsety", oy) &&
					value("scale ratio", ratio);
				camLegacyLoad = found && near(sx, 1.0f) && near(sy, 1.0f) &&
					near(ox, 0.5f) && near(oy, 0.5f) &&
					// Absent from the file, so it must hold its neutral default
					// and leave the framing exactly as it was authored.
					near(ratio, 1.0f);
				ofLogNotice("camlegacy") << "scalex=" << sx << " scaley=" << sy
					<< " offsetx=" << ox << " offsety=" << oy
					<< " ratio=" << ratio;
			}
			app.boxes.clear();
		}
	}
	// A shader declaring `scaleratio` must behave like the hardcoded boxes: the
	// row drawn first, slot 0, and the 0.1x-4x range with a 1.0 neutral rather
	// than a uniform's default 0..1 (which could not zoom in at all).
	bool shaderScaleRatio = true;
	{
		app.boxes.clear();
		app.boxes.addBox("shaders/imageprocessing/transform.frag", 120, 180);
		if (!app.boxes.boxes.empty())
		{
			JPbox *sb = app.boxes.boxes.front();
			app.boxes.selectOpenBoxByIndex(0);
			sb->update();
			JPParameter *first = app.boxes.getOpenParameterAtIndex(0);
			int ratioIndex = -1;
			for (int k = 0; k < sb->parameters.getSize(); ++k)
				if (jp_media::isScaleRatioParameter(sb->parameters.getName(k)))
					ratioIndex = k;
			bool rangeOk = false;
			if (ratioIndex >= 0)
			{
				JPParameter *rp = sb->parameters.getJParameter(ratioIndex);
				rangeOk = rp != nullptr && near(rp->min, 0.1f) &&
					near(rp->max, 4.0f) && near(rp->floatValue, 1.0f);
			}
			// Appended last in the .frag, so the pre-existing uniforms keep
			// their positions and old compositions stay aligned.
			const bool appendedLast = ratioIndex == sb->parameters.getSize()-1;
			// transform.frag declares scaley BEFORE scalex; the panel must
			// still read scalex first. Proves the canonical order overrides
			// declaration order without touching the array.
			const char *wanted[] = {"scaleratio", "scalex", "scaley",
				"offsetx", "offsety", "rotacion"};
			bool slotsOk = app.boxes.getOpenParameterCount() == 6;
			for (int sl = 0; slotsOk && sl < 6; ++sl)
			{
				JPParameter *sp = app.boxes.getOpenParameterAtIndex(sl);
				if (sp == nullptr || sp->name != wanted[sl])
				{
					slotsOk = false;
					ofLogNotice("shaderratio") << "slot " << sl << " expected "
						<< wanted[sl] << " got "
						<< (sp ? sp->name : std::string("<null>"));
				}
			}
			shaderScaleRatio = ratioIndex >= 0 && rangeOk && appendedLast &&
				slotsOk && first != nullptr &&
				jp_media::isScaleRatioParameter(first->name);
			ofLogNotice("shaderratio") << "index=" << ratioIndex
				<< " of " << sb->parameters.getSize()
				<< " slot0=" << (first ? first->name : std::string("<null>"))
				<< " rangeOk=" << rangeOk << " appendedLast=" << appendedLast;
		}
		app.boxes.clear();
	}
	// Load a spread of REAL pre-change compositions and confirm every value
	// still lands on the parameter it names. Loading by name replaced purely
	// positional loading, so this is the check that the swap was transparent -
	// the unit tests cannot see a whole savefile.
	bool realCompoLoad = true;
	{
		const char *compos[] = {"savefiles/cameffect2.xml",
			"savefiles/pruebaam2.xml", "savefiles/camefect1.xml"};
		for (const char *rel : compos)
		{
			const string path = ofToDataPath(rel, true);
			if (!ofFile::doesFileExist(path)) continue;
			ofXml xml;
			if (!xml.load(path)) continue;
			app.boxes.clear();
			app.boxes.load(path);
			int checked = 0;
			auto savedBoxes = xml.getChildren("box");
			int boxIndex = 0;
			for (auto &savedBox : savedBoxes)
			{
				if (boxIndex >= (int)app.boxes.boxes.size()) break;
				JPbox *live = app.boxes.boxes[boxIndex++];
				if (live == nullptr) continue;
				for (auto &param : savedBox.getChild("parameters").getChildren("param"))
				{
					const string pname = param.getChild("name").getValue();
					const int idx = live->parameters.indexOfName(pname);
					if (idx < 0) continue;   // renamed/absent: positional path
					if (live->parameters.getType(idx) != JPParameter::FLOAT) continue;
					const float want = param.getChild("value").getFloatValue();
					const float got = live->parameters.getFloatValue(idx);
					++checked;
					if (std::abs(want-got) > 0.002f)
					{
						realCompoLoad = false;
						ofLogNotice("compoload") << rel << " " << pname
							<< " want=" << want << " got=" << got;
					}
				}
			}
			ofLogNotice("compoload") << rel << " boxes="
				<< app.boxes.boxes.size() << " params checked=" << checked;
			app.boxes.clear();
		}
	}
	// Saving a composition must never repoint what the app opens at startup.
	//
	// Ctrl+S writes wherever you choose and then follows that file with
	// savedirectory, so a later overwrite hits the right thing. defaultCompoPath
	// is a different decision entirely - it belongs to the SETTINGS screen and
	// is persisted by saveSettings(). Nothing on the save path may touch it.
	// This test exists so that adding `defaultCompoPath = path` to saveSession
	// or saveSessionAs fails here rather than silently changing which set
	// loads on the next launch.
	bool saveKeepsDefaultCompo = true;
	{
		const string originalDefault = app.defaultCompoPath;
		const string originalDir = app.savedirectory;
		const string sentinel = "savefiles/__default_sentinel.xml";
		app.defaultCompoPath = sentinel;
		app.savedirectory = sentinel;

		const string scratch = ofToDataPath("savefiles/__savetest.xml", true);
		app.boxes.clear();
		app.boxes.addBox("cam", 40, 40);
		app.saveSession(scratch);
		// What saveSessionAs does after a successful dialog.
		app.savedirectory = scratch;

		if (app.defaultCompoPath != sentinel)
		{
			saveKeepsDefaultCompo = false;
			ofLogNotice("savedefault") << "defaultCompoPath changed to "
				<< app.defaultCompoPath;
		}
		if (!ofFile::doesFileExist(scratch))
		{
			saveKeepsDefaultCompo = false;
			ofLogNotice("savedefault") << "save produced no file at " << scratch;
		}
		ofFile::removeFile(scratch);
		app.defaultCompoPath = originalDefault;
		app.savedirectory = originalDir;
		app.boxes.clear();
	}
	// Box identity: it must survive a save/load round trip, reach inside
	// groups, and never be shared by two boxes.
	//
	// Identity is what live-output source selection binds to, so every failure
	// here is silent at the point it happens and only shows up later as an
	// output following the wrong box or going dark.
	bool boxIdentity = true;
	{
		auto fail = [&](const string &why)
		{
			boxIdentity = false;
			ofLogNotice("boxuid") << why;
		};
		const string uidPath = directory + "uids.xml";

		app.boxes.clear();
		app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 40, 40);
		app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 90, 40);
		if (app.boxes.boxes.size() < 2) fail("could not build two boxes");
		else
		{
			JPbox *a = app.boxes.boxes[0];
			JPbox *b = app.boxes.boxes[1];
			// Constructor-minted, so never empty and never shared.
			if (a->uid.empty() || b->uid.empty()) fail("ctor left a uid empty");
			if (a->uid == b->uid) fail("two fresh boxes share a uid");

			a->setOutputCandidate(true);
			const string keptUid = a->uid;
			const string otherUid = b->uid;

			// Renaming must not disturb identity - that is the whole point.
			a->name = "renamed_after_marking";
			if (a->uid != keptUid) fail("rename changed the uid");

			app.boxes.save(uidPath);
			app.boxes.clear();
			app.boxes.load(uidPath);

			if (app.boxes.boxes.size() < 2) fail("round trip lost a box");
			else
			{
				if (app.boxes.boxes[0]->uid != keptUid)
					fail("uid did not survive save/load");
				if (app.boxes.boxes[1]->uid != otherUid)
					fail("second uid did not survive save/load");
				if (!app.boxes.boxes[0]->getOutputCandidate())
					fail("tooutput flag did not survive save/load");
				if (app.boxes.boxes[1]->getOutputCandidate())
					fail("tooutput flag leaked onto an unmarked box");
				// Resolution is by uid, and finds the box even though its name
				// changed before it was ever saved.
				if (app.boxes.findBoxByUid(keptUid) != app.boxes.boxes[0])
					fail("findBoxByUid did not resolve a top-level box");
				if (app.boxes.findBoxByUid("no-such-uid") != nullptr)
					fail("findBoxByUid invented a match");
				// Only the marked box is offered as a source.
				const auto candidates = app.boxes.getOutputCandidates();
				if (candidates.size() != 1)
					fail("expected exactly one marked candidate, got " +
						ofToString((int)candidates.size()));
				else if (candidates[0].uid != keptUid)
					fail("candidate list returned the wrong box");
			}
		}

		// Duplicates are repaired shallowest-first. A depth-first rule would
		// let a nested child keep the uid and renumber the top-level box -
		// exactly the box most likely to be driving an output.
		app.boxes.clear();
		app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 40, 40);
		if (!app.boxes.boxes.empty())
		{
			JPbox *top = app.boxes.boxes.front();
			const string before = top->uid;
			app.boxes.repairBoxUids();
			if (top->uid != before)
				fail("repair renumbered a box that was already unique");
			// Force a clash and confirm it is broken up.
			app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 90, 40);
			if (app.boxes.boxes.size() >= 2)
			{
				app.boxes.boxes[1]->uid = app.boxes.boxes[0]->uid;
				app.boxes.repairBoxUids();
				if (app.boxes.boxes[0]->uid != before)
					fail("repair moved the shallower/earlier box's uid");
				if (app.boxes.boxes[1]->uid == before)
					fail("repair left a duplicate uid in place");
				if (app.boxes.boxes[1]->uid.empty())
					fail("repair produced an empty uid");
			}
		}
		app.boxes.clear();
	}
	// Live-output binding: reaches into groups, survives a rename, heals a
	// legacy name-only binding, and retires when the box is gone.
	bool outputBinding = true;
	{
		auto fail = [&](const string &why)
		{
			outputBinding = false;
			ofLogNotice("outputbind") << why;
		};
		const string groupPath = "data/groups/group_2026-07-27-18-30-48-181.xml";
		app.boxes.clear();
		app.boxes.addBox(groupPath, 40, 40);
		JPbox_preset *group = app.boxes.boxes.empty() ? nullptr :
			dynamic_cast<JPbox_preset *>(app.boxes.boxes.front());
		if (group == nullptr || group->boxes.empty())
		{
			ofLogNotice("outputbind") << "group fixture unavailable - skipped";
		}
		else
		{
			JPbox *child = group->boxes.front();
			// A box inside a group is a legitimate source; a name lookup could
			// never reach one, which is why the binding moved to uids at all.
			child->setOutputCandidate(true);
			bool foundChild = false;
			for (const auto &candidate : app.boxes.getOutputCandidates())
			{
				if (candidate.uid != child->uid) continue;
				foundChild = true;
				// Path-qualified, so two groups each holding a "mask" stay
				// tellable apart in the picker.
				if (candidate.label.find(" / ") == string::npos)
					fail("group child label is not path-qualified: " +
						candidate.label);
				if (candidate.label.find(child->name) == string::npos)
					fail("group child label lost the box name: " +
						candidate.label);
			}
			if (!foundChild) fail("marked group child is not offered as a source");
			if (app.boxes.findBoxByUid(child->uid) != child)
				fail("findBoxByUid cannot reach a group child");

			ofApp::LiveOutputConfig config;
			config.sourceMode = ofApp::LIVE_OUTPUT_FIXED_BOX;
			config.sourceUid = child->uid;
			if (app.resolveLiveOutputSource(config) != child)
				fail("uid binding did not resolve");
			// The entire point: renaming must not move the binding.
			child->name = "renamed_child";
			if (app.resolveLiveOutputSource(config) != child)
				fail("binding broke when the box was renamed");

			// Legacy settings name a TOP-LEVEL box and carry no uid. That must
			// still resolve, and adopt the uid so it is rename-proof afterwards.
			ofApp::LiveOutputConfig legacy;
			legacy.sourceMode = ofApp::LIVE_OUTPUT_FIXED_BOX;
			legacy.sourceBox = group->name;
			if (app.resolveLiveOutputSource(legacy) != group)
				fail("legacy name-only binding did not resolve");
			if (legacy.sourceUid != group->uid)
				fail("legacy binding did not adopt the uid");

			// Box gone -> nothing resolves, which is what drives the caller to
			// revert the output to MAIN Active.
			const string goneUid = child->uid;
			app.boxes.clear();
			ofApp::LiveOutputConfig dangling;
			dangling.sourceMode = ofApp::LIVE_OUTPUT_FIXED_BOX;
			dangling.sourceUid = goneUid;
			if (app.resolveLiveOutputSource(dangling) != nullptr)
				fail("a deleted box still resolved");
			if (app.boxes.hasBoxUid(goneUid))
				fail("hasBoxUid still reports a deleted box");
		}
		app.boxes.clear();
	}
	// Box button hit areas: bigger than the squares they belong to, and never
	// overlapping each other.
	//
	// The overlap invariant is the one worth a test. If the two hit rects ever
	// touch, which button a click lands on depends on the order they happen to
	// be tested in - a bug that looks like "sometimes it toggles the wrong one"
	// and is miserable to track down from a bug report.
	bool boxHitboxes = true;
	{
		auto fail = [&](const string &why)
		{
			boxHitboxes = false;
			ofLogNotice("hitbox") << why;
		};
		app.boxes.clear();
		app.boxes.addBox("shaders/imageprocessing/feedback_advance.frag", 300, 300);
		if (app.boxes.boxes.empty()) fail("no box to measure");
		else
		{
			JPbox *box = app.boxes.boxes.front();
			box->update();
			const ofRectangle onHit = box->onoff.hitBounds();
			const ofRectangle byHit = box->bypass.hitBounds();

			if (onHit.width <= box->onoff.width ||
				onHit.height <= box->onoff.height)
				fail("onoff hit area is not larger than the drawn square");
			if (byHit.width <= box->bypass.width ||
				byHit.height <= box->bypass.height)
				fail("bypass hit area is not larger than the drawn square");
			// Vertical slop is what a 12.6px target needs most on a zoomable
			// canvas, and nothing sits above or below to constrain it.
			if (onHit.height <= onHit.width)
				fail("expected more vertical than horizontal slop");
			if (onHit.intersects(byHit))
				fail("onoff and bypass hit areas overlap");
			// The padded rect must still be centred on the square it belongs
			// to, or the outline drawn from it would sit off target.
			if (std::abs(onHit.getCenter().x - box->onoff.x) > 0.01f ||
				std::abs(onHit.getCenter().y - box->onoff.y) > 0.01f)
				fail("hit area is not centred on its control");
			// And it must actually accept a click the bare square would miss.
			const float justOutside = box->onoff.y - box->onoff.height * 0.5f - 1.0f;
			if (!onHit.inside(box->onoff.x, justOutside))
				fail("a point just above the square is not clickable");

			// The texture OUT hit area must be centred on the DOT. draw_outlet
			// translates to outlet_x + outlet_size/2 before drawing, while the
			// original hit test used outlet_x - half an outlet to the left of
			// the dot. That offset is the bug this asserts against.
			const ofRectangle outlet = box->outletBounds();
			const float drawnCentreX =
				box->outlet_x + box->outlet_size * 0.5f;
			if (std::abs(outlet.getCenter().x - drawnCentreX) > 0.01f)
				fail("outlet hit area is not centred on the drawn dot: centre " +
					ofToString(outlet.getCenter().x) + " vs dot " +
					ofToString(drawnCentreX));
			if (std::abs(outlet.getCenter().y - box->outlet_y) > 0.01f)
				fail("outlet hit area is off vertically");
			if (outlet.width <= box->outlet_size ||
				outlet.height <= box->outlet_size)
				fail("outlet hit area is not larger than the dot");
			// Symmetric about the dot: the outer side is the one you actually
			// aim at, and it used to have no margin whatsoever.
			const float outerMargin = outlet.getRight() - drawnCentreX;
			const float innerMargin = drawnCentreX - outlet.getLeft();
			if (std::abs(outerMargin - innerMargin) > 0.01f)
				fail("outlet hit area is lopsided: outer " +
					ofToString(outerMargin) + " inner " +
					ofToString(innerMargin));
			if (!box->outletBounds().inside(drawnCentreX, box->outlet_y))
				fail("the dot centre is not inside its own hit area");
		}
		app.boxes.clear();
	}
	// A group composites its active child 1:1 into its own FBO, so the two must
	// come out pixel-identical.
	//
	// Rect mode is global and renderActiveRender runs during update(), so it
	// inherits whatever drew last. This forces the hostile value -
	// OF_RECTMODE_CENTER - which is exactly the state that left the group
	// holding a full-scale crop in its top-left quadrant. When that group is
	// the ACTIVE RENDER, the quarter-filled FBO paints a quarter-filled canvas,
	// and it read as intermittent because it depended on what drew last.
	bool groupComposite = true;
	{
		const string groupPath = "data/groups/group_2026-07-27-18-30-48-181.xml";
		app.boxes.clear();
		app.boxes.addBox(groupPath, 120, 120);
		JPbox_preset *group = app.boxes.boxes.empty() ? nullptr :
			dynamic_cast<JPbox_preset *>(app.boxes.boxes.front());
		if (group == nullptr || group->boxes.empty())
		{
			ofLogNotice("groupcomposite") << "group fixture unavailable - skipped";
		}
		else
		{
			group->setonoff(true);
			ofSetRectMode(OF_RECTMODE_CENTER);   // the hostile inherited state
			for (int i = 0; i < 8; ++i) group->update();

			JPbox *child = (group->activeRender >= 0 &&
				group->activeRender < (int)group->boxes.size()) ?
				group->boxes[group->activeRender] : nullptr;
			if (child == nullptr || !group->fbo.isAllocated() ||
				!child->fbo.isAllocated())
			{
				groupComposite = false;
				ofLogNotice("groupcomposite") << "no allocated child to compare";
			}
			else
			{
				ofPixels groupPixels, childPixels;
				group->fbo.readToPixels(groupPixels);
				child->fbo.readToPixels(childPixels);
				const int w = (int)groupPixels.getWidth();
				const int h = (int)groupPixels.getHeight();
				int mismatches = 0, sampled = 0;
				for (int gy = 1; gy < 8; ++gy)
				{
					for (int gx = 1; gx < 8; ++gx)
					{
						const int px = w * gx / 8;
						const int py = h * gy / 8;
						if (px >= w || py >= h) continue;
						++sampled;
						const ofColor a = groupPixels.getColor(px, py);
						const ofColor b = childPixels.getColor(px, py);
						if (std::abs((int)a.r - (int)b.r) > 8 ||
							std::abs((int)a.g - (int)b.g) > 8 ||
							std::abs((int)a.b - (int)b.b) > 8)
						{
							++mismatches;
						}
					}
				}
				if (sampled == 0 || mismatches > 0)
				{
					groupComposite = false;
					ofLogNotice("groupcomposite")
						<< "group FBO does not match its active child: "
						<< mismatches << "/" << sampled << " samples differ";
				}
			}
			ofSetRectMode(OF_RECTMODE_CORNER);
		}
		app.boxes.clear();
	}
	// A transition must take the same WALL-CLOCK time regardless of framerate.
	//
	// It used to advance by a fixed 0.02 per frame, so a fade ran 833ms at
	// 60fps and 2s at 25fps - it stretched exactly when the machine was already
	// struggling, and no duration setting could mean anything while the unit
	// was "frames" rather than milliseconds.
	bool transitionClock = true;
	{
		auto fail = [&](const string &why)
		{
			transitionClock = false;
			ofLogNotice("transclock") << why;
		};
		// Elapsed seconds to reach 1.0, fed a fixed timestep.
		auto elapsedFor = [](float durationMs, float frameSeconds)
		{
			TransitionSR t;
			t.setDurationMs(durationMs);
			t.setLerpValue(0.0f);
			float elapsed = 0.0f;
			// Generous bound: a stuck clock must fail the assert, not spin.
			for (int i = 0; i < 100000 && t.getLerpValue() < 1.0f; ++i)
			{
				t.advance(frameSeconds);
				elapsed += frameSeconds;
			}
			return elapsed;
		};

		const float atSixty = elapsedFor(800.0f, 1.0f / 60.0f);
		const float atThirty = elapsedFor(800.0f, 1.0f / 30.0f);
		const float atFifteen = elapsedFor(800.0f, 1.0f / 15.0f);
		// Within one frame of the slowest rate.
		if (std::abs(atSixty - 0.8f) > 0.07f)
			fail("800ms at 60fps took " + ofToString(atSixty, 3) + "s");
		if (std::abs(atThirty - 0.8f) > 0.07f)
			fail("800ms at 30fps took " + ofToString(atThirty, 3) + "s");
		if (std::abs(atFifteen - 0.8f) > 0.07f)
			fail("800ms at 15fps took " + ofToString(atFifteen, 3) + "s");
		// The point of the whole change: the rates agree with each other.
		if (std::abs(atSixty - atThirty) > 0.07f ||
			std::abs(atSixty - atFifteen) > 0.10f)
			fail("framerate changes the duration: 60fps=" +
				ofToString(atSixty, 3) + " 30fps=" + ofToString(atThirty, 3) +
				" 15fps=" + ofToString(atFifteen, 3));
		// And the duration is actually honoured, not just consistent.
		const float longer = elapsedFor(2000.0f, 1.0f / 60.0f);
		if (std::abs(longer - 2.0f) > 0.07f)
			fail("2000ms took " + ofToString(longer, 3) + "s");
		// A huge stall must not jump the whole fade in one frame.
		{
			TransitionSR t;
			t.setDurationMs(800.0f);
			t.setLerpValue(0.0f);
			t.advance(5.0f);
			if (t.getLerpValue() >= 1.0f)
				fail("a single 5s stall completed the whole transition");
		}
	}
	// Parameter morph: the MilkDrop half of the transition.
	bool paramMorph = true;
	{
		auto fail = [&](const string &why)
		{
			paramMorph = false;
			ofLogNotice("parammorph") << why;
		};
		JPParameter p;
		p.setup(0.20f, "scalex");          // STANDART, movtype 0
		p.min = p.nativeMin = 0.0f;
		p.max = p.nativeMax = 1.0f;

		// Half way to a target of 0.80 should emit 0.50 - and crucially must
		// NOT disturb floatLerpValue, which is the automation accumulator.
		p.floatLerpValue = 0.20f;
		p.setMorph(0.80f, 0.5f);
		p.update();
		if (std::abs(p.floatValue - 0.50f) > 0.001f)
			fail("half morph emitted " + ofToString(p.floatValue, 4) +
				" instead of 0.50");
		if (std::abs(p.floatLerpValue - 0.20f) > 0.0001f)
			fail("morph wrote floatLerpValue (" +
				ofToString(p.floatLerpValue, 4) +
				") - automation state must stay untouched");

		// Ends: 0 emits its own value, 1 emits the counterpart's.
		p.setMorph(0.80f, 0.0f);
		p.update();
		if (std::abs(p.floatValue - 0.20f) > 0.001f)
			fail("amount 0 emitted " + ofToString(p.floatValue, 4));
		p.setMorph(0.80f, 1.0f);
		p.update();
		if (std::abs(p.floatValue - 0.80f) > 0.001f)
			fail("amount 1 emitted " + ofToString(p.floatValue, 4));

		// Clearing restores the box's own value with no save/restore step.
		// This is what stops the OUTGOING box being left wearing the incoming
		// values once the fade completes.
		p.clearMorph();
		p.update();
		if (std::abs(p.floatValue - 0.20f) > 0.001f)
			fail("after clearMorph the parameter kept " +
				ofToString(p.floatValue, 4) + " instead of its own 0.20");
		if (p.isMorphing()) fail("isMorphing() still true after clearMorph");

		// The gate: a STANDART parameter has movtype 0 and needsUpdate false,
		// so JPParameterGroup::update() would skip it entirely and the morph
		// would silently do nothing on hand-set sliders.
		JPParameterGroup group;
		group.addFloatValue(0.20f, "scalex");
		JPParameter *gp = group.getJParameter(0);
		if (gp == nullptr) fail("could not build a group parameter");
		else
		{
			gp->movtype = JPParameter::STANDART;
			gp->needsUpdate = false;
			gp->floatLerpValue = 0.20f;
			gp->setMorph(0.80f, 1.0f);
			group.update();
			if (std::abs(gp->floatValue - 0.80f) > 0.001f)
				fail("a STANDART parameter did not morph: emitted " +
					ofToString(gp->floatValue, 4) +
					" - the update() gate is skipping it");
		}
	}

	// Arming across two real boxes: both directions morph, name-only matching,
	// and a clean release.
	bool morphArming = true;
	{
		auto fail = [&](const string &why)
		{
			morphArming = false;
			ofLogNotice("morpharm") << why;
		};
		const string shaderA = "shaders/imageprocessing/transform.frag";
		const string shaderB = "shaders/imageprocessing/feedback_advance.frag";

		// CASE 1 - same shader twice, so every name matches.
		app.boxes.clear();
		app.boxes.addBox(shaderA, 40, 40);
		app.boxes.addBox(shaderA, 120, 40);
		if (app.boxes.boxes.size() < 2) fail("could not build two boxes");
		else
		{
			JPbox *a = app.boxes.boxes[0];
			JPbox *b = app.boxes.boxes[1];
			const int idx = b->parameters.indexOfName("scalex");
			if (idx < 0) fail("fixture shader lost its scalex");
			else
			{
				// Distinct values so a blend is visible.
				// Both, because update() ends with floatValue = floatLerpValue -
				// setting only floatValue would be erased on the next tick.
				const int aIdx = a->parameters.indexOfName("scalex");
				a->parameters.setFloatValue(0.20f, aIdx);
				a->parameters.setFloatLerpValue(0.20f, aIdx);
				b->parameters.setFloatValue(0.80f, idx);
				b->parameters.setFloatLerpValue(0.80f, idx);
				JPParameter *ap = a->parameters.getJParameter(
					aIdx);
				JPParameter *bp = b->parameters.getJParameter(idx);

				app.boxes.requestSetActiveRender(0);
				app.boxes.requestSetActiveRender(1);   // arms the morph

				if (ap == nullptr || bp == nullptr) fail("lost the parameter");
				else
				{
					if (!bp->isMorphing()) fail("incoming box is not morphing");
					if (!ap->isMorphing()) fail("outgoing box is not morphing");
					// Incoming starts wearing the outgoing look.
					if (std::abs(bp->morphTarget - 0.20f) > 0.001f)
						fail("incoming aims at " +
							ofToString(bp->morphTarget, 3) + ", expected 0.20");
					// Outgoing leaves wearing the incoming one.
					if (std::abs(ap->morphTarget - 0.80f) > 0.001f)
						fail("outgoing aims at " +
							ofToString(ap->morphTarget, 3) + ", expected 0.80");

					for (int i = 0; i < 600 && (ap->isMorphing() ||
						bp->isMorphing()); ++i)
					{
						app.boxes.update();
					}
					if (ap->isMorphing() || bp->isMorphing())
						fail("morph never released after the transition ended");
					// And each box is back to its own value, with no restore
					// step - the outgoing box must NOT keep 0.80.
					if (std::abs(ap->floatValue - 0.20f) > 0.02f)
						fail("outgoing box kept " +
							ofToString(ap->floatValue, 3) +
							" instead of its own 0.20");
					if (std::abs(bp->floatValue - 0.80f) > 0.02f)
						fail("incoming box settled at " +
							ofToString(bp->floatValue, 3) +
							" instead of its own 0.80");
				}
			}
		}

		// CASE 2 - two shaders sharing NO parameter name. Nothing may morph.
		// copyParametersByNameOrIndex would have paired these by array
		// position; that fallback is right for cue drafts and wrong here.
		app.boxes.clear();
		app.boxes.addBox(shaderA, 40, 40);
		app.boxes.addBox(shaderB, 120, 40);
		if (app.boxes.boxes.size() >= 2)
		{
			JPbox *a = app.boxes.boxes[0];
			JPbox *b = app.boxes.boxes[1];
			int overlap = 0;
			for (int i = 0; i < b->parameters.getSize(); ++i)
				if (a->parameters.indexOfName(b->parameters.getName(i)) >= 0)
					++overlap;
			if (overlap > 0)
				ofLogNotice("morpharm")
					<< "fixtures share " << overlap
					<< " name(s) - positional check skipped";
			else
			{
				app.boxes.requestSetActiveRender(0);
				app.boxes.requestSetActiveRender(1);
				int morphing = 0;
				for (int i = 0; i < b->parameters.getSize(); ++i)
				{
					JPParameter *q = b->parameters.getJParameter(i);
					if (q != nullptr && q->isMorphing()) ++morphing;
				}
				for (int i = 0; i < a->parameters.getSize(); ++i)
				{
					JPParameter *q = a->parameters.getJParameter(i);
					if (q != nullptr && q->isMorphing()) ++morphing;
				}
				if (morphing > 0)
					fail(ofToString(morphing) + " parameters morphed between "
						"shaders that share no name - matching fell back to "
						"array position");
			}
		}
		app.boxes.clear();
	}
	// Every transition shader must compile.
	//
	// TransitionSR falls back to mix.frag when a load fails, so a GLSL error in
	// warp or dither would show up as "the transition type does nothing" rather
	// than as an error - the sort of thing that survives a whole gig unnoticed.
	bool transitionShaders = true;
	{
		const char *frags[] = {
			"shaders/private/mix.frag",
			"shaders/private/transition_warp.frag",
			"shaders/private/transition_dither.frag"
		};
		for (const char *frag : frags)
		{
			if (!ofFile::doesFileExist(ofToDataPath(frag, true)))
			{
				transitionShaders = false;
				ofLogNotice("transhader") << "missing: " << frag;
				continue;
			}
			ofShader probe;
			if (!probe.load("shaders/default.vert", frag))
			{
				transitionShaders = false;
				ofLogNotice("transhader") << "failed to compile: " << frag;
			}
		}
		// The enum is written into settings.xml, so the labels must cover every
		// value - a gap would surface as a blank button.
		for (int i = 0; i < TransitionSR::TYPE_COUNT; ++i)
		{
			const char *label = TransitionSR::typeLabel(i);
			if (label == nullptr || label[0] == '\0')
			{
				transitionShaders = false;
				ofLogNotice("transhader") << "no label for type " << i;
			}
		}
	}
	// The camera-depth box.
	//
	// The dispatch risk is specific and easy to get wrong: "camdepth" CONTAINS
	// "cam", and the box dispatch, the preset loader and the auto-namer all test
	// for "cam" with a substring search. If the camdepth branch is ever ordered
	// after the cam one, this quietly builds a CAMARITA instead - same picture,
	// no depth, no error anywhere.
	// The parallax cue must actually MEASURE motion - report energy where the
	// image changed and nothing where it did not.
	//
	// This is the one cue in the depth box that is geometry rather than a guess
	// about appearance, so it is the one worth pinning down. Driven with
	// synthetic frames rather than a camera: a real camera cannot be made to
	// move a known amount, and sensor noise would put a floor under the "did
	// not move" half of the check.
	// The heat ramp must actually be a depth ramp: white near, yellow, red at
	// middle distance, violet far - and darkening monotonically the whole way.
	//
	// Checking that "some colour came out" would pass for a greyscale copy or
	// for turbo, which runs blue-to-red and reads this box's near = bright
	// convention backwards. The checks below are the ones that separate THIS
	// ramp from those: blue must overtake green at the far end, and green must
	// overtake blue at three quarters.
	// A box may not feed its own inlet.
	//
	// It is not just a useless patch: the box would bind its output texture as
	// an input while rendering INTO that texture, which OpenGL leaves
	// undefined - driver-dependent garbage, not the feedback the gesture looks
	// like it should produce. Shader boxes already have a correct feedback path
	// through the `feedback` uniform, which works because it reads a copy.
	// The shared scheduler. Both the top level and every group now route
	// through jp_renderschedule::apply, so its rules are worth pinning down
	// directly rather than only through the two callers.
	// Tooltip placement.
	//
	// The box is drawn with OF_RECTMODE_CENTER, and the anchor the callers pass
	// is the centre of the thing being labelled - so the tooltip centre belongs
	// ON the anchor. It used to be anchorX + width / 2, which slid the box half
	// its own width to the right: a couple of pixels on "Pause", and clean off
	// the screen on a long box name, which is why it read as "tooltips are
	// broken" rather than "tooltips are offset".
	// Tooltip placement, transform resolution and truncation.
	//
	// Two implementations of jp_tooltip existed - one used by ~90 call sites and
	// one added for the box titles - and they disagreed on the delay, the anchor
	// convention and the rect mode. They are now one, so these checks guard the
	// behaviour the whole program shares.
	// Space + drag pans the canvas.
	//
	// Two halves are worth pinning down. The suppression rule first: space is a
	// pan gesture on the canvas but a printable character in a field, so panning
	// while someone types a filename is the failure mode. Then the guard
	// interaction: an armed pan must not move boxes or open a marquee, which is
	// the property that lets the gesture reuse viewportPanning instead of adding
	// state of its own.
	// Multi-select: shift-drag adds to the selection, ctrl-click toggles one box.
	//
	// The merge rule is checked directly because its invariant is not obvious:
	// the result must be duplicate-free. The multi-drag walks the selection and
	// moves every entry, so a box that appears twice is moved twice and slides
	// away from the group it was supposed to travel with.
	bool multiSelect = true;
	{
		auto fail = [&](const string &why)
		{
			multiSelect = false;
			ofLogNotice("multiselect") << why;
		};
		auto show = [](const vector<int> &v)
		{
			string out;
			for (int i : v) out += (out.empty() ? "" : ",") + ofToString(i);
			return "[" + out + "]";
		};

		vector<int> merged;

		// No base: the marquee replaces, which is the plain drag and must not
		// have changed.
		JPboxgroup::mergeSelection(merged, {}, {2, 5});
		if (merged != vector<int>({2, 5}))
			fail("a marquee with no base did not simply take its own hits: " +
				show(merged));

		// With a base: both, base first.
		JPboxgroup::mergeSelection(merged, {1, 3}, {7});
		if (merged != vector<int>({1, 3, 7}))
			fail("shift-drag did not add to the existing selection: " +
				show(merged));

		// Overlap: kept once.
		JPboxgroup::mergeSelection(merged, {1, 3}, {3, 4});
		if (merged != vector<int>({1, 3, 4}))
			fail("a box in both the base and the marquee was listed twice, so "
				"the multi-drag would move it at double speed: " + show(merged));

		// An empty marquee leaves a shift-drag's base intact rather than
		// clearing it, which is what makes shift-dragging over nothing harmless.
		JPboxgroup::mergeSelection(merged, {4, 8}, {});
		if (merged != vector<int>({4, 8}))
			fail("an empty marquee dropped the base selection: " + show(merged));

		// --- ctrl+click toggling, against the real graph ---
		const string shader = "shaders/imageprocessing/transform.frag";
		app.boxes.clear();
		for (int i = 0; i < 3; ++i)
			app.boxes.addBox(shader, 100.0f + i * 60.0f, 100.0f);
		if (app.boxes.boxes.size() != 3)
		{
			ofLogNotice("multiselect") << "fixture unavailable - skipped";
		}
		else
		{
			app.boxes.clearSelection();
			app.boxes.toggleBoxSelection(0);
			app.boxes.toggleBoxSelection(2);
			if (!app.boxes.isBoxSelected(0) || !app.boxes.isBoxSelected(2))
				fail("ctrl+click did not add boxes to the selection");
			if (app.boxes.isBoxSelected(1))
				fail("ctrl+click selected a box that was never clicked");

			// Toggling again removes just that one and leaves the rest.
			app.boxes.toggleBoxSelection(0);
			if (app.boxes.isBoxSelected(0))
				fail("ctrl+clicking a selected box did not deselect it");
			if (!app.boxes.isBoxSelected(2))
				fail("deselecting one box dropped the rest of the selection");
			if (app.boxes.getSelectedBoxIndices().size() != 1)
				fail("the selection has " +
					ofToString((int)app.boxes.getSelectedBoxIndices().size()) +
					" entries, expected 1");

			// A negative index is ignored rather than poisoning the list - the
			// click paths pass -1 for "nothing under the pointer".
			app.boxes.toggleBoxSelection(-1);
			if (app.boxes.getSelectedBoxIndices().size() != 1)
				fail("a negative index was added to the selection");
		}
		app.boxes.clearSelection();
		app.boxes.clear();
	}
	bool spacePan = true;
	{
		auto fail = [&](const string &why)
		{
			spacePan = false;
			ofLogNotice("spacepan") << why;
		};

		app.boxes.setExternalTextCaptureTest(nullptr);
		const bool restoreRenaming = app.boxes.tabRenaming;
		app.boxes.tabRenaming = false;
		if (!app.boxes.spacePanAllowed())
			fail("the gesture is off with nothing capturing text");

		app.boxes.tabRenaming = true;
		if (app.boxes.spacePanAllowed())
			fail("the gesture stays on while a box name is being typed");
		app.boxes.tabRenaming = restoreRenaming;

		app.boxes.setExternalTextCaptureTest([]() { return true; });
		if (app.boxes.spacePanAllowed())
			fail("the gesture ignores ofApp's text fields - it would pan the "
				"canvas while a filename is being typed");
		app.boxes.setExternalTextCaptureTest([]() { return false; });
		if (!app.boxes.spacePanAllowed())
			fail("the gesture stays off when no field is focused");

		const string shader = "shaders/imageprocessing/transform.frag";
		app.boxes.clear();
		app.boxes.addBox(shader, 200.0f, 200.0f);
		if (app.boxes.boxes.empty())
		{
			ofLogNotice("spacepan") << "fixture unavailable - skipped";
		}
		else
		{
			JPbox *box = app.boxes.boxes.front();
			const float startX = box->x, startY = box->y;
			// What the arm site does when space is held.
			app.boxes.viewportPanning = true;
			for (int i = 0; i < 4; ++i)
				app.boxes.update_mouseDragged(OF_MOUSE_BUTTON_LEFT);
			if (std::abs(box->x - startX) > 0.01f ||
				std::abs(box->y - startY) > 0.01f)
			{
				fail("a space-armed left drag moved a box, from (" +
					ofToString(startX) + "," + ofToString(startY) + ") to (" +
					ofToString(box->x) + "," + ofToString(box->y) + ")");
			}
			if (app.boxes.draw_SelectionRect)
				fail("a space-armed left drag opened a selection rectangle");
			app.boxes.viewportPanning = false;
		}
		app.boxes.clear();
		// Leave the hook as ofApp wired it, or every later check runs with the
		// canvas believing a field is focused.
		app.boxes.setExternalTextCaptureTest([&app]() {
			return app.anyFieldFocused() || app.saveModalActive;
		});
	}
	// Loading a session while inside a group view used to read past the end of
	// the box vector: clear() emptied the vector but left activeGroupPath, which
	// is an index path INTO it, and getActivePreset trusted its first element.
	// Reachable today through the OSC load command, so not hypothetical.
	bool groupPathAfterClear = true;
	{
		auto fail = [&](const string &why)
		{
			groupPathAfterClear = false;
			ofLogNotice("grouppath") << why;
		};
		const string groupPath = "data/groups/group_2026-07-27-18-30-48-181.xml";
		app.boxes.clear();
		app.boxes.addBox(groupPath, 120, 120);
		if (app.boxes.boxes.empty())
		{
			ofLogNotice("grouppath") << "group fixture unavailable - skipped";
		}
		else
		{
			// Stand inside the group, then pull the graph out from under it.
			app.boxes.activeGroupPath.clear();
			app.boxes.activeGroupPath.push_back(0);
			app.boxes.clear();
			if (!app.boxes.activeGroupPath.empty())
				fail("clear() left the group path pointing into an empty vector");
			if (app.boxes.getActivePreset() != nullptr)
				fail("getActivePreset returned something after a clear");

			// The bounds check itself. This one is a CONTRACT assertion, not a
			// regression test: removing the check is undefined behaviour, which
			// can silently appear to work, so it cannot be proven by reverting.
			app.boxes.activeGroupPath.clear();
			app.boxes.activeGroupPath.push_back(99);
			if (app.boxes.getActivePreset() != nullptr)
				fail("getActivePreset accepted an out-of-range first level");
			app.boxes.activeGroupPath.clear();
		}
		app.boxes.clear();
	}
	bool tooltipLayout = true;
	{
		auto fail = [&](const string &why)
		{
			tooltipLayout = false;
			ofLogNotice("tooltip") << why;
		};
		const float screenW = 1000.0f, screenH = 800.0f;
		const float lineHeight = 14.0f;

		// Centred over the anchor, and INDEPENDENT of the text width. That
		// independence is the regression guard: the box version used to offset by
		// half its own width, so a long name slid clean off the screen.
		const ofRectangle widget(480.0f, 400.0f, 40.0f, 20.0f);
		float previousCentre = -1.0f;
		for (float textW : {20.0f, 120.0f, 300.0f})
		{
			const ofRectangle box = jp_tooltip::layout(widget, textW, lineHeight,
				screenW, screenH);
			const float centre = box.x + box.width / 2.0f;
			if (std::abs(centre - 500.0f) > 0.51f)
				fail("text width " + ofToString(textW) + " put the centre at " +
					ofToString(centre) + ", expected 500 - the tooltip is being "
					"offset by its own width");
			if (previousCentre >= 0.0f && std::abs(centre - previousCentre) > 0.51f)
				fail("the centre shifts with the text width");
			previousCentre = centre;
			if (box.y + box.height > widget.y)
				fail("the tooltip is not above its widget");
		}

		// Clamped at both edges rather than clipped away.
		const ofRectangle atRight = jp_tooltip::layout(
			ofRectangle(screenW - 10.0f, 400.0f, 8.0f, 20.0f), 200.0f,
			lineHeight, screenW, screenH);
		if (atRight.x + atRight.width > screenW)
			fail("a tooltip at the right edge runs off screen, right edge at " +
				ofToString(atRight.x + atRight.width));
		const ofRectangle atLeft = jp_tooltip::layout(
			ofRectangle(2.0f, 400.0f, 8.0f, 20.0f), 200.0f, lineHeight,
			screenW, screenH);
		if (atLeft.x < 0.0f)
			fail("a tooltip at the left edge runs off screen, left edge at " +
				ofToString(atLeft.x));

		// Near the top there is no room above, so it flips BELOW. Widgets parked
		// at the top of the window are ordinary, not an edge case.
		const ofRectangle widgetTop(500.0f, 2.0f, 40.0f, 20.0f);
		const ofRectangle atTop = jp_tooltip::layout(widgetTop, 120.0f,
			lineHeight, screenW, screenH);
		if (atTop.y < 0.0f)
			fail("a tooltip near the top runs off screen, top edge at " +
				ofToString(atTop.y));
		if (atTop.y < widgetTop.y + widgetTop.height)
			fail("a tooltip near the top did not flip below its widget");

		// Truncation. Without it an over-long string overflows however the box is
		// placed, because the position clamp cannot shrink it.
		auto measure = [](const string &t) { return (float)t.size() * 10.0f; };
		const string fitted = jp_tooltip::fit(
			"a very long tooltip that will never fit", 150.0f, measure);
		if (measure(fitted) > 150.0f)
			fail("fit() returned something wider than the limit: '" + fitted + "'");
		if (fitted.size() >= 38)
			fail("fit() did not shorten the text");
		if (fitted.find("...") == string::npos)
			fail("fit() dropped text without marking it with an ellipsis");
		const string shortEnough = jp_tooltip::fit("ok", 150.0f, measure);
		if (shortEnough != "ok")
			fail("fit() altered a string that already fitted: '" + shortEnough + "'");
	}
	// The reported bug: with the canvas panned or zoomed, the tooltip landed far
	// from the box. The request happens inside the canvas' ofTranslate/ofScale
	// but the paint happens at the end of ofApp::draw, outside it.
	//
	// resolvePending takes the flush-time base as a parameter precisely so this
	// can be checked here: the harness runs in ofApp::setup, where the base matrix
	// is not the same as during draw, and the maths is RELATIVE so it must not
	// depend on which base it is.
	bool tooltipTransform = true;
	{
		auto fail = [&](const string &why)
		{
			tooltipTransform = false;
			ofLogNotice("tooltiptransform") << why;
		};
		const glm::mat4 base = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
		const ofRectangle widget(10.0f, 10.0f, 40.0f, 20.0f);

		auto resolved = [&](std::function<void()> pushTransform)
		{
			ofPushMatrix();
			pushTransform();
			jp_tooltip::request("t", widget);
			ofPopMatrix();
			string text;
			ofRectangle out;
			if (!jp_tooltip::resolvePending(base, text, out))
			{
				fail("nothing pending after request()");
				return ofRectangle();
			}
			return out;
		};
		auto near = [](float a, float b) { return std::abs(a - b) <= 0.51f; };

		// No transform: a screen-space caller must pass straight through, which is
		// what keeps the ~90 existing call sites working untouched.
		const ofRectangle plain = resolved([]{});
		if (!near(plain.x, 10.0f) || !near(plain.y, 10.0f) ||
			!near(plain.width, 40.0f) || !near(plain.height, 20.0f))
		{
			fail("with no transform the anchor changed: " + ofToString(plain));
		}

		// Pan only.
		const ofRectangle panned = resolved([]{ ofTranslate(100.0f, 50.0f); });
		if (!near(panned.x, 110.0f) || !near(panned.y, 60.0f))
			fail("pan not applied, got " + ofToString(panned));
		if (!near(panned.width, 40.0f) || !near(panned.height, 20.0f))
			fail("pan should not resize the anchor, got " + ofToString(panned));

		// Pan and zoom together - the case in the report. The anchor must scale
		// too, or the tooltip sits against where the box would be at zoom 1.
		const ofRectangle zoomed = resolved([]{
			ofTranslate(100.0f, 50.0f);
			ofScale(2.0f, 2.0f);
		});
		if (!near(zoomed.x, 120.0f) || !near(zoomed.y, 70.0f))
			fail("pan+zoom position wrong, got " + ofToString(zoomed));
		if (!near(zoomed.width, 80.0f) || !near(zoomed.height, 40.0f))
			fail("pan+zoom did not scale the anchor, got " + ofToString(zoomed));

		// Zoomed out, the anchor shrinks - the tooltip should hug the small box.
		const ofRectangle small = resolved([]{ ofScale(0.5f, 0.5f); });
		if (!near(small.width, 20.0f) || !near(small.x, 5.0f))
			fail("zoom out not applied, got " + ofToString(small));
	}
	bool renderSchedule = true;
	{
		auto fail = [&](const string &why)
		{
			renderSchedule = false;
			ofLogNotice("renderschedule") << why;
		};
		const string shader = "shaders/imageprocessing/transform.frag";
		app.boxes.clear();
		for (int i = 0; i < 8; ++i)
			app.boxes.addBox(shader, 40.0f + i * 30.0f, 40.0f);
		if (app.boxes.boxes.size() < 8 ||
			app.boxes.boxes[1]->fbohandlergroup.getSize() < 1)
		{
			ofLogNotice("renderschedule") << "fixture unavailable - skipped";
		}
		else
		{
			vector<JPbox *> &bx = app.boxes.boxes;
			// Chain: 0 feeds 1, 1 feeds 2. Root at 2 must pull in 1 and 0.
			bx[1]->fbohandlergroup.setFboPointer(&bx[0]->fbo, &bx[0]->name, 0);
			bx[2]->fbohandlergroup.setFboPointer(&bx[1]->fbo, &bx[1]->name, 0);

			jp_renderschedule::apply(bx, {2}, 1, false);
			for (int i : {0, 1, 2})
				if (!bx[i]->shouldRenderThisFrame())
					fail("box " + ofToString(i) + " is on the dependency path "
						"of the root but was not scheduled");

			// Off the path, only the staggered share may refresh. With eight
			// boxes and an interval of four, three of the five off-path boxes
			// must be idle on any given frame whatever the frame number.
			// Probed on every phase, not just one: on a synchronised schedule
			// there is always some frame where nothing is idle, and probing a
			// single frame can miss it.
			for (uint64_t f = 0; f < (uint64_t)jp_renderschedule::kPreviewInterval; ++f)
			{
				jp_renderschedule::apply(bx, {2}, f, false);
				int offPathIdle = 0;
				for (int i = 3; i < 8; ++i)
					if (!bx[i]->shouldRenderThisFrame()) ++offPathIdle;
				if (offPathIdle < 3)
					fail("frame " + ofToString((int)f) + ": only " +
						ofToString(offPathIdle) + " of 5 off-path boxes idle - "
						"the preview rate is not being applied");
			}

			// Staggered, not synchronised.
			//
			// Counting refreshes per box is NOT enough: a shared phase also
			// gives every box exactly one refresh per interval, it just puts
			// them all on the SAME frame. What matters is which frame each one
			// lands on, so this records the phase and requires the boxes to be
			// spread across all of them - that is the whole point, since a
			// shared phase spikes the entire preview cost onto one frame in
			// four instead of levelling it.
			const int interval = jp_renderschedule::kPreviewInterval;
			vector<int> refreshes(8, 0);
			vector<bool> phaseUsed(interval, false);
			for (uint64_t f = 0; f < (uint64_t)interval; ++f)
			{
				jp_renderschedule::apply(bx, {}, f, false);
				int refreshedThisFrame = 0;
				for (int i = 0; i < 8; ++i)
				{
					if (!bx[i]->shouldRenderThisFrame()) continue;
					++refreshes[i];
					++refreshedThisFrame;
				}
				if (refreshedThisFrame > 0) phaseUsed[(int)f] = true;
				// Eight boxes over four phases: two per frame, never a pile-up.
				if (refreshedThisFrame > 8 / interval)
					fail("frame " + ofToString((int)f) + " refreshed " +
						ofToString(refreshedThisFrame) + " of 8 boxes - the "
						"preview cost is spiking instead of spreading");
			}
			for (int i = 0; i < 8; ++i)
				if (refreshes[i] != 1)
					fail("box " + ofToString(i) + " refreshed " +
						ofToString(refreshes[i]) + " times in " +
						ofToString(interval) + " frames, expected exactly 1");
			for (int f = 0; f < interval; ++f)
				if (!phaseUsed[f])
					fail("no box refreshes on phase " + ofToString(f) +
						" - the stagger is not covering every frame");

			// forceFullRate is what activeSequence uses; it must override.
			jp_renderschedule::apply(bx, {}, 1, true);
			for (int i = 0; i < 8; ++i)
				if (!bx[i]->shouldRenderThisFrame())
					fail("forceFullRate left box " + ofToString(i) + " idle");

			// A root index that does not exist must be ignored, not crash or
			// poison the pass - callers pass "nothing selected" sentinels.
			jp_renderschedule::apply(bx, {-1, 99}, 1, false);

			// A CYCLE must terminate. A box cannot feed itself any more, but
			// two boxes feeding each other is still reachable, and the walk
			// would recurse for ever without the visited check.
			bx[4]->fbohandlergroup.setFboPointer(&bx[5]->fbo, &bx[5]->name, 0);
			bx[5]->fbohandlergroup.setFboPointer(&bx[4]->fbo, &bx[4]->name, 0);
			jp_renderschedule::apply(bx, {4}, 1, false);
			if (!bx[4]->shouldRenderThisFrame() || !bx[5]->shouldRenderThisFrame())
				fail("a two-box cycle was not scheduled");
		}
		app.boxes.clear();
	}
	// Every box type must actually OBEY the schedule. The flag was computed for
	// all of them but honoured by only four types, so the boxes with live
	// sources - camera, depth camera - rendered a full frame regardless.
	//
	// Deliberately excluded: kinect2 and pointercloud. Both already skip
	// redundant work through their own frame/settings version checks, and
	// gating them would delay a near/far slider repaint by up to four frames
	// for about ten microseconds of saving.
	bool scheduleObeyed = true;
	{
		auto fail = [&](const string &why)
		{
			scheduleObeyed = false;
			ofLogNotice("scheduleobeyed") << why;
		};
		auto fill = [](ofFbo &target, const ofColor &colour)
		{
			target.begin();
			ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			ofClear(colour);
			target.end();
			ofEnableAlphaBlending();
		};
		const char *types[] = {"cam", "camdepth",
			"shaders/imageprocessing/transform.frag"};
		for (const char *type : types)
		{
			app.boxes.clear();
			app.boxes.addBox(type, 40, 40);
			if (app.boxes.boxes.empty())
			{
				ofLogNotice("scheduleobeyed")
					<< "could not build " << type << " - skipped";
				continue;
			}
			JPbox *box = app.boxes.boxes.front();
			box->setonoff(true);
			// Let it settle first, so what follows is not just a slow start.
			box->setRenderThisFrame(true);
			for (int i = 0; i < 6; ++i) box->update();
			if (!box->fbo.isAllocated())
			{
				ofLogNotice("scheduleobeyed")
					<< type << " has no FBO - skipped";
				continue;
			}
			const ofColor sentinel(17, 211, 89, 255);
			fill(box->fbo, sentinel);
			box->setRenderThisFrame(false);
			for (int i = 0; i < 6; ++i) box->update();
			ofPixels after;
			box->fbo.readToPixels(after);
			if (!after.isAllocated())
			{
				ofLogNotice("scheduleobeyed") << type << " unreadable - skipped";
				continue;
			}
			const ofColor held = after.getColor((int)box->fbo.getWidth() / 2,
				(int)box->fbo.getHeight() / 2);
			if (std::abs((int)held.r - (int)sentinel.r) > 4 ||
				std::abs((int)held.g - (int)sentinel.g) > 4 ||
				std::abs((int)held.b - (int)sentinel.b) > 4)
			{
				fail(string(type) + " rendered while the schedule said not to: "
					"expected " + ofToString(sentinel) + ", got " +
					ofToString(held));
			}
		}
		app.boxes.clear();
	}
	bool selfLink = true;
	{
		auto fail = [&](const string &why)
		{
			selfLink = false;
			ofLogNotice("selflink") << why;
		};
		const string shader = "shaders/imageprocessing/transform.frag";
		app.boxes.clear();
		app.boxes.addBox(shader, 40, 40);
		app.boxes.addBox(shader, 140, 40);
		if (app.boxes.boxes.size() < 2 ||
			app.boxes.boxes[0]->fbohandlergroup.getSize() < 1)
		{
			ofLogNotice("selflink") << "fixture unavailable - skipped";
		}
		else
		{
			JPbox *self = app.boxes.boxes[0];
			JPbox *other = app.boxes.boxes[1];

			if (self->fbohandlergroup.setFboPointer(&self->fbo, &self->name, 0))
				fail("setFboPointer accepted the box's own FBO");
			if (self->fbohandlergroup.getisPointerSet(0))
				fail("a self link was stored anyway");

			// The half that stops a guard from passing by refusing everything.
			if (!self->fbohandlergroup.setFboPointer(&other->fbo, &other->name, 0))
				fail("the guard refused a legitimate link between two boxes");
			if (self->fbohandlergroup.getFboPointerReference(0) != &other->fbo)
				fail("a legitimate link did not land on the source FBO");

			// A refused attempt must leave the existing patch alone - a guard
			// written as clear-then-check would drop the good connection.
			self->fbohandlergroup.setFboPointer(&self->fbo, &self->name, 0);
			if (self->fbohandlergroup.getFboPointerReference(0) != &other->fbo)
				fail("a refused self link clobbered the existing connection");

			if (self->fbohandlergroup.setFboPointer(&other->fbo, &other->name,
				self->fbohandlergroup.getSize() + 5))
			{
				fail("an out-of-range inlet index was accepted");
			}
		}
		app.boxes.clear();
	}
	bool camDepthRamp = true;
	{
		auto fail = [&](const string &why)
		{
			camDepthRamp = false;
			ofLogNotice("camdepthramp") << why;
		};
		const string frag = "shaders/private/camdepth_show.frag";
		ofShader show;
		if (!ofFile::doesFileExist(ofToDataPath(frag, true)))
			fail("missing shader: " + frag);
		else if (!show.load("shaders/default.vert", frag))
			fail("shader does not compile: " + frag);
		else
		{
			const int width = 256, height = 4;
			// A grey ramp standing in for a depth map, 0 (far) to 255 (near).
			ofFbo depth, coloured;
			depth.allocate(width, height);
			depth.begin();
			ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			ofClear(0, 0, 0, 255);
			ofSetRectMode(OF_RECTMODE_CORNER);
			for (int i = 0; i < width; ++i)
			{
				ofSetColor(i, i, i, 255);
				ofDrawRectangle(i, 0, 1, height);
			}
			depth.end();
			ofEnableAlphaBlending();

			coloured.allocate(width, height);
			ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(255, 255, 255, 255);
			coloured.begin();
			ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			show.begin();
			show.setUniformTexture("profundidad", depth.getTexture(), 1);
			show.setUniform2f("resolution", (float)width, (float)height);
			ofDrawRectangle(0, 0, (float)width, (float)height);
			show.end();
			coloured.end();
			ofEnableAlphaBlending();

			ofPixels out;
			coloured.readToPixels(out);
			if (!out.isAllocated()) fail("could not read the ramp");
			else
			{
				auto at = [&](int x) { return out.getColor(x, height / 2); };
				const ofColor nearest = at(252);   // depth ~1.0
				const ofColor yellow  = at(191);   // ~0.75
				const ofColor red     = at(127);   // ~0.5
				const ofColor violet  = at(63);    // ~0.25
				const ofColor farthest = at(2);    // ~0.0

				if (!(nearest.r > 230 && nearest.g > 230 && nearest.b > 220))
					fail("near end is not white: " + ofToString(nearest));
				if (!(yellow.r > 200 && yellow.g > 150 && yellow.b < 110))
					fail("three-quarter mark is not yellow: " + ofToString(yellow));
				if (!(red.r > 150 && red.g < 110 && red.b < 110))
					fail("middle is not red: " + ofToString(red));
				// The check a greyscale or turbo ramp cannot pass: at the far
				// end blue has to lead green.
				if (!(violet.b > violet.g + 30))
					fail("quarter mark is not violet - blue does not lead "
						"green: " + ofToString(violet));
				if (!(yellow.g > yellow.b + 40))
					fail("three-quarter mark has blue at or above green, so the "
						"ramp is running the wrong way: " + ofToString(yellow));
				if (!(farthest.r < 60 && farthest.g < 60))
					fail("far end is not dark: " + ofToString(farthest));

				// Luminance must fall from near to far, or a displacement
				// shader downstream would read the ramp as folded terrain.
				float previousLuma = 1e9f;
				for (int x = width - 4; x >= 2; x -= 8)
				{
					const ofColor c = at(x);
					const float l = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
					if (l > previousLuma + 6.0f)
					{
						fail("luminance rises toward the far end at x=" +
							ofToString(x) + " - the ramp is not monotonic");
						break;
					}
					previousLuma = l;
				}
			}
		}
	}
	bool camDepthParallax = true;
	{
		auto fail = [&](const string &why)
		{
			camDepthParallax = false;
			ofLogNotice("camdepthparallax") << why;
		};
		const string frag = "shaders/private/camdepth_motion.frag";
		ofShader motion;
		if (!ofFile::doesFileExist(ofToDataPath(frag, true)))
			fail("missing shader: " + frag);
		else if (!motion.load("shaders/default.vert", frag))
			fail("shader does not compile: " + frag);
		else
		{
			const int size = 64;
			// Two synthetic camera frames. The LEFT square jumps between them;
			// the RIGHT square is identical in both, which is what makes the
			// check discriminating rather than just "some output appeared".
			ofFbo frames[2];
			for (int f = 0; f < 2; ++f)
			{
				frames[f].allocate(size, size);
				frames[f].begin();
				ofEnableBlendMode(OF_BLENDMODE_DISABLED);
				ofClear(0, 0, 0, 255);
				ofSetRectMode(OF_RECTMODE_CORNER);
				ofSetColor(255);
				ofDrawRectangle(f == 0 ? 4 : 24, 8, 16, 48);   // moves
				ofDrawRectangle(40, 8, 16, 48);                // does not
				frames[f].end();
				ofEnableAlphaBlending();
			}

			ofFbo buffers[2];
			for (int i = 0; i < 2; ++i)
			{
				buffers[i].allocate(size, size, GL_RGBA16F);
				buffers[i].begin();
				ofEnableBlendMode(OF_BLENDMODE_DISABLED);
				ofClear(0, 0, 0, 255);
				buffers[i].end();
				ofEnableAlphaBlending();
			}

			auto pass = [&](int frame, int read, int write)
			{
				ofSetRectMode(OF_RECTMODE_CORNER);
				ofSetColor(255, 255, 255, 255);
				buffers[write].begin();
				ofEnableBlendMode(OF_BLENDMODE_DISABLED);
				motion.begin();
				motion.setUniformTexture("camara", frames[frame].getTexture(), 1);
				motion.setUniformTexture("anterior", buffers[read].getTexture(), 2);
				motion.setUniform2f("resolution", (float)size, (float)size);
				motion.setUniform1f("espejo", 0.0f);
				// Retention OFF for the test. With the hold engaged, the first
				// pass (which differences against an all-black buffer) would
				// light up BOTH squares and leak into the second, so the
				// static half could never read zero and the check would pass
				// no matter what the shader did.
				motion.setUniform1f("retencion", 0.0f);
				motion.setUniform1f("ganancia", 4.0f);
				ofDrawRectangle(0, 0, (float)size, (float)size);
				motion.end();
				buffers[write].end();
				ofEnableAlphaBlending();
			};
			pass(0, 0, 1);   // prime: remembers frame A
			pass(1, 1, 0);   // measure: frame B against frame A

			ofFloatPixels result;
			buffers[0].readToPixels(result);
			if (!result.isAllocated()) fail("could not read the motion buffer");
			else
			{
				// Centre of the square that jumped: white in A, black in B.
				const float moved = result.getColor(12, 32).g;
				// Centre of the square that never moved.
				const float still = result.getColor(48, 32).g;
				if (moved < 0.5f)
					fail("no energy where the image changed (" +
						ofToString(moved, 3) + ") - the cue is not measuring "
						"motion at all");
				if (still > 0.05f)
					fail("energy where NOTHING moved (" + ofToString(still, 3) +
						") - the cue is reporting brightness, not motion");
				if (moved <= still)
					fail("moved (" + ofToString(moved, 3) + ") is not above "
						"static (" + ofToString(still, 3) + ")");
			}
		}
	}
	bool camDepthBox = true;
	{
		auto fail = [&](const string &why)
		{
			camDepthBox = false;
			ofLogNotice("camdepth") << why;
		};
		const string frag = "shaders/private/camdepth.frag";
		if (!ofFile::doesFileExist(ofToDataPath(frag, true)))
			fail("missing shader: " + frag);
		else
		{
			ofShader probe;
			if (!probe.load("shaders/default.vert", frag))
				fail("shader does not compile: " + frag);
		}

		app.boxes.clear();
		app.boxes.addBox("camdepth", 40, 40);
		if (app.boxes.boxes.empty()) fail("addBox(\"camdepth\") built nothing");
		else
		{
			JPbox *box = app.boxes.boxes.front();
			if (dynamic_cast<JPbox_camdepth *>(box) == nullptr)
				fail("addBox(\"camdepth\") built the wrong type - the \"cam\" "
					"substring test is winning");
			if (box->getTipo() != JPbox::CAMDEPTHBOX)
				fail("tipo is " + ofToString(box->getTipo()) + ", expected " +
					ofToString((int)JPbox::CAMDEPTHBOX));
			// The auto-namer has the same substring hazard.
			if (box->name.find("CAMARITA") != string::npos)
				fail("named CAMARITA - makeNameFromDirectory matched \"cam\" first");

			// Every control the shader reads must exist, or updateFBO silently
			// falls back to a default and the slider does nothing.
			const char *needed[] = {"camaraindex", "peso foco", "peso brillo",
				"peso vertical", "radio", "contraste", "cerca", "lejos",
				"suavizado", "bordes", "curva", "invertir", "piso abajo",
				"espejo", "peso paralaje", "peso aire", "retencion",
				"ganancia mov", "colores"};
			for (const char *n : needed)
				if (box->parameters.indexOfName(n) < 0)
					fail(string("missing parameter: ") + n);

			// Values must round-trip like any other box.
			const int focus = box->parameters.indexOfName("peso foco");
			if (focus >= 0)
			{
				box->parameters.setFloatValue(0.23f, focus);
				box->parameters.setFloatLerpValue(0.23f, focus);
				const string path = directory + "camdepth.xml";
				app.boxes.save(path);
				app.boxes.clear();
				app.boxes.load(path);
				if (app.boxes.boxes.empty()) fail("round trip lost the box");
				else
				{
					JPbox *back = app.boxes.boxes.front();
					if (dynamic_cast<JPbox_camdepth *>(back) == nullptr)
						fail("round trip rebuilt the wrong type");
					const int idx = back->parameters.indexOfName("peso foco");
					if (idx < 0 || std::abs(
						back->parameters.getFloatValue(idx) - 0.23f) > 0.002f)
						fail("parameter did not survive save/load");
				}
			}
		}
		// A plain camera box must still be a plain camera box.
		app.boxes.clear();
		app.boxes.addBox("cam", 40, 40);
		if (!app.boxes.boxes.empty() &&
			dynamic_cast<JPbox_camdepth *>(app.boxes.boxes.front()) != nullptr)
			fail("addBox(\"cam\") built a camdepth box");
		app.boxes.clear();
	}
	bool mediaSmoke = true;
	if(const char *smokePath=std::getenv("GUIPPER_MEDIA_SMOKE"))
	{
		app.boxes.clear();app.boxes.addBox(smokePath,120,180);
		auto *target=app.boxes.boxes.empty()?nullptr:dynamic_cast<JPMediaInspectable*>(app.boxes.boxes.front());
		for(int i=0;target&&i<300;++i)
		{
			app.boxes.boxes.front()->setonoff(true);app.boxes.boxes.front()->update();
			if(target->mediaReady() && (!jp_media::isVideo(smokePath) ||
				(target->mediaDurationSeconds()>0.0 && target->mediaFrameCount()>1)))break;
			ofSleepMillis(10);
		}
		mediaSmoke=target&&target->mediaReady()&&
			(!jp_media::isGif(smokePath)||target->mediaPlayable())&&
			(!jp_media::isVideo(smokePath)||(target->mediaState().muted&&
				target->mediaDurationSeconds()>0.0&&target->mediaFrameCount()>1));
	}
	ofLogNotice("jp_persistence_test") << "current=" << current
		<< " legacy=" << old << " invalid=" << clamped
		<< " shaderReload=" << shaderReload
		<< " modeMemory=" << modeMemory
		<< " rangeCapture=" << rangeCapture
		<< " midiRange=" << midiRange
		<< " cueState=" << cueState
		<< " lockDefault=" << lockDefault
		<< " mediaState=" << mediaState << " mediaBoundary=" << mediaBoundary
		<< " mediaAlpha=" << mediaAlpha << " mediaMotionClear=" << mediaMotionClear
		<< " mediaStraightMix=" << mediaStraightMix
		<< " mediaSingleComposite=" << mediaSingleComposite
		<< " mediaPausePreserves=" << mediaPausePreserves
		<< " mediaTransforms=" << mediaTransforms
		<< " mediaSkipsStatic=" << mediaSkipsStatic
		<< " mediaTurnaround=" << mediaTurnaround
		<< " mediaMidiIndex=" << mediaMidiIndex
		<< " camScaleRatio=" << camScaleRatio
		<< " camLegacyLoad=" << camLegacyLoad
		<< " shaderScaleRatio=" << shaderScaleRatio
		<< " realCompoLoad=" << realCompoLoad
		<< " boxIdentity=" << boxIdentity
		<< " outputBinding=" << outputBinding
		<< " boxHitboxes=" << boxHitboxes
		<< " groupComposite=" << groupComposite
		<< " transitionClock=" << transitionClock
		<< " paramMorph=" << paramMorph
		<< " morphArming=" << morphArming
		<< " transitionShaders=" << transitionShaders
		<< " multiSelect=" << multiSelect
		<< " spacePan=" << spacePan
		<< " groupPathAfterClear=" << groupPathAfterClear
		<< " tooltipLayout=" << tooltipLayout
		<< " tooltipTransform=" << tooltipTransform
		<< " renderSchedule=" << renderSchedule
		<< " scheduleObeyed=" << scheduleObeyed
		<< " selfLink=" << selfLink
		<< " camDepthRamp=" << camDepthRamp
		<< " camDepthParallax=" << camDepthParallax
		<< " camDepthBox=" << camDepthBox
		<< " saveKeepsDefault=" << saveKeepsDefaultCompo
		<< " mediaSmoke=" << mediaSmoke;
	return current && old && clamped && shaderReload && modeMemory &&
		rangeCapture && midiRange && cueState && lockDefault && mediaState &&
		mediaBoundary && mediaAlpha && mediaMotionClear && mediaStraightMix &&
		mediaSingleComposite && mediaPausePreserves && mediaTransforms &&
		mediaSkipsStatic && mediaTurnaround && mediaMidiIndex && camScaleRatio && camLegacyLoad && shaderScaleRatio && realCompoLoad &&
		saveKeepsDefaultCompo && boxIdentity && outputBinding && boxHitboxes && groupComposite && transitionClock &&
		paramMorph && morphArming &&
		transitionShaders && camDepthBox && camDepthParallax && camDepthRamp && selfLink &&
		renderSchedule && scheduleObeyed && tooltipLayout &&
		tooltipTransform &&
		spacePan && groupPathAfterClear && multiSelect && mediaSmoke;
}
