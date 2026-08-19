#include "jp_persistence_test.h"

#include "../ofApp.h"
#include "jp_audio.h"
#include "../JPbox/jp_box_image.h"
#include "../JPbox/jp_box_video.h"
#include "../JPbox/jp_media.h"
#include "../JPbox/jp_box_paint.h"

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
	// The advanced debug panel's data.
	//
	// The estimate and the formatting are checked directly: a wrong multiplier
	// turns the VRAM figure into a confidently-wrong number, which is worse than
	// showing none. The gathering is checked against the real graph, because the
	// whole point of the panel is telling you what is actually open.
	bool debugReport = true;
	{
		auto fail = [&](const string &why)
		{
			debugReport = false;
			ofLogNotice("debugreport") << why;
		};

		// RGBA8: four bytes a pixel, times the count.
		if (jp_debug::fboBytes(1920, 1080, 1) != 1920ull * 1080ull * 4ull)
			fail("one 1920x1080 RGBA8 target is not w*h*4");
		if (jp_debug::fboBytes(1920, 1080, 10) !=
			jp_debug::fboBytes(1920, 1080, 1) * 10ull)
			fail("the estimate does not scale with the count");
		// Degenerate inputs must give zero, not a huge or negative number.
		for (auto wh : {std::make_pair(0, 1080), std::make_pair(1920, 0)})
			if (jp_debug::fboBytes(wh.first, wh.second, 4) != 0ull)
				fail("a zero dimension did not give zero bytes");
		if (jp_debug::fboBytes(1920, 1080, 0) != 0ull)
			fail("a zero count did not give zero bytes");

		if (jp_debug::formatBytes(512ull) != "512 B")
			fail("small sizes are not printed in bytes: " +
				jp_debug::formatBytes(512ull));
		if (jp_debug::formatBytes(1024ull) != "1.0 KB")
			fail("1024 did not become 1.0 KB: " + jp_debug::formatBytes(1024ull));
		// Binary units, matching what GPU tools report.
		if (jp_debug::formatBytes(1024ull * 1024ull) != "1.0 MB")
			fail("a mebibyte did not become 1.0 MB: " +
				jp_debug::formatBytes(1024ull * 1024ull));
		if (jp_debug::formatBytes(1024ull * 1024ull * 1024ull) != "1.0 GB")
			fail("a gibibyte did not become 1.0 GB");

		// The lines both views share. Checking the CONTENT here covers the plain
		// readout and the advanced panel at once, which is the point of having
		// one builder: the advanced panel is a superset, and two copies of this
		// text would drift the moment either gained a field.
		{
			const vector<string> lines = app.buildDebugLines();
			if (lines.size() < 12)
				fail("only " + ofToString((int)lines.size()) + " debug lines - "
					"the shared builder lost fields");

			// Sections are what make the panel readable, so at least the four
			// headings must be there. The plain view ignores them; the panel
			// cannot group without them.
			const vector<ofApp::DebugRow> rows = app.buildDebugRows();
			int headings = 0;
			for (const ofApp::DebugRow &r : rows)
				if (!r.section.empty()) ++headings;
			if (headings < 4)
				fail("only " + ofToString(headings) + " sections - the panel "
					"falls back to one flat list, which is what it was fixed for");
			// Every row must be readable on its own: a label, or a label with a
			// value. A value with no label is an orphan number on screen.
			for (const ofApp::DebugRow &r : rows)
				if (r.label.empty())
					fail("a debug row has a value with no label: '" + r.value + "'");
			// The labels the panel groups under headings. Renaming one is fine;
			// LOSING one is what this catches, and it catches it for both views
			// at once because they share the builder.
			const char *required[] = {"GPU", "render size", "compo", "boxes",
				"active render", "active sequence", "transition", "update",
				"draw", "fps", "parameters", "main graph", "media passes"};
			for (const char *needle : required)
			{
				bool found = false;
				for (const string &line : lines)
				{
					if (line.find(needle) != string::npos) { found = true; break; }
				}
				if (!found)
					fail(string("the debug readout no longer reports '") +
						needle + "' - both views lost it at once");
			}
		}

		// Two-column balancing. The bug it prevents: the panel was one column,
		// grew taller than the window, and clipped its last rows off the bottom
		// edge - invisible until you looked for the row that was missing.
		{
			// Even sections split down the middle.
			if (jp_debug::balanceSplit({100.0f, 100.0f, 100.0f, 100.0f}) != 2)
				fail("four equal sections did not split in half, got " +
					ofToString((int)jp_debug::balanceSplit(
						{100.0f, 100.0f, 100.0f, 100.0f})));

			// The real shape that overflowed: 108,108,108,144,126,72. Breaking at
			// the first section past halfway gave 468 vs 198; balanced is 324/342.
			const vector<float> real = {108.0f, 108.0f, 108.0f, 144.0f, 126.0f, 72.0f};
			const size_t split = jp_debug::balanceSplit(real);
			float left = 0.0f, right = 0.0f;
			for (size_t i = 0; i < real.size(); ++i)
				(i < split ? left : right) += real[i];
			if (std::abs(left - right) > 60.0f)
				fail("the columns came out " + ofToString(left) + " vs " +
					ofToString(right) + " - unbalanced enough that the panel is "
					"as tall as one column would have been");
			// And the point of the whole exercise: the tallest column must be
			// meaningfully shorter than the single-column total.
			float total = 0.0f;
			for (float h : real) total += h;
			if (std::max(left, right) > total * 0.62f)
				fail("the tallest column is " + ofToString(std::max(left, right)) +
					" of " + ofToString(total) + " - two columns bought almost "
					"nothing, so the panel will still overflow");

			// Never zero: an empty left column would draw the whole readout in
			// the right half.
			if (jp_debug::balanceSplit({50.0f}) != 1)
				fail("a single section did not report split 1");
			if (jp_debug::balanceSplit({}) != 1)
				fail("an empty list did not report split 1");
			if (jp_debug::balanceSplit({10.0f, 500.0f}) < 1)
				fail("a lopsided pair produced an empty left column");
		}

		// --- gathered against the real graph ---
		const string shader = "shaders/imageprocessing/transform.frag";
		app.boxes.clear();
		{
			const jp_debug::Report empty = app.buildDebugReport();
			if (empty.boxCount != 0)
				fail("an empty graph reported " + ofToString(empty.boxCount) +
					" boxes");
			if (empty.fboBytes != 0ull)
				fail("an empty graph reported VRAM in use");
		}
		app.boxes.addBox(shader, 100.0f, 100.0f);
		app.boxes.addBox(shader, 200.0f, 100.0f);
		{
			const jp_debug::Report two = app.buildDebugReport();
			if (two.boxCount != 2)
				fail("two boxes reported as " + ofToString(two.boxCount));
			if (two.fboCount != 2)
				fail("two allocated FBOs reported as " +
					ofToString(two.fboCount) + " - the count walks allocated "
					"targets, not boxes, so a mismatch means one never allocated");
			// The figure must follow the render size, not a hardcoded one.
			if (two.renderWidth != jp_constants::renderWidth ||
				two.renderHeight != jp_constants::renderHeight)
			{
				fail("the report's render size does not match jp_constants");
			}
			if (two.fboBytes != jp_debug::fboBytes(two.renderWidth,
				two.renderHeight, two.fboCount))
			{
				fail("the reported VRAM is not the estimate of its own counts");
			}
		}
		app.boxes.clear();
	}
	// Colour swatch for RGB parameter triples.
	//
	// The channel is DECLARED in the shader (`// @color r`), never inferred from
	// declaration order. That is the point: mixonmix.frag declares mix_r, mix_b,
	// mix_g in that order, so position lies about the channel, and
	// faser/faseg/faseb in six other shaders are per-channel PHASE offsets, not
	// a colour at all. A name-or-position heuristic gets both wrong.
	bool colorSwatch = true;
	{
		auto fail = [&](const string &why)
		{
			colorSwatch = false;
			ofLogNotice("colorswatch") << why;
		};

		// --- the annotation parser ---
		int channel = -1;
		string group = "unset";
		if (!JPParameter::parseColorAnnotation(
			"uniform float chromared; // @color r", channel, group))
			fail("a well-formed annotation was not recognised");
		if (channel != JPParameter::COLOR_R) fail("channel r did not parse");
		if (!group.empty()) fail("a group appeared where none was written");

		if (!JPParameter::parseColorAnnotation(
			"uniform float blue1; // @color b sun", channel, group))
			fail("an annotation with a group was not recognised");
		if (channel != JPParameter::COLOR_B) fail("channel b did not parse");
		if (group != "sun") fail("the group name came out as '" + group + "'");

		// A plain uniform must stay untouched, or every slider becomes a colour.
		channel = -99; group = "untouched";
		if (JPParameter::parseColorAnnotation("uniform float umbral;", channel, group))
			fail("a line with no annotation was treated as a colour");
		if (channel != -99 || group != "untouched")
			fail("a non-annotated line wrote to its outputs anyway");

		// An unknown channel letter is a typo, not a colour.
		if (JPParameter::parseColorAnnotation("uniform float x; // @color q",
			channel, group))
			fail("an unknown channel letter was accepted");

		// --- the colour itself ---
		auto close = [](float a, float b) { return std::abs(a - b) <= 0.002f; };
		const ofFloatColor black = JPParameter::swatchColor(0.0f, 0.0f, 0.0f);
		if (!close(black.r, 0.0f) || !close(black.g, 0.0f) || !close(black.b, 0.0f))
			fail("0,0,0 did not come out black");
		const ofFloatColor red = JPParameter::swatchColor(1.0f, 0.0f, 0.0f);
		if (!close(red.r, 1.0f) || !close(red.g, 0.0f) || !close(red.b, 0.0f))
			fail("1,0,0 did not come out red");
		// A user-enabled custom range reaches past 1.0; unclamped it wraps.
		const ofFloatColor over = JPParameter::swatchColor(4.0f, -2.0f, 0.5f);
		if (!close(over.r, 1.0f) || !close(over.g, 0.0f) || !close(over.b, 0.5f))
			fail("an out-of-range value was not clamped, got " +
				ofToString(over.r) + "," + ofToString(over.g) + "," +
				ofToString(over.b));

		// --- end to end, one row per annotated shader ---
		//
		// A table because the shaders are being annotated a couple at a time:
		// adding a box to the feature is adding a row here, and every one added
		// so far stays checked. `plain` is a slider in the same shader that must
		// NOT be claimed as a colour.
		struct ShaderColorCase
		{
			const char *path;
			const char *r;
			const char *g;
			const char *b;
			const char *plain;
		};
		const ShaderColorCase cases[] = {
			{"shaders/blending/chromadist.frag",
				"chromared", "chromagreen", "chromablue", "umbral"},
			{"shaders/generative/solidcolor.frag",
				"r", "g", "b", "mivariable"},
			{"shaders/imageprocessing/chromakey.frag",
				"chroma_red", "chroma_green", "chroma_blue", "threshold"},
			// Declared r, B, G in the file. If the channel were taken from
			// position, green and blue would come out swapped - this row is the
			// whole reason the annotation names the channel.
			// mixonmix has no plain float at all - only the three channels.
			{"shaders/blending/mixonmix.frag",
				"mix_r", "mix_g", "mix_b", nullptr},
		};

		for (const ShaderColorCase &c : cases)
		{
			app.boxes.clear();
			app.boxes.addBox(c.path, 100.0f, 100.0f);
			if (app.boxes.boxes.empty())
			{
				ofLogNotice("colorswatch") << c.path << " unavailable - skipped";
				continue;
			}
			const string where = string(c.path) + ": ";
			JPParameterGroup &params = app.boxes.boxes.front()->parameters;

			auto channelOf = [&](const char *name) {
				const int idx = params.indexOfName(name);
				if (idx < 0) return -1;
				JPParameter *p = params.getJParameter(idx);
				return p == nullptr ? -1 : p->colorChannel;
			};
			auto checkChannel = [&](const char *name, int expected, const char *label) {
				const int actual = channelOf(name);
				if (actual < 0)
					fail(where + name + " is not a parameter of this shader");
				else if (actual != expected)
					fail(where + name + " is not channel " + label + " (" +
						ofToString(actual) + ")");
			};
			checkChannel(c.r, JPParameter::COLOR_R, "R");
			checkChannel(c.g, JPParameter::COLOR_G, "G");
			checkChannel(c.b, JPParameter::COLOR_B, "B");
			// nullptr means the shader has no non-colour float to check.
			if (c.plain != nullptr)
			{
				const int plainChannel = channelOf(c.plain);
				if (plainChannel < 0)
					fail(where + c.plain + " is not a parameter of this shader "
						"- the row names a slider that does not exist");
				else if (plainChannel != JPParameter::COLOR_NONE)
					fail(where + c.plain + " was claimed as a colour channel");
			}

			app.boxes.openguinumber = 0;
			app.boxes.setControllers();
			if (app.boxes.inspectorColorSwatches.size() != 1)
			{
				fail(where + "expected exactly one swatch, got " +
					ofToString((int)app.boxes.inspectorColorSwatches.size()) +
					" - one per channel would mean the group is not being "
					"closed on its last member");
				continue;
			}
			const JPboxgroup::InspectorColorSwatch &sw =
				app.boxes.inspectorColorSwatches.front();
			if (sw.r == nullptr || sw.g == nullptr || sw.b == nullptr)
			{
				fail(where + "the swatch did not resolve all three channels");
				continue;
			}
			// Bound by ANNOTATION, not by the order the uniforms appear in.
			if (sw.r->name != c.r || sw.g->name != c.g || sw.b->name != c.b)
			{
				fail(where + "the swatch bound " + sw.r->name + "/" +
					sw.g->name + "/" + sw.b->name + ", expected " + c.r + "/" +
					c.g + "/" + c.b);
			}
			if (sw.bounds.width <= 0.0f || sw.bounds.height <= 0.0f)
				fail(where + "the swatch has no area");
			// Thinner than a parameter row, and still wide enough to read the
			// colour across. A swatch that grew back to row height would crowd
			// the sliders it describes.
			if (sw.bounds.height >= 24.0f)
				fail(where + "the swatch is " + ofToString(sw.bounds.height) +
					"px tall, which is row height again rather than a strip");
			if (sw.bounds.width < 100.0f)
				fail(where + "the swatch is only " + ofToString(sw.bounds.width) +
					"px wide");
		}
		app.boxes.clear();

		// An annotation must not eat a uniform's DEFAULT VALUE.
		//
		// The scraper reads `uniform float x = 1.0;` by counting whitespace
		// tokens on the line, so appending a comment changes the count. invert
		// is the only annotated shader that declares defaults, which makes it
		// the canary for the whole convention.
		{
			app.boxes.clear();
			app.boxes.addBox("shaders/imageprocessing/invert.frag", 100.0f, 100.0f);
			if (app.boxes.boxes.empty())
			{
				ofLogNotice("colorswatch") << "invert unavailable - skipped";
			}
			else
			{
				JPParameterGroup &params = app.boxes.boxes.front()->parameters;
				for (const char *name : {"mixr", "mixg", "mixb"})
				{
					const int idx = params.indexOfName(name);
					if (idx < 0) { fail(string("invert: missing ") + name); continue; }
					JPParameter *p = params.getJParameter(idx);
					if (p == nullptr) continue;
					if (std::abs(p->floatValue - 1.0f) > 0.001f)
						fail(string("invert: ") + name + " starts at " +
							ofToString(p->floatValue) + ", not the 1.0 its "
							"declaration gives it - the @color comment broke the "
							"default-value parsing");
				}
			}
			app.boxes.clear();
		}

		// --- consistency sweep over every annotated shader ---
		//
		// The table above proves the pipeline on a handful of boxes. This walks
		// the whole shader tree instead, so a typo in any of the ~40 annotated
		// files is caught: an unknown channel letter, a duplicated channel, or a
		// group left with only two of its three channels. Reading the text is
		// enough - building 40 boxes would mean 40 shader compiles.
		{
			ofDirectory shaderRoot(ofToDataPath("shaders", true));
			shaderRoot.listDir();
			vector<string> pending;
			pending.push_back(shaderRoot.getAbsolutePath());
			int annotatedFiles = 0;
			int groupsChecked = 0;
			while (!pending.empty())
			{
				ofDirectory dir(pending.back());
				pending.pop_back();
				dir.listDir();
				for (int i = 0; i < (int)dir.size(); ++i)
				{
					const string path = dir.getPath(i);
					if (ofDirectory::doesDirectoryExist(path))
					{
						pending.push_back(path);
						continue;
					}
					if (ofFilePath::getFileExt(path) != "frag") continue;

					ofBuffer buffer = ofBufferFromFile(path);
					const string shown = ofFilePath::getFileName(path);
					// group -> which channels were seen, and how many times
					std::map<string, std::map<int, int>> seen;
					bool any = false;
					for (const auto &line : buffer.getLines())
					{
						const string text = line;
						if (text.find("@color") == string::npos) continue;
						int channel = JPParameter::COLOR_NONE;
						string group;
						if (!JPParameter::parseColorAnnotation(text, channel, group))
						{
							fail(shown + ": an @color line did not parse, so its "
								"channel is silently dropped: " + text);
							continue;
						}
						any = true;
						seen[group][channel]++;
					}
					if (!any) continue;
					++annotatedFiles;
					for (const auto &entry : seen)
					{
						++groupsChecked;
						const string where = shown + " group '" + entry.first + "'";
						if (entry.second.size() != 3)
							fail(where + " has " +
								ofToString((int)entry.second.size()) +
								" channels, expected 3 - an incomplete group "
								"never draws a swatch and looks like the feature "
								"is broken");
						for (int channel : {JPParameter::COLOR_R,
							JPParameter::COLOR_G, JPParameter::COLOR_B})
						{
							auto it = entry.second.find(channel);
							if (it == entry.second.end())
								fail(where + " is missing a channel");
							else if (it->second != 1)
								fail(where + " declares a channel " +
									ofToString(it->second) + " times");
						}
					}
				}
			}
			if (annotatedFiles < 30)
				fail("only " + ofToString(annotatedFiles) + " annotated shaders "
					"were found - the sweep is not reaching the tree");
			ofLogNotice("colorswatch") << "sweep: " << annotatedFiles
				<< " shaders, " << groupsChecked << " colour groups";
		}
	}
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
	// The paint box is the only box whose CONTENTS live in the savefile rather
	// than in a file it points at, so a persistence bug here loses the user's
	// drawing outright. Every other box would come back merely misconfigured.
	bool paintBox = true;
	{
		auto fail = [&](const string &why)
		{
			paintBox = false;
			ofLogNotice("paint") << why;
		};

		app.boxes.clear();
		app.boxes.addBox("paint", 40, 40);
		if (app.boxes.boxes.empty()) fail("addBox(\"paint\") built nothing");
		else
		{
			JPbox_paint *box = dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
			if (box == nullptr) fail("addBox(\"paint\") built the wrong type");
			else
			{
				if (box->getTipo() != JPbox::PAINTBOX)
					fail("tipo is " + ofToString(box->getTipo()) + ", expected " +
						ofToString((int)JPbox::PAINTBOX));
				if (box->name.find("PAINT") == string::npos)
					fail("makeNameFromDirectory did not name it PAINT");

				const char *needed[] = {"opacity", "playhead", "scrub"};
				for (const char *n : needed)
					if (box->parameters.indexOfName(n) < 0)
						fail(string("missing parameter: ") + n);

				// A canvas is the render size, so the inspector must not offer
				// it a fit mode - the row is laid out unconditionally otherwise.
				auto *media = dynamic_cast<JPMediaInspectable *>(box);
				if (media == nullptr) fail("not JPMediaInspectable - no transport UI");
				else if (media->mediaHasFit()) fail("reports a fit mode");

				// One inlet, and it is the tracing reference.
				if (box->fbohandlergroup.getSize() != 1)
					fail("expected exactly one inlet, got " +
						ofToString(box->fbohandlergroup.getSize()));
				else if (box->fbohandlergroup.getName(0) != "reference")
					fail("inlet is named " + box->fbohandlergroup.getName(0));

				// Build a two cel drawing: a stroke on each, a hold, a colour,
				// an eraser and an off-canvas point.
				JPPaintStroke first;
				first.r = 1.0f; first.g = 0.25f; first.b = 0.0f; first.a = 0.75f;
				first.size = 0.031f;
				for (int i = 0; i < 40; ++i)
					first.points.push_back({0.1f + i * 0.02f, 0.5f, 1.0f});
				box->commitStroke(first);

				box->addCel(false);
				JPPaintStroke second;
				second.erase = true;
				// Deliberately outside 0..1: the encoded range overshoots the
				// canvas so an overhanging stroke is not flattened onto the edge.
				second.points.push_back({-0.2f, 0.5f, 0.5f});
				second.points.push_back({1.2f, 0.5f, 1.5f});
				box->commitStroke(second);
				box->setCelHold(1, 4);
				box->document().fps = 24.0f;
				box->document().onionBefore = 2;
				box->mediaState().loopMode = JPMediaLoopMode::PingPong;

				if (box->document().frames.size() != 2)
					fail("addCel did not produce a second cel");
				if (jp_paint::tickCount(box->document()) != 5)
					fail("tickCount is " +
						ofToString(jp_paint::tickCount(box->document())) +
						", expected 5 with a hold of 4");

				const string path = directory + "paint.xml";
				app.boxes.save(path);
				app.boxes.clear();
				app.boxes.load(path);
				JPbox_paint *back = app.boxes.boxes.empty() ? nullptr :
					dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
				if (back == nullptr) fail("round trip lost the box or its type");
				else
				{
					const JPPaintDocument &d = back->document();
					if (d.frames.size() != 2) fail("round trip lost a cel");
					else
					{
						if (d.frames[1].hold != 4) fail("round trip lost the hold");
						if (d.frames[0].layers[0].strokes.size() != 1 ||
							d.frames[1].layers[0].strokes.size() != 1)
							fail("round trip lost a stroke");
						else
						{
							const JPPaintStroke &a = d.frames[0].layers[0].strokes[0];
							if (a.points.size() != first.points.size())
								fail("round trip changed the point count");
							if (std::abs(a.r - 1.0f) > 0.01f ||
								std::abs(a.g - 0.25f) > 0.01f ||
								std::abs(a.a - 0.75f) > 0.01f)
								fail("round trip lost the stroke colour");
							if (std::abs(a.size - 0.031f) > 0.0005f)
								fail("round trip lost the stroke size");
							if (!a.points.empty() &&
								std::abs(a.points[0].x - 0.1f) > 0.001f)
								fail("round trip moved a point");

							const JPPaintStroke &b = d.frames[1].layers[0].strokes[0];
							if (!b.erase) fail("round trip lost the eraser flag");
							if (b.points.size() == 2 &&
								(b.points[0].x > -0.19f || b.points[1].x < 1.19f))
								fail("round trip clamped an off-canvas point");
							if (b.points.size() == 2 &&
								std::abs(b.points[1].width - 1.5f) > 0.02f)
								fail("round trip lost a width multiplier above 1");
						}
						// Ids have to clear everything the file contained or two
						// cels could share a raster cache slot.
						int highest = 0;
						for (const JPPaintFrame &f : d.frames)
							highest = std::max(highest, f.id);
						if (d.nextFrameId <= highest)
							fail("nextFrameId did not clear the loaded ids");
					}
					if (std::abs(d.fps - 24.0f) > 0.01f) fail("round trip lost fps");
					if (d.onionBefore != 2) fail("round trip lost the onion range");
					if (back->mediaState().loopMode != JPMediaLoopMode::PingPong)
						fail("round trip lost the loop mode");
					// Undo describes a document that no longer exists.
					if (back->canUndo()) fail("load left a stale undo history");

					// Duplication, grouping and cue staging all go through this.
					app.boxes.addBox("paint", 200, 40);
					JPbox_paint *copy = app.boxes.boxes.size() < 2 ? nullptr :
						dynamic_cast<JPbox_paint *>(app.boxes.boxes[1]);
					if (copy == nullptr) fail("could not build a second paint box");
					else
					{
						copy->copyCustomStateFrom(back);
						if (copy->document().frames.size() != 2 ||
							copy->document().frames[0].layers[0].strokes.size() != 1)
							fail("copyCustomStateFrom did not carry the drawing");
						if (copy->canUndo())
							fail("copyCustomStateFrom inherited an undo history");
					}
				}

				// Undo has to reach across cel structure, not just strokes.
				app.boxes.clear();
				app.boxes.addBox("paint", 40, 40);
				JPbox_paint *fresh = app.boxes.boxes.empty() ? nullptr :
					dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
				if (fresh != nullptr)
				{
					JPPaintStroke mark;
					mark.points.push_back({0.5f, 0.5f, 1.0f});
					fresh->commitStroke(mark);
					fresh->addCel(true);
					if (fresh->document().frames.size() != 2)
						fail("addCel(duplicate) did not add a cel");
					else if (fresh->document().frames[1].layers[0].strokes.size() != 1)
						fail("addCel(duplicate) did not copy the strokes");
					if (!fresh->undo()) fail("undo refused the cel add");
					else if (fresh->document().frames.size() != 1)
						fail("undo did not remove the added cel");
					if (!fresh->undo()) fail("undo refused the stroke");
					else if (!fresh->document().frames[0].layers[0].strokes.empty())
						fail("undo did not remove the stroke");
					if (fresh->undo()) fail("undo ran past the start of history");
					if (!fresh->redo() || !fresh->redo())
						fail("redo did not replay both edits");
					else if (fresh->document().frames.size() != 2)
						fail("redo did not restore the cel");
					// The last cel is load bearing: the editor assumes one exists.
					app.boxes.clear();
					app.boxes.addBox("paint", 40, 40);
					JPbox_paint *single = dynamic_cast<JPbox_paint *>(
						app.boxes.boxes.front());
					single->deleteCel(0);
					if (single->document().frames.size() != 1)
						fail("deleted the only cel");
				}
			}
		}
		// Wiring the reference inlet used to take the program down: the panel
		// asked JPFbohandlerGroup::getFboPointer for the producer's framebuffer,
		// which returns an ofFbo BY VALUE, and kept a pointer into that
		// temporary copy. The copy died at the end of the statement - and its
		// destructor released GL handles the producing box was still using.
		{
			app.boxes.clear();
			app.boxes.addBox("paint", 40, 40);
			app.boxes.addBox("paint", 240, 40);
			if (app.boxes.boxes.size() < 2) fail("could not build two paint boxes");
			else
			{
				JPbox *producer = app.boxes.boxes[0];
				JPbox_paint *consumer =
					dynamic_cast<JPbox_paint *>(app.boxes.boxes[1]);
				if (consumer == nullptr) fail("second box is not a paint box");
				else
				{
					if (consumer->referenceFbo() != nullptr)
						fail("an unwired reference inlet resolved to something");
					consumer->fbohandlergroup.setFboPointer(
						&producer->fbo, &producer->name, 0);
					ofFbo *resolved = consumer->referenceFbo();
					if (resolved == nullptr)
						fail("a wired reference inlet resolved to nothing");
					// THE regression: it has to be the producer's own
					// framebuffer, not the address of a temporary copy of it.
					else if (resolved != &producer->fbo)
						fail("reference inlet handed back a copy, not the producer");
					// The path that crashed, end to end.
					consumer->setonoff(true);
					consumer->update();
					if (!producer->fbo.isAllocated())
						fail("the producer's framebuffer was released by the lookup");
					consumer->fbohandlergroup.deleteFboPointer(0);
					if (consumer->referenceFbo() != nullptr)
						fail("unlinking left the reference inlet resolving");
				}
			}
			app.boxes.clear();
		}

		// The eyedropper. Worth its own case because it depends on two things
		// this box has already got wrong once: the readback's row order, and the
		// fact that cels are stored PREMULTIPLIED. Miss the divide and every
		// sampled translucent colour comes back darkened.
		{
			app.boxes.clear();
			app.boxes.addBox("paint", 40, 40);
			JPbox_paint *box = dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
			if (box == nullptr) fail("could not build a paint box to sample");
			else
			{
				box->document().bgR = 0.25f; box->document().bgG = 0.5f;
				box->document().bgB = 0.75f; box->document().bgA = 1.0f;
				auto block = [&](float r, float g, float b, float a,
					float x0, float x1)
				{
					JPPaintStroke st;
					st.r = r; st.g = g; st.b = b; st.a = a;
					st.tool = (int)JPPaintTool::Lasso;
					// A rectangle in the TOP half, so a flipped row order would
					// sample the background instead and be caught.
					st.points.push_back(JPPaintPoint{x0, 0.05f, 1.0f});
					st.points.push_back(JPPaintPoint{x1, 0.05f, 1.0f});
					st.points.push_back(JPPaintPoint{x1, 0.40f, 1.0f});
					st.points.push_back(JPPaintPoint{x0, 0.40f, 1.0f});
					st.points.push_back(st.points.front());
					box->commitStroke(st);
				};
				block(1.0f, 0.4f, 0.0f, 1.0f, 0.05f, 0.45f);   // opaque
				block(0.0f, 0.8f, 1.0f, 0.5f, 0.55f, 0.95f);   // half alpha

				ofFloatColor sampled;
				if (!box->sampleColor(0.25f, 0.22f, sampled))
					fail("sampling an opaque mark failed");
				else if (std::abs(sampled.r - 1.0f) > 0.02f ||
					std::abs(sampled.g - 0.4f) > 0.02f ||
					std::abs(sampled.b - 0.0f) > 0.02f ||
					std::abs(sampled.a - 1.0f) > 0.02f)
					fail("an opaque mark sampled as " + ofToString(sampled.r, 2) +
						"," + ofToString(sampled.g, 2) + "," +
						ofToString(sampled.b, 2) + "," + ofToString(sampled.a, 2));

				// THE regression: rgb must come back at full strength, not scaled
				// by alpha.
				if (!box->sampleColor(0.75f, 0.22f, sampled))
					fail("sampling a translucent mark failed");
				else
				{
					if (std::abs(sampled.a - 0.5f) > 0.03f)
						fail("a half alpha mark sampled alpha " +
							ofToString(sampled.a, 3));
					if (std::abs(sampled.g - 0.8f) > 0.03f ||
						std::abs(sampled.b - 1.0f) > 0.03f)
						fail("a translucent sample was not un-premultiplied: got " +
							ofToString(sampled.g, 3) + "," +
							ofToString(sampled.b, 3));
				}

				// Bare canvas returns the background, which is what is on screen
				// there.
				if (!box->sampleColor(0.5f, 0.85f, sampled))
					fail("sampling bare canvas failed");
				else if (std::abs(sampled.r - 0.25f) > 0.02f ||
					std::abs(sampled.b - 0.75f) > 0.02f)
					fail("bare canvas did not sample the background colour");

				// Off canvas is a refusal, not a guess.
				if (box->sampleColor(-0.1f, 0.5f, sampled))
					fail("sampling off the left edge returned a colour");
				if (box->sampleColor(0.5f, 1.4f, sampled))
					fail("sampling below the canvas returned a colour");
			}
			app.boxes.clear();
		}

		// Layers, and the path that matters most: a savefile written BEFORE
		// layers existed has its strokes hanging straight off <frame>. If that
		// does not land on layer 0, every drawing made in round one is lost.
		{
			const string legacyPath = directory + "paint_legacy.xml";
			ofFile legacy(legacyPath, ofFile::WriteOnly);
			legacy << "<activerender>0</activerender>\n";
			legacy << "<box>\n";
			legacy << "  <nombre>OLDPAINT</nombre>\n";
			legacy << "  <x>40</x>\n  <y>40</y>\n";
			legacy << "  <directory>paint</directory>\n";
			legacy << "  <onoff>1</onoff>\n  <bypass>0</bypass>\n";
			legacy << "  <paint>\n";
			legacy << "    <fps>18</fps>\n";
			legacy << "    <frame>\n      <hold>2</hold>\n      <id>0</id>\n";
			// x 0.0 -> 16384, x 1.0 -> 49151 under the -0.5..1.5 encoding.
			legacy << "      <stroke><color>1 0 0 1</color><size>0.02</size>"
				"<erase>0</erase><tool>0</tool>"
				"<pts>16384,16384,128 49151,49151,128</pts></stroke>\n";
			legacy << "    </frame>\n";
			legacy << "    <frame>\n      <hold>1</hold>\n      <id>1</id>\n";
			legacy << "      <stroke><color>0 1 0 1</color><size>0.03</size>"
				"<erase>1</erase><tool>1</tool>"
				"<pts>20000,20000,128</pts></stroke>\n";
			legacy << "    </frame>\n";
			legacy << "  </paint>\n</box>\n";
			legacy.close();

			app.boxes.clear();
			app.boxes.load(legacyPath);
			JPbox_paint *old = app.boxes.boxes.empty() ? nullptr :
				dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
			if (old == nullptr) fail("a pre-layers savefile did not load");
			else
			{
				const JPPaintDocument &d = old->document();
				if (d.layers.size() != 1)
					fail("a pre-layers file should produce exactly one layer");
				if (d.frames.size() != 2) fail("legacy load lost a cel");
				else
				{
					if (d.frames[0].layers.size() != 1 ||
						d.frames[1].layers.size() != 1)
						fail("legacy cels did not get a layer slot");
					else if (d.frames[0].layers[0].strokes.size() != 1 ||
						d.frames[1].layers[0].strokes.size() != 1)
						fail("legacy strokes did not land on layer 0");
					else
					{
						if (std::abs(d.frames[0].layers[0].strokes[0].r - 1.0f) > 0.01f)
							fail("legacy stroke lost its colour");
						if (!d.frames[1].layers[0].strokes[0].erase)
							fail("legacy stroke lost its eraser flag");
					}
					if (d.frames[0].hold != 2) fail("legacy hold was lost");
				}
				if (std::abs(d.fps - 18.0f) > 0.01f) fail("legacy fps was lost");
			}

			// Now the round two schema, end to end: two layers, one of them a
			// hidden background at 40%, strokes on both, saved and reloaded.
			app.boxes.clear();
			app.boxes.addBox("paint", 60, 60);
			JPbox_paint *box = dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
			if (box == nullptr) fail("could not build a paint box for layers");
			else
			{
				JPPaintStroke mark;
				mark.points.push_back(JPPaintPoint{0.4f, 0.4f, 1.0f});
				mark.points.push_back(JPPaintPoint{0.6f, 0.6f, 1.0f});
				box->commitStroke(mark);                      // layer 0
				box->addLayer();                              // -> layer 1
				if (box->currentLayer() != 1) fail("addLayer did not select it");
				JPPaintStroke second;
				second.r = 0.0f; second.b = 1.0f;
				second.points.push_back(JPPaintPoint{0.2f, 0.8f, 1.0f});
				box->commitStroke(second);                    // layer 1

				// toggleLayerBackground, not a hand-built props change: the
				// adoption of the cel's strokes is part of what is under test.
				box->toggleLayerBackground(0);
				JPPaintLayerInfo props = box->document().layers[0];
				props.name = "backdrop";
				props.visible = false;
				props.opacity = 0.4f;
				box->setLayerProps(0, props);
				// A background layer's strokes are shared, so this write must be
				// visible from every cel.
				box->addCel(false);
				box->setCurrentLayer(0);
				JPPaintStroke shared;
				shared.g = 1.0f;
				shared.points.push_back(JPPaintPoint{0.5f, 0.1f, 1.0f});
				box->commitStroke(shared);
				if (box->document().layers[0].sharedStrokes.size() != 2)
					fail("a background layer did not take the shared stroke");

				const string path = directory + "paint_layers.xml";
				app.boxes.save(path);
				app.boxes.clear();
				app.boxes.load(path);
				JPbox_paint *back = app.boxes.boxes.empty() ? nullptr :
					dynamic_cast<JPbox_paint *>(app.boxes.boxes.front());
				if (back == nullptr) fail("layer round trip lost the box");
				else
				{
					const JPPaintDocument &d = back->document();
					if (d.layers.size() != 2) fail("layer round trip lost a layer");
					else
					{
						if (d.layers[0].name != "backdrop")
							fail("layer round trip lost the name");
						if (d.layers[0].visible) fail("round trip lost visibility");
						if (!d.layers[0].background)
							fail("round trip lost the background flag");
						if (std::abs(d.layers[0].opacity - 0.4f) > 0.01f)
							fail("round trip lost the layer opacity");
						if (d.layers[0].sharedStrokes.size() != 2)
							fail("round trip lost the shared strokes");
						if (d.layers[0].id == d.layers[1].id)
							fail("two layers came back with the same id");
					}
					// The arity invariant everything else relies on.
					for (const JPPaintFrame &frame : d.frames)
					{
						if (frame.layers.size() != d.layers.size())
						{
							fail("round trip broke the layer arity invariant");
							break;
						}
					}
					if (d.frames.size() == 2 &&
						d.frames[0].layers[1].strokes.size() != 1)
						fail("round trip lost a per-cel stroke on layer 1");
				}
			}
			app.boxes.clear();
		}

		// The palette is the one piece of paint state that lives OUTSIDE the
		// session, in its own file, so a broken round trip loses colours the
		// user saved deliberately and nothing else would notice.
		{
			const string palettePath = ofToDataPath("paint_palette.xml");
			// Preserve whatever the real user has, then restore it at the end -
			// a test must not eat their palette.
			vector<ofFloatColor> userPalette = app.boxes.paintPalette;

			app.boxes.paintPalette.clear();
			app.boxes.paintPalette.push_back(ofFloatColor(1.0f, 0.25f, 0.1f, 1.0f));
			app.boxes.paintPalette.push_back(ofFloatColor(0.0f, 0.5f, 1.0f, 0.5f));
			app.boxes.savePaintPalette();
			if (!ofFile(palettePath).exists()) fail("saving the palette wrote no file");

			app.boxes.paintPalette.clear();
			app.boxes.loadPaintPalette();
			if (app.boxes.paintPalette.size() != 2)
				fail("palette round trip lost a colour");
			else
			{
				const ofFloatColor &a = app.boxes.paintPalette[0];
				const ofFloatColor &b = app.boxes.paintPalette[1];
				if (std::abs(a.r - 1.0f) > 0.001f || std::abs(a.g - 0.25f) > 0.001f ||
					std::abs(a.b - 0.1f) > 0.001f)
					fail("palette round trip changed a colour");
				// Alpha matters: a 50% swatch is a different swatch.
				if (std::abs(b.a - 0.5f) > 0.001f)
					fail("palette round trip lost alpha");
			}

			// Removal is also write-through.
			app.boxes.removePaintPaletteColor(0);
			app.boxes.paintPalette.clear();
			app.boxes.loadPaintPalette();
			if (app.boxes.paintPalette.size() != 1)
				fail("removing a swatch did not persist");
			app.boxes.removePaintPaletteColor(7);
			if (app.boxes.paintPalette.size() != 1)
				fail("an out of range removal changed the palette");

			// A malformed file drops rows rather than loading garbage.
			ofFile bad(palettePath, ofFile::WriteOnly);
			bad << "<color>1 0</color>\n<color>0.2 0.3 0.4 1</color>\n";
			bad.close();
			app.boxes.loadPaintPalette();
			if (app.boxes.paintPalette.size() != 1)
				fail("a short <color> row was not skipped");

			app.boxes.paintPalette = userPalette;
            app.boxes.savePaintPalette();
		}

		// "paint" must not be swallowed by, or swallow, another dispatch branch.
		app.boxes.clear();
		app.boxes.addBox("framedifference", 40, 40);
		if (!app.boxes.boxes.empty() &&
			dynamic_cast<JPbox_paint *>(app.boxes.boxes.front()) != nullptr)
			fail("addBox(\"framedifference\") built a paint box");
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
		<< " debugReport=" << debugReport
		<< " colorSwatch=" << colorSwatch
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
		<< " paintBox=" << paintBox
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
		spacePan && groupPathAfterClear && multiSelect && colorSwatch && debugReport && paintBox && mediaSmoke;
}
