#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_controller.h"
#include "../JPutils/jp_constants.h"

class JPToogle : public JPcontroller
{
public:
	void setup(float _x, float _y, float _width, float _height, string _name, bool _boolValue);
	void setup(float _x, float _y, float _width, float _height);
	void setUseTexture(int _as);
	void drawSelectedTexture();
	void draw();

	// Draw as an on/off SWITCH rather than a filled bar: a dim row with the
	// label on the left and a small pill on the right, matching how a slider row
	// reads (label left, indicator right). The filled bar spanned the whole row
	// exactly like a slider at maximum, so a boolean looked like a value.
	//
	// Opt-in, because the same class also backs the little on/off and bypass
	// squares on a box header, which paint over it and must keep their shape.
	void setSwitchStyle(bool enabled) { switchStyle = enabled; }
	bool switchStyle = false;

	void update_movtype();
	void drawAsSwitch(); // Esto es para poner dentro de una funcion directamente el trigger

	bool activable; // VARIABLE DE CONTROL
	// "Did this press start on me" lives in JPdragobject now - see
	// notePressOrigin. It used to be a pair of members here, which could not
	// work: an inspector press rebuilds every controller, so the object holding
	// the latch was destroyed before the latch could fire and the toggles in
	// the parameter panel stopped responding to clicks entirely.

	int textureindex;

	// ESTO ES PARA EL TEXTUREINDEX
	// Collapse = Flechita para abrir el coso.
	enum type
	{
		COLLAPSE,
		IDAYVUELTA,
		GODER,
		GOIZQ,
		RAN,
		BPM_SYNC,
		// Index-aligned with JPParameter::MovType, so update_movtype() can keep
		// using textureindex directly. AUDIO_SRC == MovType::AUDIO == 6.
		AUDIO_SRC
	};

private:
};