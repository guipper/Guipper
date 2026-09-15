#include "jp_parameter_xml.h"
#include "jp_parametergroup.h"
#include "ofXml.h"

namespace
{
	void saveParameterUserState(ofXml &node, JPParameter *parameter)
	{
		if (parameter == nullptr) return;
		node.appendChild("randomlocked").set(parameter->randomLocked);
		if (parameter->variabletype == JPParameter::FLOAT)
		{
			node.appendChild("rangeenabled").set(parameter->rangeEnabled);
			node.appendChild("defaultvalue").set(parameter->defaultFloatValue);
		}
		else if (parameter->variabletype == JPParameter::BOOL)
			node.appendChild("defaultbool").set(parameter->defaultBoolValue);
	}
	void loadParameterUserState(ofXml &node, JPParameter *parameter)
	{
		if (parameter == nullptr) return;
		auto locked = node.getChild("randomlocked");
		if (locked) parameter->randomLocked = locked.getBoolValue();
		auto value = node.getChild("defaultvalue");
		if (value && parameter->variabletype == JPParameter::FLOAT)
			parameter->defaultFloatValue = ofClamp(
				value.getFloatValue(), parameter->nativeMin, parameter->nativeMax);
		if (parameter->variabletype == JPParameter::FLOAT)
		{
			auto enabled = node.getChild("rangeenabled");
			// Custom limiting is opt-in. Legacy files keep their remembered
			// endpoints, but open on the native slider domain.
			parameter->setRangeEnabled(enabled ? enabled.getBoolValue() : false);
		}
		auto boolean = node.getChild("defaultbool");
		if (boolean && parameter->variabletype == JPParameter::BOOL)
			parameter->defaultBoolValue = boolean.getBoolValue();
	}
}

void jp_parameter_xml::save(ofXml &boxNode, JPParameterGroup &group)
{
	if (group.getSize() > 0)
	{
		auto parameters = boxNode.appendChild("parameters");
		for (int k = 0; k < group.getSize(); k++)
		{

			if (group.getType(k) == group.BOOL)
			{
				auto param = parameters.appendChild("param");
				param.appendChild("name").set(group.getName(k));
				param.appendChild("value").set(group.getBoolValue(k));
				saveParameterUserState(param,
					group.getJParameter(k));
			}
			else
			{
				auto param = parameters.appendChild("param");
				param.appendChild("name").set(group.getName(k));
				param.appendChild("min").set(group.getRangeMin(k));
				param.appendChild("max").set(group.getRangeMax(k));
				param.appendChild("value").set(group.getFloatValue(k));
				param.appendChild("movtype").set(group.getMovType(k));
				param.appendChild("lastmovtype").set(group.getLastMovType(k));
				saveParameterUserState(param,
					group.getJParameter(k));
				param.appendChild("speed").set(group.getSpeed(k));
				param.appendChild("bpmrate").set(group.getBpmRate(k));
				param.appendChild("audiosource").set(group.getAudioSource(k));
				param.appendChild("audiodiv").set(group.getAudioDiv(k));
				param.appendChild("audiobase").set(group.getAudioBase(k));
				param.appendChild("audioamount").set(group.getAudioAmount(k));
				param.appendChild("audioinvert").set(group.getAudioInvert(k));
				param.appendChild("audiodrivesspeed").set(group.getAudioDrivesSpeed(k));
				param.appendChild("audiospeeddirection").set(group.getAudioSpeedDirection(k));
				param.appendChild("audiothreshold").set(group.getAudioThreshold(k));
				param.appendChild("audiocurve").set(group.getAudioCurve(k));
				param.appendChild("audioattackms").set(group.getAudioAttackMs(k));
				param.appendChild("audioreleasems").set(group.getAudioReleaseMs(k));
			}
		}
	}
}

void jp_parameter_xml::load(const ofXml &boxNode, JPParameterGroup &group,
	LoadContext context)
{
	int positionalIndex = 0;
	auto parameters = boxNode.getChild("parameters").getChildren();
	int parameterLoadLimit = group.getSize();

	for (auto &param : parameters)
	{
		// By name, falling back to position. Purely positional loading
		// froze the parameter arrays: reordering, or inserting anywhere but
		// the end, shifted every value in every saved composition.
		const int destinationIndex = context == LoadContext::Composition ?
			group.resolveLoadIndex(param.getChild("name").getValue(), positionalIndex) : positionalIndex;
		++positionalIndex;
		// continue, not break: a later <param> may still match by name even
		// when this one resolves nowhere.
		if (destinationIndex < 0 || destinationIndex >= parameterLoadLimit)
			continue;

		if (group.getType(destinationIndex) == group.FLOAT)
		{
			group.setName(param.getChild("name").getValue());
			group.setRangeMin(param.getChild("min").getFloatValue(), destinationIndex);
			group.setRangeMax(param.getChild("max").getFloatValue(), destinationIndex);
			group.setFloatLerpValue(param.getChild("value").getFloatValue(), destinationIndex);
			// Presets historically restore only the smoothed value here.
			// Keep that contract until its migration is handled explicitly.
			if (context != LoadContext::Preset)
				group.setFloatValue(param.getChild("value").getFloatValue(), destinationIndex);
			group.setmovetype(param.getChild("movtype").getIntValue(), destinationIndex);
			auto lastMoveType = param.getChild("lastmovtype");
			if (lastMoveType)
			{
				group.setlastmovetype(
					lastMoveType.getIntValue(), destinationIndex);
			}
			group.setSpeed(param.getChild("speed").getFloatValue(), destinationIndex);
			auto bpmRate = param.getChild("bpmrate");
			if (bpmRate)
			{
				group.setBpmRate(bpmRate.getIntValue(), destinationIndex);
			}
			auto audioSource = param.getChild("audiosource");
			if (audioSource)
			{
				group.setAudioSource(audioSource.getIntValue(), destinationIndex);
			}
			auto audioDiv = param.getChild("audiodiv");
			if (audioDiv)
			{
				group.setAudioDiv(audioDiv.getIntValue(), destinationIndex);
			}
			auto loadAudioFloat = [&](const char *key, auto setter)
			{
				auto node = param.getChild(key);
				if (node) (group.*setter)(node.getFloatValue(), destinationIndex);
			};
			loadAudioFloat("audiobase", &JPParameterGroup::setAudioBase);
			loadAudioFloat("audioamount", &JPParameterGroup::setAudioAmount);
			auto audioInvert = param.getChild("audioinvert");
			if (audioInvert) group.setAudioInvert(audioInvert.getBoolValue(), destinationIndex);
			auto audioDrivesSpeed = param.getChild("audiodrivesspeed");
			if (audioDrivesSpeed) group.setAudioDrivesSpeed(audioDrivesSpeed.getBoolValue(), destinationIndex);
			auto audioSpeedDir = param.getChild("audiospeeddirection");
			if (audioSpeedDir) group.setAudioSpeedDirection(audioSpeedDir.getIntValue(), destinationIndex);
			loadAudioFloat("audiothreshold", &JPParameterGroup::setAudioThreshold);
			loadAudioFloat("audiocurve", &JPParameterGroup::setAudioCurve);
			loadAudioFloat("audioattackms", &JPParameterGroup::setAudioAttackMs);
			loadAudioFloat("audioreleasems", &JPParameterGroup::setAudioReleaseMs);
		}
		else if (group.getType(destinationIndex) == group.BOOL)
		{
			group.setName(param.getChild("name").getValue());
			group.setBoolValue(param.getChild("value").getBoolValue(), destinationIndex);
		}
		loadParameterUserState(param,
			group.getJParameter(destinationIndex));
	}
}
