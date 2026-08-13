#include "jp_parametergroup.h"
#include "jp_audio_analyzer.h"
#include "jp_constants.h"
#include "jp_audio.h"
#include <algorithm>
#include <cmath>

void JPParameter::setup(float _var, string _name)

{
	name = _name;
	floatValue = _var;
	floatLerpValue = _var;
	variabletype = FLOAT;
	movtype = STANDART;
	lastMovtype = OSC;
	dir = true;

	min = 0.0;
	max = 1.0;
	speed = 0.2;
	seed = ofRandom(10000);
	bpmEligible = false;
	bpmRate = BPM_RATE_ONE;
	// Every float slider can follow audio - unlike BPM sync, which is limited
	// to uniforms parsed from a .frag.
	audioEligible = true;
	audioSource = jp_audio::SRC_LOW;
	audioDiv = jp_audio::DIV_1;
	audioBase = _var;
	audioAmount = 1.0f;
	audioInvert = false;
	audioThreshold = 0.0f;
	audioCurve = 1.0f;
	audioAttackMs = 8.0f;
	audioReleaseMs = 250.0f;
	audioShapingOpen = false;
	audioSmoothed = 0.0f;
	needsUpdate = false;
}
void JPParameter::setup(bool _var, string _name)
{
	name = _name;
	boolValue = _var;
	variabletype = BOOL;
	movtype = STANDART;
	lastMovtype = OSC;

	min = 0.0;
	max = 1.0;
	speed = 1.0;
	bpmEligible = false;
	bpmRate = BPM_RATE_ONE;
	// Bools are driven by the MIDI/threshold path, not by an automation curve.
	audioEligible = false;
	audioSource = jp_audio::SRC_LOW;
	audioDiv = jp_audio::DIV_1;
	audioBase = _var ? 1.0f : 0.0f;
	audioAmount = 1.0f;
	audioInvert = false;
	audioThreshold = 0.0f;
	audioCurve = 1.0f;
	audioAttackMs = 8.0f;
	audioReleaseMs = 250.0f;
	audioShapingOpen = false;
	audioSmoothed = 0.0f;
	needsUpdate = false;
}
float JPParameter::getBpmMultiplier() const
{
	switch (bpmRate)
	{
	case BPM_RATE_QUARTER: return 0.25f;
	case BPM_RATE_HALF: return 0.5f;
	case BPM_RATE_DOUBLE: return 2.0f;
	case BPM_RATE_QUADRUPLE: return 4.0f;
	default: return 1.0f;
	}
}
void JPParameter::cycleBpmRate()
{
	bpmRate = (std::clamp(
		bpmRate,
		int(BPM_RATE_QUARTER),
		int(BPM_RATE_QUADRUPLE)) + 1) %
		(int(BPM_RATE_QUADRUPLE) + 1);
}
void JPParameter::cycleAudioSource()
{
	audioSource = (std::clamp(audioSource, 0, int(jp_audio::SRC_COUNT) - 1) + 1) %
		int(jp_audio::SRC_COUNT);
}
void JPParameter::cycleAudioDiv()
{
	audioDiv = (std::clamp(audioDiv, 0, int(jp_audio::DIV_COUNT) - 1) + 1) %
		int(jp_audio::DIV_COUNT);
}
void JPParameter::update()
{
	// ACA DEBERIA ACTUALIARSE SI ES TIPO UN FLOAT :
	if (variabletype == FLOAT)
	{
		float absolutespeed = .015;
		if (movtype == OSC)
		{
			// floatValue += speed;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue > max)
			{
				floatLerpValue = max;
				dir = !dir;
			}
			if (floatLerpValue < min)
			{
				floatLerpValue = min;
				dir = !dir;
			}
		}
		if (movtype == GODER)
		{
			dir = true;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue > max)
			{
				floatLerpValue = min;
			}
		}
		if (movtype == GOIZQ)
		{
			// cout << "FUNCIONA" << endl;
			dir = false;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue < min)
			{
				floatLerpValue = max;
			}
		}
		if (movtype == RANDOM)
		{
			float n = ofMap(ofNoise(ofGetElapsedTimeMillis() * speed * absolutespeed *.01+ seed),
							0.0, 1.0, min, max);
			floatValue = n;
			floatLerpValue = n;
		}
		if (movtype == BPM)
		{
			if (!bpmEligible || jp_constants::bpm <= 0.0f)
			{
				floatLerpValue = min;
			}
			else
			{
				const float phase = jp_constants::getBeatPhase(
					getBpmMultiplier());
				const float decayExponent = ofLerp(
					0.5f, 12.0f, ofClamp(speed, 0.0f, 1.0f));
				const float envelope = std::pow(
					std::max(0.0f, 1.0f - phase),
					decayExponent);
				floatLerpValue = ofLerp(min, max, envelope);
			}
		}
		if (movtype == AUDIO)
		{
			const float dt = ofClamp((float)ofGetLastFrameTime(),
				1.0f / 1000.0f, 0.1f);
			if (!audioEligible || !jp_audio::isRunning())
			{
				floatLerpValue = jp_audio_internal::smoothToward(
					floatLerpValue, audioBase, audioReleaseMs, dt);
			}
			else
			{
				float shaped = jp_audio::getValue(audioSource, audioDiv);
				const float threshold = ofClamp(audioThreshold, 0.0f, 0.99f);
				shaped = ofClamp((shaped - threshold) / (1.0f - threshold), 0.0f, 1.0f);
				shaped = std::pow(shaped, ofClamp(audioCurve, 0.20f, 5.0f));
				if (audioInvert) shaped = 1.0f - shaped;

				const bool logic = audioSource == jp_audio::SRC_KICK_LOGIC ||
					audioSource == jp_audio::SRC_SNARE_LOGIC;
				if (logic)
					audioSmoothed = shaped;
				else
				{
					const float milliseconds = shaped > audioSmoothed ?
						audioAttackMs : audioReleaseMs;
					audioSmoothed = jp_audio_internal::smoothToward(
						audioSmoothed, shaped, milliseconds, dt);
				}
				const float mapped = ofLerp(min, max,
					ofClamp(audioSmoothed, 0.0f, 1.0f));
				floatLerpValue = ofLerp(audioBase, mapped,
					ofClamp(audioAmount, 0.0f, 1.0f));
			}
		}
	}

	floatValue = floatLerpValue;
	// floatLerpValue = ofRandom(1);
	// floatValue = ofRandom(1);
}

