#pragma once

#include "ofMain.h"
#include "jp_button.h"
#include "../JPutils/jp_constants.h"

// The single definition of what a screen looks like.
//
// SETTINGS, HELP, IMPORT, EDITOR and MIDI each grew their own frame, and they
// agreed on almost nothing: origins at 30/44 and 40/40, corner radii of 6, 8
// and 12, four different borders, five title styles, two fonts. Ten separate
// copies of "rounded rect + border + title" existed across the codebase.
//
// Everything framed now measures from here. NODES is deliberately excluded: it
// is a full-bleed workspace, not a document, and a frame would only cost it
// canvas area.
namespace jp_screen
{
	// The top tab bar occupies y 8..36. Screens begin 8px below it.
	constexpr float kMarginX = 30.0f;
	constexpr float kTop = 44.0f;
	constexpr float kMarginBottom = 30.0f;

	constexpr float kRadius = 8.0f;
	constexpr float kPad = 15.0f;
	// Title baseline, then subtitle baseline, then the hairline rule.
	constexpr float kTitleBaseline = 30.0f;
	constexpr float kSubtitleBaseline = 47.0f;
	// The rule sat one pixel under the subtitle's baseline, so the grey text
	// collided with the line and its descenders crossed it.
	constexpr float kRuleY = kSubtitleBaseline + 13.0f;
	// Content clears the rule by the same amount the rule clears the subtitle,
	// so the header reads as one evenly-spaced block on every screen.
	constexpr float kHeaderGap = 13.0f;
	constexpr float kHeaderH = kRuleY + kHeaderGap;
	// Readable column cap for screens whose content would otherwise stretch
	// across a very wide window.
	constexpr float kContentMaxW = 620.0f;

	// Standard outer rect. widthFraction < 1 keeps a screen narrow (IMPORT uses
	// half, so the node canvas stays visible behind it).
	inline ofRectangle frame(float widthFraction = 1.0f)
	{
		const float full = ofGetWidth() - kMarginX * 2.0f;
		const float w = std::max(300.0f, full * widthFraction);
		return ofRectangle(kMarginX, kTop, w,
			std::max(300.0f, ofGetHeight() - kTop - kMarginBottom));
	}

	// Content area: inside the padding, below the header rule.
	inline ofRectangle body(const ofRectangle &f)
	{
		return ofRectangle(f.x + kPad, f.y + kHeaderH,
			std::max(0.0f, f.width - kPad * 2.0f),
			std::max(0.0f, f.height - kHeaderH - kPad));
	}

	// Right-aligned header action slots; slot 0 is the rightmost.
	inline ofRectangle actionSlot(const ofRectangle &f, int slot,
		float w = jp_button::kWidth)
	{
		// Fixed-width slots, right to left. The comment used to claim wider
		// buttons pushed their neighbours, but the step was kWidth regardless
		// of `w`, so anything wider simply overlapped the slot to its left.
		const float x = f.getMaxX() - kPad - w -
			(float)slot * (jp_button::kWidth + jp_button::kGap);
		return ofRectangle(x, f.y + 10.0f, w, jp_button::kHeight);
	}

	// Right-to-left run of buttons whose widths differ. Pass the widths of the
	// slots to the RIGHT of this one so they actually tile.
	inline ofRectangle actionSlotRun(const ofRectangle &f,
		const std::vector<float> &widthsRightToLeft, int slot)
	{
		float x = f.getMaxX() - kPad;
		for (int i = 0; i <= slot && i < (int)widthsRightToLeft.size(); i++)
		{
			x -= widthsRightToLeft[i];
			if (i < slot) x -= jp_button::kGap;
		}
		const float w = slot < (int)widthsRightToLeft.size() ?
			widthsRightToLeft[slot] : jp_button::kWidth;
		return ofRectangle(x, f.y + 10.0f, w, jp_button::kHeight);
	}

	// Background, border, title, optional subtitle, and the hairline rule that
	// separates the header from the body.
	inline void drawFrame(const ofRectangle &f, const std::string &title,
		const std::string &subtitle = "")
	{
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);

		ofFill();
		ofSetColor(ofColor(COL_BG_DARK, 235));
		ofDrawRectRounded(f.x, f.y, f.width, f.height, kRadius);

		ofNoFill();
		ofSetLineWidth(1.5f);
		ofSetColor(ofColor(COL_ACCENT_CYAN, 80));
		ofDrawRectRounded(f.x, f.y, f.width, f.height, kRadius);
		ofFill();

		ofSetColor(COL_ACCENT_CYAN);
		jp_constants::p_font.drawString(title,
			f.x + kPad, f.y + kTitleBaseline);

		if (!subtitle.empty())
		{
			ofSetColor(COL_TEXT_MUTED);
			jp_constants::p_font.drawString(subtitle,
				f.x + kPad, f.y + kSubtitleBaseline);
		}

		ofSetColor(ofColor(COL_BORDER_MUTED, 150));
		ofSetLineWidth(1.0f);
		const float ruleY = f.y + kRuleY;
		ofDrawLine(f.x + kPad, ruleY, f.getMaxX() - kPad, ruleY);

		ofPopStyle();
	}
}
