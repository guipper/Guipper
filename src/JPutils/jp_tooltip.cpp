#include "jp_tooltip.h"

#include "jp_constants.h"
#include "jp_pointer.h"
#include "../defines.h"

#include <algorithm>

namespace
{
	constexpr float kPadX = 8.0f;
	constexpr float kPadY = 5.0f;
	constexpr float kGap = 8.0f;
	constexpr float kMargin = 6.0f;

	// One slot, not a queue: only one thing can be under the pointer.
	struct Pending
	{
		std::string text;
		ofRectangle bounds;      // in the caller's own coordinate space
		glm::mat4 modelview{1.0f};
		bool active = false;
	};
	Pending &pending()
	{
		static Pending value;
		return value;
	}

	// Which widget the hover timer is currently counting for, and since when.
	// A single pair, because only one widget can be hovered - and that is exactly
	// what lets immediate-mode widgets, which have nowhere to store a timer of
	// their own, have tooltips.
	std::string &hoveredKey()
	{
		static std::string value;
		return value;
	}
	uint64_t &hoverStartMillis()
	{
		static uint64_t value = 0;
		return value;
	}

	bool hoverMatured(const std::string &key, bool hovered)
	{
		if (!hovered)
		{
			// Only clear if THIS widget owned the timer. Clearing unconditionally
			// would let every un-hovered widget wipe the hovered one's timer,
			// depending on draw order.
			if (hoveredKey() == key)
			{
				hoveredKey().clear();
				hoverStartMillis() = 0;
			}
			return false;
		}
		if (hoveredKey() != key)
		{
			hoveredKey() = key;
			hoverStartMillis() = ofGetElapsedTimeMillis();
		}
		return hoverStartMillis() != 0 &&
			ofGetElapsedTimeMillis() - hoverStartMillis() >=
				jp_tooltip::kHoverDelayMillis;
	}

	void record(const std::string &text, const ofRectangle &bounds)
	{
		Pending &slot = pending();
		slot.text = text;
		slot.bounds = bounds;
		// The whole point: captured HERE, in whatever space the caller is drawing
		// in, and reconciled at flush time.
		slot.modelview = ofGetCurrentMatrix(OF_MATRIX_MODELVIEW);
		slot.active = true;
	}
}

