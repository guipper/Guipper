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
	nativeMin = min;
	nativeMax = max;
	rangeEnabled = false;
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
	randomLocked = false;
	defaultFloatValue = _var;
	defaultBoolValue = false;
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
	nativeMin = min;
	nativeMax = max;
	rangeEnabled = false;
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
	randomLocked = false;
	defaultFloatValue = _var ? 1.0f : 0.0f;
	defaultBoolValue = _var;
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
		const float low = effectiveMin();
		const float high = effectiveMax();
		float absolutespeed = .015;
		if (movtype == OSC)
		{
			// floatValue += speed;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue > high)
			{
				floatLerpValue = high;
				dir = !dir;
			}
			if (floatLerpValue < low)
			{
				floatLerpValue = low;
				dir = !dir;
			}
		}
		if (movtype == GODER)
		{
			dir = true;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue > high)
			{
				floatLerpValue = low;
			}
		}
		if (movtype == GOIZQ)
		{
			// cout << "FUNCIONA" << endl;
			dir = false;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue < low)
			{
				floatLerpValue = high;
			}
		}
		if (movtype == RANDOM)
		{
			float n = ofMap(ofNoise(ofGetElapsedTimeMillis() * speed * absolutespeed *.01+ seed),
							0.0, 1.0, low, high);
			floatValue = n;
			floatLerpValue = n;
		}
		if (movtype == BPM)
		{
			if (!bpmEligible || jp_constants::bpm <= 0.0f)
			{
				floatLerpValue = low;
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
				floatLerpValue = ofLerp(low, high, envelope);
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
				const float mapped = ofLerp(low, high,
					ofClamp(audioSmoothed, 0.0f, 1.0f));
				floatLerpValue = ofLerp(audioBase, mapped,
					ofClamp(audioAmount, 0.0f, 1.0f));
			}
		}
		floatLerpValue = ofClamp(floatLerpValue, low, high);
	}

	// The one crossing from automation state to emitted value, so the morph
	// belongs exactly here. Cheap early-out: this runs for every parameter of
	// every box, every frame.
	if (morphArmed && morphAmount > 0.0f)
		floatValue = ofLerp(floatLerpValue, morphTarget,
			ofClamp(morphAmount, 0.0f, 1.0f));
	else
		floatValue = floatLerpValue;
	// floatLerpValue = ofRandom(1);
	// floatValue = ofRandom(1);
}

void JPParameter::setMorph(float target, float amount)
{
	morphArmed = true;
	morphTarget = target;
	morphAmount = ofClamp(amount, 0.0f, 1.0f);
}

