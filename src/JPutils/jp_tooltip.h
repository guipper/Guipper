#pragma once

#include "ofMain.h"
#include "jp_constants.h"
#include <algorithm>
#include <cstdint>

namespace jp_tooltip
{
	inline string &hoveredKey()
	{
		static string value;
		return value;
	}

	inline uint64_t &hoverStartMillis()
	{
		static uint64_t value = 0;
		return value;
	}

	inline void draw(const string &text, float x, float y, float width, float height)
	{
		if (text.empty())
		{
			return;
		}

		const float mouseX = ofGetMouseX();
		const float mouseY = ofGetMouseY();
		const bool hovered = mouseX >= x && mouseX <= x + width &&
			mouseY >= y && mouseY <= y + height;
		const string key = text + "@" + ofToString((int)x) + ":" +
			ofToString((int)y) + ":" + ofToString((int)width) + ":" +
			ofToString((int)height);

		if (!hovered)
		{
			if (hoveredKey() == key)
			{
				hoveredKey().clear();
				hoverStartMillis() = 0;
			}
			return;
		}

		if (hoveredKey() != key)
		{
			hoveredKey() = key;
			hoverStartMillis() = ofGetElapsedTimeMillis();
		}
		if (hoverStartMillis() == 0 || ofGetElapsedTimeMillis() - hoverStartMillis() < 650)
		{
			return;
		}

		const float padX = 8.0f;
		const float padY = 5.0f;
		const float tooltipWidth = jp_constants::p_font.stringWidth(text) + padX * 2.0f;
		const float tooltipHeight = jp_constants::p_font.stringHeight(text) + padY * 2.0f;
		float tooltipX = x + width * 0.5f - tooltipWidth * 0.5f;
		float tooltipY = y - tooltipHeight - 8.0f;
		if (tooltipY < 6.0f)
		{
			tooltipY = y + height + 8.0f;
		}
		tooltipX = ofClamp(tooltipX, 6.0f, std::max(6.0f, (float)ofGetWidth() - tooltipWidth - 6.0f));
		tooltipY = ofClamp(tooltipY, 6.0f, std::max(6.0f, (float)ofGetHeight() - tooltipHeight - 6.0f));

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(0, 0, 0, 100);
		ofDrawRectRounded(tooltipX + 2.0f, tooltipY + 2.0f, tooltipWidth, tooltipHeight, 4.0f);
		ofSetColor(COL_BG_INPUT, 245);
		ofDrawRectRounded(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 4.0f);
		ofNoFill();
		ofSetColor(COL_BORDER_HOVER, 220);
		ofDrawRectRounded(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 4.0f);
		ofFill();
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(text, tooltipX + padX,
			tooltipY + padY + jp_constants::p_font.stringHeight(text));
		ofPopStyle();
	}
}
