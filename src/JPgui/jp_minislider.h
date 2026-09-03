#pragma once

#include "ofMain.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_pointer.h"

// The compact labelled value cell: name on the left, value on the right, a
// 2 px track underneath with a dot at the current position.
//
// It was written for the inspector's per-parameter audio shaping (Amount,
// Threshold, Curve, Attack, Release) and lived in an anonymous namespace inside
// jp_complexslider.cpp, which is the only reason it was not already shared. The
// AUDIO screen draws the GLOBAL stage of the same operation, so it uses the
// same cell on purpose: seeing the identical control in both places is what
// makes it obvious the two stages are the same maths applied twice.
//
// Immediate mode. The caller owns the rect, the value and the drag - there is
// no widget object, no polling from draw(), and therefore none of the
// press-latch problems the JPdragobject-derived controls have.
namespace jp_minislider
{
	constexpr float kInset = 7.0f;

	inline bool hovered(const ofRectangle &r)
	{
		if (!jp_pointer::available()) return false;
		return r.inside((float)ofGetMouseX(), (float)ofGetMouseY());
	}

	// `normalized` is 0..1 for the track fill; `value` is the text shown on the
	// right, so the caller stays free to display units the track does not know
	// about (ms, x, dB).
	inline void draw(const ofRectangle &r, const std::string &label,
		const std::string &value, float normalized, bool isHovered,
		bool muted = false, const ofColor &accent = COL_ACCENT_CYAN)
	{
		const float trackY = r.y + r.height - 5.0f;
		const float trackWidth = std::max(1.0f, r.width - kInset * 2.0f);
		const float fillWidth = trackWidth * ofClamp(normalized, 0.0f, 1.0f);

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(isHovered ? ofColor(COL_BG_HOVER, 235) :
			ofColor(COL_BG_INPUT, 210));
		ofDrawRectRounded(r.x, r.y, r.width, r.height, 3.0f);

		ofSetColor(muted ? COL_TEXT_MUTED : COL_TEXT_PRIMARY);
		ofTrueTypeFont &font = jp_constants::p2_font;
		const ofRectangle glyphBounds = font.getStringBoundingBox("Ag", 0.0f, 0.0f);
		const float textY = r.y + 7.5f -
			(glyphBounds.y + glyphBounds.height * 0.5f);
		font.drawString(label, r.x + kInset, textY);
		const float valueWidth = font.stringWidth(value);
		font.drawString(value, r.x + r.width - kInset - valueWidth, textY);

		ofSetColor(ofColor(COL_BORDER_MUTED, 175));
		ofDrawRectangle(r.x + kInset, trackY - 1.0f, trackWidth, 2.0f);
		const ofColor fill = muted ? ofColor(COL_TEXT_MUTED) : accent;
		ofSetColor(fill);
		if (fillWidth > 0.5f)
			ofDrawRectangle(r.x + kInset, trackY - 1.0f, fillWidth, 2.0f);
		ofDrawCircle(r.x + kInset + fillWidth, trackY, isHovered ? 3.0f : 2.5f);
		ofPopStyle();
	}

	// Absolute mapping, matching every other continuous control in the app: the
	// value follows the pointer's x across the track, on press and on drag.
	inline float normalizedFromMouse(const ofRectangle &r, float mouseX)
	{
		return ofMap(mouseX, r.x + kInset, r.x + r.width - kInset,
			0.0f, 1.0f, true);
	}
}
