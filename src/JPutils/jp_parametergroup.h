#pragma once

#include "ofMain.h"

class JPParameter {
public:
	void setup(float _var,string name);
	void setup(bool _var, string name);
	void update();
	int variabletype;
	enum VariableType
	{
		BOOL,
		FLOAT
	};
	enum MovType {
		STANDART,
		OSC,
		GODER,
		GOIZQ,
		RANDOM ,
	};
	int movtype;

	string name;
	float floatValue;
	float floatLerpValue;//ESTO ES PARA QUE ME CALCULE EL LERP. VAMOS A PROBARLO.
	
	float speed;
	bool boolValue;

	float min;
	float max;

	bool needsUpdate;
	//float speed;
private :
	bool dir;
	float seed;
};

class JPParameterGroup {
public:
	enum VariableType
	{
		BOOL,
		FLOAT
	};
	string name;
	void addFloatValue(float _var, string name);
	void addBoolValue(bool _var, string name);
	void clear();
	void coutData();
	string getName(int _index);
	//GETTERS : 
	int getSize();
	int getType(int _index);
	float getSpeed(int _index);
	float getFloatValue(int _index);
	float getLerpValue(int _index);
	float getMin(int _index);
	float getMax(int _index);
	bool getBoolValue(int _index);
	int getMovType(int _index);
	JPParameter *getJParameter(int _index);
	//SETTERS: 
	void setFloatValue(float _val, int _index);
	void setFloatLerpValue(float _val, int _index);
	void setBoolValue(bool _val, int _index);
	void setMin(float _val, int _index);
	void setMax(float _val, int _index);
	void setName(string _name);
	void setSpeed(float _val, int _index);
	void update();
	void setmovetype(int _movetype, int _index);
	vector <JPParameter*> parameters;
private : 
	
	
};