/*****************************************************************************/
void JPParameterGroup::addFloatValue(
	float _var, string _name, bool _bpmEligible)
{
	JPParameter *param = new JPParameter();
	param->setup(_var, _name);
	param->bpmEligible = _bpmEligible;
	parameters.push_back(param);
}
void JPParameterGroup::addBoolValue(bool _var, string _name)
{
	JPParameter *param = new JPParameter();
	param->setup(_var, _name);
	parameters.push_back(param);
}
JPParameterGroup::JPParameterGroup() {}

JPParameterGroup::~JPParameterGroup()
{
	clear();
}

JPParameterGroup::JPParameterGroup(const JPParameterGroup& other)
{
	name = other.name;
	for (size_t i = 0; i < other.parameters.size(); i++)
	{
		if (other.parameters[i] != nullptr)
		{
			JPParameter* newParam = new JPParameter(*other.parameters[i]);
			parameters.push_back(newParam);
		}
	}
}

JPParameterGroup& JPParameterGroup::operator=(const JPParameterGroup& other)
{
	if (this != &other)
	{
		clear();
		name = other.name;
		for (size_t i = 0; i < other.parameters.size(); i++)
		{
			if (other.parameters[i] != nullptr)
			{
				JPParameter* newParam = new JPParameter(*other.parameters[i]);
				parameters.push_back(newParam);
			}
		}
	}
	return *this;
}

