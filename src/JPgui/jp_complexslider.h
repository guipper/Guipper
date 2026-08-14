#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_slider.h"
#include "jp_knob.h"
#include "jp_toogle.h"
#include "jp_tooglelist.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_dragobject.h"

class JPHandler : public JPdragobject
{
public:
	JPHandler();
	~JPHandler();

	void setup(float _x, float _y, float _w, float _h);
	void draw();
	int paleta;
	bool useTexture;
	bool isLeft; // esta es solo para saber que imagen poner
};

class JPComplexSlider : public JPcontroller
{
public:
	struct LayoutMetrics
	{
		float automatedHeight = 50.0f;
		float modifierHeight = 80.0f;
		float expandedAudioHeight = 166.0f;
		float primaryRowOffset = 25.0f;
		float secondRowOffset = 35.0f;
		float shapingFirstOffset = 62.0f;
		float shapingRowStep = 30.0f;
		float shapingControlHeight = 24.0f;
		float shapingColumnGap = 10.0f;
	};
	static const LayoutMetrics &layoutMetrics();
	static float requiredHeight(const JPParameter *parameter,
		float standardHeight);

	enum AudioShapingControl
	{
		AUDIO_SHAPING_NONE = 0,
		AUDIO_SHAPING_AMOUNT,
		AUDIO_SHAPING_THRESHOLD,
		AUDIO_SHAPING_CURVE,
		AUDIO_SHAPING_ATTACK,
		AUDIO_SHAPING_RELEASE
	};

	JPComplexSlider();
	~JPComplexSlider();
	// JPTooglelist boton_collapse; //LO PONGO ACA PORQUE SI NO , NO ME DEJA OBTENERLO EN EL FOR.
	JPToogle boton_collapse;
	JPToogle boton_idayvuelta;
	JPToogle boton_bpm;
	JPdragobject bpm_rate_button;
	// Audio mode. BPM and AUDIO are mutually exclusive, so the source chip
	// reuses the rate chip's slot and the row keeps its height.
	JPToogle boton_audio;
	// The movtype this row's geometry was built for. setPosAndSize runs only on
	// a controller rebuild, but several buttons change movtype from inside
	// draw(), so the row can end up drawn against a layout for another mode.
	// JPboxgroup compares this each frame and rebuilds when it drifts.
	int builtForMovtype = -1;
	// Same reason: the rhythm sources add a division chip, which changes where
	// the second row starts.
	int builtForAudioSource = -1;
	float primaryRowY = 0.0f;
	JPdragobject audio_source_button;
	JPdragobject audio_div_button;
	JPdragobject audio_shape_button;
	JPdragobject audio_amount_button;
	JPdragobject audio_invert_button;
	JPdragobject audio_threshold_button;
	JPdragobject audio_curve_button;
	JPdragobject audio_attack_button;
	JPdragobject audio_release_button;

	JPKnob slider_speed;
	JPSlider slider_value;
	ofColor testcol;

	void setup(float _x,
			   float _y,
			   float _width,
			   float _height,
			   JPParameter *_parameters);

	float getValue();

	void draw();
	void update();
	int audioShapingControlAt(float mouseX, float mouseY) const;
	bool setAudioShapingControlFromMouse(int control, float mouseX);
	float audioShapingControlNormalized(int control) const;
	int rangeHandleAt(float mouseX, float mouseY) const;
	bool setRangeHandleFromMouse(int handle, float mouseX);
	bool valueSliderContains(float mouseX, float mouseY) const;
	float valueFromMouse(float mouseX) const;
	// void setMoveType(int _movtype);

	// void setPos(float _x, float _y);
	void setPosAndSize();
	JPHandler handler1, handler2;
	bool rangeHandleDragging = false;
};
