#pragma once

#include "ofMain.h"
#include <cstdint>
#include <functional>
#include <string>

// Hover tooltips, for anything in the program.
//
// DEFERRED, not immediate. draw() records a request and jp_tooltip::drawPending()
// paints it once at the very end of ofApp::draw. Drawing in place looked simpler
// but put every tooltip underneath whatever was drawn after it: a box tooltip
// landed beneath the next box, and a panel tooltip beneath the next panel. Same
// reasoning that already keeps jp_hitbox's debug overlay outside JPbox::draw.
//
// COORDINATE SPACES ARE HANDLED FOR YOU. draw() captures the modelview in effect
// when it is called, and drawPending() resolves it against the matrix in effect
// at flush time. A caller drawing in plain screen space contributes an identity
// transform and needs to know nothing about this; a caller inside the node
// canvas' ofTranslate(pan)/ofScale(zoom) gets its anchor mapped to the right
// pixels. The tooltip itself is always laid out in screen pixels afterwards, so
// it stays the same readable size at any zoom.
//
// Only one thing can be under the pointer, so there is a single pending slot and
// a single hover timer. That is what lets IMMEDIATE-MODE widgets - the screen
// tabs, the SETTINGS buttons, the inspector header actions - have tooltips at
// all: they are rebuilt from locals every frame and have nowhere to keep state.
namespace jp_tooltip
{
	// How long the pointer has to rest before the tooltip appears.
	constexpr uint64_t kHoverDelayMillis = 650;

	// The common case: a screen-space widget, given as a CORNER rect. Does its
	// own hover test, and honours jp_pointer so a tooltip cannot fire for a
	// widget that is covered by a dropdown or a modal.
	void draw(const std::string &text, float x, float y,
			  float width, float height);
	void draw(const std::string &text, const ofRectangle &bounds);

	// For widgets whose hover test is NOT a screen-space rect test - anything on
	// the zoomed node canvas, where JPdragobject::mouseOver() works in canvas
	// space through the mouse override. Pass that result in.
	//
	// `key` identifies the widget for the hover timer. Pass something stable,
	// such as a box uid: the screen-space overload derives its key from the rect,
	// which cannot work here because panning the canvas would change the key
	// every frame and restart the timer forever.
	void drawFor(const std::string &text, const ofRectangle &bounds,
				 bool hovered, const std::string &key);

	// Records a request unconditionally - no hover test, no delay. For a caller
	// that already owns its own timing, and the seam the transform tests use so
	// they do not have to wait out the hover delay.
	void request(const std::string &text, const ofRectangle &bounds);

	// Paints the pending request. Call EXACTLY ONCE per frame, after everything
	// else, and with no matrix of your own pushed - the transform resolution
	// assumes the matrix here is the frame's base.
	void drawPending();

	// --- Exposed for testing -------------------------------------------------

	// Where the tooltip box lands, as a CORNER rect. Takes its measurements
	// rather than a font so it can be checked without a window.
	ofRectangle layout(const ofRectangle &anchorScreen,
					   float textWidth, float lineHeight,
					   float screenWidth, float screenHeight);

	// Resolves the pending anchor into screen space against the flush-time base
	// matrix. False when nothing is pending.
	bool resolvePending(const glm::mat4 &flushBase, std::string &text,
						ofRectangle &anchorScreen);

	// Shortens text with an ellipsis until it fits maxWidth. `measure` returns
	// the pixel width of a string; injected so this is testable without a font.
	std::string fit(const std::string &text, float maxWidth,
					const std::function<float(const std::string &)> &measure);
}