void JPParameterGroup::clear()
{
	for (size_t i = 0; i < parameters.size(); i++)
	{
		delete parameters[i];
	}
	parameters.clear();
}
void JPParameterGroup::coutData()
{
	cout << "*******************************************" << endl;
	cout << "PARAMETER DATA : " << endl;
	for (int i = 0; i < parameters.size(); i++)
	{
		cout << "NAME : " << parameters[i]->name << endl;
		if (parameters[i]->variabletype == FLOAT)
		{
			cout << "TYPE : FLOAT" << endl;
			cout << "VALUE : " << parameters[i]->floatValue << endl;
		}
		else if (parameters[i]->variabletype == BOOL)
		{
			cout << "TYPE : BOOL" << endl;
			cout << "VALUE : " << parameters[i]->boolValue << endl;
		}
	}
	cout << "*******************************************" << endl;
}
void JPParameterGroup::update()
{
	for (int i = 0; i < parameters.size(); i++)
	{
		if (parameters[i]->movtype != 0 || parameters[i]->needsUpdate)
		{
			parameters[i]->update();
		}
	}
}
void JPParameterGroup::setmovetype(int _movetype, int _index)
{
	if (_index < 0 || _index >= (int)parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->setAutomationMode(_movetype);
	}
}

void JPParameter::setAutomationMode(int mode)
{
	// Load files may contain a newer or ineligible mode. Keeping validation
	// here ensures every UI, reload and persistence path has the same fallback.
	int wanted = mode;
	if (wanted < STANDART || wanted > AUDIO)
	{
		wanted = STANDART;
	}
	if ((wanted == BPM && !bpmEligible) ||
		(wanted == AUDIO && !audioEligible))
	{
		wanted = STANDART;
	}
	movtype = wanted;
	if (wanted != STANDART)
	{
		lastMovtype = wanted;
	}
}

void JPParameter::setLastAutomationMode(int mode)
{
	int wanted = mode;
	if (wanted <= STANDART || wanted > AUDIO ||
		(wanted == BPM && !bpmEligible) ||
		(wanted == AUDIO && !audioEligible))
	{
		wanted = OSC;
	}
	lastMovtype = wanted;
}

void JPParameter::toggleAutomation()
{
	if (movtype == STANDART)
	{
		setLastAutomationMode(lastMovtype);
		setAutomationMode(lastMovtype);
	}
	else
	{
		setAutomationMode(STANDART);
	}
}

