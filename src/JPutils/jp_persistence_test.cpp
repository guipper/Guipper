#include "jp_persistence_test.h"

#include "../ofApp.h"
#include "jp_audio.h"

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
	ofLogNotice("jp_persistence_test") << "current=" << current
		<< " legacy=" << old << " invalid=" << clamped
		<< " shaderReload=" << shaderReload
		<< " modeMemory=" << modeMemory
		<< " rangeCapture=" << rangeCapture
		<< " midiRange=" << midiRange
		<< " cueState=" << cueState
		<< " lockDefault=" << lockDefault;
	return current && old && clamped && shaderReload && modeMemory &&
		rangeCapture && midiRange && cueState && lockDefault;
}
