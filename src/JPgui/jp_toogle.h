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
	// A press only counts for this control if it STARTED on it. Without this,
	// holding the button anywhere - dragging a cable out of a box' OUT, moving a
	// box, sweeping a marquee - and passing over a toggle fired it, because the
	// test was just "pressed AND hovered".
	//
	// Both default to a press already in progress, so a toggle built while the
	// button is down stays disarmed until it has seen a full release: a
	// controller rebuild mid-press must not hand a live press to a fresh
	// object.
	bool pressWasDown = true;
	bool pressStartedHere = false;

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