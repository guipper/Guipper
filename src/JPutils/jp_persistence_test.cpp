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
			"audioattackms", "audioreleasems"};
		for (auto &box : xml.getChildren("box"))
			for (auto &param : box.getChild("parameters").getChildren("param"))
				for (const char *field : fields) param.removeChild(field);
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
	box->parameters.setFloatValue(0.37f, 0);
	box->parameters.setFloatLerpValue(0.37f, 0);
	box->parameters.setMin(0.12f, 0); box->parameters.setMax(0.88f, 0);
	box->parameters.setmovetype(JPParameter::AUDIO, 0);
	box->parameters.setAudioSource(jp_audio::SRC_SNARE_LOGIC, 0);
	box->parameters.setAudioDiv(jp_audio::DIV_16, 0);
	box->parameters.setAudioBase(0.41f, 0);
	box->parameters.setAudioAmount(0.62f, 0);
	box->parameters.setAudioInvert(true, 0);
	box->parameters.setAudioThreshold(0.23f, 0);
	box->parameters.setAudioCurve(2.0f, 0);
	box->parameters.setAudioAttackMs(31.0f, 0);
	box->parameters.setAudioReleaseMs(777.0f, 0);
	app.boxes.save(currentPath);

	app.boxes.clear(); app.boxes.load(currentPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	const bool current = sameParameterOrder(box, names) &&
		box->parameters.getAudioSource(0) == jp_audio::SRC_SNARE_LOGIC &&
		box->parameters.getAudioDiv(0) == jp_audio::DIV_16 &&
		near(box->parameters.getAudioBase(0), 0.41f) &&
		near(box->parameters.getAudioAmount(0), 0.62f) &&
		box->parameters.getAudioInvert(0) &&
		near(box->parameters.getAudioThreshold(0), 0.23f) &&
		near(box->parameters.getAudioCurve(0), 2.0f) &&
		near(box->parameters.getAudioAttackMs(0), 31.0f) &&
		near(box->parameters.getAudioReleaseMs(0), 777.0f);

	ofXml legacy; legacy.load(currentPath); removeAudioFields(legacy); legacy.save(legacyPath);
	app.boxes.clear(); app.boxes.load(legacyPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	const bool old = sameParameterOrder(box, names) &&
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
	invalid.save(invalidPath);
	app.boxes.clear(); app.boxes.load(invalidPath);
	box = app.boxes.boxes.empty() ? nullptr : app.boxes.boxes.front();
	const bool clamped = sameParameterOrder(box, names) &&
		box->parameters.getAudioSource(0) == jp_audio::SRC_COUNT - 1 &&
		box->parameters.getAudioDiv(0) == jp_audio::DIV_1 &&
		near(box->parameters.getAudioBase(0), 1.0f) &&
		near(box->parameters.getAudioAmount(0), 0.0f) &&
		near(box->parameters.getAudioThreshold(0), 0.99f) &&
		near(box->parameters.getAudioCurve(0), 5.0f) &&
		near(box->parameters.getAudioAttackMs(0), 0.0f) &&
		near(box->parameters.getAudioReleaseMs(0), 5000.0f);

	ofLogNotice("jp_persistence_test") << "current=" << current
		<< " legacy=" << old << " invalid=" << clamped;
	return current && old && clamped;
}
