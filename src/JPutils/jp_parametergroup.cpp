#include "jp_parametergroup.h"
#include "jp_constants.h"
#include <algorithm>
#include <cmath>

void JPParameter::setup(float _var, string _name)

{
	name = _name;
	floatValue = _var;
	floatLerpValue = _var;
	variabletype = FLOAT;
	movtype = STANDART;
	dir = true;

	min = 0.0;
	max = 1.0;
	speed = 0.2;
	seed = ofRandom(10000);
	bpmEligible = false;
	bpmRate = BPM_RATE_ONE;
	needsUpdate = false;
}
void JPParameter::setup(bool _var, string _name)
{
	name = _name;
	boolValue = _var;
	variabletype = BOOL;
	movtype = STANDART;

	min = 0.0;
	max = 1.0;
	speed = 1.0;
	bpmEligible = false;
	bpmRate = BPM_RATE_ONE;
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
	if (_index >= parameters.size())
		return;

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->movtype =
			_movetype == JPParameter::BPM &&
				!parameters[_index]->bpmEligible ?
				JPParameter::STANDART : _movetype;
	}
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