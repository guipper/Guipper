#pragma once

#include "defines.h"
#include "ofMain.h"
// Esta clase es solo para trabajar los objetos que se agarran o no se agarran. CORTA LA BOCHEN
// One switch for every clickable-area overlay in the app: GUIPPER_HITBOX=1.
//
// Runtime rather than a compile-time constant so the boxes can be checked in a
// running session without a rebuild - the pre-existing kShowInspectorClickBounds
// flags were constexpr false, which meant nobody could actually use them.
namespace jp_hitbox
{
	bool debugEnabled();
	void draw(const ofRectangle &bounds, bool hovered, bool enabled = true);
}

class JPdragobject
{
public:
	JPdragobject();
	~JPdragobject();

	float x, y;
	float width;
	float height;

	virtual void setPos(float _x, float _y)
	{
		x = _x;
		y = _y;
	}
	void setup(float _x, float _y, float _width, float _height);
	// virtual void draw();

	bool activeFlag;

	static void setMouseOverride(const ofVec2f &_mouse);
	static void clearMouseOverride();
	static float getMouseX();
	static float getMouseY();

	// Extra clickable margin around the drawn rect. Drawing uses width/height;
	// hit testing uses those PLUS this, so a control can be easier to hit
	// without changing how it looks. The two box toggles are 12.6px squares on
	// a canvas that also zooms out, which is well under any comfortable target
	// - and growing the square instead would redesign the box.
	float hitPaddingX = 0.0f;
	float hitPaddingY = 0.0f;
	// The rect actually hit-tested. Debug drawing uses this too, so what you
	// see outlined is exactly what responds - a separate rect for the overlay
	// could disagree with the real one and would be worse than no overlay.
	ofRectangle hitBounds() const;

	virtual bool mouseOver(); // Si esta encima del slider
	virtual bool mouseGrab(); // Si esta agarrado
							  // bool mouseClick();

	// Where the press currently in progress BEGAN, recorded once from ofApp's
	// real mouse events.
	//
	// The controls that actuate from inside draw() - JPToogle above all - need
	// "did this press start on me", or holding the button anywhere (dragging a
	// cable out of a box' OUT, moving a box, sweeping a marquee) fires whatever
	// it passes over. That used to be a per-object latch, which cannot work
	// here: JPboxgroup::update_mousePressed ENDS with setControllers() whenever
	// an inspector is open, so every press destroys and rebuilds the very
	// object holding the latch, and the rebuilt one - correctly assuming a
	// press is already in progress - stayed disarmed forever.
	//
	// The origin belongs to the gesture, not to any widget, so it lives here
	// and survives any number of rebuilds.
	static void notePressOrigin(float screenX, float screenY);
	// The same press, in CANVAS space. Box headers are drawn under the canvas
	// mouse override, so their hitBounds() are canvas coordinates: testing a
	// screen-space origin against them silently stops matching the moment the
	// canvas is panned or zoomed. Recorded by JPboxgroup::update_mousePressed,
	// which already computes this point.
	static void notePressOriginCanvas(float canvasX, float canvasY);
	static void clearPressOrigin();
	bool pressStartedHere() const;

protected:
	float isGrabbed2;
	static bool useMouseOverride;
	static ofVec2f mouseOverride;
	static bool pressOriginValid;
	static ofVec2f pressOrigin;
	static ofVec2f pressOriginCanvas;
};