namespace jp_tooltip
{

void draw(const std::string &text, float x, float y, float width, float height)
{
	if (text.empty()) return;

	// Screen-space rect test, gated on the pointer layer so a covered widget
	// stays quiet. Without the gate a tooltip fires straight through an open
	// dropdown or modal, which is what it used to do.
	const bool hovered = jp_pointer::available() &&
		ofRectangle(x, y, width, height).inside(
			(float)ofGetMouseX(), (float)ofGetMouseY());

	// Identity for the hover timer. Rounded to whole pixels so sub-pixel layout
	// jitter does not restart the timer every frame.
	const std::string key = text + "@" + ofToString((int)x) + ":" +
		ofToString((int)y) + ":" + ofToString((int)width) + ":" +
		ofToString((int)height);

	if (!hoverMatured(key, hovered)) return;
	record(text, ofRectangle(x, y, width, height));
}

void draw(const std::string &text, const ofRectangle &bounds)
{
	draw(text, bounds.x, bounds.y, bounds.width, bounds.height);
}

void drawFor(const std::string &text, const ofRectangle &bounds,
			 bool hovered, const std::string &key)
{
	if (text.empty()) return;
	if (!hoverMatured(key, hovered)) return;
	record(text, bounds);
}

void request(const std::string &text, const ofRectangle &bounds)
{
	if (text.empty()) return;
	record(text, bounds);
}

std::string fit(const std::string &text, float maxWidth,
				const std::function<float(const std::string &)> &measure)
{
	if (!measure || maxWidth <= 0.0f) return text;
	if (measure(text) <= maxWidth) return text;

	// Drop characters until the text plus its ellipsis fits. Linear rather than a
	// binary search because a tooltip is short and this runs at most once a frame.
	const std::string ellipsis = "...";
	if (measure(ellipsis) > maxWidth)
	{
		// Not even the ellipsis fits; nothing sensible to show.
		return std::string();
	}
	std::string kept = text;
	while (!kept.empty() && measure(kept + ellipsis) > maxWidth)
	{
		kept.pop_back();
	}
	return kept + ellipsis;
}

ofRectangle layout(const ofRectangle &anchorScreen,
				   float textWidth, float lineHeight,
				   float screenWidth, float screenHeight)
{
	const float width = textWidth + kPadX * 2.0f;
	const float height = lineHeight + kPadY * 2.0f;

	// Centred over the widget, above it.
	float x = anchorScreen.x + anchorScreen.width * 0.5f - width * 0.5f;
	float y = anchorScreen.y - height - kGap;

	// No room above: flip below rather than run off the top. Widgets parked at
	// the top of the window are the ordinary case, not an edge case.
	if (y < kMargin)
	{
		y = anchorScreen.y + anchorScreen.height + kGap;
	}

	// Kept inside the window. A tooltip clipped away is indistinguishable from
	// one that never fired, so this is not cosmetic. Guarded because clamping to
	// an impossible range would pin an oversized tooltip to the left margin -
	// fit() is what keeps it from getting that wide in the first place.
	if (width + kMargin * 2.0f < screenWidth)
	{
		x = ofClamp(x, kMargin, screenWidth - width - kMargin);
	}
	if (height + kMargin * 2.0f < screenHeight)
	{
		y = ofClamp(y, kMargin, screenHeight - height - kMargin);
	}
	return ofRectangle(x, y, width, height);
}

bool resolvePending(const glm::mat4 &flushBase, std::string &text,
					ofRectangle &anchorScreen)
{
	Pending &slot = pending();
	if (!slot.active) return false;

	// At request time the modelview was base * caller, and at flush time it is
	// back to base, so this leaves exactly the caller's own transform - the
	// canvas pan and zoom, or identity for a plain screen-space caller.
	//
	// The base cannot simply be assumed to be the identity: openFrameworks sets
	// up 2D drawing with glm::perspective plus a glm::lookAt, so the raw
	// modelview maps into eye space, not pixels. Dividing it out is what makes
	// this work without knowing anything about oF's camera or its vertical flip.
	const glm::mat4 relative = glm::inverse(flushBase) * slot.modelview;

	// Both corners, so a zoomed canvas yields a correspondingly scaled anchor
	// and the tooltip sits against the box as it actually appears on screen.
	const glm::vec4 topLeft = relative *
		glm::vec4(slot.bounds.x, slot.bounds.y, 0.0f, 1.0f);
	const glm::vec4 bottomRight = relative *
		glm::vec4(slot.bounds.x + slot.bounds.width,
			slot.bounds.y + slot.bounds.height, 0.0f, 1.0f);

	anchorScreen = ofRectangle(topLeft.x, topLeft.y,
		bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
	text = slot.text;
	return true;
}

void drawPending()
{
	std::string text;
	ofRectangle anchor;
	if (!resolvePending(ofGetCurrentMatrix(OF_MATRIX_MODELVIEW), text, anchor))
	{
		return;
	}
	// Cleared as it is drawn. Hovering re-requests every frame, so a tooltip
	// vanishes the moment the pointer leaves with no extra bookkeeping here.
	pending().active = false;

	const float screenWidth = (float)ofGetWidth();
	const float screenHeight = (float)ofGetHeight();

	// Truncate before measuring for layout, or an over-long string overflows the
	// window no matter where the box is placed.
	text = fit(text, screenWidth - (kPadX + kMargin) * 2.0f,
		[](const std::string &candidate) {
			return jp_constants::p_font.stringWidth(candidate);
		});
	if (text.empty()) return;

	// getLineHeight, NOT stringHeight: stringHeight measures the glyphs it is
	// given, so "Pause" and "Bypass" produced different box heights because of
	// the descender on the y. The line height is the same for any text.
	const float lineHeight = jp_constants::p_font.getLineHeight();
	const ofRectangle box = layout(anchor,
		jp_constants::p_font.stringWidth(text), lineHeight,
		screenWidth, screenHeight);

	// Pushed and popped because this runs at the end of ofApp::draw: the canvas
	// loop leaves OF_RECTMODE_CENTER set, and leaking anything from here would
	// reach whatever draws next.
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofFill();
	ofSetColor(0, 0, 0, 100);
	ofDrawRectRounded(box.x + 2.0f, box.y + 2.0f, box.width, box.height, 4.0f);
	ofSetColor(COL_BG_INPUT, 245);
	ofDrawRectRounded(box.x, box.y, box.width, box.height, 4.0f);
	ofNoFill();
	ofSetColor(COL_BORDER_HOVER, 220);
	ofDrawRectRounded(box.x, box.y, box.width, box.height, 4.0f);
	ofFill();
	ofSetColor(COL_TEXT_PRIMARY);
	// Baseline from the ascender rather than the measured glyph height, so the
	// text sits at the same place regardless of which letters it contains.
	jp_constants::p_font.drawString(text, box.x + kPadX,
		box.y + kPadY + jp_constants::p_font.getAscenderHeight());
	ofPopStyle();
}

}