void JPParameter::clearMorph()
{
	morphArmed = false;
	morphAmount = 0.0f;
	morphTarget = 0.0f;
	// Restore the emitted value HERE rather than waiting for the next update().
	// Once disarmed, a STANDART parameter is skipped by the gate in
	// JPParameterGroup::update() again, so nothing would ever tick it - and it
	// would stay frozen at the last blended value, which for the OUTGOING box
	// means permanently wearing the incoming look. That is precisely the
	// corruption this design set out to avoid.
	floatValue = floatLerpValue;
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
		// isMorphing() is load-bearing, not belt-and-braces: a hand-set
		// STANDART parameter has movtype 0 and needsUpdate false, so it is
		// never ticked at all - and those are exactly the parameters most
		// worth morphing. Without this the morph would appear to work on
		// animated parameters and do nothing on static ones.
		if (parameters[i]->movtype != 0 || parameters[i]->needsUpdate ||
			parameters[i]->isMorphing())
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

void JPParameter::cycleAutomationPattern()
{
	switch (movtype)
	{
	case OSC:    setAutomationMode(RANDOM); break;
	case RANDOM: setAutomationMode(GODER); break;
	case GODER:  setAutomationMode(GOIZQ); break;
	case GOIZQ:  setAutomationMode(OSC); break;
	default:     setAutomationMode(OSC); break;
	}
}

void JPParameter::captureRangeStart()
{
	setRangeStart(floatValue);
}

void JPParameter::captureRangeEnd()
{
	setRangeEnd(floatValue);
}

float JPParameter::effectiveMin() const
{
	return rangeEnabled ? min : nativeMin;
}

float JPParameter::effectiveMax() const
{
	return rangeEnabled ? max : nativeMax;
}

void JPParameter::clampToEffectiveRange()
{
	if (variabletype != FLOAT) return;
	floatValue = ofClamp(floatValue, effectiveMin(), effectiveMax());
	floatLerpValue = ofClamp(floatLerpValue, effectiveMin(), effectiveMax());
}

void JPParameter::setRangeStart(float value)
{
	min = ofClamp(value, nativeMin, nativeMax);
	if (min > max) max = min;
	clampToEffectiveRange();
}

void JPParameter::setRangeEnd(float value)
{
	max = ofClamp(value, nativeMin, nativeMax);
	if (max < min) min = max;
	clampToEffectiveRange();
}

void JPParameter::setRangeEnabled(bool enabled)
{
	rangeEnabled = enabled;
	if (rangeEnabled) clampToEffectiveRange();
}

void JPParameter::captureDefaultValue()
{
	if (variabletype == FLOAT)
		defaultFloatValue = floatValue;
	else if (variabletype == BOOL)
		defaultBoolValue = boolValue;
}

void JPParameter::restoreDefaultValue()
{
	if (randomLocked) return;
	if (variabletype == FLOAT)
	{
		floatValue = ofClamp(defaultFloatValue, effectiveMin(), effectiveMax());
		floatLerpValue = floatValue;
	}
	else if (variabletype == BOOL)
	{
		boolValue = defaultBoolValue;
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
		parameters[_index]->floatValue = ofClamp(_val,
			parameters[_index]->effectiveMin(), parameters[_index]->effectiveMax());
	}
}
void JPParameterGroup::setFloatLerpValue(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->floatLerpValue = ofClamp(_val,
			parameters[_index]->effectiveMin(), parameters[_index]->effectiveMax());
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
		parameters[_index]->nativeMin = _val;
		parameters[_index]->min = _val;
	}
}
void JPParameterGroup::setMax(float _val, int _index)
{
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->nativeMax = _val;
		parameters[_index]->max = _val;
	}
}
void JPParameterGroup::setRangeMin(float value, int index)
{
	if (index >= 0 && index < parameters.size() &&
		parameters[index]->variabletype == JPParameter::FLOAT)
		parameters[index]->setRangeStart(value);
}
void JPParameterGroup::setRangeMax(float value, int index)
{
	if (index >= 0 && index < parameters.size() &&
		parameters[index]->variabletype == JPParameter::FLOAT)
		parameters[index]->setRangeEnd(value);
}
void JPParameterGroup::setRangeEnabled(bool enabled, int index)
{
	if (index >= 0 && index < parameters.size() &&
		parameters[index]->variabletype == JPParameter::FLOAT)
		parameters[index]->setRangeEnabled(enabled);
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
		return parameters[_index]->effectiveMin();
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
		return parameters[_index]->effectiveMax();
	}
	else
	{
		return 1.0;
	}
}
float JPParameterGroup::getRangeMin(int index)
{
	return index >= 0 && index < parameters.size() ? parameters[index]->min : 0.0f;
}
float JPParameterGroup::getRangeMax(int index)
{
	return index >= 0 && index < parameters.size() ? parameters[index]->max : 1.0f;
}
float JPParameterGroup::getNativeMin(int index)
{
	return index >= 0 && index < parameters.size() ? parameters[index]->nativeMin : 0.0f;
}
float JPParameterGroup::getNativeMax(int index)
{
	return index >= 0 && index < parameters.size() ? parameters[index]->nativeMax : 1.0f;
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
int JPParameterGroup::indexOfName(const string &_name) const
{
	if (_name.empty()) return -1;
	for (int i = 0; i < (int)parameters.size(); ++i)
		if (parameters[i] != nullptr && parameters[i]->name == _name)
			return i;
	return -1;
}

int JPParameterGroup::resolveLoadIndex(const string &_name,
	int _positionalIndex) const
{
	const int byName = indexOfName(_name);
	if (byName >= 0) return byName;
	if (_positionalIndex >= 0 && _positionalIndex < (int)parameters.size())
		return _positionalIndex;
	return -1;
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

bool JPParameter::parseColorAnnotation(const string &line, int &channel,
									   string &group)
{
	const size_t at = line.find("@color");
	if (at == string::npos) return false;

	// Everything after the marker, split on whitespace. The first token is the
	// channel, the optional second is the group name.
	string rest = line.substr(at + 6);
	vector<string> tokens;
	string current;
	for (char c : rest)
	{
		if (isspace((unsigned char)c))
		{
			if (!current.empty()) { tokens.push_back(current); current.clear(); }
		}
		else
		{
			current += c;
		}
	}
	if (!current.empty()) tokens.push_back(current);
	if (tokens.empty()) return false;

	string channelToken = ofToLower(tokens[0]);
	if (channelToken == "r") channel = COLOR_R;
	else if (channelToken == "g") channel = COLOR_G;
	else if (channelToken == "b") channel = COLOR_B;
	else return false;   // an unknown channel is a typo, not a colour

	group = tokens.size() > 1 ? tokens[1] : string();
	return true;
}

ofFloatColor JPParameter::swatchColor(float r, float g, float b)
{
	// Shader colour uniforms are 0..1 - JPParameter::setup pins min/max there
	// and addFloatValue does no range work - so no /255 anywhere. The clamp is
	// for a custom range: rangeEnabled lets a value reach 4.0, and ofFloatColor
	// would carry that straight through to a wrapped or blown-out swatch.
	return ofFloatColor(ofClamp(r, 0.0f, 1.0f),
						ofClamp(g, 0.0f, 1.0f),
						ofClamp(b, 0.0f, 1.0f));
}
