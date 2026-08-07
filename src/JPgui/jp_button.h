#pragma once

#include "ofMain.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_pointer.h"

// One button renderer for the whole program.
//
// There used to be eight: two lambdas in draw_opciones and the live-output
// panel, another in the wall tab, one for the import footer, a file-local one
// in the MIDI keymap, plus hand-rolled rectangles in HELP, IMPORT, the shader
// editor and the save modal. They disagreed on corner radius, on whether the
// label was centred or left-aligned, and on whether hover did anything at all.
//
// Call sites keep their own layout; only the drawing comes from here.
namespace jp_button
{
	constexpr float kRadius = 4.0f;
	// The standard header/footer action size. Layout code is free to use its
	// own, but new code should start from these.
	constexpr float kWidth = 78.0f;
	constexpr float kHeight = 24.0f;
	constexpr float kGap = 8.0f;

	inline bool hovered(const ofRectangle &r)
	{
		// Buttons under an open dropdown were still highlighting.
		if (!jp_pointer::available()) return false;
		return r.inside((float)ofGetMouseX(), (float)ofGetMouseY());
	}

	inline void draw(const ofRectangle &r, const std::string &label,
		bool active, bool enabled = true,
		const ofColor &accent = COL_ACCENT_CYAN)
	{
		const bool over = enabled && hovered(r);

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);

		ofFill();
		if (!enabled) ofSetColor(ofColor(accent, 55));
		else if (active) ofSetColor(ofColor(accent, 215));
		else if (over) ofSetColor(COL_BG_HOVER);
		else ofSetColor(COL_BG_BUTTON);
		ofDrawRectRounded(r.x, r.y, r.width, r.height, kRadius);

		ofNoFill();
		ofSetLineWidth(1.0f);
		if (!enabled) ofSetColor(ofColor(COL_BORDER_MUTED, 160));
		else if (active || over) ofSetColor(accent);
		else ofSetColor(COL_BORDER_DEFAULT);
		ofDrawRectRounded(r.x, r.y, r.width, r.height, kRadius);
		ofFill();

		// Centred on both axes. Several of the old helpers pinned the label to
		// the left edge or to a fixed baseline, so identical buttons sat at
		// different heights depending on which screen drew them.
		ofSetColor(!enabled ? COL_TEXT_MUTED :
			((active || over) ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		const float textW = jp_constants::p_font.stringWidth(label);
		jp_constants::p_font.drawString(label,
			r.x + (r.width - textW) * 0.5f,
			r.getCenter().y + 4.0f);

		ofPopStyle();
	}
}
