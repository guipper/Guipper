#pragma once

#include "ofMain.h"
#include <string>
#include <vector>

// The advanced debug panel's data, gathered once per frame and formatted
// separately.
//
// Split from the drawing on purpose: the estimates and the formatting are where
// this can be wrong in a way nobody notices, and neither needs a window to
// check. The gathering itself lives in ofApp, which is the only place that can
// see the graph, the device pools and the render settings at once.
namespace jp_debug
{
	// One live resource: a camera, the Kinect, an NDI receiver.
	struct SourceRow
	{
		std::string kind;    // "CAM", "KINECT", "NDI"
		std::string label;   // device, stream or sender name
		std::string detail;  // status text, free form
		int users = 0;       // how many boxes share it; -1 when not refcounted
		bool live = false;   // has actually produced data
	};

	struct Report
	{
		int renderWidth = 0;
		int renderHeight = 0;
		int boxCount = 0;
		int fboCount = 0;
		unsigned long long fboBytes = 0;
		std::vector<SourceRow> sources;
		float transitionLerp = 0.0f;
		float transitionMs = 0.0f;
		int transitionType = 0;
	};

	// Bytes one RGBA8 render target of this size occupies. Pure so the estimate
	// can be checked: a wrong multiplier here turns the VRAM figure into a
	// confidently-wrong number, which is worse than no number.
	unsigned long long fboBytes(int width, int height, int count);

	// Human-readable size. Binary units, because that is what GPU tools report.
	std::string formatBytes(unsigned long long bytes);

	// Where to break a list of section heights into two columns so the two come
	// out as even as possible. Returns the index of the first section in the
	// SECOND column, always at least 1 so the left column is never empty.
	//
	// Pure because the failure it prevents is invisible in a screenshot until it
	// bites: the panel was one column, grew taller than the window, and silently
	// clipped its last rows off the bottom edge. Balancing is what buys the room
	// that readable spacing needs.
	std::size_t balanceSplit(const std::vector<float> &sectionHeights);
}
