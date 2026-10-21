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
			auto hand=node.appendChild("kinect");const auto &b=parameter->handBinding;
            hand.appendChild("version").set(1);hand.appendChild("right").set(b.right);
            hand.appendChild("signal").set(b.signal);hand.appendChild("point").set(b.point);
            hand.appendChild("inputmin").set(b.inputMin);hand.appendChild("inputmax").set(b.inputMax);
            hand.appendChild("invert").set(b.invert);hand.appendChild("smoothingms").set(b.smoothingMs);
            hand.appendChild("deadzone").set(b.deadzone);hand.appendChild("base").set(b.base);
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
			auto hand=node.getChild("kinect");auto &b=parameter->handBinding;
            b={};b.base=parameter->floatValue;
            if(hand){
                b.right=(hand.getChild("right")?hand.getChild("right").getBoolValue():true);
                b.signal=std::clamp(hand.getChild("signal").getIntValue(),0,4);
                b.point=std::clamp((hand.getChild("point")?hand.getChild("point").getIntValue():-1),-1,20);
                auto number=[&](const char *key,float fallback,float lo,float hi){auto v=hand.getChild(key);float value=v?v.getFloatValue():fallback;return std::isfinite(value)?std::clamp(value,lo,hi):fallback;};
                b.inputMin=number("inputmin",b.signal==jp_hand::Distance?.7f:b.signal==jp_hand::Pinch?.15f:0,-10000,10000);
                b.inputMax=number("inputmax",b.signal==jp_hand::Distance?1.5f:1,-10000,10000);
                if(b.inputMax<=b.inputMin){b.inputMin=0;b.inputMax=1;}
                b.invert=hand.getChild("invert").getBoolValue();b.smoothingMs=number("smoothingms",100,0,5000);b.deadzone=number("deadzone",.01f,0,.5f);
                b.base=number("base",parameter->floatValue,parameter->nativeMin,parameter->nativeMax);
            }
            parameter->resetHandRuntime();
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
				// Morph changes only emitted values; snapshots/save/history retain the
                // destination automation value, never a transition intermediate.
                auto *valueParameter=group.getJParameter(k);
                param.appendChild("value").set(valueParameter && valueParameter->movtype==JPParameter::KINECT ? valueParameter->handBinding.base : valueParameter && valueParameter->isMorphing() ?
                    valueParameter->floatLerpValue : group.getFloatValue(k));
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
			group.getJParameter(destinationIndex)->setAutomationMode(JPParameter::STANDART);
            group.setName(param.getChild("name").getValue());
			group.setRangeMin(param.getChild("min").getFloatValue(), destinationIndex);
			group.setRangeMax(param.getChild("max").getFloatValue(), destinationIndex);
			group.setFloatLerpValue(param.getChild("value").getFloatValue(), destinationIndex);
			// Static parameters do not tick after loading. Restore the emitted
			// value too, including inside presets, rather than leaving the
			// shader's initial (possibly random) value visible and re-saveable.
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
