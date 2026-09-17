#pragma once

#include "ofMain.h"
#include "jp_text_input.h"
#include "jp_constants.h"
#include <string>
#include <algorithm>

// Compatibility geometry helpers. Editing and interaction live in jp_text_input.
namespace jp_textfield
{
	// The visible slice of `text` when it is wider than the field: start is
	// pushed forward until the caret is inside the window, then end is pulled
	// back until what remains fits. Extracted because the shader-index screen
	// had this loop written out twice - once to draw and once to hit-test - and
	// the MIDI keymap was about to become a third and fourth copy.
	struct Window
	{
		int start = 0;
		int end = 0;
	};

	inline Window visibleWindow(ofTrueTypeFont &font, const std::string &text,
		int cursor, float maxW)
	{
		Window w;
		w.end = (int)text.size();
		cursor = std::max(0, std::min(cursor, (int)text.size()));
		while (w.start < cursor &&
			font.stringWidth(text.substr(w.start, cursor - w.start)) > maxW)
		{
			w.start = jp_text_edit::next(text, w.start);
		}
		while (w.end > w.start &&
			font.stringWidth(text.substr(w.start, w.end - w.start)) > maxW)
		{
			w.end = jp_text_edit::previous(text, w.end);
		}
		return w;
	}

	// Cursor index within `shown` for a click at mouseX, snapping to the
	// nearest glyph boundary rather than the glyph you happened to land on.
	inline int cursorFromX(ofTrueTypeFont &font, const std::string &shown,
		float textX, float mouseX)
	{
		int cursor = 0;
		const float relativeX = std::max(0.0f, mouseX - textX);
		for (int p = 0, i = jp_text_edit::next(shown, 0); p < (int)shown.size(); p = i, i = jp_text_edit::next(shown, i))
		{
			const float before = font.stringWidth(shown.substr(0, p));
			const float after = font.stringWidth(shown.substr(0, i));
			if (relativeX < (before + after) * 0.5f) break;
			cursor = i;
		}
		return cursor;
	}

	inline void drawSelection(ofTrueTypeFont &font, const std::string &text,
		float textX, float baselineY, float glyphH)
	{
		ofPushStyle();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 105));
		ofDrawRectangle(textX - 2.0f, baselineY - glyphH + 2.0f,
			std::max(4.0f, font.stringWidth(text) + 4.0f), glyphH);
		ofSetColor(COL_TEXT_PRIMARY);
		font.drawString(text, textX, baselineY);
		ofPopStyle();
	}

	// Blinking caret drawn at the insertion point. textX is the left edge where
	// the string starts; centerY/glyphH describe the row. Uses the font to place
	// the caret after the sub-string before the cursor (proportional-safe).
	inline void drawCaret(ofTrueTypeFont &font, const std::string &text, int cursor,
						   float textX, float centerY, float glyphH)
	{
		if (std::fmod(ofGetElapsedTimef(), 1.0f) >= 0.55f) return; // ~blink
		cursor = std::max(0, std::min(cursor, (int)text.size()));
		float cx = textX + font.stringWidth(text.substr(0, cursor));
		ofPushStyle();
		ofSetColor(COL_ACCENT_CYAN);
		ofSetLineWidth(1);
		ofDrawLine(cx, centerY - glyphH * 0.5f, cx, centerY + glyphH * 0.5f);
		ofPopStyle();
	}
}
