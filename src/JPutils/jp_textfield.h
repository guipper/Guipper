#pragma once

#include "ofMain.h"
#include "jp_constants.h"
#include <string>
#include <algorithm>

// Shared single-line text-field editing: an insertion cursor with
// LEFT/RIGHT/HOME/END, BACKSPACE/DEL, and insert-at-cursor. Reused by every
// editable field (options, save-as modal, shader search, tab rename) so they
// all behave consistently instead of append-only.
namespace jp_textfield
{
	// Handle one keystroke. Returns true if consumed. `cursor` is the insertion
	// index in [0, text.size()]. numericOnly restricts printable input to digits.
	inline bool handleKey(std::string &text, int &cursor, int key, bool numericOnly = false)
	{
		cursor = std::max(0, std::min(cursor, (int)text.size()));
		switch (key)
		{
		case OF_KEY_LEFT:  if (cursor > 0) cursor--; return true;
		case OF_KEY_RIGHT: if (cursor < (int)text.size()) cursor++; return true;
		case OF_KEY_HOME:  cursor = 0; return true;
		case OF_KEY_END:   cursor = (int)text.size(); return true;
		case OF_KEY_BACKSPACE:
			if (cursor > 0) { text.erase(text.begin() + (cursor - 1)); cursor--; }
			return true;
		case OF_KEY_DEL:
			if (cursor < (int)text.size()) { text.erase(text.begin() + cursor); }
			return true;
		default:
			break;
		}
		bool printable = numericOnly ? (key >= '0' && key <= '9') : (key >= 32 && key <= 126);
		if (printable)
		{
			text.insert(text.begin() + cursor, (char)key);
			cursor++;
			return true;
		}
		return false;
	}

	// Blinking caret drawn at the insertion point. textX is the left edge where
	// the string starts; centerY/glyphH describe the row. Uses the font to place
	// the caret after the sub-string before the cursor (proportional-safe).
	inline void drawCaret(ofTrueTypeFont &font, const std::string &text, int cursor,
						   float textX, float centerY, float glyphH)
	{
		if ((ofGetFrameNum() / 25) % 2 != 0) return; // ~blink
		cursor = std::max(0, std::min(cursor, (int)text.size()));
		float cx = textX + font.stringWidth(text.substr(0, cursor));
		ofPushStyle();
		ofSetColor(COL_ACCENT_CYAN);
		ofSetLineWidth(1);
		ofDrawLine(cx, centerY - glyphH * 0.5f, cx, centerY + glyphH * 0.5f);
		ofPopStyle();
	}
}
