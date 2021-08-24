#include "jp_parametergroup.h"

void JPParameter::setup(float _var, string _name)

{
	name = _name;
	floatValue = _var;
	floatLerpValue = _var;
	variabletype = FLOAT;
	dir = true;

	min = 0.0;
	max = 1.0;
	speed = 0.2;
	seed = ofRandom(10000);
	needsUpdate = false;
}
void JPParameter::setup(bool _var, string _name)
{
	name = _name;
	boolValue = _var;
	variabletype = BOOL;

	min = 0.0;
	max = 1.0;
	speed = 1.0;
}
void JPParameter::update()
{
	//ACA DEBERIA ACTUALIARSE SI ES TIPO UN FLOAT :
	if (variabletype == FLOAT)
	{
		float absolutespeed = .015;
		if (movtype == OSC)
		{
			//floatValue += speed;
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
			//cout << "FUNCIONA" << endl;
			dir = false;
			(dir) ? floatLerpValue += speed *absolutespeed : floatLerpValue -= speed * absolutespeed;
			if (floatLerpValue < min)
			{
				floatLerpValue = max;
			}
		}
		if (movtype == RANDOM)
		{
			float n = ofMap(ofNoise(ofGetElapsedTimeMillis() * speed * absolutespeed + seed),
							0.0, 1.0, min, max);
			floatValue = n;
			floatLerpValue = n;
		}
	}

	floatValue = floatLerpValue;
	//floatLerpValue = ofRandom(1);
	//floatValue = ofRandom(1);
}

/*****************************************************************************/
void JPParameterGroup::addFloatValue(float _var, string _name)
{
	JPParameter *param = new JPParameter();
	param->setup(_var, _name);
	parameters.push_back(param);
}
void JPParameterGroup::addBoolValue(bool _var, string _name)
{
	JPParameter *param = new JPParameter();
	param->setup(_var, _name);
	parameters.push_back(param);
}
void JPParameterGroup::clear()
{
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
	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->movtype = _movetype;
	}
}
//SETTERS
void JPParameterGroup::setFloatValue(float _val, int _index)
{
	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->floatValue = _val;
	}
}
void JPParameterGroup::setFloatLerpValue(float _val, int _index)
{
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
	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->speed = _val;
	}
}
void JPParameterGroup::setBoolValue(bool _val, int _index)
{
	//if (parameters[_index].variabletype == parameters[_index].BOOL) {
	parameters[_index]->boolValue = _val;
	//}
}
void JPParameterGroup::setMin(float _val, int _index)
{

	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->min = _val;
	}
}
void JPParameterGroup::setMax(float _val, int _index)
{
	if (parameters[_index]->variabletype == parameters[_index]->FLOAT)
	{
		parameters[_index]->max = _val;
	}
}
//GETTERS :
int JPParameterGroup::getSize()
{
	return parameters.size();
}
int JPParameterGroup::getType(int _index)
{
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
	return parameters[_index]->speed;
}
float JPParameterGroup::getFloatValue(int _index)
{
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
	//cout << endl << "MOVTYPE" << parameters[_index]->movtype << endl;
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