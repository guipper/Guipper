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
	// index in [0, text.size()]. numericOnly restricts printable input to digits;
	// allowLeadingMinus additionally permits one minus sign at the start. When
	// selectAll is supplied, editing replaces the selected value in one stroke.
	inline bool handleKey(std::string &text, int &cursor, int key,
		bool numericOnly = false, bool allowLeadingMinus = false,
		bool *selectAll = nullptr)
	{
		cursor = std::max(0, std::min(cursor, (int)text.size()));
		const bool selected = selectAll != nullptr && *selectAll;
		switch (key)
		{
		case OF_KEY_LEFT:
			cursor = selected ? 0 : std::max(0, cursor - 1);
			if (selectAll != nullptr) *selectAll = false;
			return true;
		case OF_KEY_RIGHT:
			cursor = selected ? (int)text.size() :
				std::min((int)text.size(), cursor + 1);
			if (selectAll != nullptr) *selectAll = false;
			return true;
		case OF_KEY_HOME:
			cursor = 0;
			if (selectAll != nullptr) *selectAll = false;
			return true;
		case OF_KEY_END:
			cursor = (int)text.size();
			if (selectAll != nullptr) *selectAll = false;
			return true;
		case OF_KEY_BACKSPACE:
			if (selected) { text.clear(); cursor = 0; }
			else if (cursor > 0) { text.erase(text.begin() + (cursor - 1)); cursor--; }
			if (selectAll != nullptr) *selectAll = false;
			return true;
		case OF_KEY_DEL:
			if (selected) { text.clear(); cursor = 0; }
			else if (cursor < (int)text.size()) { text.erase(text.begin() + cursor); }
			if (selectAll != nullptr) *selectAll = false;
			return true;
		default:
			break;
		}
		const bool digit = key >= '0' && key <= '9';
		const bool leadingMinus = numericOnly && allowLeadingMinus && key == '-' &&
			(selected || (cursor == 0 && text.find('-') == std::string::npos));
		bool printable = numericOnly ? (digit || leadingMinus) :
			(key >= 32 && key <= 126);
		if (printable)
		{
			if (selected) { text.clear(); cursor = 0; }
			text.insert(text.begin() + cursor, (char)key);
			cursor++;
			if (selectAll != nullptr) *selectAll = false;
			return true;
		}
		return false;
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
