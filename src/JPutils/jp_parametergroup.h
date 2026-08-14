#pragma once

#include "defines.h"
#include "ofMain.h"

class JPParameter
{
public:
	void setup(float _var, string name);
	void setup(bool _var, string name);
	void update();
	int variabletype;
	enum VariableType
	{
		BOOL,
		FLOAT
	};
	// APPEND ONLY. These integers are written verbatim into save files as
	// <movtype>, so inserting a value would silently reinterpret every
	// existing composition.
	enum MovType
	{
		STANDART,
		OSC,
		GODER,
		GOIZQ,
		RANDOM,
		BPM,
		AUDIO,
	};
	enum BpmRate
	{
		BPM_RATE_QUARTER,
		BPM_RATE_HALF,
		BPM_RATE_ONE,
		BPM_RATE_DOUBLE,
		BPM_RATE_QUADRUPLE,
	};
	int movtype;
	// Last non-standard automation mode selected for this parameter. It remains
	// available while automation is disabled and is persisted with the graph.
	int lastMovtype;
	int bpmRate;
	void setAutomationMode(int mode);
	void setLastAutomationMode(int mode);
	void toggleAutomation();
	void cycleAutomationPattern();
	void captureRangeStart();
	void captureRangeEnd();
	void setRangeStart(float value);
	void setRangeEnd(float value);
	void setRangeEnabled(bool enabled);
	void clampToEffectiveRange();
	float effectiveMin() const;
	float effectiveMax() const;

	string name;
	float floatValue;
	float floatLerpValue; // ESTO ES PARA QUE ME CALCULE EL LERP. VAMOS A PROBARLO.

	float speed;
	bool boolValue;
	bool randomLocked;
	float defaultFloatValue;
	bool defaultBoolValue;
	void captureDefaultValue();
	void restoreDefaultValue();

	float min;
	float max;
	// Native slider scale and optional remembered custom limits are separate.
	// min/max remain the serialized custom endpoints for file compatibility.
	float nativeMin;
	float nativeMax;
	bool rangeEnabled;

	// Audio reactivity: which analyser value drives this parameter, and for
	// the rhythm sources which beat subdivision. Kept on JPParameter rather
	// than in a side table so it survives being exposed from inside a group -
	// an exposed slider points at this very object.
	int audioSource;
	int audioDiv;
	bool audioEligible;
	float audioBase;
	float audioAmount;
	bool audioInvert;
	float audioThreshold;
	float audioCurve;
	float audioAttackMs;
	float audioReleaseMs;
	bool audioShapingOpen;

	bool bpmEligible;
	bool needsUpdate;
	float getBpmMultiplier() const;
	void cycleBpmRate();
	void cycleAudioSource();
	void cycleAudioDiv();
	// float speed;
private:
	bool dir;
	float seed;
	float audioSmoothed;
};

class JPParameterGroup
{
public:
	JPParameterGroup();
	~JPParameterGroup();
	JPParameterGroup(const JPParameterGroup& other);
	JPParameterGroup& operator=(const JPParameterGroup& other);

	enum VariableType
	{
		BOOL,
		FLOAT
	};
	string name;
	void addFloatValue(float _var, string name, bool bpmEligible = false);
	void addBoolValue(bool _var, string name);
	void clear();
	void coutData();
	string getName(int _index);
	// Index of a parameter by name, or -1.
	int indexOfName(const string &_name) const;
	// Where a saved <param> should be written: the parameter it names if the
	// box still has one, otherwise the position it appeared at.
	//
	// Loading used to be purely positional, which froze the parameter arrays -
	// reordering, or inserting anywhere but the end, silently shifted every
	// value in every saved composition. Matching on name first lifts that. The
	// positional fallback keeps the old behaviour for a renamed shader uniform,
	// which would otherwise lose its value entirely.
	//
	// Same rule JPboxgroup::copyParametersByNameOrIndex has always used for cue
	// drafts. Returns -1 when neither resolves in range.
	int resolveLoadIndex(const string &_name, int _positionalIndex) const;
	// GETTERS :
	int getSize();
	int getType(int _index);
	float getSpeed(int _index);
	float getFloatValue(int _index);
	float getLerpValue(int _index);
	float getMin(int _index);
	float getMax(int _index);
	float getRangeMin(int _index);
	float getRangeMax(int _index);
	float getNativeMin(int _index);
	float getNativeMax(int _index);
	int getBpmRate(int _index);
	int getAudioSource(int _index);
	int getAudioDiv(int _index);
	float getAudioBase(int _index);
	float getAudioAmount(int _index);
	bool getAudioInvert(int _index);
	float getAudioThreshold(int _index);
	float getAudioCurve(int _index);
	float getAudioAttackMs(int _index);
	float getAudioReleaseMs(int _index);
	bool getBoolValue(int _index);
	int getMovType(int _index);
	int getLastMovType(int _index);
	JPParameter *getJParameter(int _index);
	// SETTERS:
	void setFloatValue(float _val, int _index);
	void setFloatLerpValue(float _val, int _index);
	void setBoolValue(bool _val, int _index);
	void setMin(float _val, int _index);
	void setMax(float _val, int _index);
	void setRangeMin(float _val, int _index);
	void setRangeMax(float _val, int _index);
	void setRangeEnabled(bool enabled, int _index);
	void setName(string _name);
	void setSpeed(float _val, int _index);
	void setBpmRate(int _rate, int _index);
	void setAudioSource(int _source, int _index);
	void setAudioDiv(int _div, int _index);
	void setAudioBase(float value, int index);
	void setAudioAmount(float value, int index);
	void setAudioInvert(bool value, int index);
	void setAudioThreshold(float value, int index);
	void setAudioCurve(float value, int index);
	void setAudioAttackMs(float value, int index);
	void setAudioReleaseMs(float value, int index);
	void update();
	void setmovetype(int _movetype, int _index);
	void setlastmovetype(int _movetype, int _index);
	vector<JPParameter *> parameters;

private:
};
