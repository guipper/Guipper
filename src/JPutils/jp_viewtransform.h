#pragma once

#include "ofMain.h"

// Maps between a normalized 0..1 canvas and the screen rectangle a panel shows
// it in, under a pan/zoom view.
//
// This exists because the same pair of expressions is written out by hand five
// times in JPboxgroup_mapping_advanced.cpp - once in the overlay and once in
// each mouse path - and they have to agree exactly or a click lands somewhere
// other than where the cursor is. New editor code goes through this instead.
struct JPViewTransform
{
	// The letterboxed screen rect the canvas fills at zoom 1.
	ofRectangle preview;
	float zoom = 1.0f;
	ofVec2f center = ofVec2f(0.5f, 0.5f);

	ofVec2f toScreen(const ofVec2f &uv) const
	{
		return ofVec2f(
			preview.x + (0.5f + (uv.x - center.x) * zoom) * preview.width,
			preview.y + (0.5f + (uv.y - center.y) * zoom) * preview.height);
	}

	// Deliberately NOT clamped to 0..1. A stroke is allowed to run off the
	// canvas, and clamping here would flatten the overhang onto the border.
	ofVec2f toUv(const ofVec2f &screen) const
	{
		const float w = std::max(1.0f, preview.width);
		const float h = std::max(1.0f, preview.height);
		const float z = std::max(0.0001f, zoom);
		return ofVec2f(
			center.x + ((screen.x - preview.x) / w - 0.5f) / z,
			center.y + ((screen.y - preview.y) / h - 0.5f) / z);
	}

	// The screen rect the whole canvas occupies. What a texture gets drawn into.
	ofRectangle canvasRect() const
	{
		const ofVec2f topLeft = toScreen(ofVec2f(0.0f, 0.0f));
		const ofVec2f bottomRight = toScreen(ofVec2f(1.0f, 1.0f));
		return ofRectangle(topLeft.x, topLeft.y,
			bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
	}
};

namespace jp_view
{
	// Largest rect of the given aspect that fits inside `area`, centred.
	inline ofRectangle fit(const ofRectangle &area, float aspect)
	{
		if (aspect <= 0.0f || area.width <= 0.0f || area.height <= 0.0f)
		{
			return area;
		}
		float w = area.width;
		float h = w / aspect;
		if (h > area.height)
		{
			h = area.height;
			w = h * aspect;
		}
		return ofRectangle(area.x + (area.width - w) * 0.5f,
			area.y + (area.height - h) * 0.5f, w, h);
	}
}