void JPParameterGroup::setlastmovetype(int _movetype, int _index)
{
	if (_index < 0 || _index >= (int)parameters.size() ||
		parameters[_index]->variabletype != parameters[_index]->FLOAT)
	{
		return;
	}
	parameters[_index]->setLastAutomationMode(_movetype);
}
// SETTERS
void JPParameterGroup::setFloatValue(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->floatValue = _val;
	}
}
void JPParameterGroup::setFloatLerpValue(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->floatLerpValue = _val;
	}
}
void JPParameterGroup::setName(string _name)
{
	name = _name;
}
void JPParameterGroup::setSpeed(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->speed = _val;
	}
}
void JPParameterGroup::setBpmRate(int _rate, int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return;

	parameters[_index]->bpmRate = std::clamp(
		_rate,
		int(JPParameter::BPM_RATE_QUARTER),
		int(JPParameter::BPM_RATE_QUADRUPLE));
}
void JPParameterGroup::setAudioSource(int _source, int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return;

	parameters[_index]->audioSource =
		std::clamp(_source, 0, int(jp_audio::SRC_COUNT) - 1);
}
void JPParameterGroup::setAudioDiv(int _div, int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return;

	parameters[_index]->audioDiv =
		std::clamp(_div, 0, int(jp_audio::DIV_COUNT) - 1);
}
void JPParameterGroup::setAudioBase(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioBase = ofClamp(value, 0.0f, 1.0f);
}
void JPParameterGroup::setAudioAmount(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioAmount = ofClamp(value, 0.0f, 1.0f);
}
void JPParameterGroup::setAudioInvert(bool value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioInvert = value;
}
void JPParameterGroup::setAudioThreshold(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioThreshold = ofClamp(value, 0.0f, 0.99f);
}
void JPParameterGroup::setAudioCurve(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioCurve = ofClamp(value, 0.20f, 5.0f);
}
void JPParameterGroup::setAudioAttackMs(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioAttackMs = ofClamp(value, 0.0f, 2000.0f);
}
void JPParameterGroup::setAudioReleaseMs(float value, int index)
{
	if (index >= 0 && index < parameters.size()) parameters[index]->audioReleaseMs = ofClamp(value, 1.0f, 5000.0f);
}
void JPParameterGroup::setBoolValue(bool _val, int _index)
{
	if (_index >= parameters.size())
		return;

	// if (parameters[_index].variabletype == parameters[_index].BOOL) {
	parameters[_index]->boolValue = _val;
	//}
}
void JPParameterGroup::setMin(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->min = _val;
	}
}
void JPParameterGroup::setMax(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->max = _val;
	}
}
// GETTERS :
int JPParameterGroup::getSize()
{
	return parameters.size();
}
int JPParameterGroup::getType(int _index)
{
	if (_index >= parameters.size())
		return 0;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		return parameters[_index]->FLOAT;
	}
	else
	{
		return parameters[_index]->BOOL;
	}
}
float JPParameterGroup::getSpeed(int _index)
{
	if (_index >= parameters.size())
		return 0.0;

	return parameters[_index]->speed;
}
int JPParameterGroup::getBpmRate(int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return JPParameter::BPM_RATE_ONE;

	return parameters[_index]->bpmRate;
}
int JPParameterGroup::getAudioSource(int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return jp_audio::SRC_LOW;

	return parameters[_index]->audioSource;
}
int JPParameterGroup::getAudioDiv(int _index)
{
	if (_index < 0 || _index >= parameters.size())
		return jp_audio::DIV_1;

	return parameters[_index]->audioDiv;
}
float JPParameterGroup::getAudioBase(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioBase : 0.0f; }
float JPParameterGroup::getAudioAmount(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioAmount : 1.0f; }
bool JPParameterGroup::getAudioInvert(int i) { return i >= 0 && i < parameters.size() && parameters[i]->audioInvert; }
float JPParameterGroup::getAudioThreshold(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioThreshold : 0.0f; }
float JPParameterGroup::getAudioCurve(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioCurve : 1.0f; }
float JPParameterGroup::getAudioAttackMs(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioAttackMs : 8.0f; }
float JPParameterGroup::getAudioReleaseMs(int i) { return i >= 0 && i < parameters.size() ? parameters[i]->audioReleaseMs : 250.0f; }
float JPParameterGroup::getFloatValue(int _index)
{
	if (_index >= parameters.size())
		return 0.0;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		return parameters[_index]->floatValue;
	}
	else
	{
		return -10;
	}
}
float JPParameterGroup::getLerpValue(int _index)
{
	if (_index >= parameters.size())
		return 0.0;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		return parameters[_index]->floatLerpValue;
	}
	else
	{
		return -10;
	}
}
float JPParameterGroup::getMin(int _index)
{
	if (_index >= parameters.size())
		return 0.0;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		return parameters[_index]->min;
	}
	else
	{
		return 0.0;
	}
}
float JPParameterGroup::getMax(int _index)
{
	if (_index >= parameters.size())
		return 0.0;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		return parameters[_index]->max;
	}
	else
	{
		return 1.0;
	}
}
bool JPParameterGroup::getBoolValue(int _index)
{
	if (_index >= parameters.size())
		return false;

	if (parameters[_index]->variabletype == parameters[_index]->BOOL)
	{
		return parameters[_index]->boolValue;
	}
	else
	{
		return false;
	}
}
int JPParameterGroup::getMovType(int _index)
{
	if (_index >= parameters.size())
		return 0;

	// cout << endl << "MOVTYPE" << parameters[_index]->movtype << endl;
	return parameters[_index]->movtype;
}
int JPParameterGroup::getLastMovType(int _index)
{
	if (_index < 0 || _index >= (int)parameters.size())
		return JPParameter::OSC;
	return parameters[_index]->lastMovtype;
}
JPParameter *JPParameterGroup::getJParameter(int _index)
{
	return parameters.at(_index);
}
string JPParameterGroup::getName(int _index)
{
	if (getSize() >= _index)
	{
		return parameters[_index]->name;
	}
	else
	{
		return "ERROR IN GETTING NAME VALUE";
	}
}